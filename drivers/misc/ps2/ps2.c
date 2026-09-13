/**
 * @file drivers/misc/ps2/ps2.c
 * @brief Primary i8042 PS/2 controller
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include "ps2.h"
#include <kernel/loader/driver.h>
#include <kernel/arch/arch.h>
#include <kernel/misc/args.h>
#include <kernel/fs/periphfs.h>
#include <kernel/subsystems/irq.h>
#include <kernel/debug.h>
#include <kernel/misc/util.h>

/* mouse state */
uint8_t ps2_mouse_packet[4];
int ps2_mouse_packet_cycle = 0;
uint32_t ps2_last_buttons = 0;
uint8_t ps2_mouse_id = 0;
uint8_t ps2_mouse_packet_size = 0;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:PS2", __VA_ARGS__)

/**
 * @brief Read byte
 */
static int ps2_readByte(uint8_t *resp) {
    int timeout = 1000;
    while (timeout > 0) {
        if ((inportb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL)) {
            if (resp) *resp = inportb(PS2_DATA);
            else (void)inportb(PS2_DATA);
            return 0;
        }

        clock_sleep(25);
        timeout -= 25;
    }

    LOG(ERR, "Timed out reading input byte\n");
    return 1;
}

/**
 * @brief Drain the output buffer
 */
static void ps2_flushOutput() {
    while (inportb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) {
        inportb(PS2_DATA);
    }
}

/**
 * @brief Wait until the controller is ready to receive data
 */
static int ps2_waitInputClear() {
    int timeout = 1000;
    while (timeout > 0) {
        if ((inportb(PS2_STATUS) & PS2_STATUS_INPUT_FULL) == 0) return 0;
        clock_sleep(25);
        timeout -= 25;
    }

    LOG(ERR, "Timed out waiting for controller input buffer to clear\n");
    return 1;
}

/**
 * @brief Send byte
 */
static int ps2_sendByte(uint16_t port, uint8_t byte) {
    int timeout = 1000;
    while (timeout > 0) {
        if ((inportb(PS2_STATUS) & PS2_STATUS_INPUT_FULL) == 0) {
            outportb(port, byte);
            return 0;
        }

        clock_sleep(25);
        timeout -= 25;
    }

    LOG(ERR, "Writing 0x%x to port 0x%x timed out waiting for INPUT_FULL\n", port, byte);
    return 1;
}

/**
 * @brief Send PS/2 command
 */
static int ps2_sendCommand(uint8_t command) {
    int timeout = 1000;
    while (timeout > 0) {
        if ((inportb(PS2_STATUS) & PS2_STATUS_INPUT_FULL) == 0) {
            outportb(PS2_COMMAND, command);
            return 0;
        }

        clock_sleep(25);
        timeout -= 25;
    }

    LOG(ERR, "Timed out sending command 0x%x (input_full never cleared)\n", command);
    return 1;
}

/**
 * @brief Send command and get response
 */
static int ps2_sendCommandResponse(uint8_t command, uint8_t *resp) {
    if (ps2_sendByte(PS2_COMMAND, command)) return 1;
    return ps2_readByte(resp);
}

/**
 * @brief Send command with parameter
 */
static int ps2_sendCommandParameter(uint8_t command, uint8_t parameter) {
    if (ps2_sendByte(PS2_COMMAND, command)) return 1;
    if (ps2_sendByte(PS2_DATA, parameter)) return 1;
    return 0;
}

/**
 * @brief Send byte to device
 */
static int ps2_sendDeviceByte(uint8_t device, uint8_t byte) {
    if (device == 1) {
        if (ps2_sendCommand(PS2_COMMAND_WRITE_PORT2)) {
            return 1;
        }
    }

    return ps2_sendByte(PS2_DATA, byte);
}

/**
 * @brief Send byte to device, waiting for ACK
 */
static int ps2_sendDeviceByteAck(uint8_t device, uint8_t byte) {
    for (int attempt = 0; attempt < 3; attempt++) {
        if (ps2_sendDeviceByte(device, byte)) return 1;

        uint8_t resp;
        if (ps2_readByte(&resp)) return 1;

        if (resp == PS2_ACK) {
            return 0; // Success
        } else if (resp == PS2_RESEND) {
            LOG(WARN, "Device %d resending data %02x\n", device, byte);
            continue;
        } else {
            LOG(ERR, "Device %d unexpected PS/2 response: %02x\n", device, byte);
            return 1;
        }
    }

    LOG(ERR, "3 attempts expired, device %d is not listening.\n", device);
    return 1;
}

/**
 * @brief mouse irq
 */
int mouse_irq(irq_t *irq, void *context) {
    uint8_t data = inportb(PS2_DATA);

    // Get the next byte in the cycle
    ps2_mouse_packet[ps2_mouse_packet_cycle] = data;
    if (!ps2_mouse_packet_cycle && !(ps2_mouse_packet[ps2_mouse_packet_cycle] & 0x08)) {
        return IRQ_HANDLED;
    }
    ps2_mouse_packet_cycle++;
    

    // TODO: Support for more advanced PS/2 mouse modes
    if (ps2_mouse_packet_cycle < ps2_mouse_packet_size) {
        return IRQ_HANDLED;
    }

    // Reset packet cycle
    ps2_mouse_packet_cycle = 0;

    // Start building data
    int x_diff = ps2_mouse_packet[1];
    int y_diff = ps2_mouse_packet[2];
    int scroll = MOUSE_SCROLL_NONE;

    if (ps2_mouse_id == 0x03 || ps2_mouse_id == 0x04) {
        if ((int8_t)(ps2_mouse_packet[3]) < 0) {
            // Scrolling up
            scroll = MOUSE_SCROLL_UP;
        } else if ((int8_t)(ps2_mouse_packet[3]) > 0) {
            // Scrolling down
            scroll = MOUSE_SCROLL_DOWN;
        }
    }

    // Check for signatures
    if (x_diff && ps2_mouse_packet[0] & (1 << 4)) x_diff = x_diff - 0x100;
    if (y_diff && ps2_mouse_packet[0] & (1 << 5)) y_diff = y_diff - 0x100;

    // Check for overflow
    if (ps2_mouse_packet[0] & PS2_MOUSE_DATA_X_OVERFLOW) x_diff = 0;
    if (ps2_mouse_packet[0] & PS2_MOUSE_DATA_Y_OVERFLOW) y_diff = 0;

    // Get buttons
    uint32_t buttons =  (ps2_mouse_packet[0] & PS2_MOUSE_DATA_LEFTBTN ? MOUSE_BUTTON_LEFT : 0) | 
                        (ps2_mouse_packet[0] & PS2_MOUSE_DATA_RIGHTBTN ? MOUSE_BUTTON_RIGHT : 0) | 
                        (ps2_mouse_packet[0] & PS2_MOUSE_DATA_MIDDLEBTN ? MOUSE_BUTTON_MIDDLE : 0);

    if (buttons == ps2_last_buttons && !x_diff && !y_diff && !scroll) {
        return IRQ_HANDLED;
    }

    periphfs_sendMouseEventRelative(buttons, x_diff, y_diff, scroll);
    ps2_last_buttons = buttons;
    return IRQ_HANDLED;
}

/**
 * @brief Set mouse sample rate
 */
static int mouse_setRate(uint8_t device, uint8_t rate) {
    if (ps2_sendDeviceByteAck(device, PS2_MOUSE_SET_SAMPLE_RATE)) return 1;
    if (ps2_sendDeviceByteAck(device, rate)) return 1;
    return 0;
}

/**
 * @brief Mouse init
 */
static void mouse_init(uint8_t device) {
    LOG(INFO, "Initializing mouse...\n");
    mouse_setRate(device,200);
    mouse_setRate(device,100);
    mouse_setRate(device,80);

    if (ps2_sendDeviceByteAck(device, PS2_DEVCMD_IDENTIFY)) {
        LOG(ERR, "Mouse init failed (PS2_DEVCMD_IDENTIFY timed out)\n");
        return;
    } 

    uint8_t byte;
    if (ps2_readByte(&byte)) {
        LOG(ERR, "Mouse init failed (PS2_DEVCMD_IDENTIFY no bytes)\n");
        return;
    }

    ps2_mouse_id = byte;
    switch (byte) {
        case 0x00:
            // Standard PS/2 mouse
            ps2_mouse_packet_size = 3;
            break;

        case 0x03:
            // PS/2 mouse with scrollwheel
            ps2_mouse_packet_size = 4;
            break;
        
        case 0x04:
            // PS/2 mouse with 5 buttons
            ps2_mouse_packet_size = 4;
            break;

        default:
            LOG(ERR, "Unsupported PS/2 mouse %02x\n", byte);
            break;
    }

    LOG(DEBUG, "Calculated mouse packet size: %d\n", ps2_mouse_packet_size);

    if (ps2_sendDeviceByteAck(device, PS2_MOUSE_ENABLE_DATA_REPORTING)) {
        LOG(ERR, "Failed to enable mouse data reporting\n");
        return;
    }

    // Register IRQ
    irq_number_t vect;
    if (irq_allocate(global_domain, (device == 1) ? PS2_DEVICE1_IRQ : PS2_DEVICE0_IRQ, NULL, &vect)) {
        LOG(ERR, "Failed to allocate IRQ for mouse on port %d\n", device + 1);
        return;
    }

    irq_register(vect, mouse_irq, 0, (void*)(uintptr_t)device, NULL);
}

/**
 * @brief keyboard irq
 */
int keyboard_irq(irq_t *irq, void *context) {
    uint8_t ch = inportb(PS2_DATA);

	int event_type = (ch >= 0x80) ? EVENT_KEY_RELEASE : EVENT_KEY_PRESS;

    // Determine the scancode we should print
    key_scancode_t sc = ch;
	periphfs_sendKeyboardEvent(event_type, sc);

    return IRQ_HANDLED;
}

/**
 * @brief Keyboard init
 */
static void keyboard_init(uint8_t device) {
    LOG(INFO, "Initializing keyboard...\n");
    if (ps2_sendDeviceByteAck(device, PS2_KEYBOARD_SET_SCANCODE)) {
        LOG(ERR, "PS2_KEYBOARD_SET_SCANCODE failed\n");
        return;
    }

    if (ps2_sendDeviceByteAck(device, 2)) {
        LOG(ERR, "PS2_KEYBOARD_SET_SCANCODE (part 2) failed\n");
        return;
    }

    if (ps2_sendDeviceByteAck(device, PS2_DEVCMD_ENABLE_SCANNING)) {
        LOG(ERR, "PS2_DEVCMD_ENABLE_SCANNING failed\n");
        return;
    }

    // Register IRQ
    irq_number_t vect;
    if (irq_allocate(global_domain, (device == 1) ? PS2_DEVICE1_IRQ : PS2_DEVICE0_IRQ, NULL, &vect)) {
        LOG(ERR, "Failed to allocate IRQ for keyboard on port %d\n", device + 1);
        return;
    }

    irq_register(vect, keyboard_irq, 0, (void*)(uintptr_t)device, NULL);
}

/**
 * @brief Identify a device
 */
static int ps2_identify(uint8_t device) {
    // Reset the device
    if (ps2_sendDeviceByteAck(device, PS2_DEVCMD_RESET)) {
        LOG(ERR, "Device %d: PS2_DEVCMD_RESET failed\n", device);
        return 1;
    }

    // Check self test result
    uint8_t test;
    if (ps2_readByte(&test)) {
        LOG(ERR, "Device %d: PS2_DEVCMD_RESET did not give status code\n", device);
        return 1;
    }

    if (test != PS2_SELF_TEST_PASS) {
        LOG(ERR, "Device %d failed self test: response code 0x%x\n", test);
        return 1;
    }

    // Flush
    while (ps2_readByte(NULL) == 0) arch_pause_single();

    // Identify
    if (ps2_sendDeviceByteAck(device, PS2_DEVCMD_DISABLE_SCANNING)) {
        LOG(ERR, "Device %d: PS2_DEVCMD_DISABLE_SCANNING failed\n", device);
        return 1;
    }

    // Flush
    while (ps2_readByte(NULL) == 0) arch_pause_single();

    if (ps2_sendDeviceByteAck(device, PS2_DEVCMD_IDENTIFY)) {
        LOG(ERR, "Device %d: PS2_DEVCMD_IDENTIFY failed\n", device);
        return 1;
    }

    uint8_t id_bytes[2] = { 0 };
    bool have_two = false;
    
    if (ps2_readByte(&id_bytes[0])) {
        LOG(ERR, "Device %d: PS2_DEVCMD_IDENTIFY did not give any bytes\n", device);
        return 1;
    }

    if (ps2_readByte(&id_bytes[1]) == 0) {
        have_two = true;
    }


    switch (id_bytes[0]) {
        case 0x00:
            LOG(INFO, "Device %d: Standard PS/2 mouse\n", device);
            break;
        case 0x03:
            LOG(INFO, "Device %d: Mouse with scroll wheel\n", device);
            break;
        case 0x04:
            LOG(INFO, "Device %d: 5-button mouse\n", device);
            break;

        case 0xAB:
            if (have_two) {
                // Switch case in a switch case...
                switch (id_bytes[1]) {
                    case 0x83:
                    case 0xC1:
                    case 0x41:
                        LOG(INFO, "Device %d: MF2 keyboard\n", device);
                        break;

                    case 0x84:
                    case 0x54:
                        LOG(INFO, "Device %d: Short keyboard\n", device);
                        break;

                    case 0x85:
                        LOG(INFO, "Device %d: 122-key (or NCD N-97) keyboard\n", device);
                        break;

                    case 0x90:
                        LOG(INFO, "Device %d: Japanese \"G\" keyboard\n", device);
                        break;
                    
                    case 0x91:
                        LOG(INFO, "Device %d: Japanese \"P\" keyboard\n", device);
                        break;

                    case 0x92:
                        LOG(INFO, "Device %d: Japanese \"A\" keyboard\n", device);
                        break;

                    default:
                        LOG(WARN, "Device %d: Unrecognized PS/2 keyboard: %02x\n", device, id_bytes[1]);
                        break;
                }

                break;
            }

            // Fallthrough
        case 0xAC:
            if (have_two && id_bytes[1] == 0xA1) {
                LOG(INFO, "Device %d NCD Sun layout keyboard\n", device);
                break;
            }

            // Fallthrough
        default:
            LOG(WARN, "Device %d: Unrecognized device (ID byte %02x)\n", device, id_bytes[0]);
            return 1;
    }


    if (have_two == false) {
        switch (id_bytes[0]) {
            case 0x00:
            case 0x03:
            case 0x04:
                mouse_init(device);
                break;
        }
    } else {
        if (id_bytes[0] == 0xAB) {
            switch (id_bytes[1]) {
                case 0x41:
                case 0x54:
                case 0x83:
                case 0x84:
                case 0xC1:
                    keyboard_init(device);
                    break;
            }
        }
    }

    return 0;
}

/**
 * @brief Driver init method
 */
int driver_init(int argc, char *argv[]) {
    LOG(DEBUG, "i8042 begin initialization\n");

    if (ps2_sendCommand(PS2_COMMAND_DISABLE_PORT1)) goto _error;
    if (ps2_sendCommand(PS2_COMMAND_DISABLE_PORT2)) goto _error;


    // Clear output buffer
    int64_t timeout = 2000;
    while (inportb(PS2_STATUS) & 1 && timeout) {
        inportb(PS2_DATA);
        clock_sleep(25);
        timeout -= 25;
    }

    if (!timeout) {
        LOG(WARN, "PS/2 timeout detected while flushing output buffer, assuming dead controller\n");
        return DRIVER_STATUS_NO_DEVICE;
    }

    // Disable everything
    uint8_t ccb;
    if (ps2_sendCommandResponse(PS2_COMMAND_READ_CCB, &ccb)) goto _error;
    ccb &= ~(PS2_CCB_PORT1INT | PS2_CCB_PORT2INT | PS2_CCB_PORTTRANSLATION);
    if (ps2_sendCommandParameter(PS2_COMMAND_WRITE_CCB, ccb)) goto _error;

    // Test the controller now
    // NOTE: according to banan-os the controller will reset occasionally
    uint8_t test;
    if (ps2_sendCommandResponse(PS2_COMMAND_TEST_CONTROLLER, &test)) goto _error;
    if (test != PS2_CONTROLLER_TEST_PASS) {
        LOG(ERR, "PS/2 controller failed test (return value 0x%x, expected 0x%x).\n", test, PS2_CONTROLLER_TEST_PASS);
        
        if (kargs_has("--ps2-ignore-tests") == 0) {
            goto _error;
        }
    }

    if (ps2_sendCommandParameter(PS2_COMMAND_WRITE_CCB, ccb)) goto _error;

    bool ports[2] = { true, false };

    // Check for port 2
    if (ccb & PS2_CCB_PORT2CLK) {
        if (ps2_sendCommand(PS2_COMMAND_ENABLE_PORT2)) goto _error;
        
        uint8_t clk_test;
        if (ps2_sendCommandResponse(PS2_COMMAND_READ_CCB, &clk_test)) goto _error;
        if ((clk_test & PS2_CCB_PORT2CLK) == 0) {
            ports[1] = true;

            // disable it for now to be re-enabled later
            if (ps2_sendCommand(PS2_COMMAND_DISABLE_PORT2)) goto _error;
        }
    }

    LOG(INFO, "PS/2 has %d ports available\n", ports[1] ? 2 : 1);

    // Execute port tests
    if (!kargs_has("--ps2-ignore-tests")) {
        uint8_t tval;
        if (ps2_sendCommandResponse(PS2_COMMAND_TEST_PORT1, &tval)) goto _error;
        if (tval != PS2_PORT_TEST_PASS) {
            LOG(ERR, "Port 1 test failure (returned 0x%x, expected 0x00)\n", tval);
            ports[0] = false;
        }

        if (ports[1] == true) {
            if (ps2_sendCommandResponse(PS2_COMMAND_TEST_PORT2, &tval)) goto _error;
            if (tval != PS2_PORT_TEST_PASS) {
                LOG(ERR, "Port 2 test failure (returned 0x%x, expected 0x00)\n", tval);
                ports[1] = false;
            }
        }
    } else {
        LOG(INFO, "Skipping PS/2 port tests\n");
    }

    LOG(INFO, "Test results: port 1 %s, port 2 %s\n", (ports[0] ? "PASS" : "FAILED"), (ports[1] ? "PASS" : "FAILED"));

    if (ports[0]) {
        if (ps2_sendCommand(PS2_COMMAND_ENABLE_PORT1)) goto _error;
    
        if (ps2_identify(0)) {
            LOG(ERR, "Failed to identify device on port 1\n");
            ports[0] = false;
        }

        if (ps2_sendCommand(PS2_COMMAND_DISABLE_PORT1)) goto _error;
    }

    if (ports[1]) {
        if (ps2_sendCommand(PS2_COMMAND_ENABLE_PORT2)) goto _error;
    
        if (ps2_identify(1)) {
            LOG(ERR, "Failed to identify device on port 2\n");
            ports[1] = false;
        }

        if (ps2_sendCommand(PS2_COMMAND_DISABLE_PORT2)) goto _error;
    }

    if (ps2_waitInputClear()) goto _error;
    ps2_flushOutput();

    ccb |= PS2_CCB_PORT1CLK | PS2_CCB_PORT2CLK;
    ccb |= PS2_CCB_PORTTRANSLATION;
    if (ports[0]) {
        ccb |= PS2_CCB_PORT1INT;
    }

    if (ports[1]) {
        ccb |= PS2_CCB_PORT2INT;
    }

    if (ps2_sendCommandParameter(PS2_COMMAND_WRITE_CCB, ccb)) {
        LOG(ERR, "Failed to write CCB\n");
    }

    if (ports[0] && ps2_sendCommand(PS2_COMMAND_ENABLE_PORT1)) goto _error;
    if (ports[1] && ps2_sendCommand(PS2_COMMAND_ENABLE_PORT2)) goto _error;

    return DRIVER_STATUS_SUCCESS;

_error:
    LOG(ERR, "PS/2 controller initialization failed\n");
    return DRIVER_STATUS_ERROR;
}

/**
 * @brief Driver deinit method
 */
int driver_deinit() {
    return 0;
}


struct driver_metadata driver_metadata = {
    .name = "I8042 PS/2 Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit,
};
