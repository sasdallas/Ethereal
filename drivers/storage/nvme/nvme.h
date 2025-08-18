/**
 * @file drivers/storage/nvme/nvme.h
 * @brief NVMe driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _NVME_H
#define _NVME_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/misc/util.h>
#include <kernel/misc/spinlock.h>
#include <structs/list.h>
#include <kernel/drivers/pci.h>

/**** TYPES ****/

typedef struct nvme_cap {
    uint64_t mqes:16;               // Maximum queue entries supported
    uint64_t cqr:1;                 // Contiguous queues required
    uint64_t ams:2;                 // Arbitration mechanism supported
    uint64_t reserved:5;            // Rresreved
    uint64_t to:8;                  // Timeout
    uint64_t dstrd:4;               // Doorbell stride
    uint64_t nssrs:1;               // NVM subsystem reset supported
    uint64_t css:8;                 // Command sets supported
    uint64_t bps:1;                 // Boot partition support
    uint64_t cps:2;                 // Controller power scope
    uint64_t mpsmin:4;              // Memory page size minimum
    uint64_t mpsmax:4;              // Memory page size maximum
    uint64_t pmrs:1;                // Persistent memory region supported
    uint64_t cmbs:1;                // Controller memory buffer supported
    uint64_t nsss:1;                // NVM subsystem shutdown supported
    uint64_t crms:2;                // Controller ready modes supported
    uint64_t nsses:1;               // NVM subsystem shutdown enhancements supported
    uint64_t reserved2:2;           // Reserved
} nvme_cap_t;

STATIC_ASSERT(sizeof(nvme_cap_t) == sizeof(uint64_t));

typedef struct nvme_vs {
    uint32_t ter:8;                 // Tertiary version
    uint32_t mnr:8;                 // Minor version
    uint32_t mjr:16;                // Major version 
} nvme_vs_t;

STATIC_ASSERT(sizeof(nvme_vs_t) == sizeof(uint32_t));

typedef struct nvme_intms {
    uint32_t ivms:32;               // Interrupt vector mask set
} nvme_intms_t;

STATIC_ASSERT(sizeof(nvme_intms_t) == sizeof(uint32_t));

typedef struct nvme_intmc {
    uint32_t ivmc:32;               // Interrupt vector mask clear
} nvme_intmc_t;

STATIC_ASSERT(sizeof(nvme_intmc_t) == sizeof(uint32_t));

typedef struct nvme_cc {
    uint32_t en:1;                  // Enable
    uint32_t reserved:3;            // Reserved
    uint32_t css:3;                 // I/O command set selected
    uint32_t mps:4;                 // Memory page size
    uint32_t ams:3;                 // Arbitration mechanism selected
    uint32_t shn:2;                 // Shutdown notification
    uint32_t iosqes:4;              // I/O submission queue entry
    uint32_t iocqes:4;              // I/O comlpetion queue entry size
    uint32_t crime:1;               // Controller ready independent of media enable
    uint32_t reserved2:7;           // Reserved
} nvme_cc_t;

STATIC_ASSERT(sizeof(nvme_cc_t) == sizeof(uint32_t));

typedef struct nvme_csts {
    uint32_t rdy:1;                 // Ready
    uint32_t cfs:1;                 // Controller fatal status
    uint32_t shst:2;                // Shutdown status
    uint32_t nssro:1;               // NVM shutdown reset occurred
    uint32_t pp:1;                  // Processing paused
    uint32_t st:1;                  // Shutdown type        
    uint32_t reserved:25;           // Reserved
} nvme_csts_t;

STATIC_ASSERT(sizeof(nvme_csts_t) == sizeof(uint32_t));

typedef struct nvme_nssr {
    uint32_t nssrc:32;              // NVM subsystem reset control
} nvme_nssr_t;

STATIC_ASSERT(sizeof(nvme_nssr_t) == sizeof(uint32_t));

typedef struct nvme_aqa {
    uint32_t asqs:12;               // Admin submission queue size
    uint32_t reserved:4;            // Reserved
    uint32_t acqs:12;               // Admin completion queue size
    uint32_t reserved2:4;           // Reserved
} nvme_aqa_t;

STATIC_ASSERT(sizeof(nvme_aqa_t) == sizeof(uint32_t));

typedef struct nvme_asq {
    uint64_t asqb:64;               // Admin submission queue base (bits 0-12 reserved)
} nvme_asq_t;

STATIC_ASSERT(sizeof(nvme_asq_t) == sizeof(uint64_t));

typedef struct nvme_acq {
    uint64_t acqb:64;               // Admin completion queue base address
} nvme_acq_t;

STATIC_ASSERT(sizeof(nvme_acq_t) == sizeof(uint64_t));

/**
 * @brief (most of) NVMe registers
 */
typedef struct nvme_registers {
    nvme_cap_t          cap;            // CAP
    nvme_vs_t           vs;             // VS
    nvme_intms_t        intms;          // INTMS
    nvme_intmc_t        intmc;          // INTMC
    nvme_cc_t           cc;             // CC
    uint8_t             reserved[4];    // Reserved
    nvme_csts_t         csts;           // CSTS
    nvme_nssr_t         nssr;           // NSSR
    nvme_aqa_t          aqa;            // AQA
    nvme_asq_t          asq;            // ASQ
    nvme_acq_t          acq;            // ACQ
} nvme_registers_t;

STATIC_ASSERT(sizeof(nvme_registers_t) == 0x38);

/**
 * @brief NVMe doorbell structure
 */
typedef struct nvme_doorbell {
    uint32_t            sq_tail;        // Submission queue tail
    uint32_t            cq_hdbl;        // Completion queue head
} __attribute__((packed)) nvme_doorbell_t;

STATIC_ASSERT(sizeof(nvme_doorbell_t) == sizeof(uint64_t));

/**
 * @brief NVME completion queue entry
 */
typedef struct nvme_cq_entry {
    uint32_t useless[3];                // Command specific stuff
    uint16_t cid;                       // Command identifier
    uint16_t sts;                       // Status
} __attribute__((packed)) nvme_cq_entry_t;

STATIC_ASSERT(sizeof(nvme_cq_entry_t) == (sizeof(uint32_t)*4));

/**
 * @brief NVMe data pointer
 */
typedef struct nvme_data_pointer {
    union {
        struct {
            uint64_t prp1;              // PRP entry 1
            uint64_t prp2;              // PRP entry 2
        };

        uint8_t sgl1[16];               // SGL entry 1 (if CDW0.PSDT is 01b or 10b)
    };
} nvme_data_pointer_t;

/**
 * @brief NVMe generic command structure
 */
typedef struct nvme_command {
    // cdw0, figure 91, is merged into nvme_sq_entry_t
    uint32_t nsid;                      // Namespace ID
    uint32_t cdw2;                      // Command dword 2
    uint32_t cdw3;                      // Command dword 2
    uint64_t mptr;                      // Metadata pointer
    nvme_data_pointer_t dptr;           // Data pointer
    uint32_t cdw10;                     // Command dword 10
    uint32_t cdw11;                     // Command dword 11
    uint32_t cdw12;                     // Command dword 12
    uint32_t cdw13;                     // Command dword 13
    uint32_t cdw14;                     // Command dword 14
    uint32_t cdw15;                     // Command dword 15
} __attribute__((packed)) nvme_command_t;

STATIC_ASSERT(sizeof(nvme_command_t) == 15 * sizeof(uint32_t));

/**
 * @brief Identify command
 */
typedef struct nvme_identify_command {
    // cdw0, figure 91, is merged into nvme_sq_entry_t
    uint32_t nsid;                      // Namespace ID
    uint32_t reserved[4];               // Reserved
    nvme_data_pointer_t dptr;           // Data pointer
    
    union {
        struct {
            uint32_t cns:8;             // CNS
            uint32_t reserved2:8;       // Reserved
            uint32_t cntid:16;          // Controller identifier
        };

        uint32_t cdw10;
    };

    union {
        struct {
            uint32_t cnssid:16;         // CNS-specific identifier
            uint32_t reserved3:8;       // Reserved
            uint32_t csi:8;             // Command set identifier
        };

        uint32_t cdw11;
    };

    uint32_t reserved4[2];              // Reserved

    union {
        struct {
            uint32_t uidx:7;            // UUID index
            uint32_t reserved5:25;      // Reserved
        };

        uint32_t cdw14;                 // Command dword 14
    };

    uint32_t reserved6;                 // Reserved
} __attribute__((packed)) nvme_identify_command_t;

STATIC_ASSERT(sizeof(nvme_identify_command_t) == 15 * sizeof(uint32_t));

/**
 * @brief Create I/O completion queue command (Figures 474-476)
 */
typedef struct nvme_create_cq_command {
    uint32_t reserved[5];               // Reserved
    nvme_data_pointer_t dptr;           // Data pointer
    
    union {
        struct {
            uint32_t qid:16;            // Queue identifier
            uint32_t qsize:16;          // Queue size
        };

        uint32_t cdw10;
    };

    union {
        struct {
            uint32_t pc:1;              // Physically contiguous
            uint32_t ien:1;             // Interrupts enabled
            uint32_t reserved2:14;      // Reserved
            uint32_t iv:16;             // Interrupt vector              
        };

        uint32_t cdw11;
    };

    uint32_t reserved3[4];              // Reserved
} __attribute__((packed)) nvme_create_cq_command_t;

STATIC_ASSERT(sizeof(nvme_create_cq_command_t) == 15 * sizeof(uint32_t));

/**
 * @brief NVMe submission queue entry
 */
typedef struct nvme_sq_entry {
    union {
        struct {
            uint8_t dtd:2;              // Data transfer direction
            uint8_t fn:6;               // Function
        };

        uint8_t opc;                    // Opcode
    };

    uint32_t fuse:2;                    // Fused operation
    uint32_t reserved:4;                // Reserved
    uint32_t psdt:2;                    // PRP or SGL for data transfer
    uint32_t cid:16;                    // Command ID

    nvme_command_t command;             // Command structure
} __attribute__((packed)) nvme_sq_entry_t;

STATIC_ASSERT(sizeof(nvme_sq_entry_t) == 64);

/**
 * @brief NVMe identification space
 * @ref https://nvmexpress.org/wp-content/uploads/NVM-Express-Base-Specification-Revision-2.2-2025.03.11-Ratified.pdf (figure 313)
 */
typedef struct nvme_ident {
    uint16_t vid;
    uint16_t ssvid;
    char sn[20];
    char mn[40];
    char fr[8];
    uint8_t rab;
    char ieee[3];
    uint8_t cmic;
    uint8_t mdts;
    uint16_t cntlid;
    uint32_t ver;
    uint32_t rtd3r;
    uint32_t rtd3e;
    uint32_t oaes;
    uint32_t ctratt;
    uint16_t rrls;
    uint8_t bpcap;
    uint8_t reserved0;
    uint32_t nssl;
    uint16_t reserved1;
    uint8_t plsi;
    uint8_t cntrltype;
    char fguid[16];
    uint16_t crdt1;
    uint16_t crdt2;
    uint16_t crdt3;
    uint8_t crcap;
    char reserved2[105];
    char reserved3[13];
    uint8_t nvmsr;
    uint8_t vwci;
    uint8_t mec;
    uint16_t oacs;
    uint8_t acl; 
    uint8_t aerl;
    uint8_t frmw;
    uint8_t lpa;
    uint8_t elpe;
    uint8_t npss;
    uint8_t avscc;
    uint8_t apsta;
    uint16_t wctemp;
    uint16_t cctemp;
    uint16_t mtfa;
    uint32_t hmpre;
    uint32_t hmmin;
    uint8_t tnvmcap[16];
    uint8_t unvmcap[16];
    uint32_t rpmbs;
    uint16_t edstt;
    uint8_t dsto;
    uint8_t fwug;
    uint16_t kas;
    uint16_t hctma;
    uint16_t mntmt;
    uint16_t mxtmt;
    uint32_t sanicap;
    uint32_t hmminds;
    uint16_t hmmaxd;
    uint16_t nsetidmax;
    uint16_t endgidmax;
    uint8_t anatt;
    uint8_t anacap;
    uint32_t anagrpmax;
    uint32_t nanagrpid;
    uint32_t pels;
    uint16_t did;
    uint8_t kpioc;
    uint8_t reserved4;
    uint16_t mptfawr;
    uint8_t reserved5[6];
    uint8_t megcap[16];
    uint8_t tmpthha;
    uint8_t reserved6;
    uint16_t cqt;
    uint8_t reserved7[124];
    uint8_t sqes;
    uint8_t cqes;
    uint16_t maxcmd;
    uint32_t nn;
    uint16_t oncs;
    uint16_t fuses;
    uint8_t fna;
    uint8_t vwc;
    uint16_t awun;
    uint16_t awupf;
    uint8_t icsvscc;
    uint8_t nwpc;
    uint16_t acwu;
    uint16_t cdfs;
    uint32_t sgls;
    uint32_t mnan;
    uint8_t maxdna[16];
    uint32_t maxcna;
    uint32_t oaqd;
    uint8_t rhiri;
    uint8_t hirt;
    uint16_t cmmrtd;
    uint16_t nmmrtd;
    uint8_t minmrtg;
    uint8_t maxmrtg;
    uint8_t trattr;
    uint16_t mcudmq;
    uint16_t mnsudmq;
    uint16_t mcmr;
    char i_dont_want_to_fill_the_rest_in[3513];
} __attribute__((packed)) nvme_ident_t;

STATIC_ASSERT(sizeof(nvme_ident_t) == 4096);

/**
 * @brief NVMe completion event
 */
typedef struct nvme_completion {
    uint16_t cid;                       // CID
    uint16_t status;                    // Status code
} nvme_completion_t;

/**
 * @brief NVMe queue structure
 */
typedef struct nvme_queue {
    spinlock_t lock;                    // Queue lock

    uintptr_t cq;                       // Completion queue
    uintptr_t sq;                       // Submission queue

    uint32_t cq_head;                   // Completion queue head
    uint32_t sq_tail;                   // Submission queue tail
    uint8_t cq_phase;                   // Completion queue phase

    nvme_doorbell_t *doorbell;          // Doorbell
    size_t depth;                       // Queue depth
    uint16_t cid_last;                  // Last CID

    list_t *completions;                // Completion events
} nvme_queue_t;

/**
 * @brief NVMe structure
 */
typedef struct nvme {
    volatile nvme_registers_t *regs;    // Registers
    nvme_queue_t *admin_queue;          // Admin queue
    nvme_queue_t *io_queue;             // I/O queue
    pci_device_t *dev;                  // PCI device
} nvme_t;   


/**** DEFINITIONS ****/

#define NVME_CAP_CSS_NVME               (1 << 0)
#define NVME_CAP_CSS_IO                 (1 << 6)
#define NVME_CAP_CSS_ADMIN              (1 << 7)

/* Opcode values (figure 28) */
#define NVME_OPC_CREATE_SQ              0x01
#define NVME_OPC_CREATE_CQ              0x05
#define NVME_OPC_IDENTIFY               0x06

/* CNS values */
#define NVME_CNS_IDENTIFY_NAMESPACE     0x00
#define NVME_CNS_IDENTIFY_CONTROLLER    0x01
#define NVME_CNS_ACTIVE_NAMESPACES      0x02

/* Status codes (figure 102) */
#define NVME_STATUS_SUCCESS                 0x00
#define NVME_STATUS_INVALID_OPCODE          0x01
#define NVME_STATUS_INVALID_FIELD           0x02
#define NVME_STATUS_CID_CONFLICT            0x03
#define NVME_STATUS_DATA_TRANSFER_ERROR     0x04
#define NVME_STATUS_POWER_LOSS              0x05
#define NVME_STATUS_INTERNAL_ERROR          0x06
#define NVME_STATUS_CMD_ABORT               0x07
#define NVME_STATUS_CMD_ABORT_SQDEL         0x08
#define NVME_STATUS_CMD_ABORT_FUSE_FAIL     0x09
#define NVME_STATUS_CMD_ABORT_FUSE_MISS     0x0A
#define NVME_STATUS_INVALID_NAMESPACE       0x0B
#define NVME_STATUS_COMMAND_SEQ_ERROR       0x0C
#define NVME_STATUS_INVALID_SGL             0x0D
#define NVME_STATUS_INVALID_SGL_NUM         0x0E
#define NVME_STATUS_DATA_SGL_INVALID        0x0F
#define NVME_STATUS_METADATA_SGL_INVALID    0x10
#define NVME_STATUS_SGL_TYPE_INVALID        0x11
#define NVME_STATUS_INVALID_BUFFER_USE      0x12
#define NVME_STATUS_INVALID_PRP_OFFSET      0x13
#define NVME_STATUS_ATOMIC_WRITE_EXCEEDED   0x14
#define NVME_STATUS_OPERATION_DENIED        0x15
#define NVME_STATUS_SGL_OFFSET_INVALID      0x16
#define NVME_STATUS_HOST_ID_INCONSISTENT    0x18
#define NVME_STATUS_KEEP_ALIVE_EXPIRED      0x19
#define NVME_STATUS_KEEP_ALIVE_INVALID      0x1A
#define NVME_STATUS_CMD_ABORT_PREEMPT       0x1B
#define NVME_STATUS_SANITIZE_FAILED         0x1C
#define NVME_STATUS_SANITIZE_IN_PROGRESS    0x1D
#define NVME_STATUS_SGL_DATA_BLOCK_INVALID  0x1E

/* Depths */
#define NVME_ADMIN_QUEUE_DEPTH          (PAGE_SIZE / sizeof(nvme_sq_entry_t))
#define NVME_IO_QUEUE_DEPTH             (PAGE_SIZE / sizeof(nvme_sq_entry_t))

/**** MACROS ****/

#define NVME_DOORBELL_STRIDE        (1 << (2 + nvme->regs->cap.dstrd))
#define NVME_GET_DOORBELL(x)        ((nvme_doorbell_t*)(((uintptr_t)nvme->regs) + (0x1000 + (2*x) * NVME_DOORBELL_STRIDE)))

/**** FUNCTIONS ****/

/**
 * @brief Create a new NVMe queue
 * @param depth Queue depth
 * @param doorbell NVMe doorbell
 * @returns NVMe queue
 */
nvme_queue_t *nvme_createQueue(size_t depth, nvme_doorbell_t *doorbell);

/**
 * @brief Submit a command to an NVMe queue
 * @param queue The queue to submit the command to
 * @param entry The submission queue entry
 * @returns CID on success
 */
uint16_t nvme_submitQueue(nvme_queue_t *queue, nvme_sq_entry_t *entry);

/**
 * @brief Handle an IRQ in a queue
 * @param queue The queue to handle the IRQ for
 */
void nvme_irqQueue(nvme_queue_t *queue);


#endif