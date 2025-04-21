/**
 * @file drivers/example/ps2.c
 * @brief PS/2 driver for Hexahedron
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifdef __ARCH_I386__
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#endif

#include "ps2.h"
#include <kernel/loader/driver.h>
#include <kernel/fs/vfs.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <kernel/gfx/term.h>
#include <kernel/misc/args.h>
#include <stdio.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:PS2", __VA_ARGS__)

/**
 * @brief Wait for input buffer to be empty
 * @returns 1 on timeout
 */
int ps2_waitForInputClear() {
    int timeout = 100000;
    while (timeout) {
        uint8_t status = inportb(PS2_STATUS);
        if (!(status & PS2_STATUS_INPUT_FULL)) return 0;
    }

    LOG(ERR, "Timeout expired\n");
    return 1;
}

/**
 * @brief Wait for output buffer to be full
 * @returns 1 on timeout
 */
int ps2_waitForOutput() {
    int timeout = 100000;
    while (timeout) {
        uint8_t status = inportb(PS2_STATUS);
        if (status & PS2_STATUS_OUTPUT_FULL) return 0;
    }

    LOG(ERR, "Timeout expired\n");
    return 1;
}


/**
 * @brief Send PS/2 command (single-byte)
 * @param command The command byte to send
 */
void ps2_sendCommand(uint8_t command) {
    ps2_waitForInputClear();
    outportb(PS2_COMMAND, command);
}

/**
 * @brief Send PS/2 command (return value)
 * @param command The command byte to send
 * @returns PS2_DATA value
 */
uint8_t ps2_sendCommandResponse(uint8_t command) {
    ps2_waitForInputClear();
    outportb(PS2_COMMAND, command);
    ps2_waitForOutput();
    return inportb(PS2_DATA);
}

/**
 * @brief Send a multi-byte command
 * @param command The command byte to send
 * @param data The data byte to send
 */
void ps2_sendCommandParameter(uint8_t command, uint8_t data) {
    ps2_waitForInputClear();
    outportb(PS2_COMMAND, command);
    ps2_waitForInputClear();
    outportb(PS2_DATA, data);
}

/**
 * @brief Send something to the mouse PS/2 port (PORT2)
 * @param data What to send
 * @returns Usually an ACK value (0xFA)
 */
uint8_t ps2_writeMouse(uint8_t data) {
    ps2_sendCommandParameter(PS2_COMMAND_WRITE_PORT2, data);
    ps2_waitForOutput();
    return inportb(PS2_DATA);
}

/**
 * @brief Driver initialize method
 */
int driver_init(int argc, char **argv) {
    // TODO: Determine if PS/2 controller even exists. This is way harder than it looks...

    LOG(INFO, "Initializing PS/2 controller...\n");

    ps2_sendCommand(PS2_COMMAND_DISABLE_PORT1);
    ps2_sendCommand(PS2_COMMAND_DISABLE_PORT2);

    // Clear output buffer
    // TODO: Timeout implementation?
    while (inportb(PS2_STATUS) & 1) inportb(PS2_DATA);

    // Test the PS/2 controller
    uint8_t ccb = ps2_sendCommandResponse(PS2_COMMAND_READ_CCB);

    
    uint8_t test_success = ps2_sendCommandResponse(PS2_COMMAND_TEST_CONTROLLER);
    if (test_success != PS2_CONTROLLER_TEST_PASS && !kargs_has("--ps2-disable-tests")) {
        LOG(ERR, "Controller did not pass test. Error: 0x%02x\n", test_success);
        return 1;
    } else if (test_success != PS2_CONTROLLER_TEST_PASS) {
        LOG(WARN, "Ignoring PS/2 controller self test fail: 0x%02x\n", test_success);
    }

    LOG(DEBUG, "Successfully passed PS/2 controller test\n");

    // Check to see if there are two channels in the PS/2 controller
    ps2_sendCommand(PS2_COMMAND_ENABLE_PORT2);

    int dual_channel = 0;
    if (!(ps2_sendCommandResponse(PS2_COMMAND_READ_CCB) & PS2_CCB_PORT2CLK)) {
        // Dual channel PS/2 controller
        LOG(DEBUG, "Detected a dual PS/2 controller\n");
        dual_channel = 1;

        // Enable the clock for PS/2 port #2 and disable IRQs (we will configure after interface tests)
        ccb &= ~(PS2_CCB_PORT2CLK | PS2_CCB_PORT2INT);
        ps2_sendCommandParameter(PS2_COMMAND_WRITE_CCB, ccb);
    } else {
        // Single channel PS/2 controller
        LOG(DEBUG, "Single-channel PS/2 controller detected\n");
    }
    
    // Now we should test the interfaces 
    uint8_t port1_test = ps2_sendCommandResponse(PS2_COMMAND_TEST_PORT1);
    if (port1_test != PS2_PORT_TEST_PASS && !kargs_has("--ps2-disable-tests")) {
        LOG(ERR, "PS/2 controller reports a failure on Port #1: 0x%02x\n", port1_test);
        printf(COLOR_CODE_YELLOW "PS/2 controller detected failures on port #1\n");
        return 1;
    } else if (port1_test != PS2_PORT_TEST_PASS) {
        LOG(WARN, "Ignoring PS/2 Port #1 failure: 0x%02x\n", port1_test);
    }

    if (dual_channel) {
        uint8_t port2_test = ps2_sendCommandResponse(PS2_COMMAND_TEST_PORT2);
        if (port2_test != PS2_PORT_TEST_PASS && !kargs_has("--ps2-disable-tests")) {
            LOG(ERR, "PS/2 controller reports a failure on Port #2: 0x%02x\n", port1_test);
            printf(COLOR_CODE_YELLOW "PS/2 controller detected failures on port #2\n");
            return 1;
        } else if (port2_test != PS2_PORT_TEST_PASS) {
            LOG(WARN, "Ignoring PS/2 Port #2 failure: 0x%02x\n", port2_test);
        }     
    }


    // Ok, controller looks good! Let's setup our interfaces
    ccb = ps2_sendCommandResponse(PS2_COMMAND_READ_CCB);
    ccb |= PS2_CCB_PORT2INT | PS2_CCB_PORT1INT | PS2_CCB_PORTTRANSLATION;
    ps2_sendCommandParameter(PS2_COMMAND_WRITE_CCB, ccb);

    // Re-enable the ports
    ps2_sendCommand(PS2_COMMAND_ENABLE_PORT1);
    if (dual_channel) ps2_sendCommand(PS2_COMMAND_ENABLE_PORT2);

    // Initialize keyboard
    kbd_init();

    return 0;
}

/**
 * @brief Driver deinitialize method
 */
int driver_deinit() {
    return 0;
}

struct driver_metadata driver_metadata = {
    .name = "PS/2 Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};

