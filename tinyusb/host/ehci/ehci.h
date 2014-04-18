/**************************************************************************/
/*!
    @file     ehci.h
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, hathach (tinyusb.org)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    INCLUDING NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    This file is part of the tinyusb stack.
*/
/**************************************************************************/

/** \ingroup Group_HCD
 * @{
 *  \defgroup EHCI
 *  \brief EHCI driver. All documents sources mentioned here (eg section 3.5) is referring to EHCI Specs unless state otherwise
 *  @{ */

#ifndef _TUSB_EHCI_H_
#define _TUSB_EHCI_H_

#include "common/common.h"
#include "../hcd.h"

/* Abbreviation
 * HC: Host Controller
 * HCD: Host Controller Driver
 * QHD: Queue Head for non-ISO transfer
 * QTD: Queue Transfer Descriptor for non-ISO transfer
 * ITD: Iso Transfer Descriptor for highspeed
 * SITD: Split ISO Transfer Descriptor for full-speed
 * SMASK: Start Split mask for Slipt Transaction
 * CMASK: Complete Split mask for Slipt Transaction
 * RO: Read Only
 * R/WC: Read, Write to Clear
*/

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// EHCI CONFIGURATION & CONSTANTS
//--------------------------------------------------------------------+
#define HOST_HCD_XFER_INTERRUPT // TODO interrupt is used widely, should always be enalbed
#define EHCI_PERIODIC_LIST (defined HOST_HCD_XFER_INTERRUPT || defined HOST_HCD_XFER_ISOCHRONOUS)

#define	EHCI_CFG_FRAMELIST_SIZE_BITS			7			/// Framelist Size (NXP specific) (0:1024) - (1:512) - (2:256) - (3:128) - (4:64) - (5:32) - (6:16) - (7:8)
#define EHCI_FRAMELIST_SIZE  (1024 >> EHCI_CFG_FRAMELIST_SIZE_BITS)

// TODO merge OHCI with EHCI
enum {
  EHCI_MAX_ITD  = 4,
  EHCI_MAX_SITD = 16
};

//------------- Validation -------------//
STATIC_ASSERT(EHCI_CFG_FRAMELIST_SIZE_BITS <= 7, "incorrect value");

//--------------------------------------------------------------------+
// EHCI Data Structure
//--------------------------------------------------------------------+
enum ehci_queue_element_type_{
  EHCI_QUEUE_ELEMENT_ITD = 0 , ///< 0
  EHCI_QUEUE_ELEMENT_QHD     , ///< 1
  EHCI_QUEUE_ELEMENT_SITD    , ///< 2
  EHCI_QUEUE_ELEMENT_FSTN      ///< 3
};

/// EHCI PID
enum tusb_pid_{
  EHCI_PID_OUT = 0 ,
  EHCI_PID_IN      ,
  EHCI_PID_SETUP
};

/// Link pointer
typedef union {
	uint32_t address;
	struct  {
		uint32_t terminate : 1;
		uint32_t type      : 2;
	};
}ehci_link_t;

/// Queue Element Transfer Descriptor (section 3.5)
typedef struct {

	/// Word 0: Next QTD Pointer
	ehci_link_t next;

	/// Word 1: Alternate Next QTD Pointer (not used)
	union{
	  ehci_link_t alternate;
	  struct {
	    uint32_t                : 5;
	    uint32_t used           : 1;
	    uint32_t                : 10;
	    uint32_t expected_bytes : 16;
	  };
	};

	/// Word 2: qTQ Token
	volatile uint32_t pingstate_err               : 1  ; ///< If the QH.EPSfield indicates a High-speed device and the PID_Codeindicates an OUT endpoint, then this is the state bit for the Ping protocol. 0b=OUT 1b=PING
	volatile uint32_t non_hs_split_state          : 1  ; ///< This bit is ignored by the host controller unless the QH.EPSfield indicates a full- or low-speed endpoint. When a Full- or Low-speed device, the host controller uses this bit to track the state of the split-transaction. The functional requirements of the host controller for managing this state bit and the split transaction protocol depends on whether the endpoint is in the periodic or asynchronous schedule. 0b=Start Split 1b=Complete Split
	volatile uint32_t non_hs_period_missed_uframe : 1  ; ///< This bit is ignored unless the QH.EPSfield indicates a full- or low-speed endpoint and the queue head is in the periodic list. This bit is set when the host controller detected that a host-induced hold-off caused the host controller to miss a required complete-split transaction. If the host controller sets this bit to a one, then it remains a one for the duration of the transfer.
	volatile uint32_t xact_err                    : 1  ; ///< Set to a one by the Host Controller during status update in the case where the host did not receive a valid response from the device (Timeout, CRC, Bad PID, etc.)
	volatile uint32_t babble_err                  : 1  ; ///< Set to a 1 by the Host Controller during status update when a babble is detected during the transaction. In addition to setting this bit, the Host Controller also sets the Haltedbit to a 1
	volatile uint32_t buffer_err                  : 1  ; ///< Set to a 1 by the Host Controller during status update to indicate that the Host Controller is unable to keep up with the reception of incoming data (overrun) or is unable to supply data fast enough during transmission (underrun)
	volatile uint32_t halted                      : 1  ; ///< Set to a 1 by the Host Controller during status updates to indicate that a serious error has occurred at the device/endpoint addressed by this qTD. This can be caused by babble, the error counter counting down to zero, or reception of the STALL handshake from the device during a transaction. Any time that a transaction results in the Halted bit being set to a one, the Active bit is also set to 0
	volatile uint32_t active                      : 1  ; ///< Set to 1 by software to enable the execution of transactions by the Host Controller

	uint32_t pid                                  : 2  ; ///< This field is an encoding of the token which should be used for transactions associated with this transfer descriptor. 00=OUT 01=IN 10=SETUP
	volatile uint32_t cerr                        : 2  ; ///< Error Counter, This field is a 2-bit down counter that keeps track of the number of consecutive Errors detected while executing this qTD
	volatile uint32_t current_page                : 3  ; ///< This field is used as an index into the qTD buffer pointer list
	uint32_t int_on_complete                      : 1  ; ///<  If this bit is set to a one, it specifies that when this qTD is completed, the Host Controller should issue an interrupt at the next interrupt threshold

	volatile uint32_t total_bytes                 : 15 ; ///< This field specifies the total number of bytes to be moved with this transfer descriptor
	volatile uint32_t data_toggle                 : 1  ; ///< This is the data toggle sequence bit
	uint32_t                                      : 0  ; // padding to the end of current storage unit
	// End of Word 2

	/// Buffer Page Pointer List, Each element in the list is a 4K page aligned, physical memory address. The lower 12 bits in each pointer are reserved (except for the first one) as each memory pointer must reference the start of a 4K page
	uint32_t buffer[5];
} ehci_qtd_t; // XXX qtd is used to declare overlay in ehci_qhd_t -> cannot be declared with ATTR_ALIGNED(32)

STATIC_ASSERT( sizeof(ehci_qtd_t) == 32, "size is not correct" );

/// Queue Head (section 3.6)
typedef struct ATTR_ALIGNED(32) {
	/// Word 0: Queue Head Horizontal Link Pointer
	ehci_link_t next;

	/// Word 1										 : Endpoint Characteristics
	uint32_t device_address                   : 7  ; ///< This field selects the specific device serving as the data source or sink
	uint32_t non_hs_period_inactive_next_xact : 1  ; ///< This bit is used by system software to request that the host controller set the Active bit to zero. See Section 4.12.2.5 for full operational details
	uint32_t endpoint_number                  : 4  ; ///< This 4-bit field selects the particular endpoint number on the device serving as the data source or sink.
	uint32_t endpoint_speed                   : 2  ; ///< This is the speed of the associated endpoint 00b=Full 01b=Low 10b=High 11b=Reserved
	uint32_t data_toggle_control              : 1  ; ///< This bit specifies where the host controller should get the initial data toggle on an overlay transition. 0b=Ignore DT bit of qTD, 1b=Use DT bit of qTD
	uint32_t head_list_flag                   : 1  ; ///< This bit is set by System Software to mark a queue head as being the head of the reclamation list. See Section 4.8 for operational model
	uint32_t max_package_size                 : 11 ; ///< This directly corresponds to the maximum packet size of the associated endpoint (wMaxPacketSize)
	uint32_t non_hs_control_endpoint          : 1  ; ///< If the QH.EPSfield indicates the endpoint is not a high-speed device, and the endpoint is an control endpoint, then software must set this bit to a one. Otherwise it should always set this bit to a zero.
	uint32_t nak_count_reload                 : 4  ; ///< This field contains a value, which is used by the host controller to reload the Nak Counter field.
	uint32_t                                  : 0  ; // padding to the end of current storage unit
	// End of Word 1

	/// Word 2                                : Endpoint Capabilities
	uint32_t interrupt_smask                  : 8  ; ///< This field is used for all endpoint speeds. Software should set this field to a zero when the queue head is on the asynchronous schedule. A non-zero value in this field indicates an interrupt endpoint
	uint32_t non_hs_interrupt_cmask           : 8  ; ///<  This field is ignored by the host controller unless the EPSfield indicates this device is a low- or full-speed device and this queue head is in the periodic list. This field (along with the Activeand SplitX-statefields) is used to determine during which micro-frames the host controller should execute a complete-split transaction
	uint32_t hub_address                      : 7  ; ///< This field is ignored by the host controller unless the EPSfield indicates a full- or low-speed device. The value is the USB device address of the USB 2.0 Hub below which the full- or low-speed device associated with this endpoint is attached. This field is used in the split-transaction protocol. See Section 4.12.
	uint32_t hub_port                         : 7  ; ///<  This field is ignored by the host controller unless the EPSfield indicates a full- or low-speed device. The value is the port number identifier on the USB 2.0 Hub (for hub at device address Hub Addrbelow), below which the full- or low-speed device associated with this endpoint is attached. This information is used in the split-transaction protocol. See Section 4.12.
	uint32_t mult                             : 2  ; ///<  This field is a multiplier used to key the host controller as the number of successive packets the host controller may submit to the endpoint in the current execution. 00b=Reserved 01b,10b,11b= 1 (2, 3) Transaction for this endpoint/micro frame
	uint32_t                                  : 0  ; // padding to the end of current storage unit
	// End of Word 2

	/// Word 3: Current qTD Pointer
	volatile uint32_t qtd_addr;

	/// Word 4-11: Transfer Overlay
	volatile ehci_qtd_t qtd_overlay;

	//--------------------------------------------------------------------+
  /// Due to the fact QHD is 32 bytes aligned but occupies only 48 bytes
	/// thus there are 16 bytes padding free that we can make use of.
  //--------------------------------------------------------------------+
	uint8_t used;
	uint8_t is_removing;
	uint8_t pid_non_control;
	uint8_t class_code;

	uint16_t total_xferred_bytes; // number of bytes xferred until a qtd with ioc bit set
	uint8_t interval_ms; // polling interval in frames (or milisecond)
	uint8_t reserved;

	ehci_qtd_t * volatile p_qtd_list_head;	// head of the scheduled TD list
	ehci_qtd_t * volatile p_qtd_list_tail;	// tail of the scheduled TD list
} ehci_qhd_t;

STATIC_ASSERT( sizeof(ehci_qhd_t) == 64, "size is not correct" );

/// Highspeed Isochronous Transfer Descriptor (section 3.3)
typedef struct ATTR_ALIGNED(32) {
	/// Word 0: Next Link Pointer
	ehci_link_t next;

	/// Word 1-8: iTD Transaction Status and Control List
	struct  {
	  // iTD Control
		volatile uint32_t offset          : 12 ; ///< This field is a value that is an offset, expressed in bytes, from the beginning of a buffer.
		volatile uint32_t page_select     : 3  ; ///< These bits are set by software to indicate which of the buffer page pointers the offset field in this slot should be concatenated to produce the starting memory address for this transaction. The valid range of values for this field is 0 to 6
             uint32_t int_on_complete : 1  ; ///< If this bit is set to a one, it specifies that when this transaction completes, the Host Controller should issue an interrupt at the next interrupt threshold
		volatile uint32_t length          : 12 ; ///< For an OUT, this field is the number of data bytes the host controller will send during the transaction. The host controller is not required to update this field to reflect the actual number of bytes transferred during the transfer
																						 ///< For an IN, the initial value of the field is the number of bytes the host expects the endpoint to deliver. During the status update, the host controller writes back the number of bytes successfully received. The value in this register is the actual byte count
		// iTD Status
		volatile uint32_t error           : 1  ; ///< Set to a one by the Host Controller during status update in the case where the host did not receive a valid response from the device (Timeout, CRC, Bad PID, etc.). This bit may only be set for isochronous IN transactions.
		volatile uint32_t babble_err      : 1  ; ///< Set to a 1 by the Host Controller during status update when a babble is detected during the transaction
		volatile uint32_t buffer_err      : 1  ; ///< Set to a 1 by the Host Controller during status update to indicate that the Host Controller is unable to keep up with the reception of incoming data (overrun) or is unable to supply data fast enough during transmission (underrun).
		volatile uint32_t active          : 1  ; ///< Set to 1 by software to enable the execution of an isochronous transaction by the Host Controller
	} xact[8];

	///Word 9-15  Buffer Page Pointer List (Plus)
	uint32_t BufferPointer[7];

//	// FIXME: Store meta data into buffer pointer reserved for saving memory
//	/*---------- HCD Area ----------*/
//	uint32_t used;
//	uint32_t IhdIdx;
//	uint32_t reserved[6];
} ehci_itd_t;

STATIC_ASSERT( sizeof(ehci_itd_t) == 64, "size is not correct" );

/// Split (Full-Speed) Isochronous Transfer Descriptor
typedef struct ATTR_ALIGNED(32) {
  /// Word 0: Next Link Pointer
	ehci_link_t next;

	/// Word 1: siTD Endpoint Characteristics
	uint32_t device_address  : 7; ///< This field selects the specific device serving as the data source or sink.
	uint32_t                 : 1; ///< reserved
	uint32_t endpoint_number : 4; ///< This 4-bit field selects the particular endpoint number on the device serving as the data source or sink.
	uint32_t                 : 4; ///< This field is reserved and should be set to zero.
	uint32_t hub_address     : 7; ///< This field holds the device address of the transaction translators’ hub.
	uint32_t                 : 1; ///< reserved
	uint32_t port_number     : 7; ///< This field is the port number of the recipient transaction translator.
	uint32_t direction       : 1; ///<  0 = OUT; 1 = IN. This field encodes whether the full-speed transaction should be an IN or OUT.
	uint32_t                 : 0; // padding to the end of current storage unit
	// End of Word 1

	/// Word 2: Micro-frame Schedule Control
	uint8_t interrupt_smask            ; ///< This field (along with the Activeand SplitX-statefields in the Statusbyte) are used to determine during which micro-frames the host controller should execute complete-split transactions
	uint8_t non_hs_interrupt_cmask     ; ///< This field (along with the Activeand SplitX-statefields in the Statusbyte) are used to determine during which micro-frames the host controller should execute start-split transactions.
	uint16_t reserved ; ///< reserved
	// End of Word 2

	/// Word 3: siTD Transfer Status and Control
	// Status [7:0] TODO indentical to qTD Token'status --> refractor later
	volatile uint32_t                 : 1  ; // reserved
	volatile uint32_t split_state     : 1  ;
	volatile uint32_t missed_uframe   : 1  ;
	volatile uint32_t xact_err        : 1  ;
	volatile uint32_t babble_err      : 1  ;
	volatile uint32_t buffer_err      : 1  ;
	volatile uint32_t error           : 1  ;
	volatile uint32_t active          : 1  ;
	// Micro-frame Schedule Control
	volatile uint32_t cmask_progress  : 8  ; ///< This field is used by the host controller to record which split-completes have been executed. See Section 4.12.3.3.2 for behavioral requirements.
	volatile uint32_t total_bytes     : 10 ; ///< This field is initialized by software to the total number of bytes expected in this transfer. Maximum value is 1023
	volatile uint32_t                 : 4  ; ///< reserved
	volatile uint32_t page_select     : 1  ; ///< Used to indicate which data page pointer should be concatenated with the CurrentOffsetfield to construct a data buffer pointer
					 uint32_t int_on_complete : 1  ; ///< Do not interrupt when transaction is complete. 1 = Do interrupt when transaction is complete
					 uint32_t                 : 0  ; // padding to the end of current storage unit
	// End of Word 3

	/// Word 4-5: Buffer Pointer List
	uint32_t buffer[2];		// buffer[1] TP: Transaction Position - T-Count: Transaction Count

// 	union{
// 		uint32_t BufferPointer1;
// 		struct  {
// 			volatile uint32_t TCount : 3;
// 			volatile uint32_t TPosition : 2;
// 		};
// 	};

	/*---------- Word 6 ----------*/
	ehci_link_t back;

	/// SITD is 32-byte aligned but occupies only 28 --> 4 bytes for storing extra data
	uint8_t used;
	uint8_t ihd_idx;
	uint8_t reserved2[2];
} ehci_sitd_t;

STATIC_ASSERT( sizeof(ehci_sitd_t) == 32, "size is not correct" );

//--------------------------------------------------------------------+
// EHCI Operational Register
//--------------------------------------------------------------------+
enum ehci_interrupt_mask_{
  EHCI_INT_MASK_USB                   = BIT_(0),
  EHCI_INT_MASK_ERROR                 = BIT_(1),
  EHCI_INT_MASK_PORT_CHANGE           = BIT_(2),

  EHCI_INT_MASK_FRAMELIST_ROLLOVER    = BIT_(3),
  EHCI_INT_MASK_PCI_HOST_SYSTEM_ERROR = BIT_(4),
  EHCI_INT_MASK_ASYNC_ADVANCE         = BIT_(5),
  EHCI_INT_MASK_NXP_SOF               = BIT_(7),

  EHCI_INT_MASK_NXP_ASYNC             = BIT_(18),
  EHCI_INT_MASK_NXP_PERIODIC          = BIT_(19),

  EHCI_INT_MASK_ALL                   =
      EHCI_INT_MASK_USB | EHCI_INT_MASK_ERROR | EHCI_INT_MASK_PORT_CHANGE |
      EHCI_INT_MASK_FRAMELIST_ROLLOVER | EHCI_INT_MASK_PCI_HOST_SYSTEM_ERROR |
      EHCI_INT_MASK_ASYNC_ADVANCE | EHCI_INT_MASK_NXP_SOF |
      EHCI_INT_MASK_NXP_ASYNC | EHCI_INT_MASK_NXP_PERIODIC
};

enum ehci_usbcmd_pos_ {
  EHCI_USBCMD_POS_RUN_STOP               = 0,
  EHCI_USBCMD_POS_FRAMELIST_SZIE         = 2,
  EHCI_USBCMD_POS_PERIOD_ENABLE          = 4,
  EHCI_USBCMD_POS_ASYNC_ENABLE           = 5,
  EHCI_USBCMD_POS_NXP_FRAMELIST_SIZE_MSB = 15,
  EHCI_USBCMD_POS_INTERRUPT_THRESHOLD    = 16
};

enum ehci_portsc_change_mask_{
  EHCI_PORTSC_MASK_CONNECT_STATUS_CHANGE = BIT_(1),
  EHCI_PORTSC_MASK_PORT_ENABLE_CHAGNE = BIT_(3),
  EHCI_PORTSC_MASK_OVER_CURRENT_CHANGE = BIT_(5),

  EHCI_PORTSC_MASK_ALL =
      EHCI_PORTSC_MASK_CONNECT_STATUS_CHANGE |
      EHCI_PORTSC_MASK_PORT_ENABLE_CHAGNE |
      EHCI_PORTSC_MASK_OVER_CURRENT_CHANGE
};

typedef volatile struct {
  union {
    uint32_t usb_cmd                  ; ///< The Command Register indicates the command to be executed by the serial bus host controller. Writing to the register causes a command to be executed
    struct {
      uint32_t run_stop               : 1 ; ///< Default 0b. 1=Run. 0=Stop. When set to a 1, the Host Controller proceeds with execution of the schedule. The Host Controller continues execution as long as this bit is set to a 1. When this bit is set to 0, the Host Controller completes the current and any actively pipelined transactions on the USB and then halts. The Host Controller must halt within 16 micro-frames after software clears the Run bit. The HC Halted bit in the status register indicates when the Host Controller has finished its pending pipelined transactions and has entered the stopped state. Software must not write a one to this field unless the host controller is in the Halted state (i.e. HCHaltedin the USBSTS register is a one). Doing so will yield undefined results.
      uint32_t reset                  : 1 ; ///< his control bit is used by software to reset the host controller. The effects of this on Root Hub registers are similar to a Chip Hardware Reset. When software writes a one to this bit, the Host Controller resets its internal pipelines, timers, counters, state machines, etc. to their initial value. Any transaction currently in progress on USB is immediately terminated. A USB reset is not driven on downstream ports.This bit is set to zero by the Host Controller when the reset process is complete. Software cannot terminate the reset process early by writing a zero to this register. Software should not set this bit to a one when the HCHaltedbit in the USBSTS register is a zero. Attempting to reset an actively running host controller will result in undefined behavior.
      uint32_t framelist_size         : 2 ; ///< This field is R/W only if Programmable Frame List Flagin the HCCPARAMS registers is set to a one. This field specifies the size of the frame list.00b  1024 elements (4096 bytes) Default value 01b  512 elements (2048 bytes) 10b 256 elements (1024 bytes)
      uint32_t periodic_enable        : 1 ; ///< This bit controls whether the host controller skips processing the Periodic Schedule. Values mean: 0b Do not process the Periodic Schedule 1b Use the PERIODICLISTBASE register to access the Periodic Schedule.
      uint32_t async_enable           : 1 ; ///< This bit controls whether the host controller skips processing the Asynchronous Schedule. Values mean: 0b Do not process the Asynchronous Schedule 1b Use the ASYNCLISTADDR register to access the Asynchronous Schedule.
      uint32_t advacne_async          : 1 ; ///< This bit is used as a doorbell by software to tell the host controller to issue an interrupt the next time it advances asynchronous schedule. Software must write a 1 to this bit to ringthe doorbell. When the host controller has evicted all appropriate cached schedule state, it sets the Interrupt on Async Advancestatus bit in the USBSTS register. If the Interrupt on Async Advance Enablebit in the USBINTR register is a one then the host controller will assert an interrupt at the next interrupt threshold. See Section 4.8.2 for operational details. The host controller sets this bit to a zero after it has set the Interrupt on Async Advance status bit in the USBSTS register to a one. Software should not write a one to this bit when the asynchronous schedule is disabled. Doing so will yield undefined results.
      uint32_t light_reset            : 1 ; ///< This control bit is not required. If implemented, it allows the driver to reset the EHCI controller without affecting the state of the ports or the relationship to the companion host controllers. For example, the PORSTC registers should not be reset to their default values and the CF bit setting should not go to zero (retaining port ownership relationships). A host software read of this bit as zero indicates the Light Host Controller Reset has completed and it is safe for host software to re-initialize the host controller. A host software read of this bit as a one indicates the Light Host Controller Reset has not yet completed.
      uint32_t async_park             : 2 ; ///< It contains a count of the number of successive transactions the host controller is allowed to execute from a high-speed queue head on the Asynchronous schedule before continuing traversal of the Asynchronous schedule. See Section 4.10.3.2 for full operational details. Valid values are 1h to 3h. Software must not write a zero to this bit when Park Mode Enableis a one as this will result in undefined behavior.
      uint32_t                        : 1 ; ///< reserved
      uint32_t async_park_enable      : 1 ; ///< Software uses this bit to enable or disable Park mode. When this bit is one, Park mode is enabled. When this bit is a zero, Park mode is disabled.
      uint32_t                        : 3 ; ///< reserved
      uint32_t nxp_framelist_size_msb : 1 ; ///< NXP customized : Bit 2 of the Frame List Size bits \n 011b: 128 elements \n 100b: 64 elements \n 101b: 32 elements \n 110b: 16 elements \n 111b: 8 elements
      uint32_t int_threshold          : 8 ; ///< Default 08h. This field is used by system software to select the maximum rate at which the host controller will issue interrupts. The only valid values are defined below. If software writes an invalid value to this register, the results are undefined. Value Maximum Interrupt Interval 00h Reserved 01h 1 micro-frame 02h 2 micro-frames 04h 4 micro-frames 08h 8 micro-frames (default, equates to 1 ms) 10h 16 micro-frames (2 ms) 20h 32 micro-frames (4 ms) 40h 64 micro-frames (8 ms) Refer to Section 4.15 for interrupts affected by this register. Any other value in this register yields undefined results. Software modifications to this bit while HCHalted bit is equal to zero results in undefined behavior.
      uint32_t                        : 0 ; // padding to the boundary of storage unit
    }usb_cmd_bit;
  };

  union {
    uint32_t usb_sts            ; ///< This register indicates pending interrupts and various states of the Host Controller. The status resulting from a transaction on the serial bus is not indicated in this register. Software sets a bit to 0 in this register by writing a 1 to it. See Section 4.15 for additional information concerning USB interrupt conditions.
    struct {
      uint32_t usb                    : 1  ; ///< R/WC The Host Controller sets this bit to 1 on the completion of a USB transaction, which results in the retirement of a Transfer Descriptor that had its IOC bit set. \n The Host Controller also sets this bit to 1 when a short packet is detected (actual number of bytes received was less than the expected number of bytes).
      uint32_t usb_error              : 1  ; ///< R/WC The Host Controller sets this bit to 1 when completion of a USB transaction results in an error condition (e.g., error counter underflow). If the TD on which the error interrupt occurred also had its IOC bit set, both this bit and USBINT bit are set. See Section 4.15.1 for a list of the USB errors that will result in this bit being set to a one.
      uint32_t port_change_detect     : 1  ; ///< R/WC The Host Controller sets this bit to a one when any port for which the Port Ownerbit is set to zero (see Section 2.3.9) has a change bit transition from a zero to a one or a Force Port Resumebit transition from a zero to a one as a result of a J-K transition detected on a suspended port.
      uint32_t framelist_rollover     : 1  ; ///< R/WC The Host Controller sets this bit to a one when the Frame List Index(see Section 2.3.4) rolls over from its maximum value to zero. The exact value at which the rollover occurs depends on the frame list size. For example, if the frame list size (as programmed in the Frame List Sizefield of the USBCMD register) is 1024, the Frame Index Registerrolls over every time FRINDEX[13] toggles. Similarly, if the size is 512, the Host Controller sets this bit to a one every time FRINDEX[12] toggles.
      uint32_t pci_host_system_error  : 1  ; ///< R/WC (not used by NXP) The Host Controller sets this bit to 1 when a serious error occurs during a host system access involving the Host Controller module. In a PCI system, conditions that set this bit to 1 include PCI Parity error, PCI Master Abort, and PCI Target Abort. When this error occurs, the Host Controller clears the Run/Stop bit in the Command register to prevent further execution of the scheduled TDs.
      uint32_t async_advance          : 1  ; ///< R/WC 0=Default. System software can force the host controller to issue an interrupt the next time the host controller advances the asynchronous schedule by writing a one to the Interrupt on Async Advance Doorbell bit in the USBCMD register. This status bit indicates the assertion of that interrupt source.
      uint32_t                        : 1  ; ///< These bits are reserved and should be set to zero.
      uint32_t nxp_int_sof            : 1  ; ///< R/WC NXP customized:  this bit will be set every 125us and can be used by host controller driver as a time base.
      uint32_t                        : 4  ; ///< These bits are reserved and should be set to zero.
      uint32_t hc_halted              : 1  ; ///< Read-Only 1=Default. This bit is a zero whenever the Run/Stop bit is a one. The Host Controller sets this bit to one after it has stopped executing as a result of the Run/Stop bit being set to 0, either by software or by the Host Controller hardware (e.g. internal error).
      uint32_t reclamation            : 1  ; ///< Read-Only 0=Default. This is a read-only status bit, which is used to detect an empty asynchronous schedule. The operational model of empty schedule detection is described in Section 4.8.3. The valid transitions for this bit are described in Section 4.8.6.
      uint32_t period_schedule_status : 1  ; ///< Read-Only The bit reports the current real status of the Periodic Schedule. If this bit is a zero then the status of the Periodic Schedule is disabled. If this bit is a one then the status of the Periodic Schedule is enabled
      uint32_t async_schedule_status  : 1  ; ///< Read-Only 0=Default. The bit reports the current real status of the Asynchronous Schedule. If this bit is a zero then the status of the Asynchronous Schedule is disabled. If this bit is a one then the status of the Asynchronous Schedule is enabled.
      uint32_t                        : 2  ; ///< reseved
      uint32_t nxp_int_async          : 1  ; ///< R/WC NXP customized: This bit is set by the Host Controller when the cause of an interrupt is a completion of a USB transaction where the Transfer Descriptor (TD) has an interrupt on complete (IOC) bit set andthe TD was from the asynchronous schedule. This bit is also set by the Host when a short packet is detected andthe packet is on the asynchronous schedule.
      uint32_t nxp_int_period         : 1  ; ///< R/WC NXP customized: This bit is set by the Host Controller when the cause of an interrupt is a completion of a USB transaction where the Transfer Descriptor (TD) has an interrupt on complete (IOC) bit set andthe TD was from the periodic schedule.
      uint32_t                        : 12 ; ///< reserved
      uint32_t                        : 0  ; // padding to the boundary of storage unit
    }usb_sts_bit;
  };

  union{
    uint32_t usb_int_enable     ; ///< This register enables and disables reporting of the corresponding interrupt to the software. When a bit is set and the corresponding interrupt is active, an interrupt is generated to the host. Interrupt sources that are disabled in this register still appear in the USBSTS to allow the software to poll for events.
    struct {
      uint32_t usb                   : 1  ; ///< When this bit is a one, and the USBINT bit in the USBSTS register is a one, the host controller will issue an interrupt at the next interrupt threshold. The interrupt is acknowledged by software clearing the USBINTbit.
      uint32_t usb_error             : 1  ; ///< When this bit is a one, and the USBERRINT bit in the USBSTS register is a one, the host controller will issue an interrupt at the next interrupt threshold. The interrupt is acknowledged by software clearing the USBERRINTbit.
      uint32_t port_change_detect    : 1  ; ///< When this bit is a one, and the Port Change Detect bit in the USBSTS register is a one, the host controller will issue an interrupt. The interrupt is acknowledged by software clearing the Port Change Detectbit.
      uint32_t framelist_rollover    : 1  ; ///< When this bit is a one, and the Frame List Rolloverbit in the USBSTS register is a one, the host controller will issue an interrupt. The interrupt is acknowledged by software clearing the Frame List Rollover bit.
      uint32_t pci_host_system_error : 1  ; ///< (not used by NXP) When this bit is a one, and the Host System Error Statusbit in the USBSTS register is a one, the host controller will issue an interrupt. The interrupt is acknowledged by software clearing the Host System Error bit.
      uint32_t async_advance         : 1  ; ///< When this bit is a one, and the Interrupt on Async Advancebit in the USBSTS register is a one, the host controller will issue an interrupt at the next interrupt threshold. The interrupt is acknowledged by software clearing the Interrupt on Async Advancebit.
      uint32_t                       : 1  ; ///< reserved
      uint32_t nxp_int_sof           : 1  ; ///< NXP customized: if this bit is one and the SRI bit in the USBSTS register is one, the host controller will issue an interrupt. In host mode, the SRI bit will be set every 125 micro sec and can be used by the host controller as a time base. The interrupt is acknowledged by software clearing the SRI bit in the USBSTS register.
      uint32_t                       : 10 ; ///< reserved
      uint32_t nxp_int_async         : 1  ; ///< NXP customized: When this bit is a one, and the USBHSTASYNCINT bit in the USBSTS register is a one, the host controller will issue an interrupt at the next interrupt threshold. The interrupt is acknowledged by software clearing the USBHSTASYNCINT bit.
      uint32_t nxp_int_period        : 1  ; ///< NXP customized: When this bit is a one, and the USBHSTPERINT bit in the USBSTS register is a one, the host controller will issue an interrupt at the next interrupt threshold. The interrupt is acknowledged by software clearing the USBHSTPERINT bit.
      uint32_t                       : 12 ; ///< reserved
      uint32_t                       : 0  ; // padding to the boundary of storage unit
    }usb_int_enable_bit;
  };

  uint32_t frame_index        ; ///< This register is used by the host controller to index into the periodic frame list. The register updates every 125 microseconds (once each micro-frame). Bits [N:3] are used to select a particular entry in the Periodic Frame List during periodic schedule execution. The number of bits used for the index depends on the size of the frame list as set by system software in the Frame List Sizefield in the USBCMD register
  uint32_t ctrl_ds_seg        ; ///< (not used by NXP) This 32-bit register corresponds to the most significant address bits [63:32] for all EHCI data structures. If the 64-bit Addressing Capabilityfield in HCCPARAMS is a zero, then this register is not used
  uint32_t periodic_list_base ; ///< This 32-bit register contains the beginning address of the Periodic Frame List in the system memory. System software loads this register prior to starting the schedule execution by the Host Controller (see 4.1). The memory structure referenced by this physical memory pointer is assumed to be 4-Kbyte aligned. The contents of this register are combined with the Frame Index Register (FRINDEX) to enable the Host Controller to step through the Periodic Frame List in sequence.
  uint32_t async_list_base    ; ///< This 32-bit register contains the address of the next asynchronous queue head to be executed. Bits [4:0] of this register cannot be modified by system software and will always return a zero when read. The memory structure referenced by this physical memory pointer is assumed to be 32-byte (cache line) aligned
  uint32_t tt_control         ; ///< nxp embedded transaction translator (reserved by EHCI specs)
  uint32_t reserved[8]        ; ///< reserved by EHCI specs
  uint32_t config_flag        ; ///< (not used by NXP) configured flag register

  union {
    uint32_t portsc             ; ///< port status and control
    struct {
      uint32_t current_connect_status      : 1; ///< RO 1=Device is present on port. 0=No device is present. Default = 0. This value reflects the current state of the port, and may not correspond directly to the event that caused the Connect Status Change bit (Bit 1) to be set. This field is zero if Port Power is zero.
      uint32_t connect_status_change       : 1; ///< R/WC 1=Change in Current Connect Status. 0=No change. Default = 0. Indicates a change has occurred in the port's Current Connect Status. The host controller sets this bit for all changes to the port device connect status, even if system software has not cleared an existing connect status change. For example, the insertion status changes twice before system software has cleared the changed condition, hub hardware will be "setting" an already-set bit (i.e., the bit will remain set). Software sets this bit to 0 by writing a 1 to it. This field is zero if Port Power is zero.
      uint32_t port_enable                 : 1; ///< 1=Enable. 0=Disable. Default = 0. Ports can only be enabled by the host controller as a part of the reset and enable. Software cannot enable a port by writing a one to this field. The host controller will only set this bit to a one when the reset sequence determines that the attached device is a high-speed device. Ports can be disabled by either a fault condition (disconnect event or other fault condition) or by host software. Note that the bit status does not change until the port state actually changes.
      uint32_t port_enable_change          : 1; ///< R/WC 1=Port enabled/disabled status has changed. 0=No change. Default = 0. For the root hub, this bit gets set to a one only when a port is disabled due to the appropriate conditions existing at the EOF2 point (See Chapter 11 of the USB Specification for the definition of a Port Error). Software clears this bit by writing a 1 to it. This field is zero if Port Power is zero.
      uint32_t over_current_active         : 1; ///< RO Default = 0. 1=This port currently has an over-current condition. 0=This port does not have an over-current condition. This bit will automatically transition from a one to a zero when the over current condition is removed.
      uint32_t over_current_change         : 1; ///< R/WC Default = 0. 1=This bit gets set to a one when there is a change to Over-current Active. Software clears this bit by writing a one to this bit position.
      uint32_t force_port_resume           : 1; ///< 1= Resume detected/driven on port. 0=No resume (K-state) detected/driven on port. Default = 0. This functionality defined for manipulating this bit depends on the value of the Suspendbit. For example, if the port is not suspended (Suspendand Enabledbits are a one) and software transitions this bit to a one, then the effects on the bus are undefined. Software sets this bit to a 1 to drive resume signaling. The Host Controller sets this bit to a 1 if a J-to-K transition is detected while the port is in the Suspend state. When this bit transitions to a one because a J-to-K transition is detected, the Port Change Detectbit in the USBSTS register is also set to a one. If software sets this bit to a one, the host controller must not set the Port Change Detectbit.
      uint32_t suspend                     : 1; ///< 1=Port in suspend state. 0=Port not in suspend state. Default = 0. Port Enabled Bit and Suspend bit of this register define the port states as follows: Bits [Port Enabled, Suspend] Port State 0X Disable 10 Enable 11 Suspend When in suspend state, downstream propagation of data is blocked on this port, except for port reset. The blocking occurs at the end of the current transaction, if a transaction was in progress when this bit was written to 1. In the suspend state, the port is sensitive to resume detection. Note that the bit status does not change until the port is suspended and that there may be a delay in suspending a port if there is a transaction currently in progress on the USB. A write of zero to this bit is ignored by the host controller. The host controller will unconditionally set this bit to a zero when: •Software sets the Force Port Resumebit to a zero (from a one). •Software sets the Port Resetbit to a one (from a zero).
      uint32_t port_reset                  : 1; ///< 1=Port is in Reset. 0=Port is not in Reset. Default = 0. When software writes a one to this bit (from a zero)
      uint32_t nxp_highspeed_status        : 1; ///< NXP customized: 0=connected to the port is not in High-speed mode, 1=connected to the port is in High-speed mode
      uint32_t line_status                 : 2; ///< hese bits reflect the current logical levels of the D+ (bit 11) and D- (bit 10) signal lines. These bits are used for detection of low-speed USB devices prior to the port reset and enable sequence. This field is valid only when the port enable bit is zero and the current connect status bit is set to a one. The encoding of the bits are: 00b SE0, 10b J-state, 01b K-state, 11b undefined
      uint32_t port_power                  : 1; ///< 0= power off, 1= power on, Host/OTG controller requires port power control switches. This bit represents the current setting of the switch (0=off, 1=on). When power is not available on a port (i.e. PP equals a 0), the port is non-functional and will not report attaches, detaches, etc. When an over-current condition is detected on a powered port and PPC is a one, the PP bit in each affected port may be transitioned by the host controller driver from a one to a zero (removing power from the port).
      uint32_t port_owner                  : 1; ///< (not used by NXP)
      uint32_t port_indicator_control      : 2; ///< Writing to this field effects the value of the pins USB0_IND1 and USB0_IND0. 00b: Port indication is off, 01b: Amber, 10b: green, 11b: undefined
      uint32_t port_test_control           : 4; ///< When this field is zero, the port is NOT operating in a test mode. A non-zero value indicates that it is operating in test mode
      uint32_t wake_on_connect_enable      : 1; ///< Default = 0b. Writing this bit to a one enables the port to be sensitive to device connects as wake-up events. See Section 4.3 for effects of this bit on resume event behavior. Refer to Section 4.3.1 for operational model.
      uint32_t wake_on_disconnect_enable   : 1; ///<  Default = 0b. Writing this bit to a one enables the port to be sensitive to device disconnects as wake-up events. See Section 4.3 for effects of this bit on resume event behavior. Refer to Section 4.3.1 for operational model.
      uint32_t wake_on_over_current_enable : 1; ///< Default = 0b. Writing this bit to a one enables the port to be sensitive to over-current conditions as wake-up events. See Section 4.3 for effects of this bit on resume event behavior. Refer to Section 4.3.1 for operational model.
      uint32_t nxp_phy_clock_disable       : 1; ///< NXP customized: the PHY can be put into Low Power Suspend – Clock Disable when the downstream device has been put into suspend mode or when no downstream device is connected. Low power suspend is completely under the control of software. 0: enable PHY clock, 1: disable PHY clock
      uint32_t nxp_port_force_fullspeed    : 1; ///< NXP customized: Writing this bit to a 1 will force the port to only connect at Full Speed. It disables the chirp sequence that allowsthe port to identify itself as High Speed. This is useful for testing FS configurations with a HS host, hub or device.
      uint32_t                             : 1;
      uint32_t nxp_port_speed              : 2; ///< NXP customized: This register field indicates the speed atwhich the port is operating. For HS mode operation in the host controllerand HS/FS operation in the device controller the port routing steers data to the Protocol engine. For FS and LS mode operation in the host controller, the port routing steers data to the Protocol Engine w/ Embedded Transaction Translator. 0x0: Fullspeed, 0x1: Lowspeed, 0x2: Highspeed
      uint32_t                             : 0; // padding to the boundary of storage unit
    }portsc_bit;
  };
}ehci_registers_t;

//--------------------------------------------------------------------+
// EHCI Data Organization
//--------------------------------------------------------------------+
typedef struct {
  //------------- Static Async/Period List Head, Each for one controller -------------//
  ehci_qhd_t async_head[CONTROLLER_HOST_NUMBER]; /// head qhd of async list, also is used as control endpoint for address 0

#if EHCI_PERIODIC_LIST
  // for NXP ECHI, only implement 1 ms & 2 ms & 4 ms, 8 ms (framelist)
  // [0] : 1ms, [1] : 2ms, [2] : 4ms, [3] : 8 ms
  ehci_qhd_t period_head_arr[CONTROLLER_HOST_NUMBER][4];
#endif

  //------------- Data for Address 0 (use async head as its queue head) -------------//
  ehci_qtd_t addr0_qtd[3];

  struct {
    struct {
      ehci_qhd_t qhd;
      ehci_qtd_t qtd[3];
    }control;

    ehci_qhd_t  qhd[HCD_MAX_ENDPOINT]                  ; ///< Queue Head Pool
    ehci_qtd_t  qtd[HCD_MAX_XFER] ATTR_ALIGNED(32) ; ///< Queue Element Transfer Pool
//  ehci_itd_t  itd[EHCI_MAX_ITD]                  ; ///< Iso Transfer Pool
//  ehci_sitd_t sitd[EHCI_MAX_SITD]                ; ///< Split (FS) Isochronous Transfer Pool
  }device[TUSB_CFG_HOST_DEVICE_MAX];
}ehci_data_t;

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_EHCI_H_ */

/** @} */
/** @} */

