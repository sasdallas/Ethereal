/**
 * @file drivers/usb/xhci/xhci_regs.h
 * @brief xHCI registers
 * 
 * This file was written by FlareCoding for his xHCI development series.
 * It was adapted to Hexahedron for easier xHCI driver writing.
 * Thank you!
 * 
 * @copyright
 * Copyright (c) 2023 Albert Slepak
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef XHCI_REGS_H
#define XHCI_REGS_H

#include "xhci_mem.h"

typedef struct xhci_capability_registers {
    const uint8_t caplength;    // Capability Register Length
    const uint8_t reserved0;
    const uint16_t hciversion;  // Interface Version Number
    const uint32_t hcsparams1;  // Structural Parameters 1
    const uint32_t hcsparams2;  // Structural Parameters 2
    const uint32_t hcsparams3;  // Structural Parameters 3
    const uint32_t hccparams1;  // Capability Parameters 1
    const uint32_t dboff;       // Doorbell Offset
    const uint32_t rtsoff;      // Runtime Register Space Offset
    const uint32_t hccparams2;  // Capability Parameters 2
} __attribute__((packed)) xhci_cap_regs_t;

typedef struct xhci_operational_registers {
    uint32_t usbcmd;        // USB Command
    uint32_t usbsts;        // USB Status
    uint32_t pagesize;      // Page Size
    uint32_t reserved0[2];
    uint32_t dnctrl;        // Device Notification Control
    uint64_t crcr;          // Command Ring Control
    uint32_t reserved1[4];
    uint64_t dcbaap;        // Device Context Base Address Array Pointer
    uint32_t config;        // Configure
    uint32_t reserved2[49];
    // Port Register Set offset has to be calculated dynamically based on MAXPORTS
} __attribute__((packed)) xhci_op_regs_t;

#endif // XHCI_REGS_H