/**
 * @file drivers/example/ps2.c
 * @brief PS/2 driver for Hexahedron
 * 
 * @warning This driver is pretty messy
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
#include <kernel/mm/alloc.h>
#include <kernel/drivers/clock.h>
#include <kernel/debug.h>
#include <kernel/gfx/term.h>
#include <kernel/misc/args.h>
#include <kernel/misc/util.h>
#include <stdio.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:PS2", __VA_ARGS__)

/* PS/2 port count */
uint8_t ps2_port_count = 0;

/**
 * @brief Send a command to a device
 */
void ps2_sendDevice(uint8_t port, uint8_t data) {
    if (port == 1) {
        ps2_sendCommandParameter(PS2_COMMAND_WRITE_PORT2, data);
    } else {
        ps2_waitForInputClear();
        outportb(PS2_DATA, data);
    }
}

/**
 * @brief Send a command to a device and get an ACK response
 * Will wait for a PS2 ACK response, resend up to 10 times.
 * @returns 0 on success
 */
int ps2_sendDeviceACK(uint8_t port, uint8_t data) {
    for (int attempt = 0; attempt < 3; attempt++) {
        ps2_sendDevice(port, data);
        ps2_waitForOutput();
    
        uint8_t resp = inportb(PS2_DATA);

        if (resp == PS2_ACK) {
            return 0; // Success
        } else if (resp == PS2_RESEND) {
            LOG(WARN, "Device %d resending data %02x\n", port, data);
            continue;
        } else {
            LOG(ERR, "Device %d unexpected PS/2 response: %02x\n", port, resp);
            return 1;
        }
    }

    LOG(ERR, "3 attempts expired, device %d is not listening.\n", port);
    return 1;
}


/**
 * @brief Wait for input buffer to be empty
 * @returns 1 on timeout
 */
int ps2_waitForInputClear() {
    int timeout = 1000;
    while (timeout) {
        uint8_t status = inportb(PS2_STATUS);
        if (!(status & PS2_STATUS_INPUT_FULL)) return 0;
        clock_sleep(25);
        timeout -= 25;
    }

    return 1;
}

/**
 * @brief Wait for output buffer to be full
 * @returns 1 on timeout
 */
int ps2_waitForOutput() {
    int timeout = 1000;
    while (timeout) {
        uint8_t status = inportb(PS2_STATUS);
        if (status & PS2_STATUS_OUTPUT_FULL) return 0;
        clock_sleep(25);
        timeout -=  25;
    }

    return 1;
}


/**
 * @brief Send PS/2 command (single-byte)
 * @param command The command byte to send
 */
int ps2_sendCommand(uint8_t command) {
    if (ps2_waitForInputClear()) return 1;
    outportb(PS2_COMMAND, command);
    return 0;
}

/**
 * @brief Read a PS/2 byte
 * @returns LESS than 0 on error
 */
int ps2_readByte() {
    if (ps2_waitForOutput()) return -1; 
    return inportb(PS2_DATA);
}

/**
 * @brief Send PS/2 command (return value)
 * @param command The command byte to send
 * @returns PS2_DATA value
 */
int ps2_sendCommandResponse(uint8_t command) {
    if (ps2_waitForInputClear()) {
        return -1;
    }

    outportb(PS2_COMMAND, command);
    return ps2_readByte();
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
 * @brief Flush output buffer
 */
void ps2_flushOutput() {
    while (inportb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) ps2_readByte();
}

/**
 * @brief Identify PS/2 device
 */
void ps2_identifyDevices() {
    LOG(DEBUG, "PS/2 identification starting\n");

    for (int i = 0; i < ps2_port_count; i++) {
        // Reset and run device
        if (ps2_sendDeviceACK(i, PS2_DEVCMD_RESET)) {
            LOG(ERR, "Sending RESET command to device %d failed, assuming dead\n");
            continue;
        }

        // We should get a self test passed byte
        int test = ps2_readByte();
        

        // Apparently devices can send either..
        if (test < 0 || (test != PS2_SELF_TEST_PASS && test != PS2_PORT_TEST_PASS)) {
            if (test == PS2_SELF_TEST_FAIL) {
                LOG(WARN, "DEVICE %d: Self test failure\n", i);
                continue;
            } else if (test < 0) {
                LOG(INFO, "DEVICE %d: No device detected\n", i);
                continue;
            }   
        }

        // Read anything else the device sends until its ready
        ps2_flushOutput();

        // Now we can start the identification process. First disable scanning
        if (ps2_sendDeviceACK(i, PS2_DEVCMD_DISABLE_SCANNING)) {
            LOG(WARN, "Device %d: DISABLE_SCANNING failed\n", i);
            continue;
        }

        ps2_flushOutput();
        if (ps2_sendDeviceACK(i, PS2_DEVCMD_IDENTIFY)) {
            LOG(WARN, "Device %d: IDENTIFY failed\n", i);
            continue;
        }

        // Up to 2 bytes of reply with timeout
        uint8_t id_bytes[2] = { 0x0 };
        uint8_t byte_count = 0;
        for (; byte_count < 2; byte_count++) { int byte = ps2_readByte(); if (byte < 0) break; id_bytes[byte_count] = byte; }

        // Send enable scanning
        if (ps2_sendDeviceACK(i, PS2_DEVCMD_ENABLE_SCANNING)) {
            LOG(WARN, "Device %d: ENABLE_SCANNING failed\n", i);
            continue;
        }
    
        // Check those identification bytes!
        if (!byte_count) {
            LOG(INFO, "Device %d: AT keyboard (unsupported)\n", i);
            continue;
        }

        // Print identification exactly (for debug)
        switch (id_bytes[0]) {
            case 0x00:
                LOG(INFO, "Device %d: Standard PS/2 mouse\n", i);
                break;
            case 0x03:
                LOG(INFO, "Device %d: Mouse with scroll wheel\n", i);
                break;
            case 0x04:
                LOG(INFO, "Device %d: 5-button mouse\n", i);
                break;

            case 0xAB:

                if (byte_count == 2) {
                    // Switch case in a switch case...
                    switch (id_bytes[1]) {
                        case 0x83:
                        case 0xC1:
                        case 0x41:
                            LOG(INFO, "Device %d: MF2 keyboard\n", i);
                            break;

                        case 0x84:
                        case 0x54:
                            LOG(INFO, "Device %d: Short keyboard\n", i);
                            break;

                        case 0x85:
                            LOG(INFO, "Device %d: 122-key (or NCD N-97) keyboard\n", i);
                            break;

                        case 0x90:
                            LOG(INFO, "Device %d: Japanese \"G\" keyboard\n", i);
                            break;
                        
                        case 0x91:
                            LOG(INFO, "Device %d: Japanese \"P\" keyboard\n", i);
                            break;

                        case 0x92:
                            LOG(INFO, "Device %d: Japanese \"A\" keyboard\n", i);
                            break;

                        default:
                            LOG(WARN, "Device %d: Unrecognized PS/2 keyboard: %02x\n", i, id_bytes[1]);
                            break;
                    }

                    break;
                }

                // Fallthrough
            case 0xAC:
                if (byte_count == 2 && id_bytes[1] == 0xA1) {
                    LOG(INFO, "Device %d NCD Sun layout keyboard\n", i);
                    break;
                }

                // Fallthrough
            default:
                LOG(WARN, "Device %d: Unrecognized device (ID byte %02x)\n", id_bytes[0]);
                continue;
        }


        // Initialize devices
        if (!byte_count) continue; // AT keyboard unsupported

        if (byte_count == 1) {
            switch (id_bytes[0]) {
                case 0x00:
                case 0x03:
                case 0x04:
                    mouse_init(i);
                    break;
            }
        } else if (byte_count == 2) {
            if (id_bytes[0] == 0xAB) {
                switch (id_bytes[1]) {
                    case 0x41:
                    case 0x54:
                    case 0x83:
                    case 0x84:
                    case 0xC1:
                        kbd_init(i);
                        break;
                }
            }
        }
    }
}

/**
 * @brief Driver initialize method
 */
int driver_init(int argc, char **argv) {
    // TODO: Determine if PS/2 controller even exists. This is way harder than it looks...
    // TODO: Use ACPI to determine I/O ports

    LOG(INFO, "Initializing PS/2 controller...\n");

    if (ps2_sendCommand(PS2_COMMAND_DISABLE_PORT1)) {
        LOG(WARN, "Failed to send PS2_COMMAND_DISABLE_PORT1, assuming PS/2 controller doesn't exist\n");
        return DRIVER_STATUS_NO_DEVICE;
    }

    if (ps2_sendCommand(PS2_COMMAND_DISABLE_PORT2)) {
        LOG(WARN, "Failed to send PS2_COMMAND_DISABLE_PORT2, assuming PS/2 controller doesn't exist\n");
        return DRIVER_STATUS_NO_DEVICE;
    }

    // Clear output buffer
    int64_t timeout = 2000;
    while (inportb(PS2_STATUS) & 1 && timeout) {
        inportb(PS2_DATA);
        clock_sleep(25);
        timeout -= 25;
    }

    if (!timeout) {
        LOG(WARN, "PS/2 timeout detected, assuming dead controller\n");
        return DRIVER_STATUS_NO_DEVICE;
    }


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

    ps2_port_count = 1 + !!dual_channel;

    // Ok, controller looks good! Let's setup our interfaces
    ccb = ps2_sendCommandResponse(PS2_COMMAND_READ_CCB);
    ccb |= PS2_CCB_PORT2INT | PS2_CCB_PORT1INT | PS2_CCB_PORTTRANSLATION;
    ps2_sendCommandParameter(PS2_COMMAND_WRITE_CCB, ccb);

    // Re-enable the ports
    ps2_sendCommand(PS2_COMMAND_ENABLE_PORT1);
    if (dual_channel) ps2_sendCommand(PS2_COMMAND_ENABLE_PORT2);

    ps2_identifyDevices();

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

