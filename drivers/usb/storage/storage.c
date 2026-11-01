/**
 * @file drivers/usb/storage/storage.c
 * @brief USB mass storage (SCSI transparent, Bulk Only Transport)
 */

#include <kernel/drivers/usb/usb.h>
#include <kernel/drivers/storage/drive.h>
#include <kernel/loader/driver.h>
#include <kernel/mm/alloc.h>
#include <kernel/mm/arch_mmu.h>
#include <kernel/mm/pmm.h>
#include <kernel/debug.h>
#include <errno.h>
#include <string.h>

#define MSC_CLASS           0x08
#define MSC_SCSI            0x06
#define MSC_BULK_ONLY       0x50
#define MSC_RESET           0xff
#define MSC_CBW_SIGNATURE   0x43425355
#define MSC_CSW_SIGNATURE   0x53425355
#define MSC_TIMEOUT_MS      5000
#define MSC_BOUNCE_SIZE     (256 * 1024)  /* Largest data stage per READ/WRITE(10) */
#define MSC_TRB_BOUNDARY    (64 * 1024)   /* A bulk transfer may not cross this */

#define SCSI_INQUIRY        0x12
#define SCSI_READ_CAPACITY  0x25
#define SCSI_READ_10        0x28
#define SCSI_WRITE_10       0x2a
#define SCSI_SYNC_CACHE_10  0x35

#define LOG(status, ...) dprintf_module(status, "DRIVER:USB:STORAGE", __VA_ARGS__)

typedef struct msc_cbw {
    uint32_t signature;
    uint32_t tag;
    uint32_t transfer_length;
    uint8_t flags;
    uint8_t lun;
    uint8_t command_length;
    uint8_t command[16];
} __attribute__((packed)) msc_cbw_t;

typedef struct msc_csw {
    uint32_t signature;
    uint32_t tag;
    uint32_t residue;
    uint8_t status;
} __attribute__((packed)) msc_csw_t;

STATIC_ASSERT(sizeof(msc_cbw_t) == 31);
STATIC_ASSERT(sizeof(msc_csw_t) == 13);

/* One piece of a data stage. Each piece must be physically contiguous. */
typedef struct msc_segment {
    void *buffer;
    size_t length;
} msc_segment_t;

typedef struct msc_device {
    usb_device_t *device;
    usb_interface_t *interface;
    usb_pipe_t *in;
    usb_pipe_t *out;
    drive_t *drive;
    mutex_t lock;
    uint32_t next_tag;
    bool online;
    uintptr_t bounce_phys;  /* Physically contiguous, MSC_BOUNCE_SIZE bytes */
    uint8_t *bounce;
} msc_device_t;

static uint32_t msc_be32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | bytes[3];
}

static void msc_put_be32(uint8_t *bytes, uint32_t value) {
    bytes[0] = value >> 24;
    bytes[1] = value >> 16;
    bytes[2] = value >> 8;
    bytes[3] = value;
}

static usb_status_t msc_bulk(msc_device_t *msc, usb_pipe_t *pipe, void *buffer,
                             size_t length, size_t *actual) {
    usb_transfer_t *transfer = usb_allocateTransfer(msc->device);
    if (!transfer) return USB_NO_MEMORY;

    usb_prepareTransfer(transfer, pipe, buffer, length, USB_TRANSFER_DEFAULT,
                        NULL, MSC_TIMEOUT_MS, NULL);
    usb_status_t status = usb_transferSync(transfer);
    if (actual) *actual = transfer->actual_length;
    usb_freeTransfer(transfer);
    return status;
}

static usb_status_t msc_clearHalt(msc_device_t *msc, usb_pipe_t *pipe) {
    if (msc->device->state == USB_DEVICE_DISCONNECTING) return USB_ABORTED;
    usb_device_request_t request = {
        .bmRequestType = USB_RT_H2D | USB_RT_STANDARD | USB_RT_ENDP,
        .bRequest = USB_REQ_CLEAR_FEATURE,
        .wIndex = pipe->endp->desc.bEndpointAddress,
    };
    usb_status_t status = usb_request(msc->device, &request, NULL);
    if (!USB_ERROR(status)) pipe->endp->toggle = 0;
    return status;
}

/* BOT reset recovery is required after a transport or phase error. */
static void msc_reset(msc_device_t *msc) {
    if (msc->device->state == USB_DEVICE_DISCONNECTING) return;
    usb_device_request_t request = {
        .bmRequestType = USB_RT_H2D | USB_RT_CLASS | USB_RT_INTF,
        .bRequest = MSC_RESET,
        .wIndex = msc->interface->desc.bInterfaceNumber,
    };
    usb_request(msc->device, &request, NULL);

    msc_clearHalt(msc, msc->in);
    msc_clearHalt(msc, msc->out);
}

/* Caller holds lock. One CBW, optional data stage, and exactly one CSW.
   The data stage is split into one bulk transfer per segment; every segment
   but the last must be a multiple of the endpoint's max packet size. */
static int msc_commandv(msc_device_t *msc, const uint8_t *command, uint8_t command_length,
                        const msc_segment_t *segments, size_t nsegments, bool read) {
    if (!msc->online) return -ENODEV;
    if (command_length == 0 || command_length > 16) return -EINVAL;

    uint64_t total = 0;
    for (size_t i = 0; i < nsegments; i++) total += segments[i].length;
    if (total > UINT32_MAX) return -EINVAL;
    uint32_t length = total;

    msc_cbw_t cbw = {
        .signature = MSC_CBW_SIGNATURE,
        .tag = ++msc->next_tag,
        .transfer_length = length,
        .flags = read ? 0x80 : 0,
        .lun = 0,
        .command_length = command_length,
    };
    memcpy(cbw.command, command, command_length);

    size_t actual = 0;
    usb_status_t status = msc_bulk(msc, msc->out, &cbw, sizeof(cbw), &actual);
    if (USB_ERROR(status) || actual != sizeof(cbw)) goto transport_error;

    size_t data_actual = 0;
    usb_pipe_t *data_pipe = read ? msc->in : msc->out;
    for (size_t i = 0; i < nsegments; i++) {
        if (!segments[i].length) continue;
        actual = 0;
        status = msc_bulk(msc, data_pipe, segments[i].buffer, segments[i].length, &actual);
        data_actual += actual;
        /* A failed SCSI command may legitimately stall its data endpoint. */
        if (status == USB_STALLED) {
            if (USB_ERROR(msc_clearHalt(msc, data_pipe))) goto transport_error;
            break;
        } else if (USB_ERROR(status)) {
            goto transport_error;
        }
        /* Short packet: the device ended the data stage early. */
        if (actual != segments[i].length) break;
    }

    msc_csw_t csw;
    status = msc_bulk(msc, msc->in, &csw, sizeof(csw), &actual);
    if (status == USB_STALLED) {
        if (USB_ERROR(msc_clearHalt(msc, msc->in))) goto transport_error;
        status = msc_bulk(msc, msc->in, &csw, sizeof(csw), &actual);
    }
    if (USB_ERROR(status) || actual != sizeof(csw) ||
        csw.signature != MSC_CSW_SIGNATURE || csw.tag != cbw.tag ||
        csw.residue > length || csw.status > 2) goto transport_error;
    if (csw.status == 2) goto transport_error;
    if (csw.status == 1) return -EIO;
    if (csw.residue != 0 || data_actual != length) goto transport_error;
    return 0;

transport_error:
    msc_reset(msc);
    return -EIO;
}

static int msc_command(msc_device_t *msc, const uint8_t *command, uint8_t command_length,
                       void *data, uint32_t length, bool read) {
    msc_segment_t segment = { .buffer = data, .length = length };
    return msc_commandv(msc, command, command_length, &segment, length ? 1 : 0, read);
}

/* Caller holds lock. READ(10)/WRITE(10) of count sectors between the device and the
   bounce buffer. The data stage is split only at 64KiB physical boundaries, so it takes
   a handful of bulk transfers instead of one per page. Split points are page aligned,
   which keeps every transfer but the last a multiple of the max packet size. */
static int msc_io(msc_device_t *msc, uint32_t block, uint32_t count, bool write) {
    size_t bytes = (size_t)count * msc->drive->sector_size;
    if (!count || bytes > MSC_BOUNCE_SIZE || count - 1 > UINT32_MAX - block) return -EINVAL;

    msc_segment_t segments[MSC_BOUNCE_SIZE / MSC_TRB_BOUNDARY + 1];
    size_t nsegments = 0;
    for (size_t offset = 0; offset < bytes; nsegments++) {
        uintptr_t phys = msc->bounce_phys + offset;
        size_t length = MSC_TRB_BOUNDARY - (phys & (MSC_TRB_BOUNDARY - 1));
        if (length > bytes - offset) length = bytes - offset;
        segments[nsegments].buffer = msc->bounce + offset;
        segments[nsegments].length = length;
        offset += length;
    }

    uint8_t command[10] = { write ? SCSI_WRITE_10 : SCSI_READ_10 };
    msc_put_be32(&command[2], block);
    command[7] = count >> 8;
    command[8] = count;
    return msc_commandv(msc, command, sizeof(command), segments, nsegments, !write);
}

static ssize_t msc_sectors(drive_t *drive, uint64_t lba, size_t sectors,
                           uint8_t *buffer, bool write) {
    msc_device_t *msc = drive->driver;
    if (lba > drive->sectors || sectors > drive->sectors - lba) return -EINVAL;
    if (!sectors) return 0;
    if (lba + sectors - 1 > UINT32_MAX) return -EINVAL; /* READ(10)/WRITE(10) only */

    mutex_acquire(&msc->lock);
    if (!msc->online) {
        mutex_release(&msc->lock);
        return -ENODEV;
    }

    /* Caller buffers need not be physically contiguous, so always go through the bounce buffer. */
    size_t completed = 0;
    while (completed < sectors) {
        size_t count = MSC_BOUNCE_SIZE / drive->sector_size;
        if (count > sectors - completed) count = sectors - completed;
        uint8_t *data = buffer + completed * drive->sector_size;
        size_t bytes = count * drive->sector_size;

        if (write) memcpy(msc->bounce, data, bytes);
        if (msc_io(msc, lba + completed, count, write) != 0) break;
        if (!write) memcpy(data, msc->bounce, bytes);
        completed += count;
    }

    mutex_release(&msc->lock);
    return completed == sectors ? (ssize_t)completed : -EIO;
}

static ssize_t msc_read(drive_t *drive, uint64_t lba, size_t sectors, uint8_t *buffer) {
    return msc_sectors(drive, lba, sectors, buffer, false);
}

static ssize_t msc_write(drive_t *drive, uint64_t lba, size_t sectors, uint8_t *buffer) {
    return msc_sectors(drive, lba, sectors, buffer, true);
}

/* Transfer a range of cache pages, MSC_BOUNCE_SIZE per SCSI command. The part of the
   range past the end of the drive is zero-filled on read and dropped on write. */
static ssize_t msc_range(drive_t *drive, page_range_t *range, bool write) {
    msc_device_t *msc = drive->driver;
    if (range->offset < 0 || range->offset % drive->sector_size) return -EINVAL;
    if (!range->npages) return 0;

    uint64_t lba = range->offset / drive->sector_size;
    if (lba >= drive->sectors) return -EINVAL;
    size_t total = range->npages * PAGE_SIZE;
    size_t valid = total;
    if ((drive->sectors - lba) * drive->sector_size < valid) valid = (drive->sectors - lba) * drive->sector_size;
    if (lba + valid / drive->sector_size - 1 > UINT32_MAX) return -EINVAL; /* READ(10)/WRITE(10) only */

    mutex_acquire(&msc->lock);
    if (!msc->online) {
        mutex_release(&msc->lock);
        return -ENODEV;
    }

    int result = 0;
    for (size_t offset = 0; offset < valid; offset += MSC_BOUNCE_SIZE) {
        size_t bytes = valid - offset < MSC_BOUNCE_SIZE ? valid - offset : MSC_BOUNCE_SIZE;
        size_t first_page = offset / PAGE_SIZE;
        size_t npages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;

        if (write) {
            for (size_t i = 0; i < npages; i++) {
                size_t length = bytes - i * PAGE_SIZE < PAGE_SIZE ? bytes - i * PAGE_SIZE : PAGE_SIZE;
                uintptr_t p = arch_mmu_remap_physical(range->pages[first_page + i], PAGE_SIZE, REMAP_TEMPORARY);
                memcpy(msc->bounce + i * PAGE_SIZE, (void*)p, length);
                arch_mmu_unmap_physical(p, PAGE_SIZE);
            }
        }

        result = msc_io(msc, lba + offset / drive->sector_size, bytes / drive->sector_size, write);
        if (result != 0) break;

        if (!write) {
            for (size_t i = 0; i < npages; i++) {
                size_t length = bytes - i * PAGE_SIZE < PAGE_SIZE ? bytes - i * PAGE_SIZE : PAGE_SIZE;
                uintptr_t p = arch_mmu_remap_physical(range->pages[first_page + i], PAGE_SIZE, REMAP_TEMPORARY);
                memcpy((void*)p, msc->bounce + i * PAGE_SIZE, length);
                if (length < PAGE_SIZE) memset((uint8_t*)p + length, 0, PAGE_SIZE - length);
                arch_mmu_unmap_physical(p, PAGE_SIZE);
            }
        }
    }
    mutex_release(&msc->lock);
    if (result != 0) return result;

    /* Zero whole pages that lie entirely past the end of the drive. */
    if (!write) {
        for (size_t page = (valid + PAGE_SIZE - 1) / PAGE_SIZE; page < range->npages; page++) {
            uintptr_t p = arch_mmu_remap_physical(range->pages[page], PAGE_SIZE, REMAP_TEMPORARY);
            memset((void*)p, 0, PAGE_SIZE);
            arch_mmu_unmap_physical(p, PAGE_SIZE);
        }
    }

    return 0;
}

static ssize_t msc_read_range(drive_t *drive, page_range_t *range) {
    return msc_range(drive, range, false);
}

static ssize_t msc_write_range(drive_t *drive, page_range_t *range) {
    return msc_range(drive, range, true);
}

static int msc_flush(drive_t *drive) {
    msc_device_t *msc = drive->driver;
    uint8_t command[10] = { SCSI_SYNC_CACHE_10 };
    mutex_acquire(&msc->lock);
    int result = msc_command(msc, command, sizeof(command), NULL, 0, false);
    mutex_release(&msc->lock);
    return result;
}

static drive_ops_t msc_drive_ops = {
    .read_sectors = msc_read,
    .write_sectors = msc_write,
    .read_range = msc_read_range,
    .write_range = msc_write_range,
    .flush = msc_flush,
};

static int msc_match(usb_device_t *device, usb_interface_t *interface) {
    if (!interface || interface->desc.bInterfaceClass != MSC_CLASS ||
        interface->desc.bInterfaceSubClass != MSC_SCSI ||
        interface->desc.bInterfaceProtocol != MSC_BULK_ONLY) return USB_MATCH_NONE;
    return USB_MATCH_INTF_CLASS_SUBCLASS_PROTO;
}

static usb_status_t msc_attach(usb_device_t *device, usb_interface_t *interface) {
    usb_endpoint_t *in = NULL, *out = NULL;
    for (unsigned i = 0; i < interface->num_endpoints; i++) {
        usb_endpoint_t *endpoint = interface->endpoints[i];
        if (USB_ENDP_TYPE(endpoint->desc.bmAttributes) != USB_ENDP_TYPE_BULK) continue;
        if (USB_ENDP_DIRECTION(endpoint->desc.bEndpointAddress) == USB_ENDP_DIR_IN) in = endpoint;
        else out = endpoint;
    }
    if (!in || !out) return USB_BAD_DESCRIPTOR;

    msc_device_t *msc = kzalloc(sizeof(*msc));
    if (!msc) return USB_NO_MEMORY;
    MUTEX_INIT(&msc->lock);
    msc->device = device;
    msc->interface = interface;

    msc->bounce_phys = pmm_allocatePages(MSC_BOUNCE_SIZE / PAGE_SIZE, ZONE_DEFAULT);
    if (!msc->bounce_phys) {
        kfree(msc);
        return USB_NO_MEMORY;
    }
    msc->bounce = (uint8_t*)arch_mmu_remap_physical(msc->bounce_phys, MSC_BOUNCE_SIZE, 0);

    usb_status_t status = usb_openPipe(in, USB_PIPE_DEFAULT, &msc->in);
    if (USB_ERROR(status)) goto fail;
    status = usb_openPipe(out, USB_PIPE_DEFAULT, &msc->out);
    if (USB_ERROR(status)) goto fail;
    msc->online = true;

    uint8_t inquiry_cmd[6] = { SCSI_INQUIRY, 0, 0, 0, 36, 0 };
    uint8_t inquiry[36] = { 0 };
    uint8_t capacity_cmd[10] = { SCSI_READ_CAPACITY };
    uint8_t capacity[8] = { 0 };
    mutex_acquire(&msc->lock);
    int result = msc_command(msc, inquiry_cmd, sizeof(inquiry_cmd), inquiry, sizeof(inquiry), true);
    if (result == 0 && (inquiry[0] & 0x1f) != 0) result = -ENODEV; /* direct access only */
    if (result == 0) result = msc_command(msc, capacity_cmd, sizeof(capacity_cmd), capacity, sizeof(capacity), true);
    mutex_release(&msc->lock);
    if (result != 0) {
        status = USB_INVALID;
        goto fail;
    }

    uint32_t last_lba = msc_be32(capacity);
    uint32_t sector_size = msc_be32(capacity + 4);
    if (last_lba == UINT32_MAX || sector_size < 512 || sector_size > 4096 ||
        (sector_size & (sector_size - 1)) != 0) {
        status = USB_INVALID;
        goto fail;
    }

    msc->drive = drive_create(DRIVE_TYPE_SCSI, &msc_drive_ops);
    if (!msc->drive) {
        status = USB_NO_MEMORY;
        goto fail;
    }
    msc->drive->driver = msc;
    msc->drive->sectors = (uint64_t)last_lba + 1;
    msc->drive->sector_size = sector_size;
    interface->driver_priv = msc;
    LOG(INFO, "USB disk: %llu sectors of %u bytes\n", (unsigned long long)msc->drive->sectors, sector_size);
    drive_mount(msc->drive);
    return USB_SUCCESS;

fail:
    if (msc->out) usb_closePipe(msc->out);
    if (msc->in) usb_closePipe(msc->in);
    arch_mmu_unmap_physical((uintptr_t)msc->bounce, MSC_BOUNCE_SIZE);
    pmm_freePages(msc->bounce_phys, MSC_BOUNCE_SIZE / PAGE_SIZE);
    kfree(msc);
    return status;
}

static usb_status_t msc_detach(usb_device_t *device, usb_interface_t *interface) {
    msc_device_t *msc = interface->driver_priv;
    if (!msc) return USB_SUCCESS;

    mutex_acquire(&msc->lock);
    msc->online = false;
    mutex_release(&msc->lock);
    usb_closePipe(msc->out);
    usb_closePipe(msc->in);
    msc->out = NULL;
    msc->in = NULL;
    msc->interface = NULL;
    msc->device = NULL;

    /* devfs has no open-node lifetime tracking. Keep nodes and state alive so
       existing file handles fail with ENODEV instead of accessing freed memory. */
    interface->driver_priv = NULL;
    return USB_SUCCESS;
}

static usb_driver_t msc_driver = {
    .name = "USB Mass Storage",
    .ops = { .match = msc_match, .attach = msc_attach, .detach = msc_detach },
};

static int msc_init(int argc, char *argv[]) {
    usb_registerDriver(&msc_driver);
    return DRIVER_STATUS_SUCCESS;
}

static int msc_deinit(void) {
    return DRIVER_STATUS_SUCCESS;
}

struct driver_metadata driver_metadata = {
    .name = "USB Mass Storage Driver",
    .author = "Samuel Stuart",
    .init = msc_init,
    .deinit = msc_deinit,
};
