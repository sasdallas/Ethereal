/**
 * @file hexahedron/mm/pmm_zero.c
 * @brief Zero page thread
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/mm/pmm.h>
#include <kernel/task/process.h>
#include <kernel/debug.h>
#include <kernel/init.h>

/* Zero page thread */
static process_t *pmm_zero_page = NULL;

/**
 * @brief Thread entry
 */
void pmm_zeroThread(void *context) {
    for (;;) {
        size_t target = min((size_t)2048, pmm_getFreeBlocks() / 4);
        for (int i = 0; i < (int)target && pmm_getZeroPages() < target; i++) {
            if (!pmm_zeroPage()) break;
        }

        sleep_time(15,0);
        sleep_enter();
    }
}

/**
 * @brief Initialize zero thread
 */
static int pmm_zeroInit() {
    pmm_zero_page = process_createKernel("zero_page", PROCESS_KERNEL, pmm_zeroThread, NULL);
    sched_insert(pmm_zero_page->main_thread);
    return 0;
}

SCHED_INIT_ROUTINE(zeropage, INIT_FLAG_DEFAULT, pmm_zeroInit);
