/**
 * @file drivers/fs/fat/fat.c
 * @brief FAT 12/16/32 driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "fat.h"
#include <kernel/fs/vfs.h>
#include <kernel/mem/alloc.h>
#include <kernel/loader/driver.h>
#include <kernel/debug.h>
#include <string.h>
#include <errno.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:FAT", __VA_ARGS__)

/**
 * @brief FAT open method
 * @param node The node
 * @param flags The flags
 */
void fat_open(fs_node_t *node, unsigned int flags) {
    fat_node_t *fnode = (fat_node_t*)node->dev;
    fat_t *fat = (fat_t*)fnode->fat;
    
    if (!fnode->entry) {
        // This node should've had its cluster calculated in node->inode
        // Calculate the sector
        uint32_t sector;
        if (node->impl == (uint64_t)-1) {
            sector = FAT_FIRST_DATA_SECTOR(fat->bpb) - FAT_ROOTDIR_SIZE(fat->bpb);
        } else {
            sector = ((node->impl - 2) * fat->bpb.sectors_per_cluster) + FAT_FIRST_DATA_SECTOR(fat->bpb);
        }

        // Load the sector into memory
        fnode->entry = kmalloc(sizeof(fat_entry_t));
        if (fs_read(fat->dev, sector * fat->bpb.bytes_per_sector, sizeof(fat_entry_t), (uint8_t*)fnode->entry) < (ssize_t)sizeof(fat_entry_t)) {
            LOG(ERR, "Failed to read sector at %d (%d offset)\n", sector, sector * fat->bpb.bytes_per_sector);
            kfree(fnode->entry);
            return;
        }

        LOG(INFO, "Read FAT entry for sector %d\n", sector);
    }
}

/**
 * @brief FAT close method
 */
void fat_close(fs_node_t *node) {
    fat_node_t *fnode = (fat_node_t*)node->dev;
    if (fnode->entry) kfree(fnode->entry);
}

/**
 * @brief Calculate the next cluster in the chain from FAT
 * @param fat The FAT filesystem
 * @param cluster The current cluster
 * @returns The next cluster, FAT_CLUSTER_END, or FAT_CLUSTER_CORRUPT
 */
uint32_t fat_nextCluster(fat_t *fat, uint32_t cluster) {
    if (fat->type == FAT_TYPE_FAT12) {
        // Calculate the FAT offset
        uint8_t *fat_table = kmalloc(fat->bpb.bytes_per_sector * 2); // TODO: Caching?

        // Calculate the FAT offset, sector, and entry offset
        uint32_t fat_offset = cluster + (cluster / 2);
        uint32_t fat_sector = FAT_FIRST_FAT_SECTOR(fat->bpb) + (fat_offset / fat->bpb.bytes_per_sector);
        uint32_t entry_offset = fat_offset % fat->bpb.bytes_per_sector;

        // Read in FAT table
        if (fs_read(fat->dev, fat_sector * fat->bpb.bytes_per_sector, fat->bpb.bytes_per_sector * 2, (uint8_t*)fat_table) != fat->bpb.bytes_per_sector * 2) {
            LOG(ERR, "Failed to read FAT table from drive\n");
            return FAT_CLUSTER_CORRUPT;
        } 

        // Calculate table value
        unsigned short table_value = *(unsigned short*)&fat_table[entry_offset];
        table_value = (cluster & 1) ? table_value >> 4 : table_value & 0xFFF;
        
        // We're done with FAT table
        kfree(fat_table);

        if (table_value >= 0xFF8) {
            return FAT_CLUSTER_END;
        } else if (table_value == 0xFF7) {
            return FAT_CLUSTER_CORRUPT;
        }

        return table_value; 
    } else if (fat->type == FAT_TYPE_FAT16) {
        LOG(ERR, "FAT16 unimplemented\n");
    } else if (fat->type == FAT_TYPE_FAT32) {
        LOG(ERR, "FAT32 unimplemented\n");
    }

    LOG(ERR, "fat_nextCluster does not have support for FAT filesystem");
    return FAT_CLUSTER_CORRUPT;
}

/**
 * @brief Read a cluster into a buffer
 * @param fat The FAT filesystem
 * @param cluster The cluster number to read
 * @param buffer The buffer to read into
 * @returns 0 on success
 */
int fat_readCluster(fat_t *fat, uint32_t cluster, uint8_t *buffer) {
    // First calculate the sector
    uint32_t sector = ((cluster - 2) * fat->bpb.sectors_per_cluster) + FAT_FIRST_DATA_SECTOR(fat->bpb);

    // Read the sector
    ssize_t r = fs_read(fat->dev, sector * fat->bpb.bytes_per_sector, FAT_BYTES_PER_CLUSTER(fat->bpb), buffer);
    return r != FAT_BYTES_PER_CLUSTER(fat->bpb);
}

/**
 * @brief FAT read method
 * @param node The node to read
 * @param off Offset
 * @param size Size
 * @param buffer Buffer
 */
ssize_t fat_read(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    fat_node_t *fnode = (fat_node_t*)node->dev;
    fat_t *fat = (fat_t*)fnode->fat;

    if (off >= (off_t)node->length) return 0;
    if (size + off > node->length) size = node->length - off;

    // Calculate the starting cluster we should read at
    uint32_t cluster = fnode->entry->cluster_lo;
    if (fat->type == FAT_TYPE_FAT32) cluster |= (fnode->entry->cluster_high << 16);

    // Do we need to offset the cluster?
    size_t current = 0;
    if (off > FAT_BYTES_PER_CLUSTER(fat->bpb)) {
        while (cluster != FAT_CLUSTER_END && cluster != FAT_CLUSTER_CORRUPT && off - current > fat->bpb.bytes_per_sector) {
            cluster = fat_nextCluster(fat, cluster);
            current += FAT_BYTES_PER_CLUSTER(fat->bpb);
        }
    }

    // Okay, the cluster should have all the data we need. Calculate an offset
    uint32_t off_fixed = off - current;
    size_t bytes_read = 0;

    // Allocate kernel buffer
    uint8_t *kbuf = kmalloc(FAT_BYTES_PER_CLUSTER(fat->bpb));

    while (bytes_read < size + off_fixed) {
        if (cluster == FAT_CLUSTER_END || cluster == FAT_CLUSTER_CORRUPT) {
            if (cluster == FAT_CLUSTER_CORRUPT) LOG(WARN, "Corrupt cluster detected. Terminating read early.\n");
            kfree(kbuf);
            return bytes_read ? bytes_read - off_fixed : bytes_read;
        }

        // Read the cluster into kbuf
        if (fat_readCluster(fat, cluster, kbuf)) {
            kfree(kbuf);
            return -EIO;
        }

        // Do we need to copy with offset?
        size_t size_to_copy = (size - bytes_read) > FAT_BYTES_PER_CLUSTER(fat->bpb) ? FAT_BYTES_PER_CLUSTER(fat->bpb) : (size - bytes_read);

        if (!bytes_read) {
            memcpy(buffer, kbuf + off_fixed, size_to_copy - off_fixed);
        } else {
            memcpy(buffer + bytes_read, kbuf, size_to_copy);
        }

        bytes_read += FAT_BYTES_PER_CLUSTER(fat->bpb);
        cluster = fat_nextCluster(fat, cluster);
    }

    kfree(kbuf);
    return size;
}

/**
 * @brief FAT readdir method
 * @param node The node to readdir
 * @param index The index of the readdir
 */
struct dirent *fat_readdir(fs_node_t *node, unsigned long index) {
    fat_node_t *fnode = (fat_node_t*)node->dev;
    fat_t *fat = (fat_t*)fnode->fat;

    // Calculate the sector
    uint32_t sector;
    if (node->impl == (uint64_t)-1) {
        // FAT12/FAT16 root directory
        sector = FAT_FIRST_DATA_SECTOR(fat->bpb) - FAT_ROOTDIR_SIZE(fat->bpb);
    } else {
        sector = ((node->impl - 2) * fat->bpb.sectors_per_cluster) + FAT_FIRST_DATA_SECTOR(fat->bpb);
    }

    // Sector calculated, now read it into a buffer
    uint8_t buffer[fat->bpb.bytes_per_sector];
    if (fs_read(fat->dev, sector * fat->bpb.bytes_per_sector, fat->bpb.bytes_per_sector, buffer) != fat->bpb.bytes_per_sector) {
        LOG(ERR, "Could not read %d bytes at sector %d\n", fat->bpb.bytes_per_sector, sector);
        return NULL;
    }

    // Start reading
    unsigned int fileidx = 0;
    uint8_t *p = (uint8_t*)buffer;
    char lfn_name[256] = { 0 };

    while (p < buffer + fat->bpb.bytes_per_sector) {
        fat_entry_t *ent = (fat_entry_t*)p;
        if (ent->filename[0] == 0) break; // No more entries left

        // Is this a file or an LFN?
        if (ent->attributes == FAT_ATTR_LFN) {
            if (fileidx == index && ent->filename[0] & 0x40) {
                fat_lfn_t *lfn = (fat_lfn_t*)ent;
                size_t idx = 0;
                for (int i = (lfn->order & ~0x40) - 1; i >= 0; i--) {
                    lfn = (fat_lfn_t*)(p + (32*i));
                    for (int i = 0; i < 10; i += 2) { lfn_name[idx] = lfn->name[i]; if (lfn->name[i] == 0x0) goto _next_lfn; idx++;  }
                    for (int i = 0; i < 12; i += 2) { lfn_name[idx] = lfn->name2[i]; if (lfn->name2[i] == 0x0) goto _next_lfn; idx++; }
                    for (int i = 0; i < 4; i += 2) { lfn_name[idx] = lfn->name3[i]; if (lfn->name3[i] == 0x0) goto _next_lfn; idx++; }
                
                _next_lfn:
                    continue;
                }
                
                lfn_name[idx] = 0;
                p += (lfn->order & ~0x40) * sizeof(fat_lfn_t);
                continue;
            }
            
            p += sizeof(fat_lfn_t);
        } else {
            // Is the one we're looking for?
            if (fileidx == index) {
                // Yes! Construct a dirent
                struct dirent *dent = kzalloc(sizeof(struct dirent));
                
                // Do we have an LFN saved?
                if (lfn_name[0]) {
                    // Yup, it's not blank!
                    // Copy it over
                    strncpy(dent->d_name, lfn_name, 256);
                } else {
                    // Use 8.3 filename
                    strncpy(dent->d_name, ent->filename, 11);
                }

                return dent;
            }

            fileidx++;
            p += sizeof(fat_entry_t);
        }
    }

    return NULL;
}

/**
 * @brief FAT finddir method
 * @param node The node to find on
 * @param name The name of the node to look for
 */
fs_node_t *fat_finddir(fs_node_t *node, char *path) {
    fat_node_t *fnode = (fat_node_t*)node->dev;
    fat_t *fat = (fat_t*)fnode->fat;

    // Calculate the sector
    uint32_t sector;
    if (node->impl == (uint64_t)-1) {
        // FAT12/FAT16 root directory
        sector = FAT_FIRST_DATA_SECTOR(fat->bpb) - FAT_ROOTDIR_SIZE(fat->bpb);
    } else {
        sector = ((node->impl - 2) * fat->bpb.sectors_per_cluster) + FAT_FIRST_DATA_SECTOR(fat->bpb);
    }

    // Sector calculated, now read it into a buffer
    uint8_t buffer[fat->bpb.bytes_per_sector];
    if (fs_read(fat->dev, sector * fat->bpb.bytes_per_sector, fat->bpb.bytes_per_sector, buffer) != fat->bpb.bytes_per_sector) {
        LOG(ERR, "Could not read %d bytes at sector %d\n", fat->bpb.bytes_per_sector, sector);
        return NULL;
    }

    // Start reading
    uint8_t *p = (uint8_t*)buffer;
    fat_entry_t *entry = NULL;
    char ent_name[256] = { 0 };
    
    // Iterate until we can't
    // TODO: Read in more sectors
    while (p < buffer + fat->bpb.bytes_per_sector) {
        fat_entry_t *ent = (fat_entry_t*)p;
        if (ent->filename[0] == 0) break; // No more entries left
        
        // Check to see if this is an LFN
        if (ent->attributes == FAT_ATTR_LFN) {
            if (!(ent->filename[0] & 0x40)) {p += 32; continue; } // This probably indicates corrupted FAT

            // Construct the name of the LFN to see if it matches
            fat_lfn_t *lfn = (fat_lfn_t*)ent;
            size_t idx = 0;
            for (int i = (lfn->order & ~0x40) - 1; i >= 0; i--) {
                lfn = (fat_lfn_t*)(p + (32*i));
                for (int i = 0; i < 10; i += 2) { ent_name[idx] = lfn->name[i]; if (lfn->name[i] == 0x0) goto _next_lfn; idx++;  }
                for (int i = 0; i < 12; i += 2) { ent_name[idx] = lfn->name2[i]; if (lfn->name2[i] == 0x0) goto _next_lfn; idx++; }
                for (int i = 0; i < 4; i += 2) { ent_name[idx] = lfn->name3[i]; if (lfn->name3[i] == 0x0) goto _next_lfn; idx++; }

            _next_lfn:
                continue;
            }

            // Adjust p
            p += (lfn->order & ~0x40) * sizeof(fat_lfn_t);

            if (!strcmp(path, ent_name)) {
                // Found the entry! Get it
                entry = (fat_entry_t*)p;
                break;
            }
        } else {
            // Compare name
            // TODO: Pretty sure most modern FAT implementations use LFNs always, so 8.3 filename probably won't work here.
            // TODO: Stop making excuses and write the code.
            char filename[12];
            filename[11] = 0;
            strncpy(filename, ent->filename, 11);
            LOG(WARN, "Cannot check only-8.3 entry: %s (KERNEL BUG)\n", filename);
        }

        // Next entry
        p += 32;
    }

    // Did we get anything?
    if (!entry) return NULL;

    // We have an entry, construct a filesystem node
    fs_node_t *rnode = fs_node();
    strncpy(rnode->name, ent_name, 256);
    rnode->open = fat_open;
    rnode->close = fat_close;
    rnode->flags = (entry->attributes & FAT_ATTR_DIRECTORY) ? VFS_DIRECTORY : VFS_FILE;
    rnode->mask = 0666; // TODO: so.. what do i put here?
    rnode->gid = rnode->uid = 0;
    rnode->length = entry->size;

    // Get first cluster of node
    rnode->impl = (uint64_t)((entry->cluster_high << 16) | (entry->cluster_lo & 0xFFFF));

    // TODO: Set atime, ctime, mtime

    if (rnode->flags == VFS_DIRECTORY) {
        rnode->readdir = fat_readdir;
        rnode->finddir = fat_finddir;
    } else {
        rnode->read = fat_read;
        rnode->write = NULL;
    }

    // Reallocate and save entry
    fat_entry_t *e = kmalloc(sizeof(fat_entry_t));
    memcpy(e, entry, sizeof(fat_entry_t));

    fat_node_t *n = kzalloc(sizeof(fat_node_t));
    n->fat = fat;
    n->entry = e;
    rnode->dev = (void*)n;

    return rnode;
}

/**
 * @brief FAT mount method
 */
fs_node_t *fat_mount(char *argp, char *mountpoint) {
    // Open the device
    fs_node_t *dev = kopen(argp, 0);
    if (!dev) return NULL;

    // Read in the BPB
    fat_bpb_t bpb;
    if (fs_read(dev, 0, sizeof(fat_bpb_t), (uint8_t*)&bpb) != sizeof(fat_bpb_t)) {
        LOG(WARN, "Reading disk FAILED\n");
        return NULL;
    }

    // Check BPB signature
    if (bpb.identifier[0] != 0xEB || bpb.identifier[2] != 0x90) {
        // TODO: Is there another way we can check?
        LOG(WARN, "Invalid FAT filesystem (%02x %02x %02x)\n", bpb.identifier[0], bpb.identifier[1], bpb.identifier[2]);
        return NULL;
    }

    // Double-check with FAT signature
    if (bpb.fat_ebpb.signature != 0xAA55) {
        LOG(WARN, "Invalid FAT signature (0x%04x)\n", bpb.fat_ebpb.signature);
        return NULL;
    }

    LOG(INFO, "FAT filesystem was detected\n");
    LOG(INFO, "Total sectors in this FAT filesystem: %d (%d bytes per sector)\n", FAT_TOTAL_SECTORS(bpb), bpb.bytes_per_sector);
    LOG(INFO, "Table sector count: %d\n", FAT_TABLE_SECTORS(bpb));
    LOG(INFO, "First data sector: %d\n", FAT_FIRST_DATA_SECTOR(bpb));
    LOG(INFO, "First FAT sector: %d\n", FAT_FIRST_FAT_SECTOR(bpb));
    LOG(INFO, "Root directory size: %d sectors\n", FAT_ROOTDIR_SIZE(bpb));
    LOG(INFO, "Data sectors: %d\n", FAT_DATA_SECTORS(bpb));
    LOG(INFO, "Total clusters: %d\n", FAT_TOTAL_CLUSTERS(bpb));


    // Figure out the FAT type
    uint8_t fat_type = FAT_TYPE_UNKNOWN;
    if (bpb.bytes_per_sector == 0) {
        fat_type = FAT_TYPE_EXFAT;
        LOG(INFO, "FAT type: exFAT\n");
    
        // Not yet
        LOG(ERR, "exFAT is not supported yet in this FAT driver\n");
        return NULL;
    } else if (FAT_TOTAL_CLUSTERS(bpb) < 4085) {
        fat_type = FAT_TYPE_FAT12;
        LOG(INFO, "FAT type: FAT12\n");
    } else if (FAT_TOTAL_CLUSTERS(bpb) < 65525) {
        fat_type = FAT_TYPE_FAT16;
        LOG(INFO, "FAT type: FAT16\n");
    } else {
        fat_type = FAT_TYPE_FAT32;
        LOG(INFO, "FAT type: FAT32\n");
    }

    // Allocate FAT object
    fat_t *fat = kmalloc(sizeof(fat_t));
    fat->dev = dev;
    memcpy(&fat->bpb, &bpb, sizeof(fat_bpb_t));
    fat->type = fat_type;

    // Create root directory node
    fs_node_t *node = fs_node();
    strncpy(node->name, "FAT Filesystem", 256);
    node->flags = VFS_DIRECTORY;
    node->open = fat_open;
    node->readdir = fat_readdir;
    node->finddir = fat_finddir;

    // Create FAT node system
    fat_node_t *fnode = kzalloc(sizeof(fat_node_t));
    fnode->entry = NULL;
    fnode->fat = fat;
    node->dev = (void*)fnode;

    // Calculate root directory sector
    if (fat->type == FAT_TYPE_FAT32 || fat->type == FAT_TYPE_EXFAT) {
        node->impl = fat->bpb.fat32_ebpb.rootdir_cluster;
    } else {
        // FAT_FIRST_DATA_SECTOR(fat->bpb) - FAT_ROOTDIR_SIZE(fat->bpb) calculates the sector
        node->impl = (uint64_t)-1;
    }

    LOG(DEBUG, "LFN size: %d Entry size: %d Date size: %d Time size: %d\n", sizeof(fat_lfn_t), sizeof(fat_entry_t), sizeof(fat_date_t), sizeof(fat_time_t));
    LOG(DEBUG, "Root directory calculated to be at cluster %d (-1 = FAT12/FAT16)\n", node->impl);
    
    fs_open(node, 0);
    return node;
}

int driver_init(int argc, char *argv[]) {
    vfs_registerFilesystem("vfat", fat_mount);
    return 0;
}

int driver_deinit() {
    return 0;
}

struct driver_metadata driver_metadata = {
    .name = "FAT Filesystem Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};