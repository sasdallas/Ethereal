/**
 * @file drivers/usb/xhci/xhci_device.h
 * @brief xHCI device structures
 * 
 * This file was partially written by FlareCoding for his xHCI development series.
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
 * 
 * Copyright (c) 2025 Samuel Stuart
 */

#ifndef _XHCI_DEVICE_H
#define _XHCI_DEVICE_H

/**** INCLUDES ****/
#include <stdint.h>
#include "xhci_common.h"
#include <kernel/misc/pool.h>

/**** TYPES ****/

typedef struct xhci_endpoint_context64 {
    union {
        struct {
            /*
                Endpoint State (EP State). The Endpoint State identifies the current operational state of the
                endpoint.
                
                Value Definition
                    0 Disabled The endpoint is not operational
                    1 Running The endpoint is operational, either waiting for a doorbell ring or processing
                    TDs.
                    2 HaltedThe endpoint is halted due to a Halt condition detected on the USB. SW shall issue
                    Reset Endpoint Command to recover from the Halt condition and transition to the Stopped
                    state. SW may manipulate the Transfer Ring while in this state.
                    3 Stopped The endpoint is not running due to a Stop Endpoint Command or recovering
                    from a Halt condition. SW may manipulate the Transfer Ring while in this state.
                    4 Error The endpoint is not running due to a TRB Error. SW may manipulate the Transfer
                    Ring while in this state.
                    5-7 Reserved

                As Output, a Running to Halted transition is forced by the xHC if a STALL condition is detected
                on the endpoint. A Running to Error transition is forced by the xHC if a TRB Error condition is
                detected.
                As Input, this field is initialized to ‘0’ by software.
                Refer to section 4.8.3 for more information on Endpoint State.
            */
            uint32_t endpoint_state        : 3;

            // Reserved
            uint32_t rsvd0                 : 5;

            /*
                Mult. If LEC = ‘0’, then this field indicates the maximum number of bursts within an Interval that
                this endpoint supports. Mult is a “zero-based” value, where 0 to 3 represents 1 to 4 bursts,
                respectively. The valid range of values is ‘0’ to ‘2’. This field shall be ‘0’ for all endpoint types
                except for SS Isochronous.
                If LEC = ‘1’, then this field shall be RsvdZ and Mult is calculated as:
                ROUNDUP(Max ESIT Payload / Max Packet Size / (Max Burst Size + 1)) - 1
            */
            uint32_t mult                  : 2;
            
            /*
                Max Primary Streams (MaxPStreams). This field identifies the maximum number of Primary
                Stream IDs this endpoint supports. Valid values are defined below. If the value of this field is ‘0’,
                then the TR Dequeue Pointer field shall point to a Transfer Ring. If this field is > '0' then the TR
                Dequeue Pointer field shall point to a Primary Stream Context Array. Refer to section 4.12 for
                more information.
                A value of ‘0’ indicates that Streams are not supported by this endpoint and the Endpoint
                Context TR Dequeue Pointer field references a Transfer Ring.
                A value of ‘1’ to ‘15’ indicates that the Primary Stream ID Width is MaxPstreams+1 and the
                Primary Stream Array contains 2MaxPStreams+1 entries.
                For SS Bulk endpoints, the range of valid values for this field is defined by the MaxPSASize field
                in the HCCPARAMS1 register (refer to Table 5-13).
                This field shall be '0' for all SS Control, Isoch, and Interrupt endpoints, and for all non-SS
                endpoints
            */
            uint32_t max_primary_streams   : 5;
            
            /*
                Linear Stream Array (LSA). This field identifies how a Stream ID shall be interpreted.
                Setting this bit to a value of ‘1’ shall disable Secondary Stream Arrays and a Stream ID shall be
                interpreted as a linear index into the Primary Stream Array, where valid values for MaxPStreams
                are ‘1’ to ‘15’.
                A value of ‘0’ shall enable Secondary Stream Arrays, where the low order (MaxPStreams+1) bits
                of a Stream ID shall be interpreted as a linear index into the Primary Stream Array, where valid
                values for MaxPStreams are ‘1’ to ‘7’. And the high order bits of a Stream ID shall be interpreted
                as a linear index into the Secondary Stream Array.
                If MaxPStreams = ‘0’, this field RsvdZ.
                Refer to section 4.12.2 for more information.
            */
            uint32_t linear_stream_array   : 1;
            
            /*
                Interval. The period between consecutive requests to a USB endpoint to send or receive data.
                Expressed in 125 μs. increments. The period is calculated as 125 μs. * 2^Interval; e.g., an Interval
                value of 0 means a period of 125 μs. (20 = 1 * 125 μs.), a value of 1 means a period of 250 μs. (21
                = 2 * 125 μs.), a value of 4 means a period of 2 ms. (24 = 16 * 125 μs.), etc. Refer to Table 6-12
                for legal Interval field values. See further discussion of this field below. Refer to section 6.2.3.6
                for more information.
            */
            uint32_t interval              : 8;
            
            /*
                Max Endpoint Service Time Interval Payload High (Max ESIT Payload Hi). If LEC = '1', then this
                field indicates the high order 8 bits of the Max ESIT Payload value. If LEC = '0', then this field
                shall be RsvdZ. Refer to section 6.2.3.8 for more information.
            */
            uint32_t max_esit_payload_hi   : 8;
        };
        uint32_t dword0;
    };

    union {
        struct {
            // Reserved
            uint32_t rsvd1                 : 1;

            /*
                Error Count (CErr)112. This field defines a 2-bit down count, which identifies the number of
                consecutive USB Bus Errors allowed while executing a TD. If this field is programmed with a
                non-zero value when the Endpoint Context is initialized, the xHC loads this value into an internal
                Bus Error Counter before executing a USB transaction and decrements it if the transaction fails.
                If the Bus Error Counter counts from ‘1’ to ‘0’, the xHC ceases execution of the TRB, sets the
                endpoint to the Halted state, and generates a USB Transaction Error Event for the TRB that
                caused the internal Bus Error Counter to decrement to ‘0’. If system software programs this field
                to ‘0’, the xHC shall not count errors for TRBs on the Endpoint’s Transfer Ring and there shall be
                no limit on the number of TRB retries. Refer to section 4.10.2.7 for more information on the
                operation of the Bus Error Counter.
                Note: CErr does not apply to Isoch endpoints and shall be set to ‘0’ if EP Type = Isoch Out ('1') or
                Isoch In ('5').
            */
            uint32_t error_count             : 2;

            /*
                Endpoint Type (EP Type). This field identifies whether an Endpoint Context is Valid, and if so,
                what type of endpoint the context defines.
                
                Value  Endpoint      Type Direction
                0      Not Valid     N/A
                1      Isoch         Out
                2      Bulk          Out
                3      Interrupt     Out
                4      Control       Bidirectional
                5      Isoch         In
                6      Bulk          In
                7      Interrupt     In
            */
            uint32_t endpoint_type           : 3;
            
            // Reserved
            uint32_t rsvd2                   : 1;
            
            /*
                Host Initiate Disable (HID). This field affects Stream enabled endpoints, allowing the Host
                Initiated Stream selection feature to be disabled for the endpoint. Setting this bit to a value of
                ‘1’ shall disable the Host Initiated Stream selection feature. A value of ‘0’ will enable normal
                Stream operation. Refer to section 4.12.1.1 for more information.
            */
            uint32_t host_initiate_disable   : 1;
            
            /*
                Max Burst Size. This field indicates to the xHC the maximum number of consecutive USB
                transactions that should be executed per scheduling opportunity. This is a “zero-based” value,
                where 0 to 15 represents burst sizes of 1 to 16, respectively. Refer to section 6.2.3.4 for more
                information.
            */
            uint32_t max_burst_size          : 8;
            
            /*
                Max Packet Size. This field indicates the maximum packet size in bytes that this endpoint is
                capable of sending or receiving when configured. Refer to section 6.2.3.5 for more information
            */
            uint32_t max_packet_size         : 16;
        };
        uint32_t dword1;
    };

    union {
        struct {
            /*
                Dequeue Cycle State (DCS). This bit identifies the value of the xHC Consumer Cycle State (CCS)
                flag for the TRB referenced by the TR Dequeue Pointer. Refer to section 4.9.2 for more
                information. This field shall be ‘0’ if MaxPStreams > ‘0’.
            */
            uint64_t dcs                      : 1;

            // Reserved
            uint64_t rsvd3                    : 3;

            /*
                TR Dequeue Pointer. As Input, this field represents the high order bits of the 64-bit base
                address of a Transfer Ring or a Stream Context Array associated with this endpoint. If
                MaxPStreams = '0' then this field shall point to a Transfer Ring. If MaxPStreams > '0' then this
                field shall point to a Stream Context Array.
                As Output, if MaxPStreams = ‘0’ this field shall be used by the xHC to store the value of the
                Dequeue Pointer when the endpoint enters the Halted or Stopped states, and the value of the
                this field shall be undefined when the endpoint is not in the Halted or Stopped states. if
                MaxPStreams > ‘0’ then this field shall point to a Stream Context Array.
                The memory structure referenced by this physical memory pointer shall be aligned to a 16-byte
                boundary.
            */
            uint64_t tr_dequeue_ptr_address_bits    : 60;
        };
        struct {
            uint32_t dword2;
            uint32_t dword3;
        };
        uint64_t transfer_ring_dequeue_ptr;        
    };

    union {
        struct {
            /*
                Average TRB Length. This field represents the average Length of the TRBs executed by this
                endpoint. The value of this field shall be greater than ‘0’. Refer to section 4.14.1.1 and the
                implementation note TRB Lengths and System Bus Bandwidth for more information.
                The xHC shall use this parameter to calculate system bus bandwidth requirements.
            */
            uint16_t average_trb_length;

            /*
                Max Endpoint Service Time Interval Payload Low (Max ESIT Payload Lo). This field indicates
                the low order 16 bits of the Max ESIT Payload. The Max ESIT Payload represents the total
                number of bytes this endpoint will transfer during an ESIT. This field is only valid for periodic
                endpoints. Refer to section 6.2.3.8 for more information.
            */
            uint16_t max_esit_payload_lo;
        };
        uint32_t dword4;
    };

    // Padding required for 64-byte context structs
    uint32_t padding[11];
} __attribute__((packed)) xhci_endpoint_context64_t;


typedef struct xhci_endpoint_context32 {
    union {
        struct {
            /*
                Endpoint State (EP State). The Endpoint State identifies the current operational state of the
                endpoint.
                
                Value Definition
                    0 Disabled The endpoint is not operational
                    1 Running The endpoint is operational, either waiting for a doorbell ring or processing
                    TDs.
                    2 HaltedThe endpoint is halted due to a Halt condition detected on the USB. SW shall issue
                    Reset Endpoint Command to recover from the Halt condition and transition to the Stopped
                    state. SW may manipulate the Transfer Ring while in this state.
                    3 Stopped The endpoint is not running due to a Stop Endpoint Command or recovering
                    from a Halt condition. SW may manipulate the Transfer Ring while in this state.
                    4 Error The endpoint is not running due to a TRB Error. SW may manipulate the Transfer
                    Ring while in this state.
                    5-7 Reserved

                As Output, a Running to Halted transition is forced by the xHC if a STALL condition is detected
                on the endpoint. A Running to Error transition is forced by the xHC if a TRB Error condition is
                detected.
                As Input, this field is initialized to ‘0’ by software.
                Refer to section 4.8.3 for more information on Endpoint State.
            */
            uint32_t endpoint_state        : 3;

            // Reserved
            uint32_t rsvd0                 : 5;

            /*
                Mult. If LEC = ‘0’, then this field indicates the maximum number of bursts within an Interval that
                this endpoint supports. Mult is a “zero-based” value, where 0 to 3 represents 1 to 4 bursts,
                respectively. The valid range of values is ‘0’ to ‘2’. This field shall be ‘0’ for all endpoint types
                except for SS Isochronous.
                If LEC = ‘1’, then this field shall be RsvdZ and Mult is calculated as:
                ROUNDUP(Max ESIT Payload / Max Packet Size / (Max Burst Size + 1)) - 1
            */
            uint32_t mult                  : 2;
            
            /*
                Max Primary Streams (MaxPStreams). This field identifies the maximum number of Primary
                Stream IDs this endpoint supports. Valid values are defined below. If the value of this field is ‘0’,
                then the TR Dequeue Pointer field shall point to a Transfer Ring. If this field is > '0' then the TR
                Dequeue Pointer field shall point to a Primary Stream Context Array. Refer to section 4.12 for
                more information.
                A value of ‘0’ indicates that Streams are not supported by this endpoint and the Endpoint
                Context TR Dequeue Pointer field references a Transfer Ring.
                A value of ‘1’ to ‘15’ indicates that the Primary Stream ID Width is MaxPstreams+1 and the
                Primary Stream Array contains 2MaxPStreams+1 entries.
                For SS Bulk endpoints, the range of valid values for this field is defined by the MaxPSASize field
                in the HCCPARAMS1 register (refer to Table 5-13).
                This field shall be '0' for all SS Control, Isoch, and Interrupt endpoints, and for all non-SS
                endpoints
            */
            uint32_t max_primary_streams   : 5;
            
            /*
                Linear Stream Array (LSA). This field identifies how a Stream ID shall be interpreted.
                Setting this bit to a value of ‘1’ shall disable Secondary Stream Arrays and a Stream ID shall be
                interpreted as a linear index into the Primary Stream Array, where valid values for MaxPStreams
                are ‘1’ to ‘15’.
                A value of ‘0’ shall enable Secondary Stream Arrays, where the low order (MaxPStreams+1) bits
                of a Stream ID shall be interpreted as a linear index into the Primary Stream Array, where valid
                values for MaxPStreams are ‘1’ to ‘7’. And the high order bits of a Stream ID shall be interpreted
                as a linear index into the Secondary Stream Array.
                If MaxPStreams = ‘0’, this field RsvdZ.
                Refer to section 4.12.2 for more information.
            */
            uint32_t linear_stream_array   : 1;
            
            /*
                Interval. The period between consecutive requests to a USB endpoint to send or receive data.
                Expressed in 125 μs. increments. The period is calculated as 125 μs. * 2^Interval; e.g., an Interval
                value of 0 means a period of 125 μs. (20 = 1 * 125 μs.), a value of 1 means a period of 250 μs. (21
                = 2 * 125 μs.), a value of 4 means a period of 2 ms. (24 = 16 * 125 μs.), etc. Refer to Table 6-12
                for legal Interval field values. See further discussion of this field below. Refer to section 6.2.3.6
                for more information.
            */
            uint32_t interval              : 8;
            
            /*
                Max Endpoint Service Time Interval Payload High (Max ESIT Payload Hi). If LEC = '1', then this
                field indicates the high order 8 bits of the Max ESIT Payload value. If LEC = '0', then this field
                shall be RsvdZ. Refer to section 6.2.3.8 for more information.
            */
            uint32_t max_esit_payload_hi   : 8;
        };
        uint32_t dword0;
    };

    union {
        struct {
            // Reserved
            uint32_t rsvd1                 : 1;

            /*
                Error Count (CErr)112. This field defines a 2-bit down count, which identifies the number of
                consecutive USB Bus Errors allowed while executing a TD. If this field is programmed with a
                non-zero value when the Endpoint Context is initialized, the xHC loads this value into an internal
                Bus Error Counter before executing a USB transaction and decrements it if the transaction fails.
                If the Bus Error Counter counts from ‘1’ to ‘0’, the xHC ceases execution of the TRB, sets the
                endpoint to the Halted state, and generates a USB Transaction Error Event for the TRB that
                caused the internal Bus Error Counter to decrement to ‘0’. If system software programs this field
                to ‘0’, the xHC shall not count errors for TRBs on the Endpoint’s Transfer Ring and there shall be
                no limit on the number of TRB retries. Refer to section 4.10.2.7 for more information on the
                operation of the Bus Error Counter.
                Note: CErr does not apply to Isoch endpoints and shall be set to ‘0’ if EP Type = Isoch Out ('1') or
                Isoch In ('5').
            */
            uint32_t error_count             : 2;

            /*
                Endpoint Type (EP Type). This field identifies whether an Endpoint Context is Valid, and if so,
                what type of endpoint the context defines.
                
                Value  Endpoint      Type Direction
                0      Not Valid     N/A
                1      Isoch         Out
                2      Bulk          Out
                3      Interrupt     Out
                4      Control       Bidirectional
                5      Isoch         In
                6      Bulk          In
                7      Interrupt     In
            */
            uint32_t endpoint_type           : 3;
            
            // Reserved
            uint32_t rsvd2                   : 1;
            
            /*
                Host Initiate Disable (HID). This field affects Stream enabled endpoints, allowing the Host
                Initiated Stream selection feature to be disabled for the endpoint. Setting this bit to a value of
                ‘1’ shall disable the Host Initiated Stream selection feature. A value of ‘0’ will enable normal
                Stream operation. Refer to section 4.12.1.1 for more information.
            */
            uint32_t host_initiate_disable   : 1;
            
            /*
                Max Burst Size. This field indicates to the xHC the maximum number of consecutive USB
                transactions that should be executed per scheduling opportunity. This is a “zero-based” value,
                where 0 to 15 represents burst sizes of 1 to 16, respectively. Refer to section 6.2.3.4 for more
                information.
            */
            uint32_t max_burst_size          : 8;
            
            /*
                Max Packet Size. This field indicates the maximum packet size in bytes that this endpoint is
                capable of sending or receiving when configured. Refer to section 6.2.3.5 for more information
            */
            uint32_t max_packet_size         : 16;
        };
        uint32_t dword1;
    };

    union {
        struct {
            /*
                Dequeue Cycle State (DCS). This bit identifies the value of the xHC Consumer Cycle State (CCS)
                flag for the TRB referenced by the TR Dequeue Pointer. Refer to section 4.9.2 for more
                information. This field shall be ‘0’ if MaxPStreams > ‘0’.
            */
            uint64_t dcs                      : 1;

            // Reserved
            uint64_t rsvd3                    : 3;

            /*
                TR Dequeue Pointer. As Input, this field represents the high order bits of the 64-bit base
                address of a Transfer Ring or a Stream Context Array associated with this endpoint. If
                MaxPStreams = '0' then this field shall point to a Transfer Ring. If MaxPStreams > '0' then this
                field shall point to a Stream Context Array.
                As Output, if MaxPStreams = ‘0’ this field shall be used by the xHC to store the value of the
                Dequeue Pointer when the endpoint enters the Halted or Stopped states, and the value of the
                this field shall be undefined when the endpoint is not in the Halted or Stopped states. if
                MaxPStreams > ‘0’ then this field shall point to a Stream Context Array.
                The memory structure referenced by this physical memory pointer shall be aligned to a 16-byte
                boundary.
            */
            uint64_t tr_dequeue_ptr_address_bits    : 60;
        };
        struct {
            uint32_t dword2;
            uint32_t dword3;
        };
        uint64_t transfer_ring_dequeue_ptr;        
    };

    union {
        struct {
            /*
                Average TRB Length. This field represents the average Length of the TRBs executed by this
                endpoint. The value of this field shall be greater than ‘0’. Refer to section 4.14.1.1 and the
                implementation note TRB Lengths and System Bus Bandwidth for more information.
                The xHC shall use this parameter to calculate system bus bandwidth requirements.
            */
            uint16_t average_trb_length;

            /*
                Max Endpoint Service Time Interval Payload Low (Max ESIT Payload Lo). This field indicates
                the low order 16 bits of the Max ESIT Payload. The Max ESIT Payload represents the total
                number of bytes this endpoint will transfer during an ESIT. This field is only valid for periodic
                endpoints. Refer to section 6.2.3.8 for more information.
            */
            uint16_t max_esit_payload_lo;
        };
        uint32_t dword4;
    };

    // Padding required for 64-byte context structs
    uint32_t padding[3];
} __attribute__((packed)) xhci_endpoint_context32_t;


typedef struct xhci_slot_context32 {
    union {
        struct {
            uint32_t route_string   : 20;
            uint32_t speed          : 4;
            uint32_t rz             : 1;
            uint32_t mtt            : 1;
            uint32_t hub            : 1;
            uint32_t context_entries : 5;
        };
        uint32_t dword0;
    };

    union {
        struct {
            uint16_t    max_exit_latency;
            uint8_t     root_hub_port_num;
            uint8_t     port_count;
        };
        uint32_t dword1;
    };

    union {
        struct {
            uint32_t parent_hub_slot_id  : 8;
            uint32_t parent_port_number  : 8;
            uint32_t tt_think_time       : 2;
            uint32_t rsvd0               : 4;
            uint32_t interrupter_target  : 10;
        };
        uint32_t dword2;
    };

    union {
        struct {
            uint32_t device_address  : 8;
            uint32_t rsvd1           : 19;
            uint32_t slot_state      : 5;
        };
        uint32_t dword3;
    };
    
    uint32_t rsvdz[4];
} __attribute__((packed)) xhci_slot_context32_t;

typedef struct xhci_slot_context64 {
    union {
        struct {
            uint32_t route_string   : 20;
            uint32_t speed          : 4;
            uint32_t rz             : 1;
            uint32_t mtt            : 1;
            uint32_t hub            : 1;
            uint32_t context_entries : 5;
        };
        uint32_t dword0;
    };

    union {
        struct {
            uint16_t    max_exit_latency;
            uint8_t     root_hub_port_num;
            uint8_t     port_count;
        };
        uint32_t dword1;
    };

    union {
        struct {
            uint32_t parent_hub_slot_id  : 8;
            uint32_t parent_port_number  : 8;
            uint32_t tt_think_time       : 2;
            uint32_t rsvd0               : 4;
            uint32_t interrupter_target  : 10;
        };
        uint32_t dword2;
    };

    union {
        struct {
            uint32_t device_address  : 8;
            uint32_t rsvd1           : 19;
            uint32_t slot_state      : 5;
        };
        uint32_t dword3;
    };
    
    uint32_t rsvdz[4];

    // Padding required for 64-byte context structs
    uint32_t padding[8];
} __attribute__((packed)) xhci_slot_context64_t;

typedef struct xhci_device_context32 {
    xhci_slot_context32_t       slot_context;
    xhci_endpoint_context32_t   control_endpoint_context;
    xhci_endpoint_context32_t   ep[30];
} __attribute__((packed)) xhci_device_context32_t;

typedef struct xhci_device_context64 {
    xhci_slot_context64_t       slot_context;
    xhci_endpoint_context64_t   control_endpoint_context;
    xhci_endpoint_context64_t   ep[30];
} __attribute__((packed)) xhci_device_context64_t;


typedef struct xhci_input_control_context32 {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t rsvd[5];
    uint8_t config_value;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t rsvdZ;
} __attribute__((packed)) xhci_input_control32_t;

typedef struct xhci_input_control_context64 {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t rsvd[5];
    uint8_t config_value;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t rsvdZ;
    uint32_t padding[8];
} __attribute__((packed)) xhci_input_control64_t;

// !!!: how is this even worse than what's above
typedef struct xhci_input_context32 {
    xhci_input_control32_t control;
    xhci_device_context32_t device;
} __attribute__((packed)) xhci_input_context32_t;

typedef struct xhci_input_context64 {
    xhci_input_control64_t control;
    xhci_device_context64_t device;
} __attribute__((packed)) xhci_input_context64_t;


// So, if you haven't realized xHCI uses multiple types depending on whether the 64-bit addressing is used
// This is a problem in C: padding isn't interpreted properly by these structures
// Therefore our getter macros instead cast to this type, which has pointers to the correct input and device contexts
// This is hacky, but so is this driver (and yet it still works, mostly)

typedef struct xhci_input_context {
    xhci_input_control32_t *control;
    xhci_device_context32_t *device;
} xhci_input_context_t;

/**** MACROS ****/

// IMPORTANT: Do not use the values of the XHCI_CREATE_ macros.
#define XHCI_CREATE_DEVICE_CONTEXT(xhci) ((xhci_device_context32_t*)pool_allocateChunk(xhci->ctx_pool))
#define XHCI_DEVICE_CONTEXT_SIZE(xhci) ((xhci->bit64 ? sizeof(xhci_device_context64_t) : sizeof(xhci_device_context32_t)))
#define XHCI_CREATE_INPUT_CONTEXT(xhci) ((xhci_input_context32_t*)pool_allocateChunk(xhci->input_ctx_pool))
#define XHCI_INPUT_CONTEXT_SIZE(xhci) ((xhci->bit64 ? sizeof(xhci_input_context64_t) : sizeof(xhci_input_context32_t)))

// Use this!
#define XHCI_INPUT_CONTEXT(dev) (dev->xhci->bit64 ? ((xhci_input_context_t*)&(xhci_input_context_t){ .control = (xhci_input_control32_t*)&(((xhci_input_context64_t*)dev->input_ctx)->control), .device = (xhci_device_context32_t*)&(((xhci_input_context64_t*)dev->input_ctx)->device)}) : (&(xhci_input_context_t){ .control = &(((xhci_input_context32_t*)dev->input_ctx)->control), .device = &(((xhci_input_context32_t*)dev->input_ctx)->device)}))

#endif