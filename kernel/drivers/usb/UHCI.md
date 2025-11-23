/* SPDX-License-Identifier: GPL-2.0 */

/*
 * xHCI host controller driver
 *
 * Copyright (C) 2008 Intel Corp.
 *
 * Author: Sarah Sharp
 * Some code borrowed from the Linux EHCI driver.
 */

#ifndef __LINUX_XHCI_HCD_H
#define __LINUX_XHCI_HCD_H

#include <linux/usb.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/usb/hcd.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include <linux/io-64-nonatomic-hi-lo.h>

/* Code sharing between pci-quirks and xhci hcd */
#include	"xhci-ext-caps.h"
#include "pci-quirks.h"

#include "xhci-port.h"
#include "xhci-caps.h"

/* max buffer size for trace and debug messages */
#define XHCI_MSG_MAX		500

/* xHCI PCI Configuration Registers */
#define XHCI_SBRN_OFFSET	(0x60)

/* Max number of USB devices for any host controller - limit in section 6.1 */
#define MAX_HC_SLOTS		256
/* Section 5.3.3 - MaxPorts */
#define MAX_HC_PORTS		127

/*
 * xHCI register interface.
 * This corresponds to the eXtensible Host Controller Interface (xHCI)
 * Revision 0.95 specification
 */

/**
 * struct xhci_cap_regs - xHCI Host Controller Capability Registers.
 * @hc_capbase:		length of the capabilities register and HC version number
 * @hcs_params1:	HCSPARAMS1 - Structural Parameters 1
 * @hcs_params2:	HCSPARAMS2 - Structural Parameters 2
 * @hcs_params3:	HCSPARAMS3 - Structural Parameters 3
 * @hcc_params:		HCCPARAMS - Capability Parameters
 * @db_off:		DBOFF - Doorbell array offset
 * @run_regs_off:	RTSOFF - Runtime register space offset
 * @hcc_params2:	HCCPARAMS2 Capability Parameters 2, xhci 1.1 only
 */
struct xhci_cap_regs {
	__le32	hc_capbase;
	__le32	hcs_params1;
	__le32	hcs_params2;
	__le32	hcs_params3;
	__le32	hcc_params;
	__le32	db_off;
	__le32	run_regs_off;
	__le32	hcc_params2; /* xhci 1.1 */
	/* Reserved up to (CAPLENGTH - 0x1C) */
};

/* Number of registers per port */
#define	NUM_PORT_REGS	4

#define PORTSC		0
#define PORTPMSC	1
#define PORTLI		2
#define PORTHLPMC	3

/**
 * struct xhci_op_regs - xHCI Host Controller Operational Registers.
 * @command:		USBCMD - xHC command register
 * @status:		USBSTS - xHC status register
 * @page_size:		This indicates the page size that the host controller
 * 			supports.  If bit n is set, the HC supports a page size
 * 			of 2^(n+12), up to a 128MB page size.
 * 			4K is the minimum page size.
 * @cmd_ring:		CRP - 64-bit Command Ring Pointer
 * @dcbaa_ptr:		DCBAAP - 64-bit Device Context Base Address Array Pointer
 * @config_reg:		CONFIG - Configure Register
 * @port_status_base:	PORTSCn - base address for Port Status and Control
 * 			Each port has a Port Status and Control register,
 * 			followed by a Port Power Management Status and Control
 * 			register, a Port Link Info register, and a reserved
 * 			register.
 * @port_power_base:	PORTPMSCn - base address for
 * 			Port Power Management Status and Control
 * @port_link_base:	PORTLIn - base address for Port Link Info (current
 * 			Link PM state and control) for USB 2.1 and USB 3.0
 * 			devices.
 */
struct xhci_op_regs {
	__le32	command;
	__le32	status;
	__le32	page_size;
	__le32	reserved1;
	__le32	reserved2;
	__le32	dev_notification;
	__le64	cmd_ring;
	/* rsvd: offset 0x20-2F */
	__le32	reserved3[4];
	__le64	dcbaa_ptr;
	__le32	config_reg;
	/* rsvd: offset 0x3C-3FF */
	__le32	reserved4[241];
	/* port 1 registers, which serve as a base address for other ports */
	__le32	port_status_base;
	__le32	port_power_base;
	__le32	port_link_base;
	__le32	reserved5;
	/* registers for ports 2-255 */
	__le32	reserved6[NUM_PORT_REGS*254];
};

/* USBCMD - USB command - command bitmasks */
/* start/stop HC execution - do not write unless HC is halted*/
#define CMD_RUN		XHCI_CMD_RUN
/* Reset HC - resets internal HC state machine and all registers (except
 * PCI config regs).  HC does NOT drive a USB reset on the downstream ports.
 * The xHCI driver must reinitialize the xHC after setting this bit.
 */
#define CMD_RESET	(1 << 1)
/* Event Interrupt Enable - a '1' allows interrupts from the host controller */
#define CMD_EIE		XHCI_CMD_EIE
/* Host System Error Interrupt Enable - get out-of-band signal for HC errors */
#define CMD_HSEIE	XHCI_CMD_HSEIE
/* bits 4:6 are reserved (and should be preserved on writes). */
/* light reset (port status stays unchanged) - reset completed when this is 0 */
#define CMD_LRESET	(1 << 7)
/* host controller save/restore state. */
#define CMD_CSS		(1 << 8)
#define CMD_CRS		(1 << 9)
/* Enable Wrap Event - '1' means xHC generates an event when MFINDEX wraps. */
#define CMD_EWE		XHCI_CMD_EWE
/* MFINDEX power management - '1' means xHC can stop MFINDEX counter if all root
 * hubs are in U3 (selective suspend), disconnect, disabled, or powered-off.
 * '0' means the xHC can power it off if all ports are in the disconnect,
 * disabled, or powered-off state.
 */
#define CMD_PM_INDEX	(1 << 11)
/* bit 14 Extended TBC Enable, changes Isoc TRB fields to support larger TBC */
#define CMD_ETE		(1 << 14)
/* bits 15:31 are reserved (and should be preserved on writes). */

#define XHCI_RESET_LONG_USEC		(10 * 1000 * 1000)
#define XHCI_RESET_SHORT_USEC		(250 * 1000)

/* USBSTS - USB status - status bitmasks */
/* HC not running - set to 1 when run/stop bit is cleared. */
#define STS_HALT	XHCI_STS_HALT
/* serious error, e.g. PCI parity error.  The HC will clear the run/stop bit. */
#define STS_FATAL	(1 << 2)
/* event interrupt - clear this prior to clearing any IP flags in IR set*/
#define STS_EINT	(1 << 3)
/* port change detect */
#define STS_PORT	(1 << 4)
/* bits 5:7 reserved and zeroed */
/* save state status - '1' means xHC is saving state */
#define STS_SAVE	(1 << 8)
/* restore state status - '1' means xHC is restoring state */
#define STS_RESTORE	(1 << 9)
/* true: save or restore error */
#define STS_SRE		(1 << 10)
/* true: Controller Not Ready to accept doorbell or op reg writes after reset */
#define STS_CNR		XHCI_STS_CNR
/* true: internal Host Controller Error - SW needs to reset and reinitialize */
#define STS_HCE		(1 << 12)
/* bits 13:31 reserved and should be preserved */

/*
 * DNCTRL - Device Notification Control Register - dev_notification bitmasks
 * Generate a device notification event when the HC sees a transaction with a
 * notification type that matches a bit set in this bit field.
 */
#define	DEV_NOTE_MASK		(0xffff)
/* Most of the device notification types should only be used for debug.
 * SW does need to pay attention to function wake notifications.
 */
#define	DEV_NOTE_FWAKE		(1 << 1)

/* CRCR - Command Ring Control Register - cmd_ring bitmasks */
/* bit 0 - Cycle bit indicates the ownership of the command ring */
#define CMD_RING_CYCLE		(1 << 0)
/* stop ring operation after completion of the currently executing command */
#define CMD_RING_PAUSE		(1 << 1)
/* stop ring immediately - abort the currently executing command */
#define CMD_RING_ABORT		(1 << 2)
/* true: command ring is running */
#define CMD_RING_RUNNING	(1 << 3)
/* bits 63:6 - Command Ring pointer */
#define CMD_RING_PTR_MASK	GENMASK_ULL(63, 6)

/* CONFIG - Configure Register - config_reg bitmasks */
/* bits 0:7 - maximum number of device slots enabled (NumSlotsEn) */
#define MAX_DEVS(p)	((p) & 0xff)
/* bit 8: U3 Entry Enabled, assert PLC when root port enters U3, xhci 1.1 */
#define CONFIG_U3E		(1 << 8)
/* bit 9: Configuration Information Enable, xhci 1.1 */
#define CONFIG_CIE		(1 << 9)
/* bits 10:31 - reserved and should be preserved */

/* bits 15:0 - HCD page shift bit */
#define XHCI_PAGE_SIZE_MASK     0xffff

/**
 * struct xhci_intr_reg - Interrupt Register Set, v1.2 section 5.5.2.
 * @iman:		IMAN - Interrupt Management Register. Used to enable
 *			interrupts and check for pending interrupts.
 * @imod:		IMOD - Interrupt Moderation Register. Used to throttle interrupts.
 * @erst_size:		ERSTSZ - Number of segments in the Event Ring Segment Table (ERST).
 * @erst_base:		ERSTBA - Event ring segment table base address.
 * @erst_dequeue:	ERDP - Event ring dequeue pointer.
 *
 * Each interrupter (defined by a MSI-X vector) has an event ring and an Event
 * Ring Segment Table (ERST) associated with it.  The event ring is comprised of
 * multiple segments of the same size.  The HC places events on the ring and
 * "updates the Cycle bit in the TRBs to indicate to software the current
 * position of the Enqueue Pointer." The HCD (Linux) processes those events and
 * updates the dequeue pointer.
 */
struct xhci_intr_reg {
	__le32	iman;
	__le32	imod;
	__le32	erst_size;
	__le32	rsvd;
	__le64	erst_base;
	__le64	erst_dequeue;
};

/* iman bitmasks */
/* bit 0 - Interrupt Pending (IP), whether there is an interrupt pending. Write-1-to-clear. */
#define	IMAN_IP			(1 << 0)
/* bit 1 - Interrupt Enable (IE), whether the interrupter is capable of generating an interrupt */
#define	IMAN_IE			(1 << 1)

/* imod bitmasks */
/*
 * bits 15:0 - Interrupt Moderation Interval, the minimum interval between interrupts
 * (in 250ns intervals). The interval between interrupts will be longer if there are no
 * events on the event ring. Default is 4000 (1 ms).
 */
#define IMODI_MASK		(0xffff)
/* bits 31:16 - Interrupt Moderation Counter, used to count down the time to the next interrupt */
#define IMODC_MASK		(0xffff << 16)

/* erst_size bitmasks */
/* bits 15:0 - Event Ring Segment Table Size, number of ERST entries */
#define	ERST_SIZE_MASK		(0xffff)

/* erst_base bitmasks */
/* bits 63:6 - Event Ring Segment Table Base Address Register */
#define ERST_BASE_ADDRESS_MASK	GENMASK_ULL(63, 6)

/* erst_dequeue bitmasks */
/*
 * bits 2:0 - Dequeue ERST Segment Index (DESI), is the segment number (or alias) where the
 * current dequeue pointer lies. This is an optional HW hint.
 */
#define ERST_DESI_MASK		(0x7)
/*
 * bit 3 - Event Handler Busy (EHB), whether the event ring is scheduled to be serviced by
 * a work queue (or delayed service routine)?
 */
#define ERST_EHB		(1 << 3)
/* bits 63:4 - Event Ring Dequeue Pointer */
#define ERST_PTR_MASK		GENMASK_ULL(63, 4)

/**
 * struct xhci_run_regs
 * @microframe_index:
 * 		MFINDEX - current microframe number
 *
 * Section 5.5 Host Controller Runtime Registers:
 * "Software should read and write these registers using only Dword (32 bit)
 * or larger accesses"
 */
struct xhci_run_regs {
	__le32			microframe_index;
	__le32			rsvd[7];
	struct xhci_intr_reg	ir_set[128];
};

/**
 * struct doorbell_array
 *
 * Bits  0 -  7: Endpoint target
 * Bits  8 - 15: RsvdZ
 * Bits 16 - 31: Stream ID
 *
 * Section 5.6
 */
struct xhci_doorbell_array {
	__le32	doorbell[256];
};

#define DB_VALUE(ep, stream)	((((ep) + 1) & 0xff) | ((stream) << 16))
#define DB_VALUE_HOST		0x00000000

#define PLT_MASK        (0x03 << 6)
#define PLT_SYM         (0x00 << 6)
#define PLT_ASYM_RX     (0x02 << 6)
#define PLT_ASYM_TX     (0x03 << 6)

/**
 * struct xhci_container_ctx
 * @type: Type of context.  Used to calculated offsets to contained contexts.
 * @size: Size of the context data
 * @bytes: The raw context data given to HW
 * @dma: dma address of the bytes
 *
 * Represents either a Device or Input context.  Holds a pointer to the raw
 * memory used for the context (bytes) and dma address of it (dma).
 */
struct xhci_container_ctx {
	unsigned type;
#define XHCI_CTX_TYPE_DEVICE  0x1
#define XHCI_CTX_TYPE_INPUT   0x2

	int size;

	u8 *bytes;
	dma_addr_t dma;
};

/**
 * struct xhci_slot_ctx
 * @dev_info:	Route string, device speed, hub info, and last valid endpoint
 * @dev_info2:	Max exit latency for device number, root hub port number
 * @tt_info:	tt_info is used to construct split transaction tokens
 * @dev_state:	slot state and device address
 *
 * Slot Context - section 6.2.1.1.  This assumes the HC uses 32-byte context
 * structures.  If the HC uses 64-byte contexts, there is an additional 32 bytes
 * reserved at the end of the slot context for HC internal use.
 */
struct xhci_slot_ctx {
	__le32	dev_info;
	__le32	dev_info2;
	__le32	tt_info;
	__le32	dev_state;
	/* offset 0x10 to 0x1f reserved for HC internal use */
	__le32	reserved[4];
};

/* dev_info bitmasks */
/* Route String - 0:19 */
#define ROUTE_STRING_MASK	(0xfffff)
/* Device speed - values defined by PORTSC Device Speed field - 20:23 */
#define DEV_SPEED	(0xf << 20)
#define GET_DEV_SPEED(n) (((n) & DEV_SPEED) >> 20)
/* bit 24 reserved */
/* Is this LS/FS device connected through a HS hub? - bit 25 */
#define DEV_MTT		(0x1 << 25)
/* Set if the device is a hub - bit 26 */
#define DEV_HUB		(0x1 << 26)
/* Index of the last valid endpoint context in this device context - 27:31 */
#define LAST_CTX_MASK	(0x1f << 27)
#define LAST_CTX(p)	((p) << 27)
#define LAST_CTX_TO_EP_NUM(p)	(((p) >> 27) - 1)
#define SLOT_FLAG	(1 << 0)
#define EP0_FLAG	(1 << 1)

/* dev_info2 bitmasks */
/* Max Exit Latency (ms) - worst case time to wake up all links in dev path */
#define MAX_EXIT	(0xffff)
/* Root hub port number that is needed to access the USB device */
#define ROOT_HUB_PORT(p)	(((p) & 0xff) << 16)
#define DEVINFO_TO_ROOT_HUB_PORT(p)	(((p) >> 16) & 0xff)
/* Maximum number of ports under a hub device */
#define XHCI_MAX_PORTS(p)	(((p) & 0xff) << 24)
#define DEVINFO_TO_MAX_PORTS(p)	(((p) & (0xff << 24)) >> 24)

/* tt_info bitmasks */
/*
 * TT Hub Slot ID - for low or full speed devices attached to a high-speed hub
 * The Slot ID of the hub that isolates the high speed signaling from
 * this low or full-speed device.  '0' if attached to root hub port.
 */
#define TT_SLOT		(0xff)
/*
 * The number of the downstream facing port of the high-speed hub
 * '0' if the device is not low or full speed.
 */
#define TT_PORT		(0xff << 8)
#define TT_THINK_TIME(p)	(((p) & 0x3) << 16)
#define GET_TT_THINK_TIME(p)	(((p) & (0x3 << 16)) >> 16)

/* dev_state bitmasks */
/* USB device address - assigned by the HC */
#define DEV_ADDR_MASK	(0xff)
/* bits 8:26 reserved */
/* Slot state */
#define SLOT_STATE	(0x1f << 27)
#define GET_SLOT_STATE(p)	(((p) & (0x1f << 27)) >> 27)

#define SLOT_STATE_DISABLED	0
#define SLOT_STATE_ENABLED	SLOT_STATE_DISABLED
#define SLOT_STATE_DEFAULT	1
#define SLOT_STATE_ADDRESSED	2
#define SLOT_STATE_CONFIGURED	3

/**
 * struct xhci_ep_ctx
 * @ep_info:	endpoint state, streams, mult, and interval information.
 * @ep_info2:	information on endpoint type, max packet size, max burst size,
 * 		error count, and whether the HC will force an event for all
 * 		transactions.
 * @deq:	64-bit ring dequeue pointer address.  If the endpoint only
 * 		defines one stream, this points to the endpoint transfer ring.
 * 		Otherwise, it points to a stream context array, which has a
 * 		ring pointer for each flow.
 * @tx_info:
 * 		Average TRB lengths for the endpoint ring and
 * 		max payload within an Endpoint Service Interval Time (ESIT).
 *
 * Endpoint Context - section 6.2.1.2.  This assumes the HC uses 32-byte context
 * structures.  If the HC uses 64-byte contexts, there is an additional 32 bytes
 * reserved at the end of the endpoint context for HC internal use.
 */
struct xhci_ep_ctx {
	__le32	ep_info;
	__le32	ep_info2;
	__le64	deq;
	__le32	tx_info;
	/* offset 0x14 - 0x1f reserved for HC internal use */
	__le32	reserved[3];
};

/* ep_info bitmasks */
/*
 * Endpoint State - bits 0:2
 * 0 - disabled
 * 1 - running
 * 2 - halted due to halt condition - ok to manipulate endpoint ring
 * 3 - stopped
 * 4 - TRB error
 * 5-7 - reserved
 */
#define EP_STATE_MASK		(0x7)
#define EP_STATE_DISABLED	0
#define EP_STATE_RUNNING	1
#define EP_STATE_HALTED		2
#define EP_STATE_STOPPED	3
#define EP_STATE_ERROR		4
#define GET_EP_CTX_STATE(ctx)	(le32_to_cpu((ctx)->ep_info) & EP_STATE_MASK)

/* Mult - Max number of burtst within an interval, in EP companion desc. */
#define EP_MULT(p)		(((p) & 0x3) << 8)
#define CTX_TO_EP_MULT(p)	(((p) >> 8) & 0x3)
/* bits 10:14 are Max Primary Streams */
/* bit 15 is Linear Stream Array */
/* Interval - period between requests to an endpoint - 125u increments. */
#define EP_INTERVAL(p)			(((p) & 0xff) << 16)
#define EP_INTERVAL_TO_UFRAMES(p)	(1 << (((p) >> 16) & 0xff))
#define CTX_TO_EP_INTERVAL(p)		(((p) >> 16) & 0xff)
#define EP_MAXPSTREAMS_MASK		(0x1f << 10)
#define EP_MAXPSTREAMS(p)		(((p) << 10) & EP_MAXPSTREAMS_MASK)
#define CTX_TO_EP_MAXPSTREAMS(p)	(((p) & EP_MAXPSTREAMS_MASK) >> 10)
/* Endpoint is set up with a Linear Stream Array (vs. Secondary Stream Array) */
#define	EP_HAS_LSA		(1 << 15)
/* hosts with LEC=1 use bits 31:24 as ESIT high bits. */
#define CTX_TO_MAX_ESIT_PAYLOAD_HI(p)	(((p) >> 24) & 0xff)

/* ep_info2 bitmasks */
/*
 * Force Event - generate transfer events for all TRBs for this endpoint
 * This will tell the HC to ignore the IOC and ISP flags (for debugging only).
 */
#define	FORCE_EVENT	(0x1)
#define ERROR_COUNT(p)	(((p) & 0x3) << 1)
#define CTX_TO_EP_TYPE(p)	(((p) >> 3) & 0x7)
#define EP_TYPE(p)	((p) << 3)
#define ISOC_OUT_EP	1
#define BULK_OUT_EP	2
#define INT_OUT_EP	3
#define CTRL_EP		4
#define ISOC_IN_EP	5
#define BULK_IN_EP	6
#define INT_IN_EP	7
/* bit 6 reserved */
/* bit 7 is Host Initiate Disable - for disabling stream selection */
#define MAX_BURST(p)	(((p)&0xff) << 8)
#define CTX_TO_MAX_BURST(p)	(((p) >> 8) & 0xff)
#define MAX_PACKET(p)	(((p)&0xffff) << 16)
#define MAX_PACKET_MASK		(0xffff << 16)
#define MAX_PACKET_DECODED(p)	(((p) >> 16) & 0xffff)

/* tx_info bitmasks */
#define EP_AVG_TRB_LENGTH(p)		((p) & 0xffff)
#define EP_MAX_ESIT_PAYLOAD_LO(p)	(((p) & 0xffff) << 16)
#define EP_MAX_ESIT_PAYLOAD_HI(p)	((((p) >> 16) & 0xff) << 24)
#define CTX_TO_MAX_ESIT_PAYLOAD(p)	(((p) >> 16) & 0xffff)

/* deq bitmasks */
#define EP_CTX_CYCLE_MASK		(1 << 0)
/* bits 63:4 - TR Dequeue Pointer */
#define TR_DEQ_PTR_MASK			GENMASK_ULL(63, 4)


/**
 * struct xhci_input_control_context
 * Input control context; see section 6.2.5.
 *
 * @drop_context:	set the bit of the endpoint context you want to disable
 * @add_context:	set the bit of the endpoint context you want to enable
 */
struct xhci_input_control_ctx {
	__le32	drop_flags;
	__le32	add_flags;
	__le32	rsvd2[6];
};

#define	EP_IS_ADDED(ctrl_ctx, i) \
	(le32_to_cpu(ctrl_ctx->add_flags) & (1 << (i + 1)))
#define	EP_IS_DROPPED(ctrl_ctx, i)       \
	(le32_to_cpu(ctrl_ctx->drop_flags) & (1 << (i + 1)))

/* Represents everything that is needed to issue a command on the command ring.
 * It's useful to pre-allocate these for commands that cannot fail due to
 * out-of-memory errors, like freeing streams.
 */
struct xhci_command {
	/* Input context for changing device state */
	struct xhci_container_ctx	*in_ctx;
	u32				status;
	u32				comp_param;
	int				slot_id;
	/* If completion is null, no one is waiting on this command
	 * and the structure can be freed after the command completes.
	 */
	struct completion		*completion;
	union xhci_trb			*command_trb;
	struct list_head		cmd_list;
	/* xHCI command response timeout in milliseconds */
	unsigned int			timeout_ms;
};

/* drop context bitmasks */
#define	DROP_EP(x)	(0x1 << x)
/* add context bitmasks */
#define	ADD_EP(x)	(0x1 << x)

struct xhci_stream_ctx {
	/* 64-bit stream ring address, cycle state, and stream type */
	__le64	stream_ring;
	/* offset 0x14 - 0x1f reserved for HC internal use */
	__le32	reserved[2];
};

/* Stream Context Types (section 6.4.1) - bits 3:1 of stream ctx deq ptr */
#define	SCT_FOR_CTX(p)		(((p) & 0x7) << 1)
#define	CTX_TO_SCT(p)		(((p) >> 1) & 0x7)
/* Secondary stream array type, dequeue pointer is to a transfer ring */
#define	SCT_SEC_TR		0
/* Primary stream array type, dequeue pointer is to a transfer ring */
#define	SCT_PRI_TR		1
/* Dequeue pointer is for a secondary stream array (SSA) with 8 entries */
#define SCT_SSA_8		2
#define SCT_SSA_16		3
#define SCT_SSA_32		4
#define SCT_SSA_64		5
#define SCT_SSA_128		6
#define SCT_SSA_256		7

/* Assume no secondary streams for now */
struct xhci_stream_info {
	struct xhci_ring		**stream_rings;
	/* Number of streams, including stream 0 (which drivers can't use) */
	unsigned int			num_streams;
	/* The stream context array may be bigger than
	 * the number of streams the driver asked for
	 */
	struct xhci_stream_ctx		*stream_ctx_array;
	unsigned int			num_stream_ctxs;
	dma_addr_t			ctx_array_dma;
	/* For mapping physical TRB addresses to segments in stream rings */
	struct radix_tree_root		trb_address_map;
	struct xhci_command		*free_streams_command;
};

#define	SMALL_STREAM_ARRAY_SIZE		256
#define	MEDIUM_STREAM_ARRAY_SIZE	1024
#define	GET_PORT_BW_ARRAY_SIZE		256

/* Some Intel xHCI host controllers need software to keep track of the bus
 * bandwidth.  Keep track of endpoint info here.  Each root port is allocated
 * the full bus bandwidth.  We must also treat TTs (including each port under a
 * multi-TT hub) as a separate bandwidth domain.  The direct memory interface
 * (DMI) also limits the total bandwidth (across all domains) that can be used.
 */
struct xhci_bw_info {
	/* ep_interval is zero-based */
	unsigned int		ep_interval;
	/* mult and num_packets are one-based */
	unsigned int		mult;
	unsigned int		num_packets;
	unsigned int		max_packet_size;
	unsigned int		max_esit_payload;
	unsigned int		type;
};

/* "Block" sizes in bytes the hardware uses for different device speeds.
 * The logic in this part of the hardware limits the number of bits the hardware
 * can use, so must represent bandwidth in a less precise manner to mimic what
 * the scheduler hardware computes.
 */
#define	FS_BLOCK	1
#define	HS_BLOCK	4
#define	SS_BLOCK	16
#define	DMI_BLOCK	32

/* Each device speed has a protocol overhead (CRC, bit stuffing, etc) associated
 * with each byte transferred.  SuperSpeed devices have an initial overhead to
 * set up bursts.  These are in blocks, see above.  LS overhead has already been
 * translated into FS blocks.
 */
#define DMI_OVERHEAD 8
#define DMI_OVERHEAD_BURST 4
#define SS_OVERHEAD 8
#define SS_OVERHEAD_BURST 32
#define HS_OVERHEAD 26
#define FS_OVERHEAD 20
#define LS_OVERHEAD 128
/* The TTs need to claim roughly twice as much bandwidth (94 bytes per
 * microframe ~= 24Mbps) of the HS bus as the devices can actually use because
 * of overhead associated with split transfers crossing microframe boundaries.
 * 31 blocks is pure protocol overhead.
 */
#define TT_HS_OVERHEAD (31 + 94)
#define TT_DMI_OVERHEAD (25 + 12)

/* Bandwidth limits in blocks */
#define FS_BW_LIMIT		1285
#define TT_BW_LIMIT		1320
#define HS_BW_LIMIT		1607
#define SS_BW_LIMIT_IN		3906
#define DMI_BW_LIMIT_IN		3906
#define SS_BW_LIMIT_OUT		3906
#define DMI_BW_LIMIT_OUT	3906

/* Percentage of bus bandwidth reserved for non-periodic transfers */
#define FS_BW_RESERVED		10
#define HS_BW_RESERVED		20
#define SS_BW_RESERVED		10

struct xhci_virt_ep {
	struct xhci_virt_device		*vdev;	/* parent */
	unsigned int			ep_index;
	struct xhci_ring		*ring;
	/* Related to endpoints that are configured to use stream IDs only */
	struct xhci_stream_info		*stream_info;
	/* Temporary storage in case the configure endpoint command fails and we
	 * have to restore the device state to the previous state
	 */
	struct xhci_ring		*new_ring;
	unsigned int			err_count;
	unsigned int			ep_state;
#define SET_DEQ_PENDING		(1 << 0)
#define EP_HALTED		(1 << 1)	/* For stall handling */
#define EP_STOP_CMD_PENDING	(1 << 2)	/* For URB cancellation */
/* Transitioning the endpoint to using streams, don't enqueue URBs */
#define EP_GETTING_STREAMS	(1 << 3)
#define EP_HAS_STREAMS		(1 << 4)
/* Transitioning the endpoint to not using streams, don't enqueue URBs */
#define EP_GETTING_NO_STREAMS	(1 << 5)
#define EP_HARD_CLEAR_TOGGLE	(1 << 6)
#define EP_SOFT_CLEAR_TOGGLE	(1 << 7)
/* usb_hub_clear_tt_buffer is in progress */
#define EP_CLEARING_TT		(1 << 8)
	/* ----  Related to URB cancellation ---- */
	struct list_head	cancelled_td_list;
	struct xhci_hcd		*xhci;
	/* Dequeue pointer and dequeue segment for a submitted Set TR Dequeue
	 * command.  We'll need to update the ring's dequeue segment and dequeue
	 * pointer after the command completes.
	 */
	struct xhci_segment	*queued_deq_seg;
	union xhci_trb		*queued_deq_ptr;
	/*
	 * Sometimes the xHC can not process isochronous endpoint ring quickly
	 * enough, and it will miss some isoc tds on the ring and generate
	 * a Missed Service Error Event.
	 * Set skip flag when receive a Missed Service Error Event and
	 * process the missed tds on the endpoint ring.
	 */
	bool			skip;
	/* Bandwidth checking storage */
	struct xhci_bw_info	bw_info;
	struct list_head	bw_endpoint_list;
	unsigned long		stop_time;
	/* Isoch Frame ID checking storage */
	int			next_frame_id;
	/* Use new Isoch TRB layout needed for extended TBC support */
	bool			use_extended_tbc;
	/* set if this endpoint is controlled via sideband access*/
	struct xhci_sideband	*sideband;
};

enum xhci_overhead_type {
	LS_OVERHEAD_TYPE = 0,
	FS_OVERHEAD_TYPE,
	HS_OVERHEAD_TYPE,
};

struct xhci_interval_bw {
	unsigned int		num_packets;
	/* Sorted by max packet size.
	 * Head of the list is the greatest max packet size.
	 */
	struct list_head	endpoints;
	/* How many endpoints of each speed are present. */
	unsigned int		overhead[3];
};

#define	XHCI_MAX_INTERVAL	16

struct xhci_interval_bw_table {
	unsigned int		interval0_esit_payload;
	struct xhci_interval_bw	interval_bw[XHCI_MAX_INTERVAL];
	/* Includes reserved bandwidth for async endpoints */
	unsigned int		bw_used;
	unsigned int		ss_bw_in;
	unsigned int		ss_bw_out;
};

#define EP_CTX_PER_DEV		31

struct xhci_virt_device {
	int				slot_id;
	struct usb_device		*udev;
	/*
	 * Commands to the hardware are passed an "input context" that
	 * tells the hardware what to change in its data structures.
	 * The hardware will return changes in an "output context" that
	 * software must allocate for the hardware.  We need to keep
	 * track of input and output contexts separately because
	 * these commands might fail and we don't trust the hardware.
	 */
	struct xhci_container_ctx       *out_ctx;
	/* Used for addressing devices and configuration changes */
	struct xhci_container_ctx       *in_ctx;
	struct xhci_virt_ep		eps[EP_CTX_PER_DEV];
	struct xhci_port		*rhub_port;
	struct xhci_interval_bw_table	*bw_table;
	struct xhci_tt_bw_info		*tt_info;
	/*
	 * flags for state tracking based on events and issued commands.
	 * Software can not rely on states from output contexts because of
	 * latency between events and xHC updating output context values.
	 * See xhci 1.1 section 4.8.3 for more details
	 */
	unsigned long			flags;
#define VDEV_PORT_ERROR			BIT(0) /* Port error, link inactive */

	/* The current max exit latency for the enabled USB3 link states. */
	u16				current_mel;
	/* Used for the debugfs interfaces. */
	void				*debugfs_private;
	/* set if this endpoint is controlled via sideband access*/
	struct xhci_sideband	*sideband;
};

/*
 * For each roothub, keep track of the bandwidth information for each periodic
 * interval.
 *
 * If a high speed hub is attached to the roothub, each TT associated with that
 * hub is a separate bandwidth domain.  The interval information for the
 * endpoints on the devices under that TT will appear in the TT structure.
 */
struct xhci_root_port_bw_info {
	struct list_head		tts;
	unsigned int			num_active_tts;
	struct xhci_interval_bw_table	bw_table;
};

struct xhci_tt_bw_info {
	struct list_head		tt_list;
	int				slot_id;
	int				ttport;
	struct xhci_interval_bw_table	bw_table;
	int				active_eps;
};


/**
 * struct xhci_device_context_array
 * @dev_context_ptr	array of 64-bit DMA addresses for device contexts
 */
struct xhci_device_context_array {
	/* 64-bit device addresses; we only write 32-bit addresses */
	__le64			dev_context_ptrs[MAX_HC_SLOTS];
	/* private xHCD pointers */
	dma_addr_t	dma;
};
/* TODO: write function to set the 64-bit device DMA address */
/*
 * TODO: change this to be dynamically sized at HC mem init time since the HC
 * might not be able to handle the maximum number of devices possible.
 */


struct xhci_transfer_event {
	/* 64-bit buffer address, or immediate data */
	__le64	buffer;
	__le32	transfer_len;
	/* This field is interpreted differently based on the type of TRB */
	__le32	flags;
};

/* Transfer event flags bitfield, also for select command completion events */
#define TRB_TO_SLOT_ID(p)	(((p) >> 24) & 0xff)
#define SLOT_ID_FOR_TRB(p)	(((p) & 0xff) << 24)

#define TRB_TO_EP_ID(p)		(((p) >> 16) & 0x1f) /* Endpoint ID 1 - 31 */
#define EP_ID_FOR_TRB(p)	(((p) & 0x1f) << 16)

#define TRB_TO_EP_INDEX(p)	(TRB_TO_EP_ID(p) - 1) /* Endpoint index 0 - 30 */
#define EP_INDEX_FOR_TRB(p)	((((p) + 1) & 0x1f) << 16)

/* Transfer event TRB length bit mask */
#define	EVENT_TRB_LEN(p)		((p) & 0xffffff)

/* Completion Code - only applicable for some types of TRBs */
#define	COMP_CODE_MASK		(0xff << 24)
#define GET_COMP_CODE(p)	(((p) & COMP_CODE_MASK) >> 24)
#define COMP_INVALID				0
#define COMP_SUCCESS				1
#define COMP_DATA_BUFFER_ERROR			2
#define COMP_BABBLE_DETECTED_ERROR		3
#define COMP_USB_TRANSACTION_ERROR		4
#define COMP_TRB_ERROR				5
#define COMP_STALL_ERROR			6
#define COMP_RESOURCE_ERROR			7
#define COMP_BANDWIDTH_ERROR			8
#define COMP_NO_SLOTS_AVAILABLE_ERROR		9
#define COMP_INVALID_STREAM_TYPE_ERROR		10
#define COMP_SLOT_NOT_ENABLED_ERROR		11
#define COMP_ENDPOINT_NOT_ENABLED_ERROR		12
#define COMP_SHORT_PACKET			13
#define COMP_RING_UNDERRUN			14
#define COMP_RING_OVERRUN			15
#define COMP_VF_EVENT_RING_FULL_ERROR		16
#define COMP_PARAMETER_ERROR			17
#define COMP_BANDWIDTH_OVERRUN_ERROR		18
#define COMP_CONTEXT_STATE_ERROR		19
#define COMP_NO_PING_RESPONSE_ERROR		20
#define COMP_EVENT_RING_FULL_ERROR		21
#define COMP_INCOMPATIBLE_DEVICE_ERROR		22
#define COMP_MISSED_SERVICE_ERROR		23
#define COMP_COMMAND_RING_STOPPED		24
#define COMP_COMMAND_ABORTED			25
#define COMP_STOPPED				26
#define COMP_STOPPED_LENGTH_INVALID		27
#define COMP_STOPPED_SHORT_PACKET		28
#define COMP_MAX_EXIT_LATENCY_TOO_LARGE_ERROR	29
#define COMP_ISOCH_BUFFER_OVERRUN		31
#define COMP_EVENT_LOST_ERROR			32
#define COMP_UNDEFINED_ERROR			33
#define COMP_INVALID_STREAM_ID_ERROR		34
#define COMP_SECONDARY_BANDWIDTH_ERROR		35
#define COMP_SPLIT_TRANSACTION_ERROR		36

static inline const char *xhci_trb_comp_code_string(u8 status)
{
	switch (status) {
	case COMP_INVALID:
		return "Invalid";
	case COMP_SUCCESS:
		return "Success";
	case COMP_DATA_BUFFER_ERROR:
		return "Data Buffer Error";
	case COMP_BABBLE_DETECTED_ERROR:
		return "Babble Detected";
	case COMP_USB_TRANSACTION_ERROR:
		return "USB Transaction Error";
	case COMP_TRB_ERROR:
		return "TRB Error";
	case COMP_STALL_ERROR:
		return "Stall Error";
	case COMP_RESOURCE_ERROR:
		return "Resource Error";
	case COMP_BANDWIDTH_ERROR:
		return "Bandwidth Error";
	case COMP_NO_SLOTS_AVAILABLE_ERROR:
		return "No Slots Available Error";
	case COMP_INVALID_STREAM_TYPE_ERROR:
		return "Invalid Stream Type Error";
	case COMP_SLOT_NOT_ENABLED_ERROR:
		return "Slot Not Enabled Error";
	case COMP_ENDPOINT_NOT_ENABLED_ERROR:
		return "Endpoint Not Enabled Error";
	case COMP_SHORT_PACKET:
		return "Short Packet";
	case COMP_RING_UNDERRUN:
		return "Ring Underrun";
	case COMP_RING_OVERRUN:
		return "Ring Overrun";
	case COMP_VF_EVENT_RING_FULL_ERROR:
		return "VF Event Ring Full Error";
	case COMP_PARAMETER_ERROR:
		return "Parameter Error";
	case COMP_BANDWIDTH_OVERRUN_ERROR:
		return "Bandwidth Overrun Error";
	case COMP_CONTEXT_STATE_ERROR:
		return "Context State Error";
	case COMP_NO_PING_RESPONSE_ERROR:
		return "No Ping Response Error";
	case COMP_EVENT_RING_FULL_ERROR:
		return "Event Ring Full Error";
	case COMP_INCOMPATIBLE_DEVICE_ERROR:
		return "Incompatible Device Error";
	case COMP_MISSED_SERVICE_ERROR:
		return "Missed Service Error";
	case COMP_COMMAND_RING_STOPPED:
		return "Command Ring Stopped";
	case COMP_COMMAND_ABORTED:
		return "Command Aborted";
	case COMP_STOPPED:
		return "Stopped";
	case COMP_STOPPED_LENGTH_INVALID:
		return "Stopped - Length Invalid";
	case COMP_STOPPED_SHORT_PACKET:
		return "Stopped - Short Packet";
	case COMP_MAX_EXIT_LATENCY_TOO_LARGE_ERROR:
		return "Max Exit Latency Too Large Error";
	case COMP_ISOCH_BUFFER_OVERRUN:
		return "Isoch Buffer Overrun";
	case COMP_EVENT_LOST_ERROR:
		return "Event Lost Error";
	case COMP_UNDEFINED_ERROR:
		return "Undefined Error";
	case COMP_INVALID_STREAM_ID_ERROR:
		return "Invalid Stream ID Error";
	case COMP_SECONDARY_BANDWIDTH_ERROR:
		return "Secondary Bandwidth Error";
	case COMP_SPLIT_TRANSACTION_ERROR:
		return "Split Transaction Error";
	default:
		return "Unknown!!";
	}
}

struct xhci_link_trb {
	/* 64-bit segment pointer*/
	__le64 segment_ptr;
	__le32 intr_target;
	__le32 control;
};

/* control bitfields */
#define LINK_TOGGLE	(0x1<<1)

/* Command completion event TRB */
struct xhci_event_cmd {
	/* Pointer to command TRB, or the value passed by the event data trb */
	__le64 cmd_trb;
	__le32 status;
	__le32 flags;
};

/* status bitmasks */
#define COMP_PARAM(p)	((p) & 0xffffff) /* Command Completion Parameter */

/* Address device - disable SetAddress */
#define TRB_BSR		(1<<9)

/* Configure Endpoint - Deconfigure */
#define TRB_DC		(1<<9)

/* Stop Ring - Transfer State Preserve */
#define TRB_TSP		(1<<9)

enum xhci_ep_reset_type {
	EP_HARD_RESET,
	EP_SOFT_RESET,
};

/* Force Event */
#define TRB_TO_VF_INTR_TARGET(p)	(((p) & (0x3ff << 22)) >> 22)
#define TRB_TO_VF_ID(p)			(((p) & (0xff << 16)) >> 16)

/* Set Latency Tolerance Value */
#define TRB_TO_BELT(p)			(((p) & (0xfff << 16)) >> 16)

/* Get Port Bandwidth */
#define TRB_TO_DEV_SPEED(p)		(((p) & (0xf << 16)) >> 16)

/* Force Header */
#define TRB_TO_PACKET_TYPE(p)		((p) & 0x1f)
#define TRB_TO_ROOTHUB_PORT(p)		(((p) & (0xff << 24)) >> 24)

enum xhci_setup_dev {
	SETUP_CONTEXT_ONLY,
	SETUP_CONTEXT_ADDRESS,
};

/* bits 16:23 are the virtual function ID */
/* bits 24:31 are the slot ID */

/* bits 19:16 are the dev speed */
#define DEV_SPEED_FOR_TRB(p)    ((p) << 16)

/* Stop Endpoint TRB - ep_index to endpoint ID for this TRB */
#define SUSPEND_PORT_FOR_TRB(p)		(((p) & 1) << 23)
#define TRB_TO_SUSPEND_PORT(p)		(((p) & (1 << 23)) >> 23)
#define LAST_EP_INDEX			30

/* Set TR Dequeue Pointer command TRB fields, 6.4.3.9 */
#define TRB_TO_STREAM_ID(p)		((((p) & (0xffff << 16)) >> 16))
#define STREAM_ID_FOR_TRB(p)		((((p)) & 0xffff) << 16)
#define SCT_FOR_TRB(p)			(((p) & 0x7) << 1)

/* Link TRB specific fields */
#define TRB_TC			(1<<1)

/* Port Status Change Event TRB fields */
/* Port ID - bits 31:24 */
#define GET_PORT_ID(p)		(((p) & (0xff << 24)) >> 24)

#define EVENT_DATA		(1 << 2)

/* Normal TRB fields */
/* transfer_len bitmasks - bits 0:16 */
#define	TRB_LEN(p)		((p) & 0x1ffff)
/* TD Size, packets remaining in this TD, bits 21:17 (5 bits, so max 31) */
#define TRB_TD_SIZE(p)          (min((p), (u32)31) << 17)
#define GET_TD_SIZE(p)		(((p) & 0x3e0000) >> 17)
/* xhci 1.1 uses the TD_SIZE field for TBC if Extended TBC is enabled (ETE) */
#define TRB_TD_SIZE_TBC(p)      (min((p), (u32)31) << 17)
/* Interrupter Target - which MSI-X vector to target the completion event at */
#define TRB_INTR_TARGET(p)	(((p) & 0x3ff) << 22)
#define GET_INTR_TARGET(p)	(((p) >> 22) & 0x3ff)

/* Cycle bit - indicates TRB ownership by HC or HCD */
#define TRB_CYCLE		(1<<0)
/*
 * Force next event data TRB to be evaluated before task switch.
 * Used to pass OS data back after a TD completes.
 */
#define TRB_ENT			(1<<1)
/* Interrupt on short packet */
#define TRB_ISP			(1<<2)
/* Set PCIe no snoop attribute */
#define TRB_NO_SNOOP		(1<<3)
/* Chain multiple TRBs into a TD */
#define TRB_CHAIN		(1<<4)
/* Interrupt on completion */
#define TRB_IOC			(1<<5)
/* The buffer pointer contains immediate data */
#define TRB_IDT			(1<<6)
/* TDs smaller than this might use IDT */
#define TRB_IDT_MAX_SIZE	8

/* Block Event Interrupt */
#define	TRB_BEI			(1<<9)

/* Control transfer TRB specific fields */
#define TRB_DIR_IN		(1<<16)
#define	TRB_TX_TYPE(p)		((p) << 16)
#define	TRB_DATA_OUT		2
#define	TRB_DATA_IN		3

/* Isochronous TRB specific fields */
#define TRB_SIA			(1<<31)
#define TRB_FRAME_ID(p)		(((p) & 0x7ff) << 20)
#define GET_FRAME_ID(p)		(((p) >> 20) & 0x7ff)
/* Total burst count field, Rsvdz on xhci 1.1 with Extended TBC enabled (ETE) */
#define TRB_TBC(p)		(((p) & 0x3) << 7)
#define GET_TBC(p)		(((p) >> 7) & 0x3)
#define TRB_TLBPC(p)		(((p) & 0xf) << 16)
#define GET_TLBPC(p)		(((p) >> 16) & 0xf)

/* TRB cache size for xHC with TRB cache */
#define TRB_CACHE_SIZE_HS	8
#define TRB_CACHE_SIZE_SS	16

struct xhci_generic_trb {
	__le32 field[4];
};

union xhci_trb {
	struct xhci_link_trb		link;
	struct xhci_transfer_event	trans_event;
	struct xhci_event_cmd		event_cmd;
	struct xhci_generic_trb		generic;
};

/* TRB bit mask */
#define	TRB_TYPE_BITMASK	(0xfc00)
#define TRB_TYPE(p)		((p) << 10)
#define TRB_FIELD_TO_TYPE(p)	(((p) & TRB_TYPE_BITMASK) >> 10)
/* TRB type IDs */
/* bulk, interrupt, isoc scatter/gather, and control data stage */
#define TRB_NORMAL		1
/* setup stage for control transfers */
#define TRB_SETUP		2
/* data stage for control transfers */
#define TRB_DATA		3
/* status stage for control transfers */
#define TRB_STATUS		4
/* isoc transfers */
#define TRB_ISOC		5
/* TRB for linking ring segments */
#define TRB_LINK		6
#define TRB_EVENT_DATA		7
/* Transfer Ring No-op (not for the command ring) */
#define TRB_TR_NOOP		8
/* Command TRBs */
/* Enable Slot Command */
#define TRB_ENABLE_SLOT		9
/* Disable Slot Command */
#define TRB_DISABLE_SLOT	10
/* Address Device Command */
#define TRB_ADDR_DEV		11
/* Configure Endpoint Command */
#define TRB_CONFIG_EP		12
/* Evaluate Context Command */
#define TRB_EVAL_CONTEXT	13
/* Reset Endpoint Command */
#define TRB_RESET_EP		14
/* Stop Transfer Ring Command */
#define TRB_STOP_RING		15
/* Set Transfer Ring Dequeue Pointer Command */
#define TRB_SET_DEQ		16
/* Reset Device Command */
#define TRB_RESET_DEV		17
/* Force Event Command (opt) */
#define TRB_FORCE_EVENT		18
/* Negotiate Bandwidth Command (opt) */
#define TRB_NEG_BANDWIDTH	19
/* Set Latency Tolerance Value Command (opt) */
#define TRB_SET_LT		20
/* Get port bandwidth Command */
#define TRB_GET_BW		21
/* Force Header Command - generate a transaction or link management packet */
#define TRB_FORCE_HEADER	22
/* No-op Command - not for transfer rings */
#define TRB_CMD_NOOP		23
/* TRB IDs 24-31 reserved */
/* Event TRBS */
/* Transfer Event */
#define TRB_TRANSFER		32
/* Command Completion Event */
#define TRB_COMPLETION		33
/* Port Status Change Event */
#define TRB_PORT_STATUS		34
/* Bandwidth Request Event (opt) */
#define TRB_BANDWIDTH_EVENT	35
/* Doorbell Event (opt) */
#define TRB_DOORBELL		36
/* Host Controller Event */
#define TRB_HC_EVENT		37
/* Device Notification Event - device sent function wake notification */
#define TRB_DEV_NOTE		38
/* MFINDEX Wrap Event - microframe counter wrapped */
#define TRB_MFINDEX_WRAP	39
/* TRB IDs 40-47 reserved, 48-63 is vendor-defined */
#define TRB_VENDOR_DEFINED_LOW	48
/* Nec vendor-specific command completion event. */
#define	TRB_NEC_CMD_COMP	48
/* Get NEC firmware revision. */
#define	TRB_NEC_GET_FW		49

static inline const char *xhci_trb_type_string(u8 type)
{
	switch (type) {
	case TRB_NORMAL:
		return "Normal";
	case TRB_SETUP:
		return "Setup Stage";
	case TRB_DATA:
		return "Data Stage";
	case TRB_STATUS:
		return "Status Stage";
	case TRB_ISOC:
		return "Isoch";
	case TRB_LINK:
		return "Link";
	case TRB_EVENT_DATA:
		return "Event Data";
	case TRB_TR_NOOP:
		return "No-Op";
	case TRB_ENABLE_SLOT:
		return "Enable Slot Command";
	case TRB_DISABLE_SLOT:
		return "Disable Slot Command";
	case TRB_ADDR_DEV:
		return "Address Device Command";
	case TRB_CONFIG_EP:
		return "Configure Endpoint Command";
	case TRB_EVAL_CONTEXT:
		return "Evaluate Context Command";
	case TRB_RESET_EP:
		return "Reset Endpoint Command";
	case TRB_STOP_RING:
		return "Stop Ring Command";
	case TRB_SET_DEQ:
		return "Set TR Dequeue Pointer Command";
	case TRB_RESET_DEV:
		return "Reset Device Command";
	case TRB_FORCE_EVENT:
		return "Force Event Command";
	case TRB_NEG_BANDWIDTH:
		return "Negotiate Bandwidth Command";
	case TRB_SET_LT:
		return "Set Latency Tolerance Value Command";
	case TRB_GET_BW:
		return "Get Port Bandwidth Command";
	case TRB_FORCE_HEADER:
		return "Force Header Command";
	case TRB_CMD_NOOP:
		return "No-Op Command";
	case TRB_TRANSFER:
		return "Transfer Event";
	case TRB_COMPLETION:
		return "Command Completion Event";
	case TRB_PORT_STATUS:
		return "Port Status Change Event";
	case TRB_BANDWIDTH_EVENT:
		return "Bandwidth Request Event";
	case TRB_DOORBELL:
		return "Doorbell Event";
	case TRB_HC_EVENT:
		return "Host Controller Event";
	case TRB_DEV_NOTE:
		return "Device Notification Event";
	case TRB_MFINDEX_WRAP:
		return "MFINDEX Wrap Event";
	case TRB_NEC_CMD_COMP:
		return "NEC Command Completion Event";
	case TRB_NEC_GET_FW:
		return "NET Get Firmware Revision Command";
	default:
		return "UNKNOWN";
	}
}

#define TRB_TYPE_LINK(x)	(((x) & TRB_TYPE_BITMASK) == TRB_TYPE(TRB_LINK))
/* Above, but for __le32 types -- can avoid work by swapping constants: */
#define TRB_TYPE_LINK_LE32(x)	(((x) & cpu_to_le32(TRB_TYPE_BITMASK)) == \
				 cpu_to_le32(TRB_TYPE(TRB_LINK)))
#define TRB_TYPE_NOOP_LE32(x)	(((x) & cpu_to_le32(TRB_TYPE_BITMASK)) == \
				 cpu_to_le32(TRB_TYPE(TRB_TR_NOOP)))

#define NEC_FW_MINOR(p)		(((p) >> 0) & 0xff)
#define NEC_FW_MAJOR(p)		(((p) >> 8) & 0xff)

/*
 * TRBS_PER_SEGMENT must be a multiple of 4,
 * since the command ring is 64-byte aligned.
 * It must also be greater than 16.
 */
#define TRBS_PER_SEGMENT	256
/* Allow two commands + a link TRB, along with any reserved command TRBs */
#define MAX_RSVD_CMD_TRBS	(TRBS_PER_SEGMENT - 3)
#define TRB_SEGMENT_SIZE	(TRBS_PER_SEGMENT*16)
#define TRB_SEGMENT_SHIFT	(ilog2(TRB_SEGMENT_SIZE))
/* TRB buffer pointers can't cross 64KB boundaries */
#define TRB_MAX_BUFF_SHIFT		16
#define TRB_MAX_BUFF_SIZE	(1 << TRB_MAX_BUFF_SHIFT)
/* How much data is left before the 64KB boundary? */
#define TRB_BUFF_LEN_UP_TO_BOUNDARY(addr)	(TRB_MAX_BUFF_SIZE - \
					(addr & (TRB_MAX_BUFF_SIZE - 1)))
#define MAX_SOFT_RETRY		3
/*
 * Limits of consecutive isoc trbs that can Block Event Interrupt (BEI) if
 * XHCI_AVOID_BEI quirk is in use.
 */
#define AVOID_BEI_INTERVAL_MIN	8
#define AVOID_BEI_INTERVAL_MAX	32

#define xhci_for_each_ring_seg(head, seg) \
	for (seg = head; seg != NULL; seg = (seg->next != head ? seg->next : NULL))

struct xhci_segment {
	union xhci_trb		*trbs;
	/* private to HCD */
	struct xhci_segment	*next;
	unsigned int		num;
	dma_addr_t		dma;
	/* Max packet sized bounce buffer for td-fragmant alignment */
	dma_addr_t		bounce_dma;
	void			*bounce_buf;
	unsigned int		bounce_offs;
	unsigned int		bounce_len;
};

enum xhci_cancelled_td_status {
	TD_DIRTY = 0,
	TD_HALTED,
	TD_CLEARING_CACHE,
	TD_CLEARING_CACHE_DEFERRED,
	TD_CLEARED,
};

struct xhci_td {
	struct list_head	td_list;
	struct list_head	cancelled_td_list;
	int			status;
	enum xhci_cancelled_td_status	cancel_status;
	struct urb		*urb;
	struct xhci_segment	*start_seg;
	union xhci_trb		*start_trb;
	struct xhci_segment	*end_seg;
	union xhci_trb		*end_trb;
	struct xhci_segment	*bounce_seg;
	/* actual_length of the URB has already been set */
	bool			urb_length_set;
	bool			error_mid_td;
};

/*
 * xHCI command default timeout value in milliseconds.
 * USB 3.2 spec, section 9.2.6.1
 */
#define XHCI_CMD_DEFAULT_TIMEOUT	5000

/* command descriptor */
struct xhci_cd {
	struct xhci_command	*command;
	union xhci_trb		*cmd_trb;
};

enum xhci_ring_type {
	TYPE_CTRL = 0,
	TYPE_ISOC,
	TYPE_BULK,
	TYPE_INTR,
	TYPE_STREAM,
	TYPE_COMMAND,
	TYPE_EVENT,
};

static inline const char *xhci_ring_type_string(enum xhci_ring_type type)
{
	switch (type) {
	case TYPE_CTRL:
		return "CTRL";
	case TYPE_ISOC:
		return "ISOC";
	case TYPE_BULK:
		return "BULK";
	case TYPE_INTR:
		return "INTR";
	case TYPE_STREAM:
		return "STREAM";
	case TYPE_COMMAND:
		return "CMD";
	case TYPE_EVENT:
		return "EVENT";
	}

	return "UNKNOWN";
}

struct xhci_ring {
	struct xhci_segment	*first_seg;
	struct xhci_segment	*last_seg;
	union  xhci_trb		*enqueue;
	struct xhci_segment	*enq_seg;
	union  xhci_trb		*dequeue;
	struct xhci_segment	*deq_seg;
	struct list_head	td_list;
	/*
	 * Write the cycle state into the TRB cycle field to give ownership of
	 * the TRB to the host controller (if we are the producer), or to check
	 * if we own the TRB (if we are the consumer).  See section 4.9.1.
	 */
	u32			cycle_state;
	unsigned int		stream_id;
	unsigned int		num_segs;
	unsigned int		num_trbs_free; /* used only by xhci DbC */
	unsigned int		bounce_buf_len;
	enum xhci_ring_type	type;
	u32			old_trb_comp_code;
	struct radix_tree_root	*trb_address_map;
};

struct xhci_erst_entry {
	/* 64-bit event ring segment address */
	__le64	seg_addr;
	__le32	seg_size;
	/* Set to zero */
	__le32	rsvd;
};

struct xhci_erst {
	struct xhci_erst_entry	*entries;
	unsigned int		num_entries;
	/* xhci->event_ring keeps track of segment dma addresses */
	dma_addr_t		erst_dma_addr;
};

struct xhci_scratchpad {
	u64 *sp_array;
	dma_addr_t sp_dma;
	void **sp_buffers;
};

struct urb_priv {
	int	num_tds;
	int	num_tds_done;
	struct	xhci_td	td[] __counted_by(num_tds);
};

/* Number of Event Ring segments to allocate, when amount is not specified. (spec allows 32k) */
#define	ERST_DEFAULT_SEGS	2
/* Poll every 60 seconds */
#define	POLL_TIMEOUT	60
/* Stop endpoint command timeout (secs) for URB cancellation watchdog timer */
#define XHCI_STOP_EP_CMD_TIMEOUT	5
/* XXX: Make these module parameters */

struct s3_save {
	u32	command;
	u32	dev_nt;
	u64	dcbaa_ptr;
	u32	config_reg;
};

/* Use for lpm */
struct dev_info {
	u32			dev_id;
	struct	list_head	list;
};

struct xhci_bus_state {
	unsigned long		bus_suspended;
	unsigned long		next_statechange;

	/* Port suspend arrays are indexed by the portnum of the fake roothub */
	/* ports suspend status arrays - max 31 ports for USB2, 15 for USB3 */
	u32			port_c_suspend;
	u32			suspended_ports;
	u32			port_remote_wakeup;
	/* which ports have started to resume */
	unsigned long		resuming_ports;
};

struct xhci_interrupter {
	struct xhci_ring	*event_ring;
	struct xhci_erst	erst;
	struct xhci_intr_reg __iomem *ir_set;
	unsigned int		intr_num;
	bool			ip_autoclear;
	u32			isoc_bei_interval;
	/* For interrupter registers save and restore over suspend/resume */
	u32	s3_iman;
	u32	s3_imod;
	u32	s3_erst_size;
	u64	s3_erst_base;
	u64	s3_erst_dequeue;
};
/*
 * It can take up to 20 ms to transition from RExit to U0 on the
 * Intel Lynx Point LP xHCI host.
 */
#define	XHCI_MAX_REXIT_TIMEOUT_MS	20
struct xhci_port_cap {
	u32			*psi;	/* array of protocol speed ID entries */
	u8			psi_count;
	u8			psi_uid_count;
	u8			maj_rev;
	u8			min_rev;
	u32			protocol_caps;
};

struct xhci_port {
	__le32 __iomem		*addr;
	int			hw_portnum;
	int			hcd_portnum;
	struct xhci_hub		*rhub;
	struct xhci_port_cap	*port_cap;
	unsigned int		lpm_incapable:1;
	unsigned long		resume_timestamp;
	bool			rexit_active;
	/* Slot ID is the index of the device directly connected to the port */
	int			slot_id;
	struct completion	rexit_done;
	struct completion	u3exit_done;
};

struct xhci_hub {
	struct xhci_port	**ports;
	unsigned int		num_ports;
	struct usb_hcd		*hcd;
	/* keep track of bus suspend info */
	struct xhci_bus_state   bus_state;
	/* supported prococol extended capabiliy values */
	u8			maj_rev;
	u8			min_rev;
};

/* There is one xhci_hcd structure per controller */
struct xhci_hcd {
	struct usb_hcd *main_hcd;
	struct usb_hcd *shared_hcd;
	/* glue to PCI and HCD framework */
	struct xhci_cap_regs __iomem *cap_regs;
	struct xhci_op_regs __iomem *op_regs;
	struct xhci_run_regs __iomem *run_regs;
	struct xhci_doorbell_array __iomem *dba;

	/* Cached register copies of read-only HC data */
	__u32		hcs_params1;
	__u32		hcs_params2;
	__u32		hcs_params3;
	__u32		hcc_params;
	__u32		hcc_params2;

	spinlock_t	lock;

	/* packed release number */
	u16		hci_version;
	u16		max_interrupters;
	/* imod_interval in ns (I * 250ns) */
	u32		imod_interval;
	u32		page_size;
	/* MSI-X/MSI vectors */
	int		nvecs;
	/* optional clocks */
	struct clk		*clk;
	struct clk		*reg_clk;
	/* optional reset controller */
	struct reset_control *reset;
	/* data structures */
	struct xhci_device_context_array *dcbaa;
	struct xhci_interrupter **interrupters;
	struct xhci_ring	*cmd_ring;
	unsigned int            cmd_ring_state;
#define CMD_RING_STATE_RUNNING         (1 << 0)
#define CMD_RING_STATE_ABORTED         (1 << 1)
#define CMD_RING_STATE_STOPPED         (1 << 2)
	struct list_head        cmd_list;
	unsigned int		cmd_ring_reserved_trbs;
	struct delayed_work	cmd_timer;
	struct completion	cmd_ring_stop_completion;
	struct xhci_command	*current_cmd;

	/* Scratchpad */
	struct xhci_scratchpad  *scratchpad;

	/* slot enabling and address device helpers */
	/* these are not thread safe so use mutex */
	struct mutex mutex;
	/* Internal mirror of the HW's dcbaa */
	struct xhci_virt_device	*devs[MAX_HC_SLOTS];
	/* For keeping track of bandwidth domains per roothub. */
	struct xhci_root_port_bw_info	*rh_bw;

	/* DMA pools */
	struct dma_pool	*device_pool;
	struct dma_pool	*segment_pool;
	struct dma_pool	*small_streams_pool;
	struct dma_pool	*port_bw_pool;
	struct dma_pool	*medium_streams_pool;

	/* Host controller watchdog timer structures */
	unsigned int		xhc_state;
	unsigned long		run_graceperiod;
	struct s3_save		s3;
/* Host controller is dying - not responding to commands. "I'm not dead yet!"
 *
 * xHC interrupts have been disabled and a watchdog timer will (or has already)
 * halt the xHCI host, and complete all URBs with an -ESHUTDOWN code.  Any code
 * that sees this status (other than the timer that set it) should stop touching
 * hardware immediately.  Interrupt handlers should return immediately when
 * they see this status (any time they drop and re-acquire xhci->lock).
 * xhci_urb_dequeue() should call usb_hcd_check_unlink_urb() and return without
 * putting the TD on the canceled list, etc.
 *
 * There are no reports of xHCI host controllers that display this issue.
 */
#define XHCI_STATE_DYING	(1 << 0)
#define XHCI_STATE_HALTED	(1 << 1)
#define XHCI_STATE_REMOVING	(1 << 2)
	unsigned long long	quirks;
#define	XHCI_LINK_TRB_QUIRK	BIT_ULL(0)
#define XHCI_RESET_EP_QUIRK	BIT_ULL(1) /* Deprecated */
#define XHCI_NEC_HOST		BIT_ULL(2)
#define XHCI_AMD_PLL_FIX	BIT_ULL(3)
#define XHCI_SPURIOUS_SUCCESS	BIT_ULL(4)
/*
 * Certain Intel host controllers have a limit to the number of endpoint
 * contexts they can handle.  Ideally, they would signal that they can't handle
 * anymore endpoint contexts by returning a Resource Error for the Configure
 * Endpoint command, but they don't.  Instead they expect software to keep track
 * of the number of active endpoints for them, across configure endpoint
 * commands, reset device commands, disable slot commands, and address device
 * commands.
 */
#define XHCI_EP_LIMIT_QUIRK	BIT_ULL(5)
#define XHCI_BROKEN_MSI		BIT_ULL(6)
#define XHCI_RESET_ON_RESUME	BIT_ULL(7)
#define	XHCI_SW_BW_CHECKING	BIT_ULL(8)
#define XHCI_AMD_0x96_HOST	BIT_ULL(9)
#define XHCI_TRUST_TX_LENGTH	BIT_ULL(10) /* Deprecated */
#define XHCI_LPM_SUPPORT	BIT_ULL(11)
#define XHCI_INTEL_HOST		BIT_ULL(12)
#define XHCI_SPURIOUS_REBOOT	BIT_ULL(13)
#define XHCI_COMP_MODE_QUIRK	BIT_ULL(14)
#define XHCI_AVOID_BEI		BIT_ULL(15)
#define XHCI_PLAT		BIT_ULL(16) /* Deprecated */
#define XHCI_SLOW_SUSPEND	BIT_ULL(17)
#define XHCI_SPURIOUS_WAKEUP	BIT_ULL(18)
/* For controllers with a broken beyond repair streams implementation */
#define XHCI_BROKEN_STREAMS	BIT_ULL(19)
#define XHCI_PME_STUCK_QUIRK	BIT_ULL(20)
#define XHCI_MTK_HOST		BIT_ULL(21)
#define XHCI_SSIC_PORT_UNUSED	BIT_ULL(22)
#define XHCI_NO_64BIT_SUPPORT	BIT_ULL(23)
#define XHCI_MISSING_CAS	BIT_ULL(24)
/* For controller with a broken Port Disable implementation */
#define XHCI_BROKEN_PORT_PED	BIT_ULL(25)
#define XHCI_LIMIT_ENDPOINT_INTERVAL_7	BIT_ULL(26)
#define XHCI_U2_DISABLE_WAKE	BIT_ULL(27)
#define XHCI_ASMEDIA_MODIFY_FLOWCONTROL	BIT_ULL(28)
#define XHCI_HW_LPM_DISABLE	BIT_ULL(29)
#define XHCI_SUSPEND_DELAY	BIT_ULL(30)
#define XHCI_INTEL_USB_ROLE_SW	BIT_ULL(31)
#define XHCI_ZERO_64B_REGS	BIT_ULL(32)
#define XHCI_DEFAULT_PM_RUNTIME_ALLOW	BIT_ULL(33)
#define XHCI_RESET_PLL_ON_DISCONNECT	BIT_ULL(34)
#define XHCI_SNPS_BROKEN_SUSPEND    BIT_ULL(35)
/* Reserved. It was XHCI_RENESAS_FW_QUIRK */
#define XHCI_SKIP_PHY_INIT	BIT_ULL(37)
#define XHCI_DISABLE_SPARSE	BIT_ULL(38)
#define XHCI_SG_TRB_CACHE_SIZE_QUIRK	BIT_ULL(39)
#define XHCI_NO_SOFT_RETRY	BIT_ULL(40)
#define XHCI_BROKEN_D3COLD_S2I	BIT_ULL(41)
#define XHCI_EP_CTX_BROKEN_DCS	BIT_ULL(42)
#define XHCI_SUSPEND_RESUME_CLKS	BIT_ULL(43)
#define XHCI_RESET_TO_DEFAULT	BIT_ULL(44)
#define XHCI_TRB_OVERFETCH	BIT_ULL(45)
#define XHCI_ZHAOXIN_HOST	BIT_ULL(46)
#define XHCI_WRITE_64_HI_LO	BIT_ULL(47)
#define XHCI_CDNS_SCTX_QUIRK	BIT_ULL(48)
#define XHCI_ETRON_HOST	BIT_ULL(49)
#define XHCI_LIMIT_ENDPOINT_INTERVAL_9 BIT_ULL(50)

	unsigned int		num_active_eps;
	unsigned int		limit_active_eps;
	struct xhci_port	*hw_ports;
	struct xhci_hub		usb2_rhub;
	struct xhci_hub		usb3_rhub;
	/* support xHCI 1.0 spec USB2 hardware LPM */
	unsigned		hw_lpm_support:1;
	/* Broken Suspend flag for SNPS Suspend resume issue */
	unsigned		broken_suspend:1;
	/* Indicates that omitting hcd is supported if root hub has no ports */
	unsigned		allow_single_roothub:1;
	/* cached extended protocol port capabilities */
	struct xhci_port_cap	*port_caps;
	unsigned int		num_port_caps;
	/* Compliance Mode Recovery Data */
	struct timer_list	comp_mode_recovery_timer;
	u32			port_status_u0;
	u16			test_mode;
/* Compliance Mode Timer Triggered every 2 seconds */
#define COMP_MODE_RCVRY_MSECS 2000

	struct dentry		*debugfs_root;
	struct dentry		*debugfs_slots;
	struct list_head	regset_list;

	void			*dbc;
	/* platform-specific data -- must come last */
	unsigned long		priv[] __aligned(sizeof(s64));
};

/* Platform specific overrides to generic XHCI hc_driver ops */
struct xhci_driver_overrides {
	size_t extra_priv_size;
	int (*reset)(struct usb_hcd *hcd);
	int (*start)(struct usb_hcd *hcd);
	int (*add_endpoint)(struct usb_hcd *hcd, struct usb_device *udev,
			    struct usb_host_endpoint *ep);
	int (*drop_endpoint)(struct usb_hcd *hcd, struct usb_device *udev,
			     struct usb_host_endpoint *ep);
	int (*check_bandwidth)(struct usb_hcd *, struct usb_device *);
	void (*reset_bandwidth)(struct usb_hcd *, struct usb_device *);
	int (*update_hub_device)(struct usb_hcd *hcd, struct usb_device *hdev,
			    struct usb_tt *tt, gfp_t mem_flags);
	int (*hub_control)(struct usb_hcd *hcd, u16 typeReq, u16 wValue,
			   u16 wIndex, char *buf, u16 wLength);
};

#define	XHCI_CFC_DELAY		10

/* convert between an HCD pointer and the corresponding EHCI_HCD */
static inline struct xhci_hcd *hcd_to_xhci(struct usb_hcd *hcd)
{
	struct usb_hcd *primary_hcd;

	if (usb_hcd_is_primary_hcd(hcd))
		primary_hcd = hcd;
	else
		primary_hcd = hcd->primary_hcd;

	return (struct xhci_hcd *) (primary_hcd->hcd_priv);
}

static inline struct usb_hcd *xhci_to_hcd(struct xhci_hcd *xhci)
{
	return xhci->main_hcd;
}

static inline struct usb_hcd *xhci_get_usb3_hcd(struct xhci_hcd *xhci)
{
	if (xhci->shared_hcd)
		return xhci->shared_hcd;

	if (!xhci->usb2_rhub.num_ports)
		return xhci->main_hcd;

	return NULL;
}

static inline bool xhci_hcd_is_usb3(struct usb_hcd *hcd)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);

	return hcd == xhci_get_usb3_hcd(xhci);
}

static inline bool xhci_has_one_roothub(struct xhci_hcd *xhci)
{
	return xhci->allow_single_roothub &&
	       (!xhci->usb2_rhub.num_ports || !xhci->usb3_rhub.num_ports);
}

#define xhci_dbg(xhci, fmt, args...) \
	dev_dbg(xhci_to_hcd(xhci)->self.controller , fmt , ## args)
#define xhci_err(xhci, fmt, args...) \
	dev_err(xhci_to_hcd(xhci)->self.controller , fmt , ## args)
#define xhci_warn(xhci, fmt, args...) \
	dev_warn(xhci_to_hcd(xhci)->self.controller , fmt , ## args)
#define xhci_info(xhci, fmt, args...) \
	dev_info(xhci_to_hcd(xhci)->self.controller , fmt , ## args)

/*
 * Registers should always be accessed with double word or quad word accesses.
 *
 * Some xHCI implementations may support 64-bit address pointers.  Registers
 * with 64-bit address pointers should be written to with dword accesses by
 * writing the low dword first (ptr[0]), then the high dword (ptr[1]) second.
 * xHCI implementations that do not support 64-bit address pointers will ignore
 * the high dword, and write order is irrelevant.
 */
static inline u64 xhci_read_64(const struct xhci_hcd *xhci,
		__le64 __iomem *regs)
{
	return lo_hi_readq(regs);
}
static inline void xhci_write_64(struct xhci_hcd *xhci,
				 const u64 val, __le64 __iomem *regs)
{
	lo_hi_writeq(val, regs);
}


/*
 * Reportedly, some chapters of v0.95 spec said that Link TRB always has its chain bit set.
 * Other chapters and later specs say that it should only be set if the link is inside a TD
 * which continues from the end of one segment to the next segment.
 *
 * Some 0.95 hardware was found to misbehave if any link TRB doesn't have the chain bit set.
 *
 * 0.96 hardware from AMD and NEC was found to ignore unchained isochronous link TRBs when
 * "resynchronizing the pipe" after a Missed Service Error.
 */
static inline bool xhci_link_chain_quirk(struct xhci_hcd *xhci, enum xhci_ring_type type)
{
	return (xhci->quirks & XHCI_LINK_TRB_QUIRK) ||
	       (type == TYPE_ISOC && (xhci->quirks & (XHCI_AMD_0x96_HOST | XHCI_NEC_HOST)));
}

/* xHCI debugging */
char *xhci_get_slot_state(struct xhci_hcd *xhci,
		struct xhci_container_ctx *ctx);
void xhci_dbg_trace(struct xhci_hcd *xhci, void (*trace)(struct va_format *),
			const char *fmt, ...);

/* xHCI memory management */
void xhci_mem_cleanup(struct xhci_hcd *xhci);
int xhci_mem_init(struct xhci_hcd *xhci, gfp_t flags);
void xhci_free_virt_device(struct xhci_hcd *xhci, struct xhci_virt_device *dev, int slot_id);
int xhci_alloc_virt_device(struct xhci_hcd *xhci, int slot_id, struct usb_device *udev, gfp_t flags);
int xhci_setup_addressable_virt_dev(struct xhci_hcd *xhci, struct usb_device *udev);
void xhci_copy_ep0_dequeue_into_input_ctx(struct xhci_hcd *xhci,
		struct usb_device *udev);
unsigned int xhci_get_endpoint_index(struct usb_endpoint_descriptor *desc);
unsigned int xhci_last_valid_endpoint(u32 added_ctxs);
void xhci_endpoint_zero(struct xhci_hcd *xhci, struct xhci_virt_device *virt_dev, struct usb_host_endpoint *ep);
void xhci_update_tt_active_eps(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev,
		int old_active_eps);
void xhci_clear_endpoint_bw_info(struct xhci_bw_info *bw_info);
void xhci_update_bw_info(struct xhci_hcd *xhci,
		struct xhci_container_ctx *in_ctx,
		struct xhci_input_control_ctx *ctrl_ctx,
		struct xhci_virt_device *virt_dev);
void xhci_endpoint_copy(struct xhci_hcd *xhci,
		struct xhci_container_ctx *in_ctx,
		struct xhci_container_ctx *out_ctx,
		unsigned int ep_index);
void xhci_slot_copy(struct xhci_hcd *xhci,
		struct xhci_container_ctx *in_ctx,
		struct xhci_container_ctx *out_ctx);
int xhci_endpoint_init(struct xhci_hcd *xhci, struct xhci_virt_device *virt_dev,
		struct usb_device *udev, struct usb_host_endpoint *ep,
		gfp_t mem_flags);
struct xhci_ring *xhci_ring_alloc(struct xhci_hcd *xhci, unsigned int num_segs,
		enum xhci_ring_type type, unsigned int max_packet, gfp_t flags);
void xhci_ring_free(struct xhci_hcd *xhci, struct xhci_ring *ring);
int xhci_ring_expansion(struct xhci_hcd *xhci, struct xhci_ring *ring,
		unsigned int num_trbs, gfp_t flags);
void xhci_initialize_ring_info(struct xhci_ring *ring);
void xhci_free_endpoint_ring(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev,
		unsigned int ep_index);
struct xhci_stream_info *xhci_alloc_stream_info(struct xhci_hcd *xhci,
		unsigned int num_stream_ctxs,
		unsigned int num_streams,
		unsigned int max_packet, gfp_t flags);
void xhci_free_stream_info(struct xhci_hcd *xhci,
		struct xhci_stream_info *stream_info);
void xhci_setup_streams_ep_input_ctx(struct xhci_hcd *xhci,
		struct xhci_ep_ctx *ep_ctx,
		struct xhci_stream_info *stream_info);
void xhci_setup_no_streams_ep_input_ctx(struct xhci_ep_ctx *ep_ctx,
		struct xhci_virt_ep *ep);
void xhci_free_device_endpoint_resources(struct xhci_hcd *xhci,
	struct xhci_virt_device *virt_dev, bool drop_control_ep);
struct xhci_ring *xhci_dma_to_transfer_ring(
		struct xhci_virt_ep *ep,
		u64 address);
struct xhci_command *xhci_alloc_command(struct xhci_hcd *xhci,
		bool allocate_completion, gfp_t mem_flags);
struct xhci_command *xhci_alloc_command_with_ctx(struct xhci_hcd *xhci,
		bool allocate_completion, gfp_t mem_flags);
void xhci_urb_free_priv(struct urb_priv *urb_priv);
void xhci_free_command(struct xhci_hcd *xhci,
		struct xhci_command *command);
struct xhci_container_ctx *xhci_alloc_container_ctx(struct xhci_hcd *xhci,
		int type, gfp_t flags);
void xhci_free_container_ctx(struct xhci_hcd *xhci,
		struct xhci_container_ctx *ctx);
struct xhci_container_ctx *xhci_alloc_port_bw_ctx(struct xhci_hcd *xhci,
		gfp_t flags);
void xhci_free_port_bw_ctx(struct xhci_hcd *xhci,
		struct xhci_container_ctx *ctx);
struct xhci_interrupter *
xhci_create_secondary_interrupter(struct usb_hcd *hcd, unsigned int segs,
				  u32 imod_interval, unsigned int intr_num);
void xhci_remove_secondary_interrupter(struct usb_hcd
				       *hcd, struct xhci_interrupter *ir);
void xhci_skip_sec_intr_events(struct xhci_hcd *xhci,
			       struct xhci_ring *ring,
			       struct xhci_interrupter *ir);

/* xHCI host controller glue */
typedef void (*xhci_get_quirks_t)(struct device *, struct xhci_hcd *);
int xhci_handshake(void __iomem *ptr, u32 mask, u32 done, u64 timeout_us);
void xhci_quiesce(struct xhci_hcd *xhci);
int xhci_halt(struct xhci_hcd *xhci);
int xhci_start(struct xhci_hcd *xhci);
int xhci_reset(struct xhci_hcd *xhci, u64 timeout_us);
int xhci_run(struct usb_hcd *hcd);
int xhci_gen_setup(struct usb_hcd *hcd, xhci_get_quirks_t get_quirks);
void xhci_shutdown(struct usb_hcd *hcd);
void xhci_stop(struct usb_hcd *hcd);
void xhci_init_driver(struct hc_driver *drv,
		      const struct xhci_driver_overrides *over);
int xhci_add_endpoint(struct usb_hcd *hcd, struct usb_device *udev,
		      struct usb_host_endpoint *ep);
int xhci_drop_endpoint(struct usb_hcd *hcd, struct usb_device *udev,
		       struct usb_host_endpoint *ep);
int xhci_check_bandwidth(struct usb_hcd *hcd, struct usb_device *udev);
void xhci_reset_bandwidth(struct usb_hcd *hcd, struct usb_device *udev);
int xhci_update_hub_device(struct usb_hcd *hcd, struct usb_device *hdev,
			   struct usb_tt *tt, gfp_t mem_flags);
int xhci_disable_slot(struct xhci_hcd *xhci, u32 slot_id);
int xhci_disable_and_free_slot(struct xhci_hcd *xhci, u32 slot_id);
int xhci_ext_cap_init(struct xhci_hcd *xhci);

int xhci_suspend(struct xhci_hcd *xhci, bool do_wakeup);
int xhci_resume(struct xhci_hcd *xhci, bool power_lost, bool is_auto_resume);

irqreturn_t xhci_irq(struct usb_hcd *hcd);
irqreturn_t xhci_msi_irq(int irq, void *hcd);
int xhci_alloc_dev(struct usb_hcd *hcd, struct usb_device *udev);
int xhci_alloc_tt_info(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev,
		struct usb_device *hdev,
		struct usb_tt *tt, gfp_t mem_flags);
int xhci_set_interrupter_moderation(struct xhci_interrupter *ir,
				    u32 imod_interval);
int xhci_enable_interrupter(struct xhci_interrupter *ir);
int xhci_disable_interrupter(struct xhci_hcd *xhci, struct xhci_interrupter *ir);

/* xHCI ring, segment, TRB, and TD functions */
dma_addr_t xhci_trb_virt_to_dma(struct xhci_segment *seg, union xhci_trb *trb);
int xhci_is_vendor_info_code(struct xhci_hcd *xhci, unsigned int trb_comp_code);
void xhci_ring_cmd_db(struct xhci_hcd *xhci);
int xhci_queue_slot_control(struct xhci_hcd *xhci, struct xhci_command *cmd,
		u32 trb_type, u32 slot_id);
int xhci_queue_address_device(struct xhci_hcd *xhci, struct xhci_command *cmd,
		dma_addr_t in_ctx_ptr, u32 slot_id, enum xhci_setup_dev);
int xhci_queue_vendor_command(struct xhci_hcd *xhci, struct xhci_command *cmd,
		u32 field1, u32 field2, u32 field3, u32 field4);
int xhci_queue_stop_endpoint(struct xhci_hcd *xhci, struct xhci_command *cmd,
		int slot_id, unsigned int ep_index, int suspend);
int xhci_queue_ctrl_tx(struct xhci_hcd *xhci, gfp_t mem_flags, struct urb *urb,
		int slot_id, unsigned int ep_index);
int xhci_queue_bulk_tx(struct xhci_hcd *xhci, gfp_t mem_flags, struct urb *urb,
		int slot_id, unsigned int ep_index);
int xhci_queue_intr_tx(struct xhci_hcd *xhci, gfp_t mem_flags, struct urb *urb,
		int slot_id, unsigned int ep_index);
int xhci_queue_isoc_tx_prepare(struct xhci_hcd *xhci, gfp_t mem_flags,
		struct urb *urb, int slot_id, unsigned int ep_index);
int xhci_queue_configure_endpoint(struct xhci_hcd *xhci,
		struct xhci_command *cmd, dma_addr_t in_ctx_ptr, u32 slot_id,
		bool command_must_succeed);
int xhci_queue_get_port_bw(struct xhci_hcd *xhci,
		struct xhci_command *cmd, dma_addr_t in_ctx_ptr,
		u8 dev_speed, bool command_must_succeed);
int xhci_get_port_bandwidth(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx,
		u8 dev_speed);
int xhci_queue_evaluate_context(struct xhci_hcd *xhci, struct xhci_command *cmd,
		dma_addr_t in_ctx_ptr, u32 slot_id, bool command_must_succeed);
int xhci_queue_reset_ep(struct xhci_hcd *xhci, struct xhci_command *cmd,
		int slot_id, unsigned int ep_index,
		enum xhci_ep_reset_type reset_type);
int xhci_queue_reset_device(struct xhci_hcd *xhci, struct xhci_command *cmd,
		u32 slot_id);
void xhci_handle_command_timeout(struct work_struct *work);

void xhci_ring_ep_doorbell(struct xhci_hcd *xhci, unsigned int slot_id,
		unsigned int ep_index, unsigned int stream_id);
void xhci_ring_doorbell_for_active_rings(struct xhci_hcd *xhci,
		unsigned int slot_id,
		unsigned int ep_index);
void xhci_cleanup_command_queue(struct xhci_hcd *xhci);
void inc_deq(struct xhci_hcd *xhci, struct xhci_ring *ring);
unsigned int count_trbs(u64 addr, u64 len);
int xhci_stop_endpoint_sync(struct xhci_hcd *xhci, struct xhci_virt_ep *ep,
			    int suspend, gfp_t gfp_flags);
void xhci_process_cancelled_tds(struct xhci_virt_ep *ep);
void xhci_update_erst_dequeue(struct xhci_hcd *xhci,
			      struct xhci_interrupter *ir,
			      bool clear_ehb);
void xhci_add_interrupter(struct xhci_hcd *xhci, unsigned int intr_num);
int xhci_usb_endpoint_maxp(struct usb_device *udev,
			   struct usb_host_endpoint *host_ep);

/* xHCI roothub code */
void xhci_set_link_state(struct xhci_hcd *xhci, struct xhci_port *port,
				u32 link_state);
void xhci_test_and_clear_bit(struct xhci_hcd *xhci, struct xhci_port *port,
				u32 port_bit);
int xhci_hub_control(struct usb_hcd *hcd, u16 typeReq, u16 wValue, u16 wIndex,
		char *buf, u16 wLength);
int xhci_hub_status_data(struct usb_hcd *hcd, char *buf);
int xhci_find_raw_port_number(struct usb_hcd *hcd, int port1);
struct xhci_hub *xhci_get_rhub(struct usb_hcd *hcd);
enum usb_link_tunnel_mode xhci_port_is_tunneled(struct xhci_hcd *xhci,
						struct xhci_port *port);
void xhci_hc_died(struct xhci_hcd *xhci);

#ifdef CONFIG_PM
int xhci_bus_suspend(struct usb_hcd *hcd);
int xhci_bus_resume(struct usb_hcd *hcd);
unsigned long xhci_get_resuming_ports(struct usb_hcd *hcd);
#else
#define	xhci_bus_suspend	NULL
#define	xhci_bus_resume		NULL
#define	xhci_get_resuming_ports	NULL
#endif	/* CONFIG_PM */

u32 xhci_port_state_to_neutral(u32 state);
void xhci_ring_device(struct xhci_hcd *xhci, int slot_id);

/* xHCI contexts */
struct xhci_input_control_ctx *xhci_get_input_control_ctx(struct xhci_container_ctx *ctx);
struct xhci_slot_ctx *xhci_get_slot_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx);
struct xhci_ep_ctx *xhci_get_ep_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx, unsigned int ep_index);

struct xhci_ring *xhci_triad_to_transfer_ring(struct xhci_hcd *xhci,
		unsigned int slot_id, unsigned int ep_index,
		unsigned int stream_id);

static inline struct xhci_ring *xhci_urb_to_transfer_ring(struct xhci_hcd *xhci,
								struct urb *urb)
{
	return xhci_triad_to_transfer_ring(xhci, urb->dev->slot_id,
					xhci_get_endpoint_index(&urb->ep->desc),
					urb->stream_id);
}

/*
 * TODO: As per spec Isochronous IDT transmissions are supported. We bypass
 * them anyways as we where unable to find a device that matches the
 * constraints.
 */
static inline bool xhci_urb_suitable_for_idt(struct urb *urb)
{
	if (!usb_endpoint_xfer_isoc(&urb->ep->desc) && usb_urb_dir_out(urb) &&
	    usb_endpoint_maxp(&urb->ep->desc) >= TRB_IDT_MAX_SIZE &&
	    urb->transfer_buffer_length <= TRB_IDT_MAX_SIZE &&
	    !(urb->transfer_flags & URB_NO_TRANSFER_DMA_MAP) &&
	    !urb->num_sgs)
		return true;

	return false;
}

static inline char *xhci_slot_state_string(u32 state)
{
	switch (state) {
	case SLOT_STATE_ENABLED:
		return "enabled/disabled";
	case SLOT_STATE_DEFAULT:
		return "default";
	case SLOT_STATE_ADDRESSED:
		return "addressed";
	case SLOT_STATE_CONFIGURED:
		return "configured";
	default:
		return "reserved";
	}
}

static inline const char *xhci_decode_trb(char *str, size_t size,
					  u32 field0, u32 field1, u32 field2, u32 field3)
{
	int type = TRB_FIELD_TO_TYPE(field3);

	switch (type) {
	case TRB_LINK:
		snprintf(str, size,
			"LINK %08x%08x intr %d type '%s' flags %c:%c:%c:%c",
			field1, field0, GET_INTR_TARGET(field2),
			xhci_trb_type_string(type),
			field3 & TRB_IOC ? 'I' : 'i',
			field3 & TRB_CHAIN ? 'C' : 'c',
			field3 & TRB_TC ? 'T' : 't',
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_TRANSFER:
	case TRB_COMPLETION:
	case TRB_PORT_STATUS:
	case TRB_BANDWIDTH_EVENT:
	case TRB_DOORBELL:
	case TRB_HC_EVENT:
	case TRB_DEV_NOTE:
	case TRB_MFINDEX_WRAP:
		snprintf(str, size,
			"TRB %08x%08x status '%s' len %d slot %d ep %d type '%s' flags %c:%c",
			field1, field0,
			xhci_trb_comp_code_string(GET_COMP_CODE(field2)),
			EVENT_TRB_LEN(field2), TRB_TO_SLOT_ID(field3),
			TRB_TO_EP_ID(field3),
			xhci_trb_type_string(type),
			field3 & EVENT_DATA ? 'E' : 'e',
			field3 & TRB_CYCLE ? 'C' : 'c');

		break;
	case TRB_SETUP:
		snprintf(str, size,
			"bRequestType %02x bRequest %02x wValue %02x%02x wIndex %02x%02x wLength %d length %d TD size %d intr %d type '%s' flags %c:%c:%c",
				field0 & 0xff,
				(field0 & 0xff00) >> 8,
				(field0 & 0xff000000) >> 24,
				(field0 & 0xff0000) >> 16,
				(field1 & 0xff00) >> 8,
				field1 & 0xff,
				(field1 & 0xff000000) >> 16 |
				(field1 & 0xff0000) >> 16,
				TRB_LEN(field2), GET_TD_SIZE(field2),
				GET_INTR_TARGET(field2),
				xhci_trb_type_string(type),
				field3 & TRB_IDT ? 'I' : 'i',
				field3 & TRB_IOC ? 'I' : 'i',
				field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_DATA:
		snprintf(str, size,
			 "Buffer %08x%08x length %d TD size %d intr %d type '%s' flags %c:%c:%c:%c:%c:%c:%c",
				field1, field0, TRB_LEN(field2), GET_TD_SIZE(field2),
				GET_INTR_TARGET(field2),
				xhci_trb_type_string(type),
				field3 & TRB_IDT ? 'I' : 'i',
				field3 & TRB_IOC ? 'I' : 'i',
				field3 & TRB_CHAIN ? 'C' : 'c',
				field3 & TRB_NO_SNOOP ? 'S' : 's',
				field3 & TRB_ISP ? 'I' : 'i',
				field3 & TRB_ENT ? 'E' : 'e',
				field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_STATUS:
		snprintf(str, size,
			 "Buffer %08x%08x length %d TD size %d intr %d type '%s' flags %c:%c:%c:%c",
				field1, field0, TRB_LEN(field2), GET_TD_SIZE(field2),
				GET_INTR_TARGET(field2),
				xhci_trb_type_string(type),
				field3 & TRB_IOC ? 'I' : 'i',
				field3 & TRB_CHAIN ? 'C' : 'c',
				field3 & TRB_ENT ? 'E' : 'e',
				field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_NORMAL:
	case TRB_EVENT_DATA:
	case TRB_TR_NOOP:
		snprintf(str, size,
			"Buffer %08x%08x length %d TD size %d intr %d type '%s' flags %c:%c:%c:%c:%c:%c:%c:%c",
			field1, field0, TRB_LEN(field2), GET_TD_SIZE(field2),
			GET_INTR_TARGET(field2),
			xhci_trb_type_string(type),
			field3 & TRB_BEI ? 'B' : 'b',
			field3 & TRB_IDT ? 'I' : 'i',
			field3 & TRB_IOC ? 'I' : 'i',
			field3 & TRB_CHAIN ? 'C' : 'c',
			field3 & TRB_NO_SNOOP ? 'S' : 's',
			field3 & TRB_ISP ? 'I' : 'i',
			field3 & TRB_ENT ? 'E' : 'e',
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_ISOC:
		snprintf(str, size,
			"Buffer %08x%08x length %d TD size/TBC %d intr %d type '%s' TBC %u TLBPC %u frame_id %u flags %c:%c:%c:%c:%c:%c:%c:%c:%c",
			field1, field0, TRB_LEN(field2), GET_TD_SIZE(field2),
			GET_INTR_TARGET(field2),
			xhci_trb_type_string(type),
			GET_TBC(field3),
			GET_TLBPC(field3),
			GET_FRAME_ID(field3),
			field3 & TRB_SIA ? 'S' : 's',
			field3 & TRB_BEI ? 'B' : 'b',
			field3 & TRB_IDT ? 'I' : 'i',
			field3 & TRB_IOC ? 'I' : 'i',
			field3 & TRB_CHAIN ? 'C' : 'c',
			field3 & TRB_NO_SNOOP ? 'S' : 's',
			field3 & TRB_ISP ? 'I' : 'i',
			field3 & TRB_ENT ? 'E' : 'e',
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_CMD_NOOP:
	case TRB_ENABLE_SLOT:
		snprintf(str, size,
			"%s: flags %c",
			xhci_trb_type_string(type),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_DISABLE_SLOT:
	case TRB_NEG_BANDWIDTH:
		snprintf(str, size,
			"%s: slot %d flags %c",
			xhci_trb_type_string(type),
			TRB_TO_SLOT_ID(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_ADDR_DEV:
		snprintf(str, size,
			"%s: ctx %08x%08x slot %d flags %c:%c",
			xhci_trb_type_string(type),
			field1, field0,
			TRB_TO_SLOT_ID(field3),
			field3 & TRB_BSR ? 'B' : 'b',
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_CONFIG_EP:
		snprintf(str, size,
			"%s: ctx %08x%08x slot %d flags %c:%c",
			xhci_trb_type_string(type),
			field1, field0,
			TRB_TO_SLOT_ID(field3),
			field3 & TRB_DC ? 'D' : 'd',
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_EVAL_CONTEXT:
		snprintf(str, size,
			"%s: ctx %08x%08x slot %d flags %c",
			xhci_trb_type_string(type),
			field1, field0,
			TRB_TO_SLOT_ID(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_RESET_EP:
		snprintf(str, size,
			"%s: ctx %08x%08x slot %d ep %d flags %c:%c",
			xhci_trb_type_string(type),
			field1, field0,
			TRB_TO_SLOT_ID(field3),
			TRB_TO_EP_ID(field3),
			field3 & TRB_TSP ? 'T' : 't',
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_STOP_RING:
		snprintf(str, size,
			"%s: slot %d sp %d ep %d flags %c",
			xhci_trb_type_string(type),
			TRB_TO_SLOT_ID(field3),
			TRB_TO_SUSPEND_PORT(field3),
			TRB_TO_EP_ID(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_SET_DEQ:
		snprintf(str, size,
			"%s: deq %08x%08x stream %d slot %d ep %d flags %c",
			xhci_trb_type_string(type),
			field1, field0,
			TRB_TO_STREAM_ID(field2),
			TRB_TO_SLOT_ID(field3),
			TRB_TO_EP_ID(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_RESET_DEV:
		snprintf(str, size,
			"%s: slot %d flags %c",
			xhci_trb_type_string(type),
			TRB_TO_SLOT_ID(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_FORCE_EVENT:
		snprintf(str, size,
			"%s: event %08x%08x vf intr %d vf id %d flags %c",
			xhci_trb_type_string(type),
			field1, field0,
			TRB_TO_VF_INTR_TARGET(field2),
			TRB_TO_VF_ID(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_SET_LT:
		snprintf(str, size,
			"%s: belt %d flags %c",
			xhci_trb_type_string(type),
			TRB_TO_BELT(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_GET_BW:
		snprintf(str, size,
			"%s: ctx %08x%08x slot %d speed %d flags %c",
			xhci_trb_type_string(type),
			field1, field0,
			TRB_TO_SLOT_ID(field3),
			TRB_TO_DEV_SPEED(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_FORCE_HEADER:
		snprintf(str, size,
			"%s: info %08x%08x%08x pkt type %d roothub port %d flags %c",
			xhci_trb_type_string(type),
			field2, field1, field0 & 0xffffffe0,
			TRB_TO_PACKET_TYPE(field0),
			TRB_TO_ROOTHUB_PORT(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	default:
		snprintf(str, size,
			"type '%s' -> raw %08x %08x %08x %08x",
			xhci_trb_type_string(type),
			field0, field1, field2, field3);
	}

	return str;
}

static inline const char *xhci_decode_ctrl_ctx(char *str,
		unsigned long drop, unsigned long add)
{
	unsigned int	bit;
	int		ret = 0;

	str[0] = '\0';

	if (drop) {
		ret = sprintf(str, "Drop:");
		for_each_set_bit(bit, &drop, 32)
			ret += sprintf(str + ret, " %d%s",
				       bit / 2,
				       bit % 2 ? "in":"out");
		ret += sprintf(str + ret, ", ");
	}

	if (add) {
		ret += sprintf(str + ret, "Add:%s%s",
			       (add & SLOT_FLAG) ? " slot":"",
			       (add & EP0_FLAG) ? " ep0":"");
		add &= ~(SLOT_FLAG | EP0_FLAG);
		for_each_set_bit(bit, &add, 32)
			ret += sprintf(str + ret, " %d%s",
				       bit / 2,
				       bit % 2 ? "in":"out");
	}
	return str;
}

static inline const char *xhci_decode_slot_context(char *str,
		u32 info, u32 info2, u32 tt_info, u32 state)
{
	u32 speed;
	u32 hub;
	u32 mtt;
	int ret = 0;

	speed = info & DEV_SPEED;
	hub = info & DEV_HUB;
	mtt = info & DEV_MTT;

	ret = sprintf(str, "RS %05x %s%s%s Ctx Entries %d MEL %d us Port# %d/%d",
			info & ROUTE_STRING_MASK,
			({ char *s;
			switch (speed) {
			case SLOT_SPEED_FS:
				s = "full-speed";
				break;
			case SLOT_SPEED_LS:
				s = "low-speed";
				break;
			case SLOT_SPEED_HS:
				s = "high-speed";
				break;
			case SLOT_SPEED_SS:
				s = "super-speed";
				break;
			case SLOT_SPEED_SSP:
				s = "super-speed plus";
				break;
			default:
				s = "UNKNOWN speed";
			} s; }),
			mtt ? " multi-TT" : "",
			hub ? " Hub" : "",
			(info & LAST_CTX_MASK) >> 27,
			info2 & MAX_EXIT,
			DEVINFO_TO_ROOT_HUB_PORT(info2),
			DEVINFO_TO_MAX_PORTS(info2));

	ret += sprintf(str + ret, " [TT Slot %d Port# %d TTT %d Intr %d] Addr %d State %s",
			tt_info & TT_SLOT, (tt_info & TT_PORT) >> 8,
			GET_TT_THINK_TIME(tt_info), GET_INTR_TARGET(tt_info),
			state & DEV_ADDR_MASK,
			xhci_slot_state_string(GET_SLOT_STATE(state)));

	return str;
}


static inline const char *xhci_portsc_link_state_string(u32 portsc)
{
	switch (portsc & PORT_PLS_MASK) {
	case XDEV_U0:
		return "U0";
	case XDEV_U1:
		return "U1";
	case XDEV_U2:
		return "U2";
	case XDEV_U3:
		return "U3";
	case XDEV_DISABLED:
		return "Disabled";
	case XDEV_RXDETECT:
		return "RxDetect";
	case XDEV_INACTIVE:
		return "Inactive";
	case XDEV_POLLING:
		return "Polling";
	case XDEV_RECOVERY:
		return "Recovery";
	case XDEV_HOT_RESET:
		return "Hot Reset";
	case XDEV_COMP_MODE:
		return "Compliance mode";
	case XDEV_TEST_MODE:
		return "Test mode";
	case XDEV_RESUME:
		return "Resume";
	default:
		break;
	}
	return "Unknown";
}

static inline const char *xhci_decode_portsc(char *str, u32 portsc)
{
	int ret;

	ret = sprintf(str, "0x%08x ", portsc);

	if (portsc == ~(u32)0)
		return str;

	ret += sprintf(str + ret, "%s %s %s Link:%s PortSpeed:%d ",
		      portsc & PORT_POWER	? "Powered" : "Powered-off",
		      portsc & PORT_CONNECT	? "Connected" : "Not-connected",
		      portsc & PORT_PE		? "Enabled" : "Disabled",
		      xhci_portsc_link_state_string(portsc),
		      DEV_PORT_SPEED(portsc));

	if (portsc & PORT_OC)
		ret += sprintf(str + ret, "OverCurrent ");
	if (portsc & PORT_RESET)
		ret += sprintf(str + ret, "In-Reset ");

	ret += sprintf(str + ret, "Change: ");
	if (portsc & PORT_CSC)
		ret += sprintf(str + ret, "CSC ");
	if (portsc & PORT_PEC)
		ret += sprintf(str + ret, "PEC ");
	if (portsc & PORT_WRC)
		ret += sprintf(str + ret, "WRC ");
	if (portsc & PORT_OCC)
		ret += sprintf(str + ret, "OCC ");
	if (portsc & PORT_RC)
		ret += sprintf(str + ret, "PRC ");
	if (portsc & PORT_PLC)
		ret += sprintf(str + ret, "PLC ");
	if (portsc & PORT_CEC)
		ret += sprintf(str + ret, "CEC ");
	if (portsc & PORT_CAS)
		ret += sprintf(str + ret, "CAS ");

	ret += sprintf(str + ret, "Wake: ");
	if (portsc & PORT_WKCONN_E)
		ret += sprintf(str + ret, "WCE ");
	if (portsc & PORT_WKDISC_E)
		ret += sprintf(str + ret, "WDE ");
	if (portsc & PORT_WKOC_E)
		ret += sprintf(str + ret, "WOE ");

	return str;
}

static inline const char *xhci_decode_usbsts(char *str, u32 usbsts)
{
	int ret = 0;

	ret = sprintf(str, " 0x%08x", usbsts);

	if (usbsts == ~(u32)0)
		return str;

	if (usbsts & STS_HALT)
		ret += sprintf(str + ret, " HCHalted");
	if (usbsts & STS_FATAL)
		ret += sprintf(str + ret, " HSE");
	if (usbsts & STS_EINT)
		ret += sprintf(str + ret, " EINT");
	if (usbsts & STS_PORT)
		ret += sprintf(str + ret, " PCD");
	if (usbsts & STS_SAVE)
		ret += sprintf(str + ret, " SSS");
	if (usbsts & STS_RESTORE)
		ret += sprintf(str + ret, " RSS");
	if (usbsts & STS_SRE)
		ret += sprintf(str + ret, " SRE");
	if (usbsts & STS_CNR)
		ret += sprintf(str + ret, " CNR");
	if (usbsts & STS_HCE)
		ret += sprintf(str + ret, " HCE");

	return str;
}

static inline const char *xhci_decode_doorbell(char *str, u32 slot, u32 doorbell)
{
	u8 ep;
	u16 stream;
	int ret;

	ep = (doorbell & 0xff);
	stream = doorbell >> 16;

	if (slot == 0) {
		sprintf(str, "Command Ring %d", doorbell);
		return str;
	}
	ret = sprintf(str, "Slot %d ", slot);
	if (ep > 0 && ep < 32)
		ret = sprintf(str + ret, "ep%d%s",
			      ep / 2,
			      ep % 2 ? "in" : "out");
	else if (ep == 0 || ep < 248)
		ret = sprintf(str + ret, "Reserved %d", ep);
	else
		ret = sprintf(str + ret, "Vendor Defined %d", ep);
	if (stream)
		ret = sprintf(str + ret, " Stream %d", stream);

	return str;
}

static inline const char *xhci_ep_state_string(u8 state)
{
	switch (state) {
	case EP_STATE_DISABLED:
		return "disabled";
	case EP_STATE_RUNNING:
		return "running";
	case EP_STATE_HALTED:
		return "halted";
	case EP_STATE_STOPPED:
		return "stopped";
	case EP_STATE_ERROR:
		return "error";
	default:
		return "INVALID";
	}
}

static inline const char *xhci_ep_type_string(u8 type)
{
	switch (type) {
	case ISOC_OUT_EP:
		return "Isoc OUT";
	case BULK_OUT_EP:
		return "Bulk OUT";
	case INT_OUT_EP:
		return "Int OUT";
	case CTRL_EP:
		return "Ctrl";
	case ISOC_IN_EP:
		return "Isoc IN";
	case BULK_IN_EP:
		return "Bulk IN";
	case INT_IN_EP:
		return "Int IN";
	default:
		return "INVALID";
	}
}

static inline const char *xhci_decode_ep_context(char *str, u32 info,
		u32 info2, u64 deq, u32 tx_info)
{
	int ret;

	u32 esit;
	u16 maxp;
	u16 avg;

	u8 max_pstr;
	u8 ep_state;
	u8 interval;
	u8 ep_type;
	u8 burst;
	u8 cerr;
	u8 mult;

	bool lsa;
	bool hid;

	esit = CTX_TO_MAX_ESIT_PAYLOAD_HI(info) << 16 |
		CTX_TO_MAX_ESIT_PAYLOAD(tx_info);

	ep_state = info & EP_STATE_MASK;
	max_pstr = CTX_TO_EP_MAXPSTREAMS(info);
	interval = CTX_TO_EP_INTERVAL(info);
	mult = CTX_TO_EP_MULT(info) + 1;
	lsa = !!(info & EP_HAS_LSA);

	cerr = (info2 & (3 << 1)) >> 1;
	ep_type = CTX_TO_EP_TYPE(info2);
	hid = !!(info2 & (1 << 7));
	burst = CTX_TO_MAX_BURST(info2);
	maxp = MAX_PACKET_DECODED(info2);

	avg = EP_AVG_TRB_LENGTH(tx_info);

	ret = sprintf(str, "State %s mult %d max P. Streams %d %s",
			xhci_ep_state_string(ep_state), mult,
			max_pstr, lsa ? "LSA " : "");

	ret += sprintf(str + ret, "interval %d us max ESIT payload %d CErr %d ",
			(1 << interval) * 125, esit, cerr);

	ret += sprintf(str + ret, "Type %s %sburst %d maxp %d deq %016llx ",
			xhci_ep_type_string(ep_type), hid ? "HID" : "",
			burst, maxp, deq);

	ret += sprintf(str + ret, "avg trb len %d", avg);

	return str;
}

#endif /* __LINUX_XHCI_HCD_H */

//=====================================================================
// C code below
//=====================================================================


// SPDX-License-Identifier: GPL-2.0
/*
 * xHCI host controller driver
 *
 * Copyright (C) 2008 Intel Corp.
 *
 * Author: Sarah Sharp
 * Some code borrowed from the Linux EHCI driver.
 */

#include <linux/jiffies.h>
#include <linux/pci.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/string_choices.h>
#include <linux/dmi.h>
#include <linux/dma-mapping.h>
#include <linux/usb/xhci-sideband.h>

#include "xhci.h"
#include "xhci-trace.h"
#include "xhci-debugfs.h"
#include "xhci-dbgcap.h"

#define DRIVER_AUTHOR "Sarah Sharp"
#define DRIVER_DESC "'eXtensible' Host Controller (xHC) Driver"

#define	PORT_WAKE_BITS	(PORT_WKOC_E | PORT_WKDISC_E | PORT_WKCONN_E)

/* Some 0.95 hardware can't handle the chain bit on a Link TRB being cleared */
static int link_quirk;
module_param(link_quirk, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(link_quirk, "Don't clear the chain bit on a link TRB");

static unsigned long long quirks;
module_param(quirks, ullong, S_IRUGO);
MODULE_PARM_DESC(quirks, "Bit flags for quirks to be enabled as default");

static bool td_on_ring(struct xhci_td *td, struct xhci_ring *ring)
{
	struct xhci_segment *seg;

	if (!td || !td->start_seg)
		return false;

	xhci_for_each_ring_seg(ring->first_seg, seg) {
		if (seg == td->start_seg)
			return true;
	}

	return false;
}

/*
 * xhci_handshake - spin reading hc until handshake completes or fails
 * @ptr: address of hc register to be read
 * @mask: bits to look at in result of read
 * @done: value of those bits when handshake succeeds
 * @usec: timeout in microseconds
 *
 * Returns negative errno, or zero on success
 *
 * Success happens when the "mask" bits have the specified value (hardware
 * handshake done).  There are two failure modes:  "usec" have passed (major
 * hardware flakeout), or the register reads as all-ones (hardware removed).
 */
int xhci_handshake(void __iomem *ptr, u32 mask, u32 done, u64 timeout_us)
{
	u32	result;
	int	ret;

	ret = readl_poll_timeout_atomic(ptr, result,
					(result & mask) == done ||
					result == U32_MAX,
					1, timeout_us);
	if (result == U32_MAX)		/* card removed */
		return -ENODEV;

	return ret;
}

/*
 * Disable interrupts and begin the xHCI halting process.
 */
void xhci_quiesce(struct xhci_hcd *xhci)
{
	u32 halted;
	u32 cmd;
	u32 mask;

	mask = ~(XHCI_IRQS);
	halted = readl(&xhci->op_regs->status) & STS_HALT;
	if (!halted)
		mask &= ~CMD_RUN;

	cmd = readl(&xhci->op_regs->command);
	cmd &= mask;
	writel(cmd, &xhci->op_regs->command);
}

/*
 * Force HC into halt state.
 *
 * Disable any IRQs and clear the run/stop bit.
 * HC will complete any current and actively pipelined transactions, and
 * should halt within 16 ms of the run/stop bit being cleared.
 * Read HC Halted bit in the status register to see when the HC is finished.
 */
int xhci_halt(struct xhci_hcd *xhci)
{
	int ret;

	xhci_dbg_trace(xhci, trace_xhci_dbg_init, "// Halt the HC");
	xhci_quiesce(xhci);

	ret = xhci_handshake(&xhci->op_regs->status,
			STS_HALT, STS_HALT, XHCI_MAX_HALT_USEC);
	if (ret) {
		if (!(xhci->xhc_state & XHCI_STATE_DYING))
			xhci_warn(xhci, "Host halt failed, %d\n", ret);
		return ret;
	}

	xhci->xhc_state |= XHCI_STATE_HALTED;
	xhci->cmd_ring_state = CMD_RING_STATE_STOPPED;

	return ret;
}

/*
 * Set the run bit and wait for the host to be running.
 */
int xhci_start(struct xhci_hcd *xhci)
{
	u32 temp;
	int ret;

	temp = readl(&xhci->op_regs->command);
	temp |= (CMD_RUN);
	xhci_dbg_trace(xhci, trace_xhci_dbg_init, "// Turn on HC, cmd = 0x%x.",
			temp);
	writel(temp, &xhci->op_regs->command);

	/*
	 * Wait for the HCHalted Status bit to be 0 to indicate the host is
	 * running.
	 */
	ret = xhci_handshake(&xhci->op_regs->status,
			STS_HALT, 0, XHCI_MAX_HALT_USEC);
	if (ret == -ETIMEDOUT)
		xhci_err(xhci, "Host took too long to start, "
				"waited %u microseconds.\n",
				XHCI_MAX_HALT_USEC);
	if (!ret) {
		/* clear state flags. Including dying, halted or removing */
		xhci->xhc_state = 0;
		xhci->run_graceperiod = jiffies + msecs_to_jiffies(500);
	}

	return ret;
}

/*
 * Reset a halted HC.
 *
 * This resets pipelines, timers, counters, state machines, etc.
 * Transactions will be terminated immediately, and operational registers
 * will be set to their defaults.
 */
int xhci_reset(struct xhci_hcd *xhci, u64 timeout_us)
{
	u32 command;
	u32 state;
	int ret;

	state = readl(&xhci->op_regs->status);

	if (state == ~(u32)0) {
		if (!(xhci->xhc_state & XHCI_STATE_DYING))
			xhci_warn(xhci, "Host not accessible, reset failed.\n");
		return -ENODEV;
	}

	if ((state & STS_HALT) == 0) {
		xhci_warn(xhci, "Host controller not halted, aborting reset.\n");
		return 0;
	}

	xhci_dbg_trace(xhci, trace_xhci_dbg_init, "// Reset the HC");
	command = readl(&xhci->op_regs->command);
	command |= CMD_RESET;
	writel(command, &xhci->op_regs->command);

	/* Existing Intel xHCI controllers require a delay of 1 mS,
	 * after setting the CMD_RESET bit, and before accessing any
	 * HC registers. This allows the HC to complete the
	 * reset operation and be ready for HC register access.
	 * Without this delay, the subsequent HC register access,
	 * may result in a system hang very rarely.
	 */
	if (xhci->quirks & XHCI_INTEL_HOST)
		udelay(1000);

	ret = xhci_handshake(&xhci->op_regs->command, CMD_RESET, 0, timeout_us);
	if (ret)
		return ret;

	if (xhci->quirks & XHCI_ASMEDIA_MODIFY_FLOWCONTROL)
		usb_asmedia_modifyflowcontrol(to_pci_dev(xhci_to_hcd(xhci)->self.controller));

	xhci_dbg_trace(xhci, trace_xhci_dbg_init,
			 "Wait for controller to be ready for doorbell rings");
	/*
	 * xHCI cannot write to any doorbells or operational registers other
	 * than status until the "Controller Not Ready" flag is cleared.
	 */
	ret = xhci_handshake(&xhci->op_regs->status, STS_CNR, 0, timeout_us);

	xhci->usb2_rhub.bus_state.port_c_suspend = 0;
	xhci->usb2_rhub.bus_state.suspended_ports = 0;
	xhci->usb2_rhub.bus_state.resuming_ports = 0;
	xhci->usb3_rhub.bus_state.port_c_suspend = 0;
	xhci->usb3_rhub.bus_state.suspended_ports = 0;
	xhci->usb3_rhub.bus_state.resuming_ports = 0;

	return ret;
}

static void xhci_zero_64b_regs(struct xhci_hcd *xhci)
{
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;
	struct iommu_domain *domain;
	int err, i;
	u64 val;
	u32 intrs;

	/*
	 * Some Renesas controllers get into a weird state if they are
	 * reset while programmed with 64bit addresses (they will preserve
	 * the top half of the address in internal, non visible
	 * registers). You end up with half the address coming from the
	 * kernel, and the other half coming from the firmware. Also,
	 * changing the programming leads to extra accesses even if the
	 * controller is supposed to be halted. The controller ends up with
	 * a fatal fault, and is then ripe for being properly reset.
	 *
	 * Special care is taken to only apply this if the device is behind
	 * an iommu. Doing anything when there is no iommu is definitely
	 * unsafe...
	 */
	domain = iommu_get_domain_for_dev(dev);
	if (!(xhci->quirks & XHCI_ZERO_64B_REGS) || !domain ||
	    domain->type == IOMMU_DOMAIN_IDENTITY)
		return;

	xhci_info(xhci, "Zeroing 64bit base registers, expecting fault\n");

	/* Clear HSEIE so that faults do not get signaled */
	val = readl(&xhci->op_regs->command);
	val &= ~CMD_HSEIE;
	writel(val, &xhci->op_regs->command);

	/* Clear HSE (aka FATAL) */
	val = readl(&xhci->op_regs->status);
	val |= STS_FATAL;
	writel(val, &xhci->op_regs->status);

	/* Now zero the registers, and brace for impact */
	val = xhci_read_64(xhci, &xhci->op_regs->dcbaa_ptr);
	if (upper_32_bits(val))
		xhci_write_64(xhci, 0, &xhci->op_regs->dcbaa_ptr);
	val = xhci_read_64(xhci, &xhci->op_regs->cmd_ring);
	if (upper_32_bits(val))
		xhci_write_64(xhci, 0, &xhci->op_regs->cmd_ring);

	intrs = min_t(u32, HCS_MAX_INTRS(xhci->hcs_params1),
		      ARRAY_SIZE(xhci->run_regs->ir_set));

	for (i = 0; i < intrs; i++) {
		struct xhci_intr_reg __iomem *ir;

		ir = &xhci->run_regs->ir_set[i];
		val = xhci_read_64(xhci, &ir->erst_base);
		if (upper_32_bits(val))
			xhci_write_64(xhci, 0, &ir->erst_base);
		val= xhci_read_64(xhci, &ir->erst_dequeue);
		if (upper_32_bits(val))
			xhci_write_64(xhci, 0, &ir->erst_dequeue);
	}

	/* Wait for the fault to appear. It will be cleared on reset */
	err = xhci_handshake(&xhci->op_regs->status,
			     STS_FATAL, STS_FATAL,
			     XHCI_MAX_HALT_USEC);
	if (!err)
		xhci_info(xhci, "Fault detected\n");
}

int xhci_enable_interrupter(struct xhci_interrupter *ir)
{
	u32 iman;

	if (!ir || !ir->ir_set)
		return -EINVAL;

	iman = readl(&ir->ir_set->iman);
	iman &= ~IMAN_IP;
	iman |= IMAN_IE;
	writel(iman, &ir->ir_set->iman);

	/* Read operation to guarantee the write has been flushed from posted buffers */
	readl(&ir->ir_set->iman);
	return 0;
}

int xhci_disable_interrupter(struct xhci_hcd *xhci, struct xhci_interrupter *ir)
{
	u32 iman;

	if (!ir || !ir->ir_set)
		return -EINVAL;

	iman = readl(&ir->ir_set->iman);
	iman &= ~IMAN_IP;
	iman &= ~IMAN_IE;
	writel(iman, &ir->ir_set->iman);

	iman = readl(&ir->ir_set->iman);
	if (iman & IMAN_IP)
		xhci_dbg(xhci, "%s: Interrupt pending\n", __func__);

	return 0;
}

/* interrupt moderation interval imod_interval in nanoseconds */
int xhci_set_interrupter_moderation(struct xhci_interrupter *ir,
				    u32 imod_interval)
{
	u32 imod;

	if (!ir || !ir->ir_set)
		return -EINVAL;

	/* IMODI value in IMOD register is in 250ns increments */
	imod_interval = umin(imod_interval / 250, IMODI_MASK);

	imod = readl(&ir->ir_set->imod);
	imod &= ~IMODI_MASK;
	imod |= imod_interval;
	writel(imod, &ir->ir_set->imod);

	return 0;
}

static void compliance_mode_recovery(struct timer_list *t)
{
	struct xhci_hcd *xhci;
	struct usb_hcd *hcd;
	struct xhci_hub *rhub;
	u32 temp;
	int i;

	xhci = timer_container_of(xhci, t, comp_mode_recovery_timer);
	rhub = &xhci->usb3_rhub;
	hcd = rhub->hcd;

	if (!hcd)
		return;

	for (i = 0; i < rhub->num_ports; i++) {
		temp = readl(rhub->ports[i]->addr);
		if ((temp & PORT_PLS_MASK) == USB_SS_PORT_LS_COMP_MOD) {
			/*
			 * Compliance Mode Detected. Letting USB Core
			 * handle the Warm Reset
			 */
			xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
					"Compliance mode detected->port %d",
					i + 1);
			xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
					"Attempting compliance mode recovery");

			if (hcd->state == HC_STATE_SUSPENDED)
				usb_hcd_resume_root_hub(hcd);

			usb_hcd_poll_rh_status(hcd);
		}
	}

	if (xhci->port_status_u0 != ((1 << rhub->num_ports) - 1))
		mod_timer(&xhci->comp_mode_recovery_timer,
			jiffies + msecs_to_jiffies(COMP_MODE_RCVRY_MSECS));
}

/*
 * Quirk to work around issue generated by the SN65LVPE502CP USB3.0 re-driver
 * that causes ports behind that hardware to enter compliance mode sometimes.
 * The quirk creates a timer that polls every 2 seconds the link state of
 * each host controller's port and recovers it by issuing a Warm reset
 * if Compliance mode is detected, otherwise the port will become "dead" (no
 * device connections or disconnections will be detected anymore). Becasue no
 * status event is generated when entering compliance mode (per xhci spec),
 * this quirk is needed on systems that have the failing hardware installed.
 */
static void compliance_mode_recovery_timer_init(struct xhci_hcd *xhci)
{
	xhci->port_status_u0 = 0;
	timer_setup(&xhci->comp_mode_recovery_timer, compliance_mode_recovery,
		    0);
	xhci->comp_mode_recovery_timer.expires = jiffies +
			msecs_to_jiffies(COMP_MODE_RCVRY_MSECS);

	add_timer(&xhci->comp_mode_recovery_timer);
	xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
			"Compliance mode recovery timer initialized");
}

/*
 * This function identifies the systems that have installed the SN65LVPE502CP
 * USB3.0 re-driver and that need the Compliance Mode Quirk.
 * Systems:
 * Vendor: Hewlett-Packard -> System Models: Z420, Z620 and Z820
 */
static bool xhci_compliance_mode_recovery_timer_quirk_check(void)
{
	const char *dmi_product_name, *dmi_sys_vendor;

	dmi_product_name = dmi_get_system_info(DMI_PRODUCT_NAME);
	dmi_sys_vendor = dmi_get_system_info(DMI_SYS_VENDOR);
	if (!dmi_product_name || !dmi_sys_vendor)
		return false;

	if (!(strstr(dmi_sys_vendor, "Hewlett-Packard")))
		return false;

	if (strstr(dmi_product_name, "Z420") ||
			strstr(dmi_product_name, "Z620") ||
			strstr(dmi_product_name, "Z820") ||
			strstr(dmi_product_name, "Z1 Workstation"))
		return true;

	return false;
}

static int xhci_all_ports_seen_u0(struct xhci_hcd *xhci)
{
	return (xhci->port_status_u0 == ((1 << xhci->usb3_rhub.num_ports) - 1));
}

static void xhci_hcd_page_size(struct xhci_hcd *xhci)
{
	u32 page_size;

	page_size = readl(&xhci->op_regs->page_size) & XHCI_PAGE_SIZE_MASK;
	if (!is_power_of_2(page_size)) {
		xhci_warn(xhci, "Invalid page size register = 0x%x\n", page_size);
		/* Fallback to 4K page size, since that's common */
		page_size = 1;
	}

	xhci->page_size = page_size << 12;
	xhci_dbg_trace(xhci, trace_xhci_dbg_init, "HCD page size set to %iK",
		       xhci->page_size >> 10);
}

static void xhci_enable_max_dev_slots(struct xhci_hcd *xhci)
{
	u32 config_reg;
	u32 max_slots;

	max_slots = HCS_MAX_SLOTS(xhci->hcs_params1);
	xhci_dbg_trace(xhci, trace_xhci_dbg_init, "xHC can handle at most %d device slots",
		       max_slots);

	config_reg = readl(&xhci->op_regs->config_reg);
	config_reg &= ~HCS_SLOTS_MASK;
	config_reg |= max_slots;

	xhci_dbg_trace(xhci, trace_xhci_dbg_init, "Setting Max device slots reg = 0x%x",
		       config_reg);
	writel(config_reg, &xhci->op_regs->config_reg);
}

static void xhci_set_cmd_ring_deq(struct xhci_hcd *xhci)
{
	dma_addr_t deq_dma;
	u64 crcr;

	deq_dma = xhci_trb_virt_to_dma(xhci->cmd_ring->deq_seg, xhci->cmd_ring->dequeue);
	deq_dma &= CMD_RING_PTR_MASK;

	crcr = xhci_read_64(xhci, &xhci->op_regs->cmd_ring);
	crcr &= ~CMD_RING_PTR_MASK;
	crcr |= deq_dma;

	crcr &= ~CMD_RING_CYCLE;
	crcr |= xhci->cmd_ring->cycle_state;

	xhci_dbg_trace(xhci, trace_xhci_dbg_init, "Setting command ring address to 0x%llx", crcr);
	xhci_write_64(xhci, crcr, &xhci->op_regs->cmd_ring);
}

static void xhci_set_doorbell_ptr(struct xhci_hcd *xhci)
{
	u32 offset;

	offset = readl(&xhci->cap_regs->db_off) & DBOFF_MASK;
	xhci->dba = (void __iomem *)xhci->cap_regs + offset;
	xhci_dbg_trace(xhci, trace_xhci_dbg_init,
		       "Doorbell array is located at offset 0x%x from cap regs base addr", offset);
}

/*
 * Enable USB 3.0 device notifications for function remote wake, which is necessary
 * for allowing USB 3.0 devices to do remote wakeup from U3 (device suspend).
 */
static void xhci_set_dev_notifications(struct xhci_hcd *xhci)
{
	u32 dev_notf;

	dev_notf = readl(&xhci->op_regs->dev_notification);
	dev_notf &= ~DEV_NOTE_MASK;
	dev_notf |= DEV_NOTE_FWAKE;
	writel(dev_notf, &xhci->op_regs->dev_notification);
}

/*
 * Initialize memory for HCD and xHC (one-time init).
 *
 * Program the PAGESIZE register, initialize the device context array, create
 * device contexts (?), set up a command ring segment (or two?), create event
 * ring (one for now).
 */
static int xhci_init(struct usb_hcd *hcd)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	int retval;

	xhci_dbg_trace(xhci, trace_xhci_dbg_init, "Starting %s", __func__);
	spin_lock_init(&xhci->lock);

	INIT_LIST_HEAD(&xhci->cmd_list);
	INIT_DELAYED_WORK(&xhci->cmd_timer, xhci_handle_command_timeout);
	init_completion(&xhci->cmd_ring_stop_completion);
	xhci_hcd_page_size(xhci);
	memset(xhci->devs, 0, MAX_HC_SLOTS * sizeof(*xhci->devs));

	retval = xhci_mem_init(xhci, GFP_KERNEL);
	if (retval)
		return retval;

	/* Set the Number of Device Slots Enabled to the maximum supported value */
	xhci_enable_max_dev_slots(xhci);

	/* Set the address in the Command Ring Control register */
	xhci_set_cmd_ring_deq(xhci);

	/* Set Device Context Base Address Array pointer */
	xhci_write_64(xhci, xhci->dcbaa->dma, &xhci->op_regs->dcbaa_ptr);

	/* Set Doorbell array pointer */
	xhci_set_doorbell_ptr(xhci);

	/* Set USB 3.0 device notifications for function remote wake */
	xhci_set_dev_notifications(xhci);

	/* Initialize the Primary interrupter */
	xhci_add_interrupter(xhci, 0);
	xhci->interrupters[0]->isoc_bei_interval = AVOID_BEI_INTERVAL_MAX;

	/* Initializing Compliance Mode Recovery Data If Needed */
	if (xhci_compliance_mode_recovery_timer_quirk_check()) {
		xhci->quirks |= XHCI_COMP_MODE_QUIRK;
		compliance_mode_recovery_timer_init(xhci);
	}

	xhci_dbg_trace(xhci, trace_xhci_dbg_init, "Finished %s", __func__);
	return 0;
}

/*-------------------------------------------------------------------------*/

static int xhci_run_finished(struct xhci_hcd *xhci)
{
	struct xhci_interrupter *ir = xhci->interrupters[0];
	unsigned long	flags;
	u32		temp;

	/*
	 * Enable interrupts before starting the host (xhci 4.2 and 5.5.2).
	 * Protect the short window before host is running with a lock
	 */
	spin_lock_irqsave(&xhci->lock, flags);

	xhci_dbg_trace(xhci, trace_xhci_dbg_init, "Enable interrupts");
	temp = readl(&xhci->op_regs->command);
	temp |= (CMD_EIE);
	writel(temp, &xhci->op_regs->command);

	xhci_dbg_trace(xhci, trace_xhci_dbg_init, "Enable primary interrupter");
	xhci_enable_interrupter(ir);

	if (xhci_start(xhci)) {
		xhci_halt(xhci);
		spin_unlock_irqrestore(&xhci->lock, flags);
		return -ENODEV;
	}

	xhci->cmd_ring_state = CMD_RING_STATE_RUNNING;

	if (xhci->quirks & XHCI_NEC_HOST)
		xhci_ring_cmd_db(xhci);

	spin_unlock_irqrestore(&xhci->lock, flags);

	return 0;
}

/*
 * Start the HC after it was halted.
 *
 * This function is called by the USB core when the HC driver is added.
 * Its opposite is xhci_stop().
 *
 * xhci_init() must be called once before this function can be called.
 * Reset the HC, enable device slot contexts, program DCBAAP, and
 * set command ring pointer and event ring pointer.
 *
 * Setup MSI-X vectors and enable interrupts.
 */
int xhci_run(struct usb_hcd *hcd)
{
	u64 temp_64;
	int ret;
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct xhci_interrupter *ir = xhci->interrupters[0];
	/* Start the xHCI host controller running only after the USB 2.0 roothub
	 * is setup.
	 */

	hcd->uses_new_polling = 1;
	if (hcd->msi_enabled)
		ir->ip_autoclear = true;

	if (!usb_hcd_is_primary_hcd(hcd))
		return xhci_run_finished(xhci);

	xhci_dbg_trace(xhci, trace_xhci_dbg_init, "xhci_run");

	temp_64 = xhci_read_64(xhci, &ir->ir_set->erst_dequeue);
	temp_64 &= ERST_PTR_MASK;
	xhci_dbg_trace(xhci, trace_xhci_dbg_init,
			"ERST deq = 64'h%0lx", (long unsigned int) temp_64);

	xhci_set_interrupter_moderation(ir, xhci->imod_interval);

	if (xhci->quirks & XHCI_NEC_HOST) {
		struct xhci_command *command;

		command = xhci_alloc_command(xhci, false, GFP_KERNEL);
		if (!command)
			return -ENOMEM;

		ret = xhci_queue_vendor_command(xhci, command, 0, 0, 0,
				TRB_TYPE(TRB_NEC_GET_FW));
		if (ret)
			xhci_free_command(xhci, command);
	}
	xhci_dbg_trace(xhci, trace_xhci_dbg_init,
			"Finished %s for main hcd", __func__);

	xhci_create_dbc_dev(xhci);

	xhci_debugfs_init(xhci);

	if (xhci_has_one_roothub(xhci))
		return xhci_run_finished(xhci);

	set_bit(HCD_FLAG_DEFER_RH_REGISTER, &hcd->flags);

	return 0;
}
EXPORT_SYMBOL_GPL(xhci_run);

/*
 * Stop xHCI driver.
 *
 * This function is called by the USB core when the HC driver is removed.
 * Its opposite is xhci_run().
 *
 * Disable device contexts, disable IRQs, and quiesce the HC.
 * Reset the HC, finish any completed transactions, and cleanup memory.
 */
void xhci_stop(struct usb_hcd *hcd)
{
	u32 temp;
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct xhci_interrupter *ir = xhci->interrupters[0];

	mutex_lock(&xhci->mutex);

	/* Only halt host and free memory after both hcds are removed */
	if (!usb_hcd_is_primary_hcd(hcd)) {
		mutex_unlock(&xhci->mutex);
		return;
	}

	xhci_remove_dbc_dev(xhci);

	spin_lock_irq(&xhci->lock);
	xhci->xhc_state |= XHCI_STATE_HALTED;
	xhci->cmd_ring_state = CMD_RING_STATE_STOPPED;
	xhci_halt(xhci);
	xhci_reset(xhci, XHCI_RESET_SHORT_USEC);
	spin_unlock_irq(&xhci->lock);

	/* Deleting Compliance Mode Recovery Timer */
	if ((xhci->quirks & XHCI_COMP_MODE_QUIRK) &&
			(!(xhci_all_ports_seen_u0(xhci)))) {
		timer_delete_sync(&xhci->comp_mode_recovery_timer);
		xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
				"%s: compliance mode recovery timer deleted",
				__func__);
	}

	if (xhci->quirks & XHCI_AMD_PLL_FIX)
		usb_amd_dev_put();

	xhci_dbg_trace(xhci, trace_xhci_dbg_init,
			"// Disabling event ring interrupts");
	temp = readl(&xhci->op_regs->status);
	writel((temp & ~0x1fff) | STS_EINT, &xhci->op_regs->status);
	xhci_disable_interrupter(xhci, ir);

	xhci_dbg_trace(xhci, trace_xhci_dbg_init, "cleaning up memory");
	xhci_mem_cleanup(xhci);
	xhci_debugfs_exit(xhci);
	xhci_dbg_trace(xhci, trace_xhci_dbg_init,
			"xhci_stop completed - status = %x",
			readl(&xhci->op_regs->status));
	mutex_unlock(&xhci->mutex);
}
EXPORT_SYMBOL_GPL(xhci_stop);

/*
 * Shutdown HC (not bus-specific)
 *
 * This is called when the machine is rebooting or halting.  We assume that the
 * machine will be powered off, and the HC's internal state will be reset.
 * Don't bother to free memory.
 *
 * This will only ever be called with the main usb_hcd (the USB3 roothub).
 */
void xhci_shutdown(struct usb_hcd *hcd)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);

	if (xhci->quirks & XHCI_SPURIOUS_REBOOT)
		usb_disable_xhci_ports(to_pci_dev(hcd->self.sysdev));

	/* Don't poll the roothubs after shutdown. */
	xhci_dbg(xhci, "%s: stopping usb%d port polling.\n",
			__func__, hcd->self.busnum);
	clear_bit(HCD_FLAG_POLL_RH, &hcd->flags);
	timer_delete_sync(&hcd->rh_timer);

	if (xhci->shared_hcd) {
		clear_bit(HCD_FLAG_POLL_RH, &xhci->shared_hcd->flags);
		timer_delete_sync(&xhci->shared_hcd->rh_timer);
	}

	spin_lock_irq(&xhci->lock);
	xhci_halt(xhci);

	/*
	 * Workaround for spurious wakeps at shutdown with HSW, and for boot
	 * firmware delay in ADL-P PCH if port are left in U3 at shutdown
	 */
	if (xhci->quirks & XHCI_SPURIOUS_WAKEUP ||
	    xhci->quirks & XHCI_RESET_TO_DEFAULT)
		xhci_reset(xhci, XHCI_RESET_SHORT_USEC);

	spin_unlock_irq(&xhci->lock);

	xhci_dbg_trace(xhci, trace_xhci_dbg_init,
			"xhci_shutdown completed - status = %x",
			readl(&xhci->op_regs->status));
}
EXPORT_SYMBOL_GPL(xhci_shutdown);

#ifdef CONFIG_PM
static void xhci_save_registers(struct xhci_hcd *xhci)
{
	struct xhci_interrupter *ir;
	unsigned int i;

	xhci->s3.command = readl(&xhci->op_regs->command);
	xhci->s3.dev_nt = readl(&xhci->op_regs->dev_notification);
	xhci->s3.dcbaa_ptr = xhci_read_64(xhci, &xhci->op_regs->dcbaa_ptr);
	xhci->s3.config_reg = readl(&xhci->op_regs->config_reg);

	/* save both primary and all secondary interrupters */
	/* fixme, shold we lock  to prevent race with remove secondary interrupter? */
	for (i = 0; i < xhci->max_interrupters; i++) {
		ir = xhci->interrupters[i];
		if (!ir)
			continue;

		ir->s3_erst_size = readl(&ir->ir_set->erst_size);
		ir->s3_erst_base = xhci_read_64(xhci, &ir->ir_set->erst_base);
		ir->s3_erst_dequeue = xhci_read_64(xhci, &ir->ir_set->erst_dequeue);
		ir->s3_iman = readl(&ir->ir_set->iman);
		ir->s3_imod = readl(&ir->ir_set->imod);
	}
}

static void xhci_restore_registers(struct xhci_hcd *xhci)
{
	struct xhci_interrupter *ir;
	unsigned int i;

	writel(xhci->s3.command, &xhci->op_regs->command);
	writel(xhci->s3.dev_nt, &xhci->op_regs->dev_notification);
	xhci_write_64(xhci, xhci->s3.dcbaa_ptr, &xhci->op_regs->dcbaa_ptr);
	writel(xhci->s3.config_reg, &xhci->op_regs->config_reg);

	/* FIXME should we lock to protect against freeing of interrupters */
	for (i = 0; i < xhci->max_interrupters; i++) {
		ir = xhci->interrupters[i];
		if (!ir)
			continue;

		writel(ir->s3_erst_size, &ir->ir_set->erst_size);
		xhci_write_64(xhci, ir->s3_erst_base, &ir->ir_set->erst_base);
		xhci_write_64(xhci, ir->s3_erst_dequeue, &ir->ir_set->erst_dequeue);
		writel(ir->s3_iman, &ir->ir_set->iman);
		writel(ir->s3_imod, &ir->ir_set->imod);
	}
}

/*
 * The whole command ring must be cleared to zero when we suspend the host.
 *
 * The host doesn't save the command ring pointer in the suspend well, so we
 * need to re-program it on resume.  Unfortunately, the pointer must be 64-byte
 * aligned, because of the reserved bits in the command ring dequeue pointer
 * register.  Therefore, we can't just set the dequeue pointer back in the
 * middle of the ring (TRBs are 16-byte aligned).
 */
static void xhci_clear_command_ring(struct xhci_hcd *xhci)
{
	struct xhci_ring *ring;
	struct xhci_segment *seg;

	ring = xhci->cmd_ring;
	xhci_for_each_ring_seg(ring->first_seg, seg) {
		/* erase all TRBs before the link */
		memset(seg->trbs, 0, sizeof(union xhci_trb) * (TRBS_PER_SEGMENT - 1));
		/* clear link cycle bit */
		seg->trbs[TRBS_PER_SEGMENT - 1].link.control &= cpu_to_le32(~TRB_CYCLE);
	}

	xhci_initialize_ring_info(ring);
	/*
	 * Reset the hardware dequeue pointer.
	 * Yes, this will need to be re-written after resume, but we're paranoid
	 * and want to make sure the hardware doesn't access bogus memory
	 * because, say, the BIOS or an SMI started the host without changing
	 * the command ring pointers.
	 */
	xhci_set_cmd_ring_deq(xhci);
}

/*
 * Disable port wake bits if do_wakeup is not set.
 *
 * Also clear a possible internal port wake state left hanging for ports that
 * detected termination but never successfully enumerated (trained to 0U).
 * Internal wake causes immediate xHCI wake after suspend. PORT_CSC write done
 * at enumeration clears this wake, force one here as well for unconnected ports
 */

static void xhci_disable_hub_port_wake(struct xhci_hcd *xhci,
				       struct xhci_hub *rhub,
				       bool do_wakeup)
{
	unsigned long flags;
	u32 t1, t2, portsc;
	int i;

	spin_lock_irqsave(&xhci->lock, flags);

	for (i = 0; i < rhub->num_ports; i++) {
		portsc = readl(rhub->ports[i]->addr);
		t1 = xhci_port_state_to_neutral(portsc);
		t2 = t1;

		/* clear wake bits if do_wake is not set */
		if (!do_wakeup)
			t2 &= ~PORT_WAKE_BITS;

		/* Don't touch csc bit if connected or connect change is set */
		if (!(portsc & (PORT_CSC | PORT_CONNECT)))
			t2 |= PORT_CSC;

		if (t1 != t2) {
			writel(t2, rhub->ports[i]->addr);
			xhci_dbg(xhci, "config port %d-%d wake bits, portsc: 0x%x, write: 0x%x\n",
				 rhub->hcd->self.busnum, i + 1, portsc, t2);
		}
	}
	spin_unlock_irqrestore(&xhci->lock, flags);
}

static bool xhci_pending_portevent(struct xhci_hcd *xhci)
{
	struct xhci_port	**ports;
	int			port_index;
	u32			status;
	u32			portsc;

	status = readl(&xhci->op_regs->status);
	if (status & STS_EINT)
		return true;
	/*
	 * Checking STS_EINT is not enough as there is a lag between a change
	 * bit being set and the Port Status Change Event that it generated
	 * being written to the Event Ring. See note in xhci 1.1 section 4.19.2.
	 */

	port_index = xhci->usb2_rhub.num_ports;
	ports = xhci->usb2_rhub.ports;
	while (port_index--) {
		portsc = readl(ports[port_index]->addr);
		if (portsc & PORT_CHANGE_MASK ||
		    (portsc & PORT_PLS_MASK) == XDEV_RESUME)
			return true;
	}
	port_index = xhci->usb3_rhub.num_ports;
	ports = xhci->usb3_rhub.ports;
	while (port_index--) {
		portsc = readl(ports[port_index]->addr);
		if (portsc & (PORT_CHANGE_MASK | PORT_CAS) ||
		    (portsc & PORT_PLS_MASK) == XDEV_RESUME)
			return true;
	}
	return false;
}

/*
 * Stop HC (not bus-specific)
 *
 * This is called when the machine transition into S3/S4 mode.
 *
 */
int xhci_suspend(struct xhci_hcd *xhci, bool do_wakeup)
{
	int			rc = 0;
	unsigned int		delay = XHCI_MAX_HALT_USEC * 2;
	struct usb_hcd		*hcd = xhci_to_hcd(xhci);
	u32			command;
	u32			res;

	if (!hcd->state)
		return 0;

	if (hcd->state != HC_STATE_SUSPENDED ||
	    (xhci->shared_hcd && xhci->shared_hcd->state != HC_STATE_SUSPENDED))
		return -EINVAL;

	/* Clear root port wake on bits if wakeup not allowed. */
	xhci_disable_hub_port_wake(xhci, &xhci->usb3_rhub, do_wakeup);
	xhci_disable_hub_port_wake(xhci, &xhci->usb2_rhub, do_wakeup);

	if (!HCD_HW_ACCESSIBLE(hcd))
		return 0;

	xhci_dbc_suspend(xhci);

	/* Don't poll the roothubs on bus suspend. */
	xhci_dbg(xhci, "%s: stopping usb%d port polling.\n",
		 __func__, hcd->self.busnum);
	clear_bit(HCD_FLAG_POLL_RH, &hcd->flags);
	timer_delete_sync(&hcd->rh_timer);
	if (xhci->shared_hcd) {
		clear_bit(HCD_FLAG_POLL_RH, &xhci->shared_hcd->flags);
		timer_delete_sync(&xhci->shared_hcd->rh_timer);
	}

	if (xhci->quirks & XHCI_SUSPEND_DELAY)
		usleep_range(1000, 1500);

	spin_lock_irq(&xhci->lock);
	clear_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);
	if (xhci->shared_hcd)
		clear_bit(HCD_FLAG_HW_ACCESSIBLE, &xhci->shared_hcd->flags);
	/* step 1: stop endpoint */
	/* skipped assuming that port suspend has done */

	/* step 2: clear Run/Stop bit */
	command = readl(&xhci->op_regs->command);
	command &= ~CMD_RUN;
	writel(command, &xhci->op_regs->command);

	/* Some chips from Fresco Logic need an extraordinary delay */
	delay *= (xhci->quirks & XHCI_SLOW_SUSPEND) ? 10 : 1;

	if (xhci_handshake(&xhci->op_regs->status,
		      STS_HALT, STS_HALT, delay)) {
		xhci_warn(xhci, "WARN: xHC CMD_RUN timeout\n");
		spin_unlock_irq(&xhci->lock);
		return -ETIMEDOUT;
	}
	xhci_clear_command_ring(xhci);

	/* step 3: save registers */
	xhci_save_registers(xhci);

	/* step 4: set CSS flag */
	command = readl(&xhci->op_regs->command);
	command |= CMD_CSS;
	writel(command, &xhci->op_regs->command);
	xhci->broken_suspend = 0;
	if (xhci_handshake(&xhci->op_regs->status,
				STS_SAVE, 0, 20 * 1000)) {
	/*
	 * AMD SNPS xHC 3.0 occasionally does not clear the
	 * SSS bit of USBSTS and when driver tries to poll
	 * to see if the xHC clears BIT(8) which never happens
	 * and driver assumes that controller is not responding
	 * and times out. To workaround this, its good to check
	 * if SRE and HCE bits are not set (as per xhci
	 * Section 5.4.2) and bypass the timeout.
	 */
		res = readl(&xhci->op_regs->status);
		if ((xhci->quirks & XHCI_SNPS_BROKEN_SUSPEND) &&
		    (((res & STS_SRE) == 0) &&
				((res & STS_HCE) == 0))) {
			xhci->broken_suspend = 1;
		} else {
			xhci_warn(xhci, "WARN: xHC save state timeout\n");
			spin_unlock_irq(&xhci->lock);
			return -ETIMEDOUT;
		}
	}
	spin_unlock_irq(&xhci->lock);

	/*
	 * Deleting Compliance Mode Recovery Timer because the xHCI Host
	 * is about to be suspended.
	 */
	if ((xhci->quirks & XHCI_COMP_MODE_QUIRK) &&
			(!(xhci_all_ports_seen_u0(xhci)))) {
		timer_delete_sync(&xhci->comp_mode_recovery_timer);
		xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
				"%s: compliance mode recovery timer deleted",
				__func__);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(xhci_suspend);

/*
 * start xHC (not bus-specific)
 *
 * This is called when the machine transition from S3/S4 mode.
 *
 */
int xhci_resume(struct xhci_hcd *xhci, bool power_lost, bool is_auto_resume)
{
	u32			command, temp = 0;
	struct usb_hcd		*hcd = xhci_to_hcd(xhci);
	int			retval = 0;
	bool			comp_timer_running = false;
	bool			pending_portevent = false;
	bool			suspended_usb3_devs = false;

	if (!hcd->state)
		return 0;

	/* Wait a bit if either of the roothubs need to settle from the
	 * transition into bus suspend.
	 */

	if (time_before(jiffies, xhci->usb2_rhub.bus_state.next_statechange) ||
	    time_before(jiffies, xhci->usb3_rhub.bus_state.next_statechange))
		msleep(100);

	set_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);
	if (xhci->shared_hcd)
		set_bit(HCD_FLAG_HW_ACCESSIBLE, &xhci->shared_hcd->flags);

	spin_lock_irq(&xhci->lock);

	if (xhci->quirks & XHCI_RESET_ON_RESUME || xhci->broken_suspend)
		power_lost = true;

	if (!power_lost) {
		/*
		 * Some controllers might lose power during suspend, so wait
		 * for controller not ready bit to clear, just as in xHC init.
		 */
		retval = xhci_handshake(&xhci->op_regs->status,
					STS_CNR, 0, 10 * 1000 * 1000);
		if (retval) {
			xhci_warn(xhci, "Controller not ready at resume %d\n",
				  retval);
			spin_unlock_irq(&xhci->lock);
			return retval;
		}
		/* step 1: restore register */
		xhci_restore_registers(xhci);
		/* step 2: initialize command ring buffer */
		xhci_set_cmd_ring_deq(xhci);
		/* step 3: restore state and start state*/
		/* step 3: set CRS flag */
		command = readl(&xhci->op_regs->command);
		command |= CMD_CRS;
		writel(command, &xhci->op_regs->command);
		/*
		 * Some controllers take up to 55+ ms to complete the controller
		 * restore so setting the timeout to 100ms. Xhci specification
		 * doesn't mention any timeout value.
		 */
		if (xhci_handshake(&xhci->op_regs->status,
			      STS_RESTORE, 0, 100 * 1000)) {
			xhci_warn(xhci, "WARN: xHC restore state timeout\n");
			spin_unlock_irq(&xhci->lock);
			return -ETIMEDOUT;
		}
	}

	temp = readl(&xhci->op_regs->status);

	/* re-initialize the HC on Restore Error, or Host Controller Error */
	if ((temp & (STS_SRE | STS_HCE)) &&
	    !(xhci->xhc_state & XHCI_STATE_REMOVING)) {
		if (!power_lost)
			xhci_warn(xhci, "xHC error in resume, USBSTS 0x%x, Reinit\n", temp);
		power_lost = true;
	}

	if (power_lost) {
		if ((xhci->quirks & XHCI_COMP_MODE_QUIRK) &&
				!(xhci_all_ports_seen_u0(xhci))) {
			timer_delete_sync(&xhci->comp_mode_recovery_timer);
			xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
				"Compliance Mode Recovery Timer deleted!");
		}

		/* Let the USB core know _both_ roothubs lost power. */
		usb_root_hub_lost_power(xhci->main_hcd->self.root_hub);
		if (xhci->shared_hcd)
			usb_root_hub_lost_power(xhci->shared_hcd->self.root_hub);

		xhci_dbg(xhci, "Stop HCD\n");
		xhci_halt(xhci);
		xhci_zero_64b_regs(xhci);
		if (xhci->xhc_state & XHCI_STATE_REMOVING)
			retval = -ENODEV;
		else
			retval = xhci_reset(xhci, XHCI_RESET_LONG_USEC);
		spin_unlock_irq(&xhci->lock);
		if (retval)
			return retval;

		xhci_dbg(xhci, "// Disabling event ring interrupts\n");
		temp = readl(&xhci->op_regs->status);
		writel((temp & ~0x1fff) | STS_EINT, &xhci->op_regs->status);
		xhci_disable_interrupter(xhci, xhci->interrupters[0]);

		xhci_dbg(xhci, "cleaning up memory\n");
		xhci_mem_cleanup(xhci);
		xhci_debugfs_exit(xhci);
		xhci_dbg(xhci, "xhci_stop completed - status = %x\n",
			    readl(&xhci->op_regs->status));

		/* USB core calls the PCI reinit and start functions twice:
		 * first with the primary HCD, and then with the secondary HCD.
		 * If we don't do the same, the host will never be started.
		 */
		xhci_dbg(xhci, "Initialize the xhci_hcd\n");
		retval = xhci_init(hcd);
		if (retval)
			return retval;
		comp_timer_running = true;

		xhci_dbg(xhci, "Start the primary HCD\n");
		retval = xhci_run(hcd);
		if (!retval && xhci->shared_hcd) {
			xhci_dbg(xhci, "Start the secondary HCD\n");
			retval = xhci_run(xhci->shared_hcd);
		}
		if (retval)
			return retval;
		/*
		 * Resume roothubs unconditionally as PORTSC change bits are not
		 * immediately visible after xHC reset
		 */
		hcd->state = HC_STATE_SUSPENDED;

		if (xhci->shared_hcd) {
			xhci->shared_hcd->state = HC_STATE_SUSPENDED;
			usb_hcd_resume_root_hub(xhci->shared_hcd);
		}
		usb_hcd_resume_root_hub(hcd);

		goto done;
	}

	/* step 4: set Run/Stop bit */
	command = readl(&xhci->op_regs->command);
	command |= CMD_RUN;
	writel(command, &xhci->op_regs->command);
	xhci_handshake(&xhci->op_regs->status, STS_HALT,
		  0, 250 * 1000);

	/* step 5: walk topology and initialize portsc,
	 * portpmsc and portli
	 */
	/* this is done in bus_resume */

	/* step 6: restart each of the previously
	 * Running endpoints by ringing their doorbells
	 */

	spin_unlock_irq(&xhci->lock);

	xhci_dbc_resume(xhci);

	if (retval == 0) {
		/*
		 * Resume roothubs only if there are pending events.
		 * USB 3 devices resend U3 LFPS wake after a 100ms delay if
		 * the first wake signalling failed, give it that chance if
		 * there are suspended USB 3 devices.
		 */
		if (xhci->usb3_rhub.bus_state.suspended_ports ||
		    xhci->usb3_rhub.bus_state.bus_suspended)
			suspended_usb3_devs = true;

		pending_portevent = xhci_pending_portevent(xhci);

		if (suspended_usb3_devs && !pending_portevent && is_auto_resume) {
			msleep(120);
			pending_portevent = xhci_pending_portevent(xhci);
		}

		if (pending_portevent) {
			if (xhci->shared_hcd)
				usb_hcd_resume_root_hub(xhci->shared_hcd);
			usb_hcd_resume_root_hub(hcd);
		}
	}
done:
	/*
	 * If system is subject to the Quirk, Compliance Mode Timer needs to
	 * be re-initialized Always after a system resume. Ports are subject
	 * to suffer the Compliance Mode issue again. It doesn't matter if
	 * ports have entered previously to U0 before system's suspension.
	 */
	if ((xhci->quirks & XHCI_COMP_MODE_QUIRK) && !comp_timer_running)
		compliance_mode_recovery_timer_init(xhci);

	if (xhci->quirks & XHCI_ASMEDIA_MODIFY_FLOWCONTROL)
		usb_asmedia_modifyflowcontrol(to_pci_dev(hcd->self.controller));

	/* Re-enable port polling. */
	xhci_dbg(xhci, "%s: starting usb%d port polling.\n",
		 __func__, hcd->self.busnum);
	if (xhci->shared_hcd) {
		set_bit(HCD_FLAG_POLL_RH, &xhci->shared_hcd->flags);
		usb_hcd_poll_rh_status(xhci->shared_hcd);
	}
	set_bit(HCD_FLAG_POLL_RH, &hcd->flags);
	usb_hcd_poll_rh_status(hcd);

	return retval;
}
EXPORT_SYMBOL_GPL(xhci_resume);
#endif	/* CONFIG_PM */

/*-------------------------------------------------------------------------*/

static int xhci_map_temp_buffer(struct usb_hcd *hcd, struct urb *urb)
{
	void *temp;
	int ret = 0;
	unsigned int buf_len;
	enum dma_data_direction dir;

	dir = usb_urb_dir_in(urb) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	buf_len = urb->transfer_buffer_length;

	temp = kzalloc_node(buf_len, GFP_ATOMIC,
			    dev_to_node(hcd->self.sysdev));
	if (!temp)
		return -ENOMEM;

	if (usb_urb_dir_out(urb))
		sg_pcopy_to_buffer(urb->sg, urb->num_sgs,
				   temp, buf_len, 0);

	urb->transfer_buffer = temp;
	urb->transfer_dma = dma_map_single(hcd->self.sysdev,
					   urb->transfer_buffer,
					   urb->transfer_buffer_length,
					   dir);

	if (dma_mapping_error(hcd->self.sysdev,
			      urb->transfer_dma)) {
		ret = -EAGAIN;
		kfree(temp);
	} else {
		urb->transfer_flags |= URB_DMA_MAP_SINGLE;
	}

	return ret;
}

static bool xhci_urb_temp_buffer_required(struct usb_hcd *hcd,
					  struct urb *urb)
{
	bool ret = false;
	unsigned int i;
	unsigned int len = 0;
	unsigned int trb_size;
	unsigned int max_pkt;
	struct scatterlist *sg;
	struct scatterlist *tail_sg;

	tail_sg = urb->sg;
	max_pkt = xhci_usb_endpoint_maxp(urb->dev, urb->ep);

	if (!urb->num_sgs)
		return ret;

	if (urb->dev->speed >= USB_SPEED_SUPER)
		trb_size = TRB_CACHE_SIZE_SS;
	else
		trb_size = TRB_CACHE_SIZE_HS;

	if (urb->transfer_buffer_length != 0 &&
	    !(urb->transfer_flags & URB_NO_TRANSFER_DMA_MAP)) {
		for_each_sg(urb->sg, sg, urb->num_sgs, i) {
			len = len + sg->length;
			if (i > trb_size - 2) {
				len = len - tail_sg->length;
				if (len < max_pkt) {
					ret = true;
					break;
				}

				tail_sg = sg_next(tail_sg);
			}
		}
	}
	return ret;
}

static void xhci_unmap_temp_buf(struct usb_hcd *hcd, struct urb *urb)
{
	unsigned int len;
	unsigned int buf_len;
	enum dma_data_direction dir;

	dir = usb_urb_dir_in(urb) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;

	buf_len = urb->transfer_buffer_length;

	if (IS_ENABLED(CONFIG_HAS_DMA) &&
	    (urb->transfer_flags & URB_DMA_MAP_SINGLE))
		dma_unmap_single(hcd->self.sysdev,
				 urb->transfer_dma,
				 urb->transfer_buffer_length,
				 dir);

	if (usb_urb_dir_in(urb)) {
		len = sg_pcopy_from_buffer(urb->sg, urb->num_sgs,
					   urb->transfer_buffer,
					   buf_len,
					   0);
		if (len != buf_len) {
			xhci_dbg(hcd_to_xhci(hcd),
				 "Copy from tmp buf to urb sg list failed\n");
			urb->actual_length = len;
		}
	}
	urb->transfer_flags &= ~URB_DMA_MAP_SINGLE;
	kfree(urb->transfer_buffer);
	urb->transfer_buffer = NULL;
}

/*
 * Bypass the DMA mapping if URB is suitable for Immediate Transfer (IDT),
 * we'll copy the actual data into the TRB address register. This is limited to
 * transfers up to 8 bytes on output endpoints of any kind with wMaxPacketSize
 * >= 8 bytes. If suitable for IDT only one Transfer TRB per TD is allowed.
 */
static int xhci_map_urb_for_dma(struct usb_hcd *hcd, struct urb *urb,
				gfp_t mem_flags)
{
	struct xhci_hcd *xhci;

	xhci = hcd_to_xhci(hcd);

	if (xhci_urb_suitable_for_idt(urb))
		return 0;

	if (xhci->quirks & XHCI_SG_TRB_CACHE_SIZE_QUIRK) {
		if (xhci_urb_temp_buffer_required(hcd, urb))
			return xhci_map_temp_buffer(hcd, urb);
	}
	return usb_hcd_map_urb_for_dma(hcd, urb, mem_flags);
}

static void xhci_unmap_urb_for_dma(struct usb_hcd *hcd, struct urb *urb)
{
	struct xhci_hcd *xhci;
	bool unmap_temp_buf = false;

	xhci = hcd_to_xhci(hcd);

	if (urb->num_sgs && (urb->transfer_flags & URB_DMA_MAP_SINGLE))
		unmap_temp_buf = true;

	if ((xhci->quirks & XHCI_SG_TRB_CACHE_SIZE_QUIRK) && unmap_temp_buf)
		xhci_unmap_temp_buf(hcd, urb);
	else
		usb_hcd_unmap_urb_for_dma(hcd, urb);
}

/**
 * xhci_get_endpoint_index - Used for passing endpoint bitmasks between the core and
 * HCDs.  Find the index for an endpoint given its descriptor.  Use the return
 * value to right shift 1 for the bitmask.
 * @desc: USB endpoint descriptor to determine index for
 *
 * Index  = (epnum * 2) + direction - 1,
 * where direction = 0 for OUT, 1 for IN.
 * For control endpoints, the IN index is used (OUT index is unused), so
 * index = (epnum * 2) + direction - 1 = (epnum * 2) + 1 - 1 = (epnum * 2)
 */
unsigned int xhci_get_endpoint_index(struct usb_endpoint_descriptor *desc)
{
	unsigned int index;
	if (usb_endpoint_xfer_control(desc))
		index = (unsigned int) (usb_endpoint_num(desc)*2);
	else
		index = (unsigned int) (usb_endpoint_num(desc)*2) +
			(usb_endpoint_dir_in(desc) ? 1 : 0) - 1;
	return index;
}
EXPORT_SYMBOL_GPL(xhci_get_endpoint_index);

/* The reverse operation to xhci_get_endpoint_index. Calculate the USB endpoint
 * address from the XHCI endpoint index.
 */
static unsigned int xhci_get_endpoint_address(unsigned int ep_index)
{
	unsigned int number = DIV_ROUND_UP(ep_index, 2);
	unsigned int direction = ep_index % 2 ? USB_DIR_OUT : USB_DIR_IN;
	return direction | number;
}

/* Find the flag for this endpoint (for use in the control context).  Use the
 * endpoint index to create a bitmask.  The slot context is bit 0, endpoint 0 is
 * bit 1, etc.
 */
static unsigned int xhci_get_endpoint_flag(struct usb_endpoint_descriptor *desc)
{
	return 1 << (xhci_get_endpoint_index(desc) + 1);
}

/* Compute the last valid endpoint context index.  Basically, this is the
 * endpoint index plus one.  For slot contexts with more than valid endpoint,
 * we find the most significant bit set in the added contexts flags.
 * e.g. ep 1 IN (with epnum 0x81) => added_ctxs = 0b1000
 * fls(0b1000) = 4, but the endpoint context index is 3, so subtract one.
 */
unsigned int xhci_last_valid_endpoint(u32 added_ctxs)
{
	return fls(added_ctxs) - 1;
}

/* Returns 1 if the arguments are OK;
 * returns 0 this is a root hub; returns -EINVAL for NULL pointers.
 */
static int xhci_check_args(struct usb_hcd *hcd, struct usb_device *udev,
		struct usb_host_endpoint *ep, int check_ep, bool check_virt_dev,
		const char *func) {
	struct xhci_hcd	*xhci;
	struct xhci_virt_device	*virt_dev;

	if (!hcd || (check_ep && !ep) || !udev) {
		pr_debug("xHCI %s called with invalid args\n", func);
		return -EINVAL;
	}
	if (!udev->parent) {
		pr_debug("xHCI %s called for root hub\n", func);
		return 0;
	}

	xhci = hcd_to_xhci(hcd);
	if (check_virt_dev) {
		if (!udev->slot_id || !xhci->devs[udev->slot_id]) {
			xhci_dbg(xhci, "xHCI %s called with unaddressed device\n",
					func);
			return -EINVAL;
		}

		virt_dev = xhci->devs[udev->slot_id];
		if (virt_dev->udev != udev) {
			xhci_dbg(xhci, "xHCI %s called with udev and "
					  "virt_dev does not match\n", func);
			return -EINVAL;
		}
	}

	if (xhci->xhc_state & XHCI_STATE_HALTED)
		return -ENODEV;

	return 1;
}

static int xhci_configure_endpoint(struct xhci_hcd *xhci,
		struct usb_device *udev, struct xhci_command *command,
		bool ctx_change, bool must_succeed);

/*
 * Full speed devices may have a max packet size greater than 8 bytes, but the
 * USB core doesn't know that until it reads the first 8 bytes of the
 * descriptor.  If the usb_device's max packet size changes after that point,
 * we need to issue an evaluate context command and wait on it.
 */
static int xhci_check_ep0_maxpacket(struct xhci_hcd *xhci, struct xhci_virt_device *vdev)
{
	struct xhci_input_control_ctx *ctrl_ctx;
	struct xhci_ep_ctx *ep_ctx;
	struct xhci_command *command;
	int max_packet_size;
	int hw_max_packet_size;
	int ret = 0;

	ep_ctx = xhci_get_ep_ctx(xhci, vdev->out_ctx, 0);
	hw_max_packet_size = MAX_PACKET_DECODED(le32_to_cpu(ep_ctx->ep_info2));
	max_packet_size = usb_endpoint_maxp(&vdev->udev->ep0.desc);

	if (hw_max_packet_size == max_packet_size)
		return 0;

	switch (max_packet_size) {
	case 8: case 16: case 32: case 64: case 9:
		xhci_dbg_trace(xhci,  trace_xhci_dbg_context_change,
				"Max Packet Size for ep 0 changed.");
		xhci_dbg_trace(xhci,  trace_xhci_dbg_context_change,
				"Max packet size in usb_device = %d",
				max_packet_size);
		xhci_dbg_trace(xhci,  trace_xhci_dbg_context_change,
				"Max packet size in xHCI HW = %d",
				hw_max_packet_size);
		xhci_dbg_trace(xhci,  trace_xhci_dbg_context_change,
				"Issuing evaluate context command.");

		command = xhci_alloc_command(xhci, true, GFP_KERNEL);
		if (!command)
			return -ENOMEM;

		command->in_ctx = vdev->in_ctx;
		ctrl_ctx = xhci_get_input_control_ctx(command->in_ctx);
		if (!ctrl_ctx) {
			xhci_warn(xhci, "%s: Could not get input context, bad type.\n",
					__func__);
			ret = -ENOMEM;
			break;
		}
		/* Set up the modified control endpoint 0 */
		xhci_endpoint_copy(xhci, vdev->in_ctx, vdev->out_ctx, 0);

		ep_ctx = xhci_get_ep_ctx(xhci, command->in_ctx, 0);
		ep_ctx->ep_info &= cpu_to_le32(~EP_STATE_MASK);/* must clear */
		ep_ctx->ep_info2 &= cpu_to_le32(~MAX_PACKET_MASK);
		ep_ctx->ep_info2 |= cpu_to_le32(MAX_PACKET(max_packet_size));

		ctrl_ctx->add_flags = cpu_to_le32(EP0_FLAG);
		ctrl_ctx->drop_flags = 0;

		ret = xhci_configure_endpoint(xhci, vdev->udev, command,
					      true, false);
		/* Clean up the input context for later use by bandwidth functions */
		ctrl_ctx->add_flags = cpu_to_le32(SLOT_FLAG);
		break;
	default:
		dev_dbg(&vdev->udev->dev, "incorrect max packet size %d for ep0\n",
			max_packet_size);
		return -EINVAL;
	}

	kfree(command->completion);
	kfree(command);

	return ret;
}

/*
 * non-error returns are a promise to giveback() the urb later
 * we drop ownership so next owner (or urb unlink) can get it
 */
static int xhci_urb_enqueue(struct usb_hcd *hcd, struct urb *urb, gfp_t mem_flags)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	unsigned long flags;
	int ret = 0;
	unsigned int slot_id, ep_index;
	unsigned int *ep_state;
	struct urb_priv	*urb_priv;
	int num_tds;

	ep_index = xhci_get_endpoint_index(&urb->ep->desc);

	if (usb_endpoint_xfer_isoc(&urb->ep->desc))
		num_tds = urb->number_of_packets;
	else if (usb_endpoint_is_bulk_out(&urb->ep->desc) &&
	    urb->transfer_buffer_length > 0 &&
	    urb->transfer_flags & URB_ZERO_PACKET &&
	    !(urb->transfer_buffer_length % usb_endpoint_maxp(&urb->ep->desc)))
		num_tds = 2;
	else
		num_tds = 1;

	urb_priv = kzalloc(struct_size(urb_priv, td, num_tds), mem_flags);
	if (!urb_priv)
		return -ENOMEM;

	urb_priv->num_tds = num_tds;
	urb_priv->num_tds_done = 0;
	urb->hcpriv = urb_priv;

	trace_xhci_urb_enqueue(urb);

	spin_lock_irqsave(&xhci->lock, flags);

	ret = xhci_check_args(hcd, urb->dev, urb->ep,
			      true, true, __func__);
	if (ret <= 0) {
		ret = ret ? ret : -EINVAL;
		goto free_priv;
	}

	slot_id = urb->dev->slot_id;

	if (!HCD_HW_ACCESSIBLE(hcd)) {
		ret = -ESHUTDOWN;
		goto free_priv;
	}

	if (xhci->devs[slot_id]->flags & VDEV_PORT_ERROR) {
		xhci_dbg(xhci, "Can't queue urb, port error, link inactive\n");
		ret = -ENODEV;
		goto free_priv;
	}

	if (xhci->xhc_state & XHCI_STATE_DYING) {
		xhci_dbg(xhci, "Ep 0x%x: URB %p submitted for non-responsive xHCI host.\n",
			 urb->ep->desc.bEndpointAddress, urb);
		ret = -ESHUTDOWN;
		goto free_priv;
	}

	ep_state = &xhci->devs[slot_id]->eps[ep_index].ep_state;

	if (*ep_state & (EP_GETTING_STREAMS | EP_GETTING_NO_STREAMS)) {
		xhci_warn(xhci, "WARN: Can't enqueue URB, ep in streams transition state %x\n",
			  *ep_state);
		ret = -EINVAL;
		goto free_priv;
	}
	if (*ep_state & EP_SOFT_CLEAR_TOGGLE) {
		xhci_warn(xhci, "Can't enqueue URB while manually clearing toggle\n");
		ret = -EINVAL;
		goto free_priv;
	}

	switch (usb_endpoint_type(&urb->ep->desc)) {

	case USB_ENDPOINT_XFER_CONTROL:
		ret = xhci_queue_ctrl_tx(xhci, GFP_ATOMIC, urb,
					 slot_id, ep_index);
		break;
	case USB_ENDPOINT_XFER_BULK:
		ret = xhci_queue_bulk_tx(xhci, GFP_ATOMIC, urb,
					 slot_id, ep_index);
		break;
	case USB_ENDPOINT_XFER_INT:
		ret = xhci_queue_intr_tx(xhci, GFP_ATOMIC, urb,
				slot_id, ep_index);
		break;
	case USB_ENDPOINT_XFER_ISOC:
		ret = xhci_queue_isoc_tx_prepare(xhci, GFP_ATOMIC, urb,
				slot_id, ep_index);
	}

	if (ret) {
free_priv:
		xhci_urb_free_priv(urb_priv);
		urb->hcpriv = NULL;
	}
	spin_unlock_irqrestore(&xhci->lock, flags);
	return ret;
}

/*
 * Remove the URB's TD from the endpoint ring.  This may cause the HC to stop
 * USB transfers, potentially stopping in the middle of a TRB buffer.  The HC
 * should pick up where it left off in the TD, unless a Set Transfer Ring
 * Dequeue Pointer is issued.
 *
 * The TRBs that make up the buffers for the canceled URB will be "removed" from
 * the ring.  Since the ring is a contiguous structure, they can't be physically
 * removed.  Instead, there are two options:
 *
 *  1) If the HC is in the middle of processing the URB to be canceled, we
 *     simply move the ring's dequeue pointer past those TRBs using the Set
 *     Transfer Ring Dequeue Pointer command.  This will be the common case,
 *     when drivers timeout on the last submitted URB and attempt to cancel.
 *
 *  2) If the HC is in the middle of a different TD, we turn the TRBs into a
 *     series of 1-TRB transfer no-op TDs.  (No-ops shouldn't be chained.)  The
 *     HC will need to invalidate the any TRBs it has cached after the stop
 *     endpoint command, as noted in the xHCI 0.95 errata.
 *
 *  3) The TD may have completed by the time the Stop Endpoint Command
 *     completes, so software needs to handle that case too.
 *
 * This function should protect against the TD enqueueing code ringing the
 * doorbell while this code is waiting for a Stop Endpoint command to complete.
 * It also needs to account for multiple cancellations on happening at the same
 * time for the same endpoint.
 *
 * Note that this function can be called in any context, or so says
 * usb_hcd_unlink_urb()
 */
static int xhci_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
{
	unsigned long flags;
	int ret, i;
	u32 temp;
	struct xhci_hcd *xhci;
	struct urb_priv	*urb_priv;
	struct xhci_td *td;
	unsigned int ep_index;
	struct xhci_ring *ep_ring;
	struct xhci_virt_ep *ep;
	struct xhci_command *command;
	struct xhci_virt_device *vdev;

	xhci = hcd_to_xhci(hcd);
	spin_lock_irqsave(&xhci->lock, flags);

	trace_xhci_urb_dequeue(urb);

	/* Make sure the URB hasn't completed or been unlinked already */
	ret = usb_hcd_check_unlink_urb(hcd, urb, status);
	if (ret)
		goto done;

	/* give back URB now if we can't queue it for cancel */
	vdev = xhci->devs[urb->dev->slot_id];
	urb_priv = urb->hcpriv;
	if (!vdev || !urb_priv)
		goto err_giveback;

	ep_index = xhci_get_endpoint_index(&urb->ep->desc);
	ep = &vdev->eps[ep_index];
	ep_ring = xhci_urb_to_transfer_ring(xhci, urb);
	if (!ep || !ep_ring)
		goto err_giveback;

	/* If xHC is dead take it down and return ALL URBs in xhci_hc_died() */
	temp = readl(&xhci->op_regs->status);
	if (temp == ~(u32)0 || xhci->xhc_state & XHCI_STATE_DYING) {
		xhci_hc_died(xhci);
		goto done;
	}

	/*
	 * check ring is not re-allocated since URB was enqueued. If it is, then
	 * make sure none of the ring related pointers in this URB private data
	 * are touched, such as td_list, otherwise we overwrite freed data
	 */
	if (!td_on_ring(&urb_priv->td[0], ep_ring)) {
		xhci_err(xhci, "Canceled URB td not found on endpoint ring");
		for (i = urb_priv->num_tds_done; i < urb_priv->num_tds; i++) {
			td = &urb_priv->td[i];
			if (!list_empty(&td->cancelled_td_list))
				list_del_init(&td->cancelled_td_list);
		}
		goto err_giveback;
	}

	if (xhci->xhc_state & XHCI_STATE_HALTED) {
		xhci_dbg_trace(xhci, trace_xhci_dbg_cancel_urb,
				"HC halted, freeing TD manually.");
		for (i = urb_priv->num_tds_done;
		     i < urb_priv->num_tds;
		     i++) {
			td = &urb_priv->td[i];
			if (!list_empty(&td->td_list))
				list_del_init(&td->td_list);
			if (!list_empty(&td->cancelled_td_list))
				list_del_init(&td->cancelled_td_list);
		}
		goto err_giveback;
	}

	i = urb_priv->num_tds_done;
	if (i < urb_priv->num_tds)
		xhci_dbg_trace(xhci, trace_xhci_dbg_cancel_urb,
				"Cancel URB %p, dev %s, ep 0x%x, "
				"starting at offset 0x%llx",
				urb, urb->dev->devpath,
				urb->ep->desc.bEndpointAddress,
				(unsigned long long) xhci_trb_virt_to_dma(
					urb_priv->td[i].start_seg,
					urb_priv->td[i].start_trb));

	for (; i < urb_priv->num_tds; i++) {
		td = &urb_priv->td[i];
		/* TD can already be on cancelled list if ep halted on it */
		if (list_empty(&td->cancelled_td_list)) {
			td->cancel_status = TD_DIRTY;
			list_add_tail(&td->cancelled_td_list,
				      &ep->cancelled_td_list);
		}
	}

	/* These completion handlers will sort out cancelled TDs for us */
	if (ep->ep_state & (EP_STOP_CMD_PENDING | EP_HALTED | SET_DEQ_PENDING)) {
		xhci_dbg(xhci, "Not queuing Stop Endpoint on slot %d ep %d in state 0x%x\n",
				urb->dev->slot_id, ep_index, ep->ep_state);
		goto done;
	}

	/* In this case no commands are pending but the endpoint is stopped */
	if (ep->ep_state & EP_CLEARING_TT) {
		/* and cancelled TDs can be given back right away */
		xhci_dbg(xhci, "Invalidating TDs instantly on slot %d ep %d in state 0x%x\n",
				urb->dev->slot_id, ep_index, ep->ep_state);
		xhci_process_cancelled_tds(ep);
	} else {
		/* Otherwise, queue a new Stop Endpoint command */
		command = xhci_alloc_command(xhci, false, GFP_ATOMIC);
		if (!command) {
			ret = -ENOMEM;
			goto done;
		}
		ep->stop_time = jiffies;
		ep->ep_state |= EP_STOP_CMD_PENDING;
		xhci_queue_stop_endpoint(xhci, command, urb->dev->slot_id,
					 ep_index, 0);
		xhci_ring_cmd_db(xhci);
	}
done:
	spin_unlock_irqrestore(&xhci->lock, flags);
	return ret;

err_giveback:
	if (urb_priv)
		xhci_urb_free_priv(urb_priv);
	usb_hcd_unlink_urb_from_ep(hcd, urb);
	spin_unlock_irqrestore(&xhci->lock, flags);
	usb_hcd_giveback_urb(hcd, urb, -ESHUTDOWN);
	return ret;
}

/* Drop an endpoint from a new bandwidth configuration for this device.
 * Only one call to this function is allowed per endpoint before
 * check_bandwidth() or reset_bandwidth() must be called.
 * A call to xhci_drop_endpoint() followed by a call to xhci_add_endpoint() will
 * add the endpoint to the schedule with possibly new parameters denoted by a
 * different endpoint descriptor in usb_host_endpoint.
 * A call to xhci_add_endpoint() followed by a call to xhci_drop_endpoint() is
 * not allowed.
 *
 * The USB core will not allow URBs to be queued to an endpoint that is being
 * disabled, so there's no need for mutual exclusion to protect
 * the xhci->devs[slot_id] structure.
 */
int xhci_drop_endpoint(struct usb_hcd *hcd, struct usb_device *udev,
		       struct usb_host_endpoint *ep)
{
	struct xhci_hcd *xhci;
	struct xhci_container_ctx *in_ctx, *out_ctx;
	struct xhci_input_control_ctx *ctrl_ctx;
	unsigned int ep_index;
	struct xhci_ep_ctx *ep_ctx;
	u32 drop_flag;
	u32 new_add_flags, new_drop_flags;
	int ret;

	ret = xhci_check_args(hcd, udev, ep, 1, true, __func__);
	if (ret <= 0)
		return ret;
	xhci = hcd_to_xhci(hcd);
	if (xhci->xhc_state & XHCI_STATE_DYING)
		return -ENODEV;

	xhci_dbg(xhci, "%s called for udev %p\n", __func__, udev);
	drop_flag = xhci_get_endpoint_flag(&ep->desc);
	if (drop_flag == SLOT_FLAG || drop_flag == EP0_FLAG) {
		xhci_dbg(xhci, "xHCI %s - can't drop slot or ep 0 %#x\n",
				__func__, drop_flag);
		return 0;
	}

	in_ctx = xhci->devs[udev->slot_id]->in_ctx;
	out_ctx = xhci->devs[udev->slot_id]->out_ctx;
	ctrl_ctx = xhci_get_input_control_ctx(in_ctx);
	if (!ctrl_ctx) {
		xhci_warn(xhci, "%s: Could not get input context, bad type.\n",
				__func__);
		return 0;
	}

	ep_index = xhci_get_endpoint_index(&ep->desc);
	ep_ctx = xhci_get_ep_ctx(xhci, out_ctx, ep_index);
	/* If the HC already knows the endpoint is disabled,
	 * or the HCD has noted it is disabled, ignore this request
	 */
	if ((GET_EP_CTX_STATE(ep_ctx) == EP_STATE_DISABLED) ||
	    le32_to_cpu(ctrl_ctx->drop_flags) &
	    xhci_get_endpoint_flag(&ep->desc)) {
		/* Do not warn when called after a usb_device_reset */
		if (xhci->devs[udev->slot_id]->eps[ep_index].ring != NULL)
			xhci_warn(xhci, "xHCI %s called with disabled ep %p\n",
				  __func__, ep);
		return 0;
	}

	ctrl_ctx->drop_flags |= cpu_to_le32(drop_flag);
	new_drop_flags = le32_to_cpu(ctrl_ctx->drop_flags);

	ctrl_ctx->add_flags &= cpu_to_le32(~drop_flag);
	new_add_flags = le32_to_cpu(ctrl_ctx->add_flags);

	xhci_debugfs_remove_endpoint(xhci, xhci->devs[udev->slot_id], ep_index);

	xhci_endpoint_zero(xhci, xhci->devs[udev->slot_id], ep);

	xhci_dbg(xhci, "drop ep 0x%x, slot id %d, new drop flags = %#x, new add flags = %#x\n",
			(unsigned int) ep->desc.bEndpointAddress,
			udev->slot_id,
			(unsigned int) new_drop_flags,
			(unsigned int) new_add_flags);
	return 0;
}
EXPORT_SYMBOL_GPL(xhci_drop_endpoint);

/* Add an endpoint to a new possible bandwidth configuration for this device.
 * Only one call to this function is allowed per endpoint before
 * check_bandwidth() or reset_bandwidth() must be called.
 * A call to xhci_drop_endpoint() followed by a call to xhci_add_endpoint() will
 * add the endpoint to the schedule with possibly new parameters denoted by a
 * different endpoint descriptor in usb_host_endpoint.
 * A call to xhci_add_endpoint() followed by a call to xhci_drop_endpoint() is
 * not allowed.
 *
 * The USB core will not allow URBs to be queued to an endpoint until the
 * configuration or alt setting is installed in the device, so there's no need
 * for mutual exclusion to protect the xhci->devs[slot_id] structure.
 */
int xhci_add_endpoint(struct usb_hcd *hcd, struct usb_device *udev,
		      struct usb_host_endpoint *ep)
{
	struct xhci_hcd *xhci;
	struct xhci_container_ctx *in_ctx;
	unsigned int ep_index;
	struct xhci_input_control_ctx *ctrl_ctx;
	struct xhci_ep_ctx *ep_ctx;
	u32 added_ctxs;
	u32 new_add_flags, new_drop_flags;
	struct xhci_virt_device *virt_dev;
	int ret = 0;

	ret = xhci_check_args(hcd, udev, ep, 1, true, __func__);
	if (ret <= 0) {
		/* So we won't queue a reset ep command for a root hub */
		ep->hcpriv = NULL;
		return ret;
	}
	xhci = hcd_to_xhci(hcd);
	if (xhci->xhc_state & XHCI_STATE_DYING)
		return -ENODEV;

	added_ctxs = xhci_get_endpoint_flag(&ep->desc);
	if (added_ctxs == SLOT_FLAG || added_ctxs == EP0_FLAG) {
		/* FIXME when we have to issue an evaluate endpoint command to
		 * deal with ep0 max packet size changing once we get the
		 * descriptors
		 */
		xhci_dbg(xhci, "xHCI %s - can't add slot or ep 0 %#x\n",
				__func__, added_ctxs);
		return 0;
	}

	virt_dev = xhci->devs[udev->slot_id];
	in_ctx = virt_dev->in_ctx;
	ctrl_ctx = xhci_get_input_control_ctx(in_ctx);
	if (!ctrl_ctx) {
		xhci_warn(xhci, "%s: Could not get input context, bad type.\n",
				__func__);
		return 0;
	}

	ep_index = xhci_get_endpoint_index(&ep->desc);
	/* If this endpoint is already in use, and the upper layers are trying
	 * to add it again without dropping it, reject the addition.
	 */
	if (virt_dev->eps[ep_index].ring &&
			!(le32_to_cpu(ctrl_ctx->drop_flags) & added_ctxs)) {
		xhci_warn(xhci, "Trying to add endpoint 0x%x "
				"without dropping it.\n",
				(unsigned int) ep->desc.bEndpointAddress);
		return -EINVAL;
	}

	/* If the HCD has already noted the endpoint is enabled,
	 * ignore this request.
	 */
	if (le32_to_cpu(ctrl_ctx->add_flags) & added_ctxs) {
		xhci_warn(xhci, "xHCI %s called with enabled ep %p\n",
				__func__, ep);
		return 0;
	}

	/*
	 * Configuration and alternate setting changes must be done in
	 * process context, not interrupt context (or so documenation
	 * for usb_set_interface() and usb_set_configuration() claim).
	 */
	if (xhci_endpoint_init(xhci, virt_dev, udev, ep, GFP_NOIO) < 0) {
		dev_dbg(&udev->dev, "%s - could not initialize ep %#x\n",
				__func__, ep->desc.bEndpointAddress);
		return -ENOMEM;
	}

	ctrl_ctx->add_flags |= cpu_to_le32(added_ctxs);
	new_add_flags = le32_to_cpu(ctrl_ctx->add_flags);

	/* If xhci_endpoint_disable() was called for this endpoint, but the
	 * xHC hasn't been notified yet through the check_bandwidth() call,
	 * this re-adds a new state for the endpoint from the new endpoint
	 * descriptors.  We must drop and re-add this endpoint, so we leave the
	 * drop flags alone.
	 */
	new_drop_flags = le32_to_cpu(ctrl_ctx->drop_flags);

	/* Store the usb_device pointer for later use */
	ep->hcpriv = udev;

	ep_ctx = xhci_get_ep_ctx(xhci, virt_dev->in_ctx, ep_index);
	trace_xhci_add_endpoint(ep_ctx);

	xhci_dbg(xhci, "add ep 0x%x, slot id %d, new drop flags = %#x, new add flags = %#x\n",
			(unsigned int) ep->desc.bEndpointAddress,
			udev->slot_id,
			(unsigned int) new_drop_flags,
			(unsigned int) new_add_flags);
	return 0;
}
EXPORT_SYMBOL_GPL(xhci_add_endpoint);

static void xhci_zero_in_ctx(struct xhci_hcd *xhci, struct xhci_virt_device *virt_dev)
{
	struct xhci_input_control_ctx *ctrl_ctx;
	struct xhci_ep_ctx *ep_ctx;
	struct xhci_slot_ctx *slot_ctx;
	int i;

	ctrl_ctx = xhci_get_input_control_ctx(virt_dev->in_ctx);
	if (!ctrl_ctx) {
		xhci_warn(xhci, "%s: Could not get input context, bad type.\n",
				__func__);
		return;
	}

	/* When a device's add flag and drop flag are zero, any subsequent
	 * configure endpoint command will leave that endpoint's state
	 * untouched.  Make sure we don't leave any old state in the input
	 * endpoint contexts.
	 */
	ctrl_ctx->drop_flags = 0;
	ctrl_ctx->add_flags = 0;
	slot_ctx = xhci_get_slot_ctx(xhci, virt_dev->in_ctx);
	slot_ctx->dev_info &= cpu_to_le32(~LAST_CTX_MASK);
	/* Endpoint 0 is always valid */
	slot_ctx->dev_info |= cpu_to_le32(LAST_CTX(1));
	for (i = 1; i < 31; i++) {
		ep_ctx = xhci_get_ep_ctx(xhci, virt_dev->in_ctx, i);
		ep_ctx->ep_info = 0;
		ep_ctx->ep_info2 = 0;
		ep_ctx->deq = 0;
		ep_ctx->tx_info = 0;
	}
}

static int xhci_configure_endpoint_result(struct xhci_hcd *xhci,
		struct usb_device *udev, u32 *cmd_status)
{
	int ret;

	switch (*cmd_status) {
	case COMP_COMMAND_ABORTED:
	case COMP_COMMAND_RING_STOPPED:
		xhci_warn(xhci, "Timeout while waiting for configure endpoint command\n");
		ret = -ETIME;
		break;
	case COMP_RESOURCE_ERROR:
		dev_warn(&udev->dev,
			 "Not enough host controller resources for new device state.\n");
		ret = -ENOMEM;
		/* FIXME: can we allocate more resources for the HC? */
		break;
	case COMP_BANDWIDTH_ERROR:
	case COMP_SECONDARY_BANDWIDTH_ERROR:
		dev_warn(&udev->dev,
			 "Not enough bandwidth for new device state.\n");
		ret = -ENOSPC;
		/* FIXME: can we go back to the old state? */
		break;
	case COMP_TRB_ERROR:
		/* the HCD set up something wrong */
		dev_warn(&udev->dev, "ERROR: Endpoint drop flag = 0, "
				"add flag = 1, "
				"and endpoint is not disabled.\n");
		ret = -EINVAL;
		break;
	case COMP_INCOMPATIBLE_DEVICE_ERROR:
		dev_warn(&udev->dev,
			 "ERROR: Incompatible device for endpoint configure command.\n");
		ret = -ENODEV;
		break;
	case COMP_SUCCESS:
		xhci_dbg_trace(xhci, trace_xhci_dbg_context_change,
				"Successful Endpoint Configure command");
		ret = 0;
		break;
	default:
		xhci_err(xhci, "ERROR: unexpected command completion code 0x%x.\n",
				*cmd_status);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int xhci_evaluate_context_result(struct xhci_hcd *xhci,
		struct usb_device *udev, u32 *cmd_status)
{
	int ret;

	switch (*cmd_status) {
	case COMP_COMMAND_ABORTED:
	case COMP_COMMAND_RING_STOPPED:
		xhci_warn(xhci, "Timeout while waiting for evaluate context command\n");
		ret = -ETIME;
		break;
	case COMP_PARAMETER_ERROR:
		dev_warn(&udev->dev,
			 "WARN: xHCI driver setup invalid evaluate context command.\n");
		ret = -EINVAL;
		break;
	case COMP_SLOT_NOT_ENABLED_ERROR:
		dev_warn(&udev->dev,
			"WARN: slot not enabled for evaluate context command.\n");
		ret = -EINVAL;
		break;
	case COMP_CONTEXT_STATE_ERROR:
		dev_warn(&udev->dev,
			"WARN: invalid context state for evaluate context command.\n");
		ret = -EINVAL;
		break;
	case COMP_INCOMPATIBLE_DEVICE_ERROR:
		dev_warn(&udev->dev,
			"ERROR: Incompatible device for evaluate context command.\n");
		ret = -ENODEV;
		break;
	case COMP_MAX_EXIT_LATENCY_TOO_LARGE_ERROR:
		/* Max Exit Latency too large error */
		dev_warn(&udev->dev, "WARN: Max Exit Latency too large\n");
		ret = -EINVAL;
		break;
	case COMP_SUCCESS:
		xhci_dbg_trace(xhci, trace_xhci_dbg_context_change,
				"Successful evaluate context command");
		ret = 0;
		break;
	default:
		xhci_err(xhci, "ERROR: unexpected command completion code 0x%x.\n",
			*cmd_status);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static u32 xhci_count_num_new_endpoints(struct xhci_hcd *xhci,
		struct xhci_input_control_ctx *ctrl_ctx)
{
	u32 valid_add_flags;
	u32 valid_drop_flags;

	/* Ignore the slot flag (bit 0), and the default control endpoint flag
	 * (bit 1).  The default control endpoint is added during the Address
	 * Device command and is never removed until the slot is disabled.
	 */
	valid_add_flags = le32_to_cpu(ctrl_ctx->add_flags) >> 2;
	valid_drop_flags = le32_to_cpu(ctrl_ctx->drop_flags) >> 2;

	/* Use hweight32 to count the number of ones in the add flags, or
	 * number of endpoints added.  Don't count endpoints that are changed
	 * (both added and dropped).
	 */
	return hweight32(valid_add_flags) -
		hweight32(valid_add_flags & valid_drop_flags);
}

static unsigned int xhci_count_num_dropped_endpoints(struct xhci_hcd *xhci,
		struct xhci_input_control_ctx *ctrl_ctx)
{
	u32 valid_add_flags;
	u32 valid_drop_flags;

	valid_add_flags = le32_to_cpu(ctrl_ctx->add_flags) >> 2;
	valid_drop_flags = le32_to_cpu(ctrl_ctx->drop_flags) >> 2;

	return hweight32(valid_drop_flags) -
		hweight32(valid_add_flags & valid_drop_flags);
}

/*
 * We need to reserve the new number of endpoints before the configure endpoint
 * command completes.  We can't subtract the dropped endpoints from the number
 * of active endpoints until the command completes because we can oversubscribe
 * the host in this case:
 *
 *  - the first configure endpoint command drops more endpoints than it adds
 *  - a second configure endpoint command that adds more endpoints is queued
 *  - the first configure endpoint command fails, so the config is unchanged
 *  - the second command may succeed, even though there isn't enough resources
 *
 * Must be called with xhci->lock held.
 */
static int xhci_reserve_host_resources(struct xhci_hcd *xhci,
		struct xhci_input_control_ctx *ctrl_ctx)
{
	u32 added_eps;

	added_eps = xhci_count_num_new_endpoints(xhci, ctrl_ctx);
	if (xhci->num_active_eps + added_eps > xhci->limit_active_eps) {
		xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
				"Not enough ep ctxs: "
				"%u active, need to add %u, limit is %u.",
				xhci->num_active_eps, added_eps,
				xhci->limit_active_eps);
		return -ENOMEM;
	}
	xhci->num_active_eps += added_eps;
	xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
			"Adding %u ep ctxs, %u now active.", added_eps,
			xhci->num_active_eps);
	return 0;
}

/*
 * The configure endpoint was failed by the xHC for some other reason, so we
 * need to revert the resources that failed configuration would have used.
 *
 * Must be called with xhci->lock held.
 */
static void xhci_free_host_resources(struct xhci_hcd *xhci,
		struct xhci_input_control_ctx *ctrl_ctx)
{
	u32 num_failed_eps;

	num_failed_eps = xhci_count_num_new_endpoints(xhci, ctrl_ctx);
	xhci->num_active_eps -= num_failed_eps;
	xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
			"Removing %u failed ep ctxs, %u now active.",
			num_failed_eps,
			xhci->num_active_eps);
}

/*
 * Now that the command has completed, clean up the active endpoint count by
 * subtracting out the endpoints that were dropped (but not changed).
 *
 * Must be called with xhci->lock held.
 */
static void xhci_finish_resource_reservation(struct xhci_hcd *xhci,
		struct xhci_input_control_ctx *ctrl_ctx)
{
	u32 num_dropped_eps;

	num_dropped_eps = xhci_count_num_dropped_endpoints(xhci, ctrl_ctx);
	xhci->num_active_eps -= num_dropped_eps;
	if (num_dropped_eps)
		xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
				"Removing %u dropped ep ctxs, %u now active.",
				num_dropped_eps,
				xhci->num_active_eps);
}

static unsigned int xhci_get_block_size(struct usb_device *udev)
{
	switch (udev->speed) {
	case USB_SPEED_LOW:
	case USB_SPEED_FULL:
		return FS_BLOCK;
	case USB_SPEED_HIGH:
		return HS_BLOCK;
	case USB_SPEED_SUPER:
	case USB_SPEED_SUPER_PLUS:
		return SS_BLOCK;
	case USB_SPEED_UNKNOWN:
	default:
		/* Should never happen */
		return 1;
	}
}

static unsigned int
xhci_get_largest_overhead(struct xhci_interval_bw *interval_bw)
{
	if (interval_bw->overhead[LS_OVERHEAD_TYPE])
		return LS_OVERHEAD;
	if (interval_bw->overhead[FS_OVERHEAD_TYPE])
		return FS_OVERHEAD;
	return HS_OVERHEAD;
}

/* If we are changing a LS/FS device under a HS hub,
 * make sure (if we are activating a new TT) that the HS bus has enough
 * bandwidth for this new TT.
 */
static int xhci_check_tt_bw_table(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev,
		int old_active_eps)
{
	struct xhci_interval_bw_table *bw_table;
	struct xhci_tt_bw_info *tt_info;

	/* Find the bandwidth table for the root port this TT is attached to. */
	bw_table = &xhci->rh_bw[virt_dev->rhub_port->hw_portnum].bw_table;
	tt_info = virt_dev->tt_info;
	/* If this TT already had active endpoints, the bandwidth for this TT
	 * has already been added.  Removing all periodic endpoints (and thus
	 * making the TT enactive) will only decrease the bandwidth used.
	 */
	if (old_active_eps)
		return 0;
	if (old_active_eps == 0 && tt_info->active_eps != 0) {
		if (bw_table->bw_used + TT_HS_OVERHEAD > HS_BW_LIMIT)
			return -ENOMEM;
		return 0;
	}
	/* Not sure why we would have no new active endpoints...
	 *
	 * Maybe because of an Evaluate Context change for a hub update or a
	 * control endpoint 0 max packet size change?
	 * FIXME: skip the bandwidth calculation in that case.
	 */
	return 0;
}

static int xhci_check_ss_bw(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev)
{
	unsigned int bw_reserved;

	bw_reserved = DIV_ROUND_UP(SS_BW_RESERVED*SS_BW_LIMIT_IN, 100);
	if (virt_dev->bw_table->ss_bw_in > (SS_BW_LIMIT_IN - bw_reserved))
		return -ENOMEM;

	bw_reserved = DIV_ROUND_UP(SS_BW_RESERVED*SS_BW_LIMIT_OUT, 100);
	if (virt_dev->bw_table->ss_bw_out > (SS_BW_LIMIT_OUT - bw_reserved))
		return -ENOMEM;

	return 0;
}

/*
 * This algorithm is a very conservative estimate of the worst-case scheduling
 * scenario for any one interval.  The hardware dynamically schedules the
 * packets, so we can't tell which microframe could be the limiting factor in
 * the bandwidth scheduling.  This only takes into account periodic endpoints.
 *
 * Obviously, we can't solve an NP complete problem to find the minimum worst
 * case scenario.  Instead, we come up with an estimate that is no less than
 * the worst case bandwidth used for any one microframe, but may be an
 * over-estimate.
 *
 * We walk the requirements for each endpoint by interval, starting with the
 * smallest interval, and place packets in the schedule where there is only one
 * possible way to schedule packets for that interval.  In order to simplify
 * this algorithm, we record the largest max packet size for each interval, and
 * assume all packets will be that size.
 *
 * For interval 0, we obviously must schedule all packets for each interval.
 * The bandwidth for interval 0 is just the amount of data to be transmitted
 * (the sum of all max ESIT payload sizes, plus any overhead per packet times
 * the number of packets).
 *
 * For interval 1, we have two possible microframes to schedule those packets
 * in.  For this algorithm, if we can schedule the same number of packets for
 * each possible scheduling opportunity (each microframe), we will do so.  The
 * remaining number of packets will be saved to be transmitted in the gaps in
 * the next interval's scheduling sequence.
 *
 * As we move those remaining packets to be scheduled with interval 2 packets,
 * we have to double the number of remaining packets to transmit.  This is
 * because the intervals are actually powers of 2, and we would be transmitting
 * the previous interval's packets twice in this interval.  We also have to be
 * sure that when we look at the largest max packet size for this interval, we
 * also look at the largest max packet size for the remaining packets and take
 * the greater of the two.
 *
 * The algorithm continues to evenly distribute packets in each scheduling
 * opportunity, and push the remaining packets out, until we get to the last
 * interval.  Then those packets and their associated overhead are just added
 * to the bandwidth used.
 */
static int xhci_check_bw_table(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev,
		int old_active_eps)
{
	unsigned int bw_reserved;
	unsigned int max_bandwidth;
	unsigned int bw_used;
	unsigned int block_size;
	struct xhci_interval_bw_table *bw_table;
	unsigned int packet_size = 0;
	unsigned int overhead = 0;
	unsigned int packets_transmitted = 0;
	unsigned int packets_remaining = 0;
	unsigned int i;

	if (virt_dev->udev->speed >= USB_SPEED_SUPER)
		return xhci_check_ss_bw(xhci, virt_dev);

	if (virt_dev->udev->speed == USB_SPEED_HIGH) {
		max_bandwidth = HS_BW_LIMIT;
		/* Convert percent of bus BW reserved to blocks reserved */
		bw_reserved = DIV_ROUND_UP(HS_BW_RESERVED * max_bandwidth, 100);
	} else {
		max_bandwidth = FS_BW_LIMIT;
		bw_reserved = DIV_ROUND_UP(FS_BW_RESERVED * max_bandwidth, 100);
	}

	bw_table = virt_dev->bw_table;
	/* We need to translate the max packet size and max ESIT payloads into
	 * the units the hardware uses.
	 */
	block_size = xhci_get_block_size(virt_dev->udev);

	/* If we are manipulating a LS/FS device under a HS hub, double check
	 * that the HS bus has enough bandwidth if we are activing a new TT.
	 */
	if (virt_dev->tt_info) {
		xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
				"Recalculating BW for rootport %u",
				virt_dev->rhub_port->hw_portnum + 1);
		if (xhci_check_tt_bw_table(xhci, virt_dev, old_active_eps)) {
			xhci_warn(xhci, "Not enough bandwidth on HS bus for "
					"newly activated TT.\n");
			return -ENOMEM;
		}
		xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
				"Recalculating BW for TT slot %u port %u",
				virt_dev->tt_info->slot_id,
				virt_dev->tt_info->ttport);
	} else {
		xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
				"Recalculating BW for rootport %u",
				virt_dev->rhub_port->hw_portnum + 1);
	}

	/* Add in how much bandwidth will be used for interval zero, or the
	 * rounded max ESIT payload + number of packets * largest overhead.
	 */
	bw_used = DIV_ROUND_UP(bw_table->interval0_esit_payload, block_size) +
		bw_table->interval_bw[0].num_packets *
		xhci_get_largest_overhead(&bw_table->interval_bw[0]);

	for (i = 1; i < XHCI_MAX_INTERVAL; i++) {
		unsigned int bw_added;
		unsigned int largest_mps;
		unsigned int interval_overhead;

		/*
		 * How many packets could we transmit in this interval?
		 * If packets didn't fit in the previous interval, we will need
		 * to transmit that many packets twice within this interval.
		 */
		packets_remaining = 2 * packets_remaining +
			bw_table->interval_bw[i].num_packets;

		/* Find the largest max packet size of this or the previous
		 * interval.
		 */
		if (list_empty(&bw_table->interval_bw[i].endpoints))
			largest_mps = 0;
		else {
			struct xhci_virt_ep *virt_ep;
			struct list_head *ep_entry;

			ep_entry = bw_table->interval_bw[i].endpoints.next;
			virt_ep = list_entry(ep_entry,
					struct xhci_virt_ep, bw_endpoint_list);
			/* Convert to blocks, rounding up */
			largest_mps = DIV_ROUND_UP(
					virt_ep->bw_info.max_packet_size,
					block_size);
		}
		if (largest_mps > packet_size)
			packet_size = largest_mps;

		/* Use the larger overhead of this or the previous interval. */
		interval_overhead = xhci_get_largest_overhead(
				&bw_table->interval_bw[i]);
		if (interval_overhead > overhead)
			overhead = interval_overhead;

		/* How many packets can we evenly distribute across
		 * (1 << (i + 1)) possible scheduling opportunities?
		 */
		packets_transmitted = packets_remaining >> (i + 1);

		/* Add in the bandwidth used for those scheduled packets */
		bw_added = packets_transmitted * (overhead + packet_size);

		/* How many packets do we have remaining to transmit? */
		packets_remaining = packets_remaining % (1 << (i + 1));

		/* What largest max packet size should those packets have? */
		/* If we've transmitted all packets, don't carry over the
		 * largest packet size.
		 */
		if (packets_remaining == 0) {
			packet_size = 0;
			overhead = 0;
		} else if (packets_transmitted > 0) {
			/* Otherwise if we do have remaining packets, and we've
			 * scheduled some packets in this interval, take the
			 * largest max packet size from endpoints with this
			 * interval.
			 */
			packet_size = largest_mps;
			overhead = interval_overhead;
		}
		/* Otherwise carry over packet_size and overhead from the last
		 * time we had a remainder.
		 */
		bw_used += bw_added;
		if (bw_used > max_bandwidth) {
			xhci_warn(xhci, "Not enough bandwidth. "
					"Proposed: %u, Max: %u\n",
				bw_used, max_bandwidth);
			return -ENOMEM;
		}
	}
	/*
	 * Ok, we know we have some packets left over after even-handedly
	 * scheduling interval 15.  We don't know which microframes they will
	 * fit into, so we over-schedule and say they will be scheduled every
	 * microframe.
	 */
	if (packets_remaining > 0)
		bw_used += overhead + packet_size;

	if (!virt_dev->tt_info && virt_dev->udev->speed == USB_SPEED_HIGH) {
		/* OK, we're manipulating a HS device attached to a
		 * root port bandwidth domain.  Include the number of active TTs
		 * in the bandwidth used.
		 */
		bw_used += TT_HS_OVERHEAD *
			xhci->rh_bw[virt_dev->rhub_port->hw_portnum].num_active_tts;
	}

	xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
		"Final bandwidth: %u, Limit: %u, Reserved: %u, "
		"Available: %u " "percent",
		bw_used, max_bandwidth, bw_reserved,
		(max_bandwidth - bw_used - bw_reserved) * 100 /
		max_bandwidth);

	bw_used += bw_reserved;
	if (bw_used > max_bandwidth) {
		xhci_warn(xhci, "Not enough bandwidth. Proposed: %u, Max: %u\n",
				bw_used, max_bandwidth);
		return -ENOMEM;
	}

	bw_table->bw_used = bw_used;
	return 0;
}

static bool xhci_is_async_ep(unsigned int ep_type)
{
	return (ep_type != ISOC_OUT_EP && ep_type != INT_OUT_EP &&
					ep_type != ISOC_IN_EP &&
					ep_type != INT_IN_EP);
}

static bool xhci_is_sync_in_ep(unsigned int ep_type)
{
	return (ep_type == ISOC_IN_EP || ep_type == INT_IN_EP);
}

static unsigned int xhci_get_ss_bw_consumed(struct xhci_bw_info *ep_bw)
{
	unsigned int mps = DIV_ROUND_UP(ep_bw->max_packet_size, SS_BLOCK);

	if (ep_bw->ep_interval == 0)
		return SS_OVERHEAD_BURST +
			(ep_bw->mult * ep_bw->num_packets *
					(SS_OVERHEAD + mps));
	return DIV_ROUND_UP(ep_bw->mult * ep_bw->num_packets *
				(SS_OVERHEAD + mps + SS_OVERHEAD_BURST),
				1 << ep_bw->ep_interval);

}

static void xhci_drop_ep_from_interval_table(struct xhci_hcd *xhci,
		struct xhci_bw_info *ep_bw,
		struct xhci_interval_bw_table *bw_table,
		struct usb_device *udev,
		struct xhci_virt_ep *virt_ep,
		struct xhci_tt_bw_info *tt_info)
{
	struct xhci_interval_bw	*interval_bw;
	int normalized_interval;

	if (xhci_is_async_ep(ep_bw->type))
		return;

	if (udev->speed >= USB_SPEED_SUPER) {
		if (xhci_is_sync_in_ep(ep_bw->type))
			xhci->devs[udev->slot_id]->bw_table->ss_bw_in -=
				xhci_get_ss_bw_consumed(ep_bw);
		else
			xhci->devs[udev->slot_id]->bw_table->ss_bw_out -=
				xhci_get_ss_bw_consumed(ep_bw);
		return;
	}

	/* SuperSpeed endpoints never get added to intervals in the table, so
	 * this check is only valid for HS/FS/LS devices.
	 */
	if (list_empty(&virt_ep->bw_endpoint_list))
		return;
	/* For LS/FS devices, we need to translate the interval expressed in
	 * microframes to frames.
	 */
	if (udev->speed == USB_SPEED_HIGH)
		normalized_interval = ep_bw->ep_interval;
	else
		normalized_interval = ep_bw->ep_interval - 3;

	if (normalized_interval == 0)
		bw_table->interval0_esit_payload -= ep_bw->max_esit_payload;
	interval_bw = &bw_table->interval_bw[normalized_interval];
	interval_bw->num_packets -= ep_bw->num_packets;
	switch (udev->speed) {
	case USB_SPEED_LOW:
		interval_bw->overhead[LS_OVERHEAD_TYPE] -= 1;
		break;
	case USB_SPEED_FULL:
		interval_bw->overhead[FS_OVERHEAD_TYPE] -= 1;
		break;
	case USB_SPEED_HIGH:
		interval_bw->overhead[HS_OVERHEAD_TYPE] -= 1;
		break;
	default:
		/* Should never happen because only LS/FS/HS endpoints will get
		 * added to the endpoint list.
		 */
		return;
	}
	if (tt_info)
		tt_info->active_eps -= 1;
	list_del_init(&virt_ep->bw_endpoint_list);
}

static void xhci_add_ep_to_interval_table(struct xhci_hcd *xhci,
		struct xhci_bw_info *ep_bw,
		struct xhci_interval_bw_table *bw_table,
		struct usb_device *udev,
		struct xhci_virt_ep *virt_ep,
		struct xhci_tt_bw_info *tt_info)
{
	struct xhci_interval_bw	*interval_bw;
	struct xhci_virt_ep *smaller_ep;
	int normalized_interval;

	if (xhci_is_async_ep(ep_bw->type))
		return;

	if (udev->speed == USB_SPEED_SUPER) {
		if (xhci_is_sync_in_ep(ep_bw->type))
			xhci->devs[udev->slot_id]->bw_table->ss_bw_in +=
				xhci_get_ss_bw_consumed(ep_bw);
		else
			xhci->devs[udev->slot_id]->bw_table->ss_bw_out +=
				xhci_get_ss_bw_consumed(ep_bw);
		return;
	}

	/* For LS/FS devices, we need to translate the interval expressed in
	 * microframes to frames.
	 */
	if (udev->speed == USB_SPEED_HIGH)
		normalized_interval = ep_bw->ep_interval;
	else
		normalized_interval = ep_bw->ep_interval - 3;

	if (normalized_interval == 0)
		bw_table->interval0_esit_payload += ep_bw->max_esit_payload;
	interval_bw = &bw_table->interval_bw[normalized_interval];
	interval_bw->num_packets += ep_bw->num_packets;
	switch (udev->speed) {
	case USB_SPEED_LOW:
		interval_bw->overhead[LS_OVERHEAD_TYPE] += 1;
		break;
	case USB_SPEED_FULL:
		interval_bw->overhead[FS_OVERHEAD_TYPE] += 1;
		break;
	case USB_SPEED_HIGH:
		interval_bw->overhead[HS_OVERHEAD_TYPE] += 1;
		break;
	default:
		/* Should never happen because only LS/FS/HS endpoints will get
		 * added to the endpoint list.
		 */
		return;
	}

	if (tt_info)
		tt_info->active_eps += 1;
	/* Insert the endpoint into the list, largest max packet size first. */
	list_for_each_entry(smaller_ep, &interval_bw->endpoints,
			bw_endpoint_list) {
		if (ep_bw->max_packet_size >=
				smaller_ep->bw_info.max_packet_size) {
			/* Add the new ep before the smaller endpoint */
			list_add_tail(&virt_ep->bw_endpoint_list,
					&smaller_ep->bw_endpoint_list);
			return;
		}
	}
	/* Add the new endpoint at the end of the list. */
	list_add_tail(&virt_ep->bw_endpoint_list,
			&interval_bw->endpoints);
}

void xhci_update_tt_active_eps(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev,
		int old_active_eps)
{
	struct xhci_root_port_bw_info *rh_bw_info;
	if (!virt_dev->tt_info)
		return;

	rh_bw_info = &xhci->rh_bw[virt_dev->rhub_port->hw_portnum];
	if (old_active_eps == 0 &&
				virt_dev->tt_info->active_eps != 0) {
		rh_bw_info->num_active_tts += 1;
		rh_bw_info->bw_table.bw_used += TT_HS_OVERHEAD;
	} else if (old_active_eps != 0 &&
				virt_dev->tt_info->active_eps == 0) {
		rh_bw_info->num_active_tts -= 1;
		rh_bw_info->bw_table.bw_used -= TT_HS_OVERHEAD;
	}
}

static int xhci_reserve_bandwidth(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev,
		struct xhci_container_ctx *in_ctx)
{
	struct xhci_bw_info ep_bw_info[31];
	int i;
	struct xhci_input_control_ctx *ctrl_ctx;
	int old_active_eps = 0;

	if (virt_dev->tt_info)
		old_active_eps = virt_dev->tt_info->active_eps;

	ctrl_ctx = xhci_get_input_control_ctx(in_ctx);
	if (!ctrl_ctx) {
		xhci_warn(xhci, "%s: Could not get input context, bad type.\n",
				__func__);
		return -ENOMEM;
	}

	for (i = 0; i < 31; i++) {
		if (!EP_IS_ADDED(ctrl_ctx, i) && !EP_IS_DROPPED(ctrl_ctx, i))
			continue;

		/* Make a copy of the BW info in case we need to revert this */
		memcpy(&ep_bw_info[i], &virt_dev->eps[i].bw_info,
				sizeof(ep_bw_info[i]));
		/* Drop the endpoint from the interval table if the endpoint is
		 * being dropped or changed.
		 */
		if (EP_IS_DROPPED(ctrl_ctx, i))
			xhci_drop_ep_from_interval_table(xhci,
					&virt_dev->eps[i].bw_info,
					virt_dev->bw_table,
					virt_dev->udev,
					&virt_dev->eps[i],
					virt_dev->tt_info);
	}
	/* Overwrite the information stored in the endpoints' bw_info */
	xhci_update_bw_info(xhci, virt_dev->in_ctx, ctrl_ctx, virt_dev);
	for (i = 0; i < 31; i++) {
		/* Add any changed or added endpoints to the interval table */
		if (EP_IS_ADDED(ctrl_ctx, i))
			xhci_add_ep_to_interval_table(xhci,
					&virt_dev->eps[i].bw_info,
					virt_dev->bw_table,
					virt_dev->udev,
					&virt_dev->eps[i],
					virt_dev->tt_info);
	}

	if (!xhci_check_bw_table(xhci, virt_dev, old_active_eps)) {
		/* Ok, this fits in the bandwidth we have.
		 * Update the number of active TTs.
		 */
		xhci_update_tt_active_eps(xhci, virt_dev, old_active_eps);
		return 0;
	}

	/* We don't have enough bandwidth for this, revert the stored info. */
	for (i = 0; i < 31; i++) {
		if (!EP_IS_ADDED(ctrl_ctx, i) && !EP_IS_DROPPED(ctrl_ctx, i))
			continue;

		/* Drop the new copies of any added or changed endpoints from
		 * the interval table.
		 */
		if (EP_IS_ADDED(ctrl_ctx, i)) {
			xhci_drop_ep_from_interval_table(xhci,
					&virt_dev->eps[i].bw_info,
					virt_dev->bw_table,
					virt_dev->udev,
					&virt_dev->eps[i],
					virt_dev->tt_info);
		}
		/* Revert the endpoint back to its old information */
		memcpy(&virt_dev->eps[i].bw_info, &ep_bw_info[i],
				sizeof(ep_bw_info[i]));
		/* Add any changed or dropped endpoints back into the table */
		if (EP_IS_DROPPED(ctrl_ctx, i))
			xhci_add_ep_to_interval_table(xhci,
					&virt_dev->eps[i].bw_info,
					virt_dev->bw_table,
					virt_dev->udev,
					&virt_dev->eps[i],
					virt_dev->tt_info);
	}
	return -ENOMEM;
}

/*
 * Synchronous XHCI stop endpoint helper.  Issues the stop endpoint command and
 * waits for the command completion before returning.  This does not call
 * xhci_handle_cmd_stop_ep(), which has additional handling for 'context error'
 * cases, along with transfer ring cleanup.
 *
 * xhci_stop_endpoint_sync() is intended to be utilized by clients that manage
 * their own transfer ring, such as offload situations.
 */
int xhci_stop_endpoint_sync(struct xhci_hcd *xhci, struct xhci_virt_ep *ep, int suspend,
			    gfp_t gfp_flags)
{
	struct xhci_command *command;
	unsigned long flags;
	int ret;

	command = xhci_alloc_command(xhci, true, gfp_flags);
	if (!command)
		return -ENOMEM;

	spin_lock_irqsave(&xhci->lock, flags);
	ret = xhci_queue_stop_endpoint(xhci, command, ep->vdev->slot_id,
				       ep->ep_index, suspend);
	if (ret < 0) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		goto out;
	}

	xhci_ring_cmd_db(xhci);
	spin_unlock_irqrestore(&xhci->lock, flags);

	wait_for_completion(command->completion);

	/* No handling for COMP_CONTEXT_STATE_ERROR done at command completion*/
	if (command->status == COMP_COMMAND_ABORTED ||
	    command->status == COMP_COMMAND_RING_STOPPED) {
		xhci_warn(xhci, "Timeout while waiting for stop endpoint command\n");
		ret = -ETIME;
	}
out:
	xhci_free_command(xhci, command);

	return ret;
}
EXPORT_SYMBOL_GPL(xhci_stop_endpoint_sync);

/*
 * xhci_usb_endpoint_maxp - get endpoint max packet size
 * @host_ep: USB host endpoint to be checked
 *
 * Returns max packet from the correct descriptor
 */
int xhci_usb_endpoint_maxp(struct usb_device *udev,
			   struct usb_host_endpoint *host_ep)
{
	if (usb_endpoint_is_hs_isoc_double(udev, host_ep))
		return le16_to_cpu(host_ep->eusb2_isoc_ep_comp.wMaxPacketSize);
	return usb_endpoint_maxp(&host_ep->desc);
}

/* Issue a configure endpoint command or evaluate context command
 * and wait for it to finish.
 */
static int xhci_configure_endpoint(struct xhci_hcd *xhci,
		struct usb_device *udev,
		struct xhci_command *command,
		bool ctx_change, bool must_succeed)
{
	int ret;
	unsigned long flags;
	struct xhci_input_control_ctx *ctrl_ctx;
	struct xhci_virt_device *virt_dev;
	struct xhci_slot_ctx *slot_ctx;

	if (!command)
		return -EINVAL;

	spin_lock_irqsave(&xhci->lock, flags);

	if (xhci->xhc_state & XHCI_STATE_DYING) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		return -ESHUTDOWN;
	}

	virt_dev = xhci->devs[udev->slot_id];

	ctrl_ctx = xhci_get_input_control_ctx(command->in_ctx);
	if (!ctrl_ctx) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		xhci_warn(xhci, "%s: Could not get input context, bad type.\n",
				__func__);
		return -ENOMEM;
	}

	if ((xhci->quirks & XHCI_EP_LIMIT_QUIRK) &&
			xhci_reserve_host_resources(xhci, ctrl_ctx)) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		xhci_warn(xhci, "Not enough host resources, "
				"active endpoint contexts = %u\n",
				xhci->num_active_eps);
		return -ENOMEM;
	}
	if ((xhci->quirks & XHCI_SW_BW_CHECKING) && !ctx_change &&
	    xhci_reserve_bandwidth(xhci, virt_dev, command->in_ctx)) {
		if ((xhci->quirks & XHCI_EP_LIMIT_QUIRK))
			xhci_free_host_resources(xhci, ctrl_ctx);
		spin_unlock_irqrestore(&xhci->lock, flags);
		xhci_warn(xhci, "Not enough bandwidth\n");
		return -ENOMEM;
	}

	slot_ctx = xhci_get_slot_ctx(xhci, command->in_ctx);

	trace_xhci_configure_endpoint_ctrl_ctx(ctrl_ctx);
	trace_xhci_configure_endpoint(slot_ctx);

	if (!ctx_change)
		ret = xhci_queue_configure_endpoint(xhci, command,
				command->in_ctx->dma,
				udev->slot_id, must_succeed);
	else
		ret = xhci_queue_evaluate_context(xhci, command,
				command->in_ctx->dma,
				udev->slot_id, must_succeed);
	if (ret < 0) {
		if ((xhci->quirks & XHCI_EP_LIMIT_QUIRK))
			xhci_free_host_resources(xhci, ctrl_ctx);
		spin_unlock_irqrestore(&xhci->lock, flags);
		xhci_dbg_trace(xhci,  trace_xhci_dbg_context_change,
				"FIXME allocate a new ring segment");
		return -ENOMEM;
	}
	xhci_ring_cmd_db(xhci);
	spin_unlock_irqrestore(&xhci->lock, flags);

	/* Wait for the configure endpoint command to complete */
	wait_for_completion(command->completion);

	if (!ctx_change)
		ret = xhci_configure_endpoint_result(xhci, udev,
						     &command->status);
	else
		ret = xhci_evaluate_context_result(xhci, udev,
						   &command->status);

	if ((xhci->quirks & XHCI_EP_LIMIT_QUIRK)) {
		spin_lock_irqsave(&xhci->lock, flags);
		/* If the command failed, remove the reserved resources.
		 * Otherwise, clean up the estimate to include dropped eps.
		 */
		if (ret)
			xhci_free_host_resources(xhci, ctrl_ctx);
		else
			xhci_finish_resource_reservation(xhci, ctrl_ctx);
		spin_unlock_irqrestore(&xhci->lock, flags);
	}
	return ret;
}

static void xhci_check_bw_drop_ep_streams(struct xhci_hcd *xhci,
	struct xhci_virt_device *vdev, int i)
{
	struct xhci_virt_ep *ep = &vdev->eps[i];

	if (ep->ep_state & EP_HAS_STREAMS) {
		xhci_warn(xhci, "WARN: endpoint 0x%02x has streams on set_interface, freeing streams.\n",
				xhci_get_endpoint_address(i));
		xhci_free_stream_info(xhci, ep->stream_info);
		ep->stream_info = NULL;
		ep->ep_state &= ~EP_HAS_STREAMS;
	}
}

/* Called after one or more calls to xhci_add_endpoint() or
 * xhci_drop_endpoint().  If this call fails, the USB core is expected
 * to call xhci_reset_bandwidth().
 *
 * Since we are in the middle of changing either configuration or
 * installing a new alt setting, the USB core won't allow URBs to be
 * enqueued for any endpoint on the old config or interface.  Nothing
 * else should be touching the xhci->devs[slot_id] structure, so we
 * don't need to take the xhci->lock for manipulating that.
 */
int xhci_check_bandwidth(struct usb_hcd *hcd, struct usb_device *udev)
{
	int i;
	int ret = 0;
	struct xhci_hcd *xhci;
	struct xhci_virt_device	*virt_dev;
	struct xhci_input_control_ctx *ctrl_ctx;
	struct xhci_slot_ctx *slot_ctx;
	struct xhci_command *command;

	ret = xhci_check_args(hcd, udev, NULL, 0, true, __func__);
	if (ret <= 0)
		return ret;
	xhci = hcd_to_xhci(hcd);
	if ((xhci->xhc_state & XHCI_STATE_DYING) ||
		(xhci->xhc_state & XHCI_STATE_REMOVING))
		return -ENODEV;

	xhci_dbg(xhci, "%s called for udev %p\n", __func__, udev);
	virt_dev = xhci->devs[udev->slot_id];

	command = xhci_alloc_command(xhci, true, GFP_KERNEL);
	if (!command)
		return -ENOMEM;

	command->in_ctx = virt_dev->in_ctx;

	/* See section 4.6.6 - A0 = 1; A1 = D0 = D1 = 0 */
	ctrl_ctx = xhci_get_input_control_ctx(command->in_ctx);
	if (!ctrl_ctx) {
		xhci_warn(xhci, "%s: Could not get input context, bad type.\n",
				__func__);
		ret = -ENOMEM;
		goto command_cleanup;
	}
	ctrl_ctx->add_flags |= cpu_to_le32(SLOT_FLAG);
	ctrl_ctx->add_flags &= cpu_to_le32(~EP0_FLAG);
	ctrl_ctx->drop_flags &= cpu_to_le32(~(SLOT_FLAG | EP0_FLAG));

	/* Don't issue the command if there's no endpoints to update. */
	if (ctrl_ctx->add_flags == cpu_to_le32(SLOT_FLAG) &&
	    ctrl_ctx->drop_flags == 0) {
		ret = 0;
		goto command_cleanup;
	}
	/* Fix up Context Entries field. Minimum value is EP0 == BIT(1). */
	slot_ctx = xhci_get_slot_ctx(xhci, virt_dev->in_ctx);
	for (i = 31; i >= 1; i--) {
		__le32 le32 = cpu_to_le32(BIT(i));

		if ((virt_dev->eps[i-1].ring && !(ctrl_ctx->drop_flags & le32))
		    || (ctrl_ctx->add_flags & le32) || i == 1) {
			slot_ctx->dev_info &= cpu_to_le32(~LAST_CTX_MASK);
			slot_ctx->dev_info |= cpu_to_le32(LAST_CTX(i));
			break;
		}
	}

	ret = xhci_configure_endpoint(xhci, udev, command,
			false, false);
	if (ret)
		/* Callee should call reset_bandwidth() */
		goto command_cleanup;

	/* Free any rings that were dropped, but not changed. */
	for (i = 1; i < 31; i++) {
		if ((le32_to_cpu(ctrl_ctx->drop_flags) & (1 << (i + 1))) &&
		    !(le32_to_cpu(ctrl_ctx->add_flags) & (1 << (i + 1)))) {
			xhci_free_endpoint_ring(xhci, virt_dev, i);
			xhci_check_bw_drop_ep_streams(xhci, virt_dev, i);
		}
	}
	xhci_zero_in_ctx(xhci, virt_dev);
	/*
	 * Install any rings for completely new endpoints or changed endpoints,
	 * and free any old rings from changed endpoints.
	 */
	for (i = 1; i < 31; i++) {
		if (!virt_dev->eps[i].new_ring)
			continue;
		/* Only free the old ring if it exists.
		 * It may not if this is the first add of an endpoint.
		 */
		if (virt_dev->eps[i].ring) {
			xhci_free_endpoint_ring(xhci, virt_dev, i);
		}
		xhci_check_bw_drop_ep_streams(xhci, virt_dev, i);
		virt_dev->eps[i].ring = virt_dev->eps[i].new_ring;
		virt_dev->eps[i].new_ring = NULL;
		xhci_debugfs_create_endpoint(xhci, virt_dev, i);
	}
command_cleanup:
	kfree(command->completion);
	kfree(command);

	return ret;
}
EXPORT_SYMBOL_GPL(xhci_check_bandwidth);

void xhci_reset_bandwidth(struct usb_hcd *hcd, struct usb_device *udev)
{
	struct xhci_hcd *xhci;
	struct xhci_virt_device	*virt_dev;
	int i, ret;

	ret = xhci_check_args(hcd, udev, NULL, 0, true, __func__);
	if (ret <= 0)
		return;
	xhci = hcd_to_xhci(hcd);

	xhci_dbg(xhci, "%s called for udev %p\n", __func__, udev);
	virt_dev = xhci->devs[udev->slot_id];
	/* Free any rings allocated for added endpoints */
	for (i = 0; i < 31; i++) {
		if (virt_dev->eps[i].new_ring) {
			xhci_debugfs_remove_endpoint(xhci, virt_dev, i);
			xhci_ring_free(xhci, virt_dev->eps[i].new_ring);
			virt_dev->eps[i].new_ring = NULL;
		}
	}
	xhci_zero_in_ctx(xhci, virt_dev);
}
EXPORT_SYMBOL_GPL(xhci_reset_bandwidth);

/* Get the available bandwidth of the ports under the xhci roothub */
int xhci_get_port_bandwidth(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx,
			    u8 dev_speed)
{
	struct xhci_command *cmd;
	unsigned long flags;
	int ret;

	if (!ctx || !xhci)
		return -EINVAL;

	cmd = xhci_alloc_command(xhci, true, GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->in_ctx = ctx;

	/* get xhci port bandwidth, refer to xhci rev1_2 protocol 4.6.15 */
	spin_lock_irqsave(&xhci->lock, flags);

	ret = xhci_queue_get_port_bw(xhci, cmd, ctx->dma, dev_speed, 0);
	if (ret) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		goto err_out;
	}
	xhci_ring_cmd_db(xhci);
	spin_unlock_irqrestore(&xhci->lock, flags);

	wait_for_completion(cmd->completion);
err_out:
	kfree(cmd->completion);
	kfree(cmd);

	return ret;
}

static void xhci_setup_input_ctx_for_config_ep(struct xhci_hcd *xhci,
		struct xhci_container_ctx *in_ctx,
		struct xhci_container_ctx *out_ctx,
		struct xhci_input_control_ctx *ctrl_ctx,
		u32 add_flags, u32 drop_flags)
{
	ctrl_ctx->add_flags = cpu_to_le32(add_flags);
	ctrl_ctx->drop_flags = cpu_to_le32(drop_flags);
	xhci_slot_copy(xhci, in_ctx, out_ctx);
	ctrl_ctx->add_flags |= cpu_to_le32(SLOT_FLAG);
}

static void xhci_endpoint_disable(struct usb_hcd *hcd,
				  struct usb_host_endpoint *host_ep)
{
	struct xhci_hcd		*xhci;
	struct xhci_virt_device	*vdev;
	struct xhci_virt_ep	*ep;
	struct usb_device	*udev;
	unsigned long		flags;
	unsigned int		ep_index;

	xhci = hcd_to_xhci(hcd);
rescan:
	spin_lock_irqsave(&xhci->lock, flags);

	udev = (struct usb_device *)host_ep->hcpriv;
	if (!udev || !udev->slot_id)
		goto done;

	vdev = xhci->devs[udev->slot_id];
	if (!vdev)
		goto done;

	ep_index = xhci_get_endpoint_index(&host_ep->desc);
	ep = &vdev->eps[ep_index];

	/* wait for hub_tt_work to finish clearing hub TT */
	if (ep->ep_state & EP_CLEARING_TT) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		schedule_timeout_uninterruptible(1);
		goto rescan;
	}

	if (ep->ep_state)
		xhci_dbg(xhci, "endpoint disable with ep_state 0x%x\n",
			 ep->ep_state);
done:
	host_ep->hcpriv = NULL;
	spin_unlock_irqrestore(&xhci->lock, flags);
}

/*
 * Called after usb core issues a clear halt control message.
 * The host side of the halt should already be cleared by a reset endpoint
 * command issued when the STALL event was received.
 *
 * The reset endpoint command may only be issued to endpoints in the halted
 * state. For software that wishes to reset the data toggle or sequence number
 * of an endpoint that isn't in the halted state this function will issue a
 * configure endpoint command with the Drop and Add bits set for the target
 * endpoint. Refer to the additional note in xhci spcification section 4.6.8.
 *
 * vdev may be lost due to xHC restore error and re-initialization during S3/S4
 * resume. A new vdev will be allocated later by xhci_discover_or_reset_device()
 */

static void xhci_endpoint_reset(struct usb_hcd *hcd,
		struct usb_host_endpoint *host_ep)
{
	struct xhci_hcd *xhci;
	struct usb_device *udev;
	struct xhci_virt_device *vdev;
	struct xhci_virt_ep *ep;
	struct xhci_input_control_ctx *ctrl_ctx;
	struct xhci_command *stop_cmd, *cfg_cmd;
	unsigned int ep_index;
	unsigned long flags;
	u32 ep_flag;
	int err;

	xhci = hcd_to_xhci(hcd);
	ep_index = xhci_get_endpoint_index(&host_ep->desc);

	/*
	 * Usb core assumes a max packet value for ep0 on FS devices until the
	 * real value is read from the descriptor. Core resets Ep0 if values
	 * mismatch. Reconfigure the xhci ep0 endpoint context here in that case
	 */
	if (usb_endpoint_xfer_control(&host_ep->desc) && ep_index == 0) {

		udev = container_of(host_ep, struct usb_device, ep0);
		if (udev->speed != USB_SPEED_FULL || !udev->slot_id)
			return;

		vdev = xhci->devs[udev->slot_id];
		if (!vdev || vdev->udev != udev)
			return;

		xhci_check_ep0_maxpacket(xhci, vdev);

		/* Nothing else should be done here for ep0 during ep reset */
		return;
	}

	if (!host_ep->hcpriv)
		return;
	udev = (struct usb_device *) host_ep->hcpriv;
	vdev = xhci->devs[udev->slot_id];

	if (!udev->slot_id || !vdev)
		return;

	ep = &vdev->eps[ep_index];

	/* Bail out if toggle is already being cleared by a endpoint reset */
	spin_lock_irqsave(&xhci->lock, flags);
	if (ep->ep_state & EP_HARD_CLEAR_TOGGLE) {
		ep->ep_state &= ~EP_HARD_CLEAR_TOGGLE;
		spin_unlock_irqrestore(&xhci->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&xhci->lock, flags);
	/* Only interrupt and bulk ep's use data toggle, USB2 spec 5.5.4-> */
	if (usb_endpoint_xfer_control(&host_ep->desc) ||
	    usb_endpoint_xfer_isoc(&host_ep->desc))
		return;

	ep_flag = xhci_get_endpoint_flag(&host_ep->desc);

	if (ep_flag == SLOT_FLAG || ep_flag == EP0_FLAG)
		return;

	stop_cmd = xhci_alloc_command(xhci, true, GFP_NOWAIT);
	if (!stop_cmd)
		return;

	cfg_cmd = xhci_alloc_command_with_ctx(xhci, true, GFP_NOWAIT);
	if (!cfg_cmd)
		goto cleanup;

	spin_lock_irqsave(&xhci->lock, flags);

	/* block queuing new trbs and ringing ep doorbell */
	ep->ep_state |= EP_SOFT_CLEAR_TOGGLE;

	/*
	 * Make sure endpoint ring is empty before resetting the toggle/seq.
	 * Driver is required to synchronously cancel all transfer request.
	 * Stop the endpoint to force xHC to update the output context
	 */

	if (!list_empty(&ep->ring->td_list)) {
		dev_err(&udev->dev, "EP not empty, refuse reset\n");
		spin_unlock_irqrestore(&xhci->lock, flags);
		xhci_free_command(xhci, cfg_cmd);
		goto cleanup;
	}

	err = xhci_queue_stop_endpoint(xhci, stop_cmd, udev->slot_id,
					ep_index, 0);
	if (err < 0) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		xhci_free_command(xhci, cfg_cmd);
		xhci_dbg(xhci, "%s: Failed to queue stop ep command, %d ",
				__func__, err);
		goto cleanup;
	}

	xhci_ring_cmd_db(xhci);
	spin_unlock_irqrestore(&xhci->lock, flags);

	wait_for_completion(stop_cmd->completion);

	spin_lock_irqsave(&xhci->lock, flags);

	/* config ep command clears toggle if add and drop ep flags are set */
	ctrl_ctx = xhci_get_input_control_ctx(cfg_cmd->in_ctx);
	if (!ctrl_ctx) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		xhci_free_command(xhci, cfg_cmd);
		xhci_warn(xhci, "%s: Could not get input context, bad type.\n",
				__func__);
		goto cleanup;
	}

	xhci_setup_input_ctx_for_config_ep(xhci, cfg_cmd->in_ctx, vdev->out_ctx,
					   ctrl_ctx, ep_flag, ep_flag);
	xhci_endpoint_copy(xhci, cfg_cmd->in_ctx, vdev->out_ctx, ep_index);

	err = xhci_queue_configure_endpoint(xhci, cfg_cmd, cfg_cmd->in_ctx->dma,
				      udev->slot_id, false);
	if (err < 0) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		xhci_free_command(xhci, cfg_cmd);
		xhci_dbg(xhci, "%s: Failed to queue config ep command, %d ",
				__func__, err);
		goto cleanup;
	}

	xhci_ring_cmd_db(xhci);
	spin_unlock_irqrestore(&xhci->lock, flags);

	wait_for_completion(cfg_cmd->completion);

	xhci_free_command(xhci, cfg_cmd);
cleanup:
	xhci_free_command(xhci, stop_cmd);
	spin_lock_irqsave(&xhci->lock, flags);
	if (ep->ep_state & EP_SOFT_CLEAR_TOGGLE)
		ep->ep_state &= ~EP_SOFT_CLEAR_TOGGLE;
	spin_unlock_irqrestore(&xhci->lock, flags);
}

static int xhci_check_streams_endpoint(struct xhci_hcd *xhci,
		struct usb_device *udev, struct usb_host_endpoint *ep,
		unsigned int slot_id)
{
	int ret;
	unsigned int ep_index;
	unsigned int ep_state;

	if (!ep)
		return -EINVAL;
	ret = xhci_check_args(xhci_to_hcd(xhci), udev, ep, 1, true, __func__);
	if (ret <= 0)
		return ret ? ret : -EINVAL;
	if (usb_ss_max_streams(&ep->ss_ep_comp) == 0) {
		xhci_warn(xhci, "WARN: SuperSpeed Endpoint Companion"
				" descriptor for ep 0x%x does not support streams\n",
				ep->desc.bEndpointAddress);
		return -EINVAL;
	}

	ep_index = xhci_get_endpoint_index(&ep->desc);
	ep_state = xhci->devs[slot_id]->eps[ep_index].ep_state;
	if (ep_state & EP_HAS_STREAMS ||
			ep_state & EP_GETTING_STREAMS) {
		xhci_warn(xhci, "WARN: SuperSpeed bulk endpoint 0x%x "
				"already has streams set up.\n",
				ep->desc.bEndpointAddress);
		xhci_warn(xhci, "Send email to xHCI maintainer and ask for "
				"dynamic stream context array reallocation.\n");
		return -EINVAL;
	}
	if (!list_empty(&xhci->devs[slot_id]->eps[ep_index].ring->td_list)) {
		xhci_warn(xhci, "Cannot setup streams for SuperSpeed bulk "
				"endpoint 0x%x; URBs are pending.\n",
				ep->desc.bEndpointAddress);
		return -EINVAL;
	}
	return 0;
}

static void xhci_calculate_streams_entries(struct xhci_hcd *xhci,
		unsigned int *num_streams, unsigned int *num_stream_ctxs)
{
	unsigned int max_streams;

	/* The stream context array size must be a power of two */
	*num_stream_ctxs = roundup_pow_of_two(*num_streams);
	/*
	 * Find out how many primary stream array entries the host controller
	 * supports.  Later we may use secondary stream arrays (similar to 2nd
	 * level page entries), but that's an optional feature for xHCI host
	 * controllers. xHCs must support at least 4 stream IDs.
	 */
	max_streams = HCC_MAX_PSA(xhci->hcc_params);
	if (*num_stream_ctxs > max_streams) {
		xhci_dbg(xhci, "xHCI HW only supports %u stream ctx entries.\n",
				max_streams);
		*num_stream_ctxs = max_streams;
		*num_streams = max_streams;
	}
}

/* Returns an error code if one of the endpoint already has streams.
 * This does not change any data structures, it only checks and gathers
 * information.
 */
static int xhci_calculate_streams_and_bitmask(struct xhci_hcd *xhci,
		struct usb_device *udev,
		struct usb_host_endpoint **eps, unsigned int num_eps,
		unsigned int *num_streams, u32 *changed_ep_bitmask)
{
	unsigned int max_streams;
	unsigned int endpoint_flag;
	int i;
	int ret;

	for (i = 0; i < num_eps; i++) {
		ret = xhci_check_streams_endpoint(xhci, udev,
				eps[i], udev->slot_id);
		if (ret < 0)
			return ret;

		max_streams = usb_ss_max_streams(&eps[i]->ss_ep_comp);
		if (max_streams < (*num_streams - 1)) {
			xhci_dbg(xhci, "Ep 0x%x only supports %u stream IDs.\n",
					eps[i]->desc.bEndpointAddress,
					max_streams);
			*num_streams = max_streams+1;
		}

		endpoint_flag = xhci_get_endpoint_flag(&eps[i]->desc);
		if (*changed_ep_bitmask & endpoint_flag)
			return -EINVAL;
		*changed_ep_bitmask |= endpoint_flag;
	}
	return 0;
}

static u32 xhci_calculate_no_streams_bitmask(struct xhci_hcd *xhci,
		struct usb_device *udev,
		struct usb_host_endpoint **eps, unsigned int num_eps)
{
	u32 changed_ep_bitmask = 0;
	unsigned int slot_id;
	unsigned int ep_index;
	unsigned int ep_state;
	int i;

	slot_id = udev->slot_id;
	if (!xhci->devs[slot_id])
		return 0;

	for (i = 0; i < num_eps; i++) {
		ep_index = xhci_get_endpoint_index(&eps[i]->desc);
		ep_state = xhci->devs[slot_id]->eps[ep_index].ep_state;
		/* Are streams already being freed for the endpoint? */
		if (ep_state & EP_GETTING_NO_STREAMS) {
			xhci_warn(xhci, "WARN Can't disable streams for "
					"endpoint 0x%x, "
					"streams are being disabled already\n",
					eps[i]->desc.bEndpointAddress);
			return 0;
		}
		/* Are there actually any streams to free? */
		if (!(ep_state & EP_HAS_STREAMS) &&
				!(ep_state & EP_GETTING_STREAMS)) {
			xhci_warn(xhci, "WARN Can't disable streams for "
					"endpoint 0x%x, "
					"streams are already disabled!\n",
					eps[i]->desc.bEndpointAddress);
			xhci_warn(xhci, "WARN xhci_free_streams() called "
					"with non-streams endpoint\n");
			return 0;
		}
		changed_ep_bitmask |= xhci_get_endpoint_flag(&eps[i]->desc);
	}
	return changed_ep_bitmask;
}

/*
 * The USB device drivers use this function (through the HCD interface in USB
 * core) to prepare a set of bulk endpoints to use streams.  Streams are used to
 * coordinate mass storage command queueing across multiple endpoints (basically
 * a stream ID == a task ID).
 *
 * Setting up streams involves allocating the same size stream context array
 * for each endpoint and issuing a configure endpoint command for all endpoints.
 *
 * Don't allow the call to succeed if one endpoint only supports one stream
 * (which means it doesn't support streams at all).
 *
 * Drivers may get less stream IDs than they asked for, if the host controller
 * hardware or endpoints claim they can't support the number of requested
 * stream IDs.
 */
static int xhci_alloc_streams(struct usb_hcd *hcd, struct usb_device *udev,
		struct usb_host_endpoint **eps, unsigned int num_eps,
		unsigned int num_streams, gfp_t mem_flags)
{
	int i, ret;
	struct xhci_hcd *xhci;
	struct xhci_virt_device *vdev;
	struct xhci_command *config_cmd;
	struct xhci_input_control_ctx *ctrl_ctx;
	unsigned int ep_index;
	unsigned int num_stream_ctxs;
	unsigned int max_packet;
	unsigned long flags;
	u32 changed_ep_bitmask = 0;

	if (!eps)
		return -EINVAL;

	/* Add one to the number of streams requested to account for
	 * stream 0 that is reserved for xHCI usage.
	 */
	num_streams += 1;
	xhci = hcd_to_xhci(hcd);
	xhci_dbg(xhci, "Driver wants %u stream IDs (including stream 0).\n",
			num_streams);

	/* MaxPSASize value 0 (2 streams) means streams are not supported */
	if ((xhci->quirks & XHCI_BROKEN_STREAMS) ||
			HCC_MAX_PSA(xhci->hcc_params) < 4) {
		xhci_dbg(xhci, "xHCI controller does not support streams.\n");
		return -ENOSYS;
	}

	config_cmd = xhci_alloc_command_with_ctx(xhci, true, mem_flags);
	if (!config_cmd)
		return -ENOMEM;

	ctrl_ctx = xhci_get_input_control_ctx(config_cmd->in_ctx);
	if (!ctrl_ctx) {
		xhci_warn(xhci, "%s: Could not get input context, bad type.\n",
				__func__);
		xhci_free_command(xhci, config_cmd);
		return -ENOMEM;
	}

	/* Check to make sure all endpoints are not already configured for
	 * streams.  While we're at it, find the maximum number of streams that
	 * all the endpoints will support and check for duplicate endpoints.
	 */
	spin_lock_irqsave(&xhci->lock, flags);
	ret = xhci_calculate_streams_and_bitmask(xhci, udev, eps,
			num_eps, &num_streams, &changed_ep_bitmask);
	if (ret < 0) {
		xhci_free_command(xhci, config_cmd);
		spin_unlock_irqrestore(&xhci->lock, flags);
		return ret;
	}
	if (num_streams <= 1) {
		xhci_warn(xhci, "WARN: endpoints can't handle "
				"more than one stream.\n");
		xhci_free_command(xhci, config_cmd);
		spin_unlock_irqrestore(&xhci->lock, flags);
		return -EINVAL;
	}
	vdev = xhci->devs[udev->slot_id];
	/* Mark each endpoint as being in transition, so
	 * xhci_urb_enqueue() will reject all URBs.
	 */
	for (i = 0; i < num_eps; i++) {
		ep_index = xhci_get_endpoint_index(&eps[i]->desc);
		vdev->eps[ep_index].ep_state |= EP_GETTING_STREAMS;
	}
	spin_unlock_irqrestore(&xhci->lock, flags);

	/* Setup internal data structures and allocate HW data structures for
	 * streams (but don't install the HW structures in the input context
	 * until we're sure all memory allocation succeeded).
	 */
	xhci_calculate_streams_entries(xhci, &num_streams, &num_stream_ctxs);
	xhci_dbg(xhci, "Need %u stream ctx entries for %u stream IDs.\n",
			num_stream_ctxs, num_streams);

	for (i = 0; i < num_eps; i++) {
		ep_index = xhci_get_endpoint_index(&eps[i]->desc);
		max_packet = usb_endpoint_maxp(&eps[i]->desc);
		vdev->eps[ep_index].stream_info = xhci_alloc_stream_info(xhci,
				num_stream_ctxs,
				num_streams,
				max_packet, mem_flags);
		if (!vdev->eps[ep_index].stream_info)
			goto cleanup;
		/* Set maxPstreams in endpoint context and update deq ptr to
		 * point to stream context array. FIXME
		 */
	}

	/* Set up the input context for a configure endpoint command. */
	for (i = 0; i < num_eps; i++) {
		struct xhci_ep_ctx *ep_ctx;

		ep_index = xhci_get_endpoint_index(&eps[i]->desc);
		ep_ctx = xhci_get_ep_ctx(xhci, config_cmd->in_ctx, ep_index);

		xhci_endpoint_copy(xhci, config_cmd->in_ctx,
				vdev->out_ctx, ep_index);
		xhci_setup_streams_ep_input_ctx(xhci, ep_ctx,
				vdev->eps[ep_index].stream_info);
	}
	/* Tell the HW to drop its old copy of the endpoint context info
	 * and add the updated copy from the input context.
	 */
	xhci_setup_input_ctx_for_config_ep(xhci, config_cmd->in_ctx,
			vdev->out_ctx, ctrl_ctx,
			changed_ep_bitmask, changed_ep_bitmask);

	/* Issue and wait for the configure endpoint command */
	ret = xhci_configure_endpoint(xhci, udev, config_cmd,
			false, false);

	/* xHC rejected the configure endpoint command for some reason, so we
	 * leave the old ring intact and free our internal streams data
	 * structure.
	 */
	if (ret < 0)
		goto cleanup;

	spin_lock_irqsave(&xhci->lock, flags);
	for (i = 0; i < num_eps; i++) {
		ep_index = xhci_get_endpoint_index(&eps[i]->desc);
		vdev->eps[ep_index].ep_state &= ~EP_GETTING_STREAMS;
		xhci_dbg(xhci, "Slot %u ep ctx %u now has streams.\n",
			 udev->slot_id, ep_index);
		vdev->eps[ep_index].ep_state |= EP_HAS_STREAMS;
	}
	xhci_free_command(xhci, config_cmd);
	spin_unlock_irqrestore(&xhci->lock, flags);

	for (i = 0; i < num_eps; i++) {
		ep_index = xhci_get_endpoint_index(&eps[i]->desc);
		xhci_debugfs_create_stream_files(xhci, vdev, ep_index);
	}
	/* Subtract 1 for stream 0, which drivers can't use */
	return num_streams - 1;

cleanup:
	/* If it didn't work, free the streams! */
	for (i = 0; i < num_eps; i++) {
		ep_index = xhci_get_endpoint_index(&eps[i]->desc);
		xhci_free_stream_info(xhci, vdev->eps[ep_index].stream_info);
		vdev->eps[ep_index].stream_info = NULL;
		/* FIXME Unset maxPstreams in endpoint context and
		 * update deq ptr to point to normal string ring.
		 */
		vdev->eps[ep_index].ep_state &= ~EP_GETTING_STREAMS;
		vdev->eps[ep_index].ep_state &= ~EP_HAS_STREAMS;
		xhci_endpoint_zero(xhci, vdev, eps[i]);
	}
	xhci_free_command(xhci, config_cmd);
	return -ENOMEM;
}

/* Transition the endpoint from using streams to being a "normal" endpoint
 * without streams.
 *
 * Modify the endpoint context state, submit a configure endpoint command,
 * and free all endpoint rings for streams if that completes successfully.
 */
static int xhci_free_streams(struct usb_hcd *hcd, struct usb_device *udev,
		struct usb_host_endpoint **eps, unsigned int num_eps,
		gfp_t mem_flags)
{
	int i, ret;
	struct xhci_hcd *xhci;
	struct xhci_virt_device *vdev;
	struct xhci_command *command;
	struct xhci_input_control_ctx *ctrl_ctx;
	unsigned int ep_index;
	unsigned long flags;
	u32 changed_ep_bitmask;

	xhci = hcd_to_xhci(hcd);
	vdev = xhci->devs[udev->slot_id];

	/* Set up a configure endpoint command to remove the streams rings */
	spin_lock_irqsave(&xhci->lock, flags);
	changed_ep_bitmask = xhci_calculate_no_streams_bitmask(xhci,
			udev, eps, num_eps);
	if (changed_ep_bitmask == 0) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		return -EINVAL;
	}

	/* Use the xhci_command structure from the first endpoint.  We may have
	 * allocated too many, but the driver may call xhci_free_streams() for
	 * each endpoint it grouped into one call to xhci_alloc_streams().
	 */
	ep_index = xhci_get_endpoint_index(&eps[0]->desc);
	command = vdev->eps[ep_index].stream_info->free_streams_command;
	ctrl_ctx = xhci_get_input_control_ctx(command->in_ctx);
	if (!ctrl_ctx) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		xhci_warn(xhci, "%s: Could not get input context, bad type.\n",
				__func__);
		return -EINVAL;
	}

	for (i = 0; i < num_eps; i++) {
		struct xhci_ep_ctx *ep_ctx;

		ep_index = xhci_get_endpoint_index(&eps[i]->desc);
		ep_ctx = xhci_get_ep_ctx(xhci, command->in_ctx, ep_index);
		xhci->devs[udev->slot_id]->eps[ep_index].ep_state |=
			EP_GETTING_NO_STREAMS;

		xhci_endpoint_copy(xhci, command->in_ctx,
				vdev->out_ctx, ep_index);
		xhci_setup_no_streams_ep_input_ctx(ep_ctx,
				&vdev->eps[ep_index]);
	}
	xhci_setup_input_ctx_for_config_ep(xhci, command->in_ctx,
			vdev->out_ctx, ctrl_ctx,
			changed_ep_bitmask, changed_ep_bitmask);
	spin_unlock_irqrestore(&xhci->lock, flags);

	/* Issue and wait for the configure endpoint command,
	 * which must succeed.
	 */
	ret = xhci_configure_endpoint(xhci, udev, command,
			false, true);

	/* xHC rejected the configure endpoint command for some reason, so we
	 * leave the streams rings intact.
	 */
	if (ret < 0)
		return ret;

	spin_lock_irqsave(&xhci->lock, flags);
	for (i = 0; i < num_eps; i++) {
		ep_index = xhci_get_endpoint_index(&eps[i]->desc);
		xhci_free_stream_info(xhci, vdev->eps[ep_index].stream_info);
		vdev->eps[ep_index].stream_info = NULL;
		/* FIXME Unset maxPstreams in endpoint context and
		 * update deq ptr to point to normal string ring.
		 */
		vdev->eps[ep_index].ep_state &= ~EP_GETTING_NO_STREAMS;
		vdev->eps[ep_index].ep_state &= ~EP_HAS_STREAMS;
	}
	spin_unlock_irqrestore(&xhci->lock, flags);

	return 0;
}

/*
 * Deletes endpoint resources for endpoints that were active before a Reset
 * Device command, or a Disable Slot command.  The Reset Device command leaves
 * the control endpoint intact, whereas the Disable Slot command deletes it.
 *
 * Must be called with xhci->lock held.
 */
void xhci_free_device_endpoint_resources(struct xhci_hcd *xhci,
	struct xhci_virt_device *virt_dev, bool drop_control_ep)
{
	int i;
	unsigned int num_dropped_eps = 0;
	unsigned int drop_flags = 0;

	for (i = (drop_control_ep ? 0 : 1); i < 31; i++) {
		if (virt_dev->eps[i].ring) {
			drop_flags |= 1 << i;
			num_dropped_eps++;
		}
	}
	xhci->num_active_eps -= num_dropped_eps;
	if (num_dropped_eps)
		xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
				"Dropped %u ep ctxs, flags = 0x%x, "
				"%u now active.",
				num_dropped_eps, drop_flags,
				xhci->num_active_eps);
}

static void xhci_free_dev(struct usb_hcd *hcd, struct usb_device *udev);

/*
 * This submits a Reset Device Command, which will set the device state to 0,
 * set the device address to 0, and disable all the endpoints except the default
 * control endpoint.  The USB core should come back and call
 * xhci_address_device(), and then re-set up the configuration.  If this is
 * called because of a usb_reset_and_verify_device(), then the old alternate
 * settings will be re-installed through the normal bandwidth allocation
 * functions.
 *
 * Wait for the Reset Device command to finish.  Remove all structures
 * associated with the endpoints that were disabled.  Clear the input device
 * structure? Reset the control endpoint 0 max packet size?
 *
 * If the virt_dev to be reset does not exist or does not match the udev,
 * it means the device is lost, possibly due to the xHC restore error and
 * re-initialization during S3/S4. In this case, call xhci_alloc_dev() to
 * re-allocate the device.
 */
static int xhci_discover_or_reset_device(struct usb_hcd *hcd,
		struct usb_device *udev)
{
	int ret, i;
	unsigned long flags;
	struct xhci_hcd *xhci;
	unsigned int slot_id;
	struct xhci_virt_device *virt_dev;
	struct xhci_command *reset_device_cmd;
	struct xhci_slot_ctx *slot_ctx;
	int old_active_eps = 0;

	ret = xhci_check_args(hcd, udev, NULL, 0, false, __func__);
	if (ret <= 0)
		return ret;
	xhci = hcd_to_xhci(hcd);
	slot_id = udev->slot_id;
	virt_dev = xhci->devs[slot_id];
	if (!virt_dev) {
		xhci_dbg(xhci, "The device to be reset with slot ID %u does "
				"not exist. Re-allocate the device\n", slot_id);
		ret = xhci_alloc_dev(hcd, udev);
		if (ret == 1)
			return 0;
		else
			return -EINVAL;
	}

	if (virt_dev->tt_info)
		old_active_eps = virt_dev->tt_info->active_eps;

	if (virt_dev->udev != udev) {
		/* If the virt_dev and the udev does not match, this virt_dev
		 * may belong to another udev.
		 * Re-allocate the device.
		 */
		xhci_dbg(xhci, "The device to be reset with slot ID %u does "
				"not match the udev. Re-allocate the device\n",
				slot_id);
		ret = xhci_alloc_dev(hcd, udev);
		if (ret == 1)
			return 0;
		else
			return -EINVAL;
	}

	/* If device is not setup, there is no point in resetting it */
	slot_ctx = xhci_get_slot_ctx(xhci, virt_dev->out_ctx);
	if (GET_SLOT_STATE(le32_to_cpu(slot_ctx->dev_state)) ==
						SLOT_STATE_DISABLED)
		return 0;

	if (xhci->quirks & XHCI_ETRON_HOST) {
		/*
		 * Obtaining a new device slot to inform the xHCI host that
		 * the USB device has been reset.
		 */
		ret = xhci_disable_and_free_slot(xhci, udev->slot_id);
		if (!ret) {
			ret = xhci_alloc_dev(hcd, udev);
			if (ret == 1)
				ret = 0;
			else
				ret = -EINVAL;
		}
		return ret;
	}

	trace_xhci_discover_or_reset_device(slot_ctx);

	xhci_dbg(xhci, "Resetting device with slot ID %u\n", slot_id);
	/* Allocate the command structure that holds the struct completion.
	 * Assume we're in process context, since the normal device reset
	 * process has to wait for the device anyway.  Storage devices are
	 * reset as part of error handling, so use GFP_NOIO instead of
	 * GFP_KERNEL.
	 */
	reset_device_cmd = xhci_alloc_command(xhci, true, GFP_NOIO);
	if (!reset_device_cmd) {
		xhci_dbg(xhci, "Couldn't allocate command structure.\n");
		return -ENOMEM;
	}

	/* Attempt to submit the Reset Device command to the command ring */
	spin_lock_irqsave(&xhci->lock, flags);

	ret = xhci_queue_reset_device(xhci, reset_device_cmd, slot_id);
	if (ret) {
		xhci_dbg(xhci, "FIXME: allocate a command ring segment\n");
		spin_unlock_irqrestore(&xhci->lock, flags);
		goto command_cleanup;
	}
	xhci_ring_cmd_db(xhci);
	spin_unlock_irqrestore(&xhci->lock, flags);

	/* Wait for the Reset Device command to finish */
	wait_for_completion(reset_device_cmd->completion);

	/* The Reset Device command can't fail, according to the 0.95/0.96 spec,
	 * unless we tried to reset a slot ID that wasn't enabled,
	 * or the device wasn't in the addressed or configured state.
	 */
	ret = reset_device_cmd->status;
	switch (ret) {
	case COMP_COMMAND_ABORTED:
	case COMP_COMMAND_RING_STOPPED:
		xhci_warn(xhci, "Timeout waiting for reset device command\n");
		ret = -ETIME;
		goto command_cleanup;
	case COMP_SLOT_NOT_ENABLED_ERROR: /* 0.95 completion for bad slot ID */
	case COMP_CONTEXT_STATE_ERROR: /* 0.96 completion code for same thing */
		xhci_dbg(xhci, "Can't reset device (slot ID %u) in %s state\n",
				slot_id,
				xhci_get_slot_state(xhci, virt_dev->out_ctx));
		xhci_dbg(xhci, "Not freeing device rings.\n");
		/* Don't treat this as an error.  May change my mind later. */
		ret = 0;
		goto command_cleanup;
	case COMP_SUCCESS:
		xhci_dbg(xhci, "Successful reset device command.\n");
		break;
	default:
		if (xhci_is_vendor_info_code(xhci, ret))
			break;
		xhci_warn(xhci, "Unknown completion code %u for "
				"reset device command.\n", ret);
		ret = -EINVAL;
		goto command_cleanup;
	}

	/* Free up host controller endpoint resources */
	if ((xhci->quirks & XHCI_EP_LIMIT_QUIRK)) {
		spin_lock_irqsave(&xhci->lock, flags);
		/* Don't delete the default control endpoint resources */
		xhci_free_device_endpoint_resources(xhci, virt_dev, false);
		spin_unlock_irqrestore(&xhci->lock, flags);
	}

	/* Everything but endpoint 0 is disabled, so free the rings. */
	for (i = 1; i < 31; i++) {
		struct xhci_virt_ep *ep = &virt_dev->eps[i];

		if (ep->ep_state & EP_HAS_STREAMS) {
			xhci_warn(xhci, "WARN: endpoint 0x%02x has streams on device reset, freeing streams.\n",
					xhci_get_endpoint_address(i));
			xhci_free_stream_info(xhci, ep->stream_info);
			ep->stream_info = NULL;
			ep->ep_state &= ~EP_HAS_STREAMS;
		}

		if (ep->ring) {
			if (ep->sideband)
				xhci_sideband_notify_ep_ring_free(ep->sideband, i);
			xhci_debugfs_remove_endpoint(xhci, virt_dev, i);
			xhci_free_endpoint_ring(xhci, virt_dev, i);
		}
		if (!list_empty(&virt_dev->eps[i].bw_endpoint_list))
			xhci_drop_ep_from_interval_table(xhci,
					&virt_dev->eps[i].bw_info,
					virt_dev->bw_table,
					udev,
					&virt_dev->eps[i],
					virt_dev->tt_info);
		xhci_clear_endpoint_bw_info(&virt_dev->eps[i].bw_info);
	}
	/* If necessary, update the number of active TTs on this root port */
	xhci_update_tt_active_eps(xhci, virt_dev, old_active_eps);
	virt_dev->flags = 0;
	ret = 0;

command_cleanup:
	xhci_free_command(xhci, reset_device_cmd);
	return ret;
}

/*
 * At this point, the struct usb_device is about to go away, the device has
 * disconnected, and all traffic has been stopped and the endpoints have been
 * disabled.  Free any HC data structures associated with that device.
 */
static void xhci_free_dev(struct usb_hcd *hcd, struct usb_device *udev)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct xhci_virt_device *virt_dev;
	struct xhci_slot_ctx *slot_ctx;
	unsigned long flags;
	int i, ret;

	/*
	 * We called pm_runtime_get_noresume when the device was attached.
	 * Decrement the counter here to allow controller to runtime suspend
	 * if no devices remain.
	 */
	if (xhci->quirks & XHCI_RESET_ON_RESUME)
		pm_runtime_put_noidle(hcd->self.controller);

	ret = xhci_check_args(hcd, udev, NULL, 0, true, __func__);
	/* If the host is halted due to driver unload, we still need to free the
	 * device.
	 */
	if (ret <= 0 && ret != -ENODEV)
		return;

	virt_dev = xhci->devs[udev->slot_id];
	slot_ctx = xhci_get_slot_ctx(xhci, virt_dev->out_ctx);
	trace_xhci_free_dev(slot_ctx);

	/* Stop any wayward timer functions (which may grab the lock) */
	for (i = 0; i < 31; i++)
		virt_dev->eps[i].ep_state &= ~EP_STOP_CMD_PENDING;
	virt_dev->udev = NULL;
	xhci_disable_slot(xhci, udev->slot_id);

	spin_lock_irqsave(&xhci->lock, flags);
	xhci_free_virt_device(xhci, virt_dev, udev->slot_id);
	spin_unlock_irqrestore(&xhci->lock, flags);

}

int xhci_disable_slot(struct xhci_hcd *xhci, u32 slot_id)
{
	struct xhci_command *command;
	unsigned long flags;
	u32 state;
	int ret;

	command = xhci_alloc_command(xhci, true, GFP_KERNEL);
	if (!command)
		return -ENOMEM;

	xhci_debugfs_remove_slot(xhci, slot_id);

	spin_lock_irqsave(&xhci->lock, flags);
	/* Don't disable the slot if the host controller is dead. */
	state = readl(&xhci->op_regs->status);
	if (state == 0xffffffff || (xhci->xhc_state & XHCI_STATE_DYING) ||
			(xhci->xhc_state & XHCI_STATE_HALTED)) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		kfree(command);
		return -ENODEV;
	}

	ret = xhci_queue_slot_control(xhci, command, TRB_DISABLE_SLOT,
				slot_id);
	if (ret) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		kfree(command);
		return ret;
	}
	xhci_ring_cmd_db(xhci);
	spin_unlock_irqrestore(&xhci->lock, flags);

	wait_for_completion(command->completion);

	if (command->status != COMP_SUCCESS)
		xhci_warn(xhci, "Unsuccessful disable slot %u command, status %d\n",
			  slot_id, command->status);

	xhci_free_command(xhci, command);

	return 0;
}

int xhci_disable_and_free_slot(struct xhci_hcd *xhci, u32 slot_id)
{
	struct xhci_virt_device *vdev = xhci->devs[slot_id];
	int ret;

	ret = xhci_disable_slot(xhci, slot_id);
	xhci_free_virt_device(xhci, vdev, slot_id);
	return ret;
}

/*
 * Checks if we have enough host controller resources for the default control
 * endpoint.
 *
 * Must be called with xhci->lock held.
 */
static int xhci_reserve_host_control_ep_resources(struct xhci_hcd *xhci)
{
	if (xhci->num_active_eps + 1 > xhci->limit_active_eps) {
		xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
				"Not enough ep ctxs: "
				"%u active, need to add 1, limit is %u.",
				xhci->num_active_eps, xhci->limit_active_eps);
		return -ENOMEM;
	}
	xhci->num_active_eps += 1;
	xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
			"Adding 1 ep ctx, %u now active.",
			xhci->num_active_eps);
	return 0;
}


/*
 * Returns 0 if the xHC ran out of device slots, the Enable Slot command
 * timed out, or allocating memory failed.  Returns 1 on success.
 */
int xhci_alloc_dev(struct usb_hcd *hcd, struct usb_device *udev)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct xhci_virt_device *vdev;
	struct xhci_slot_ctx *slot_ctx;
	unsigned long flags;
	int ret, slot_id;
	struct xhci_command *command;

	command = xhci_alloc_command(xhci, true, GFP_KERNEL);
	if (!command)
		return 0;

	spin_lock_irqsave(&xhci->lock, flags);
	ret = xhci_queue_slot_control(xhci, command, TRB_ENABLE_SLOT, 0);
	if (ret) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		xhci_dbg(xhci, "FIXME: allocate a command ring segment\n");
		xhci_free_command(xhci, command);
		return 0;
	}
	xhci_ring_cmd_db(xhci);
	spin_unlock_irqrestore(&xhci->lock, flags);

	wait_for_completion(command->completion);
	slot_id = command->slot_id;

	if (!slot_id || command->status != COMP_SUCCESS) {
		xhci_err(xhci, "Error while assigning device slot ID: %s\n",
			 xhci_trb_comp_code_string(command->status));
		xhci_err(xhci, "Max number of devices this xHCI host supports is %u.\n",
				HCS_MAX_SLOTS(
					readl(&xhci->cap_regs->hcs_params1)));
		xhci_free_command(xhci, command);
		return 0;
	}

	xhci_free_command(xhci, command);

	if ((xhci->quirks & XHCI_EP_LIMIT_QUIRK)) {
		spin_lock_irqsave(&xhci->lock, flags);
		ret = xhci_reserve_host_control_ep_resources(xhci);
		if (ret) {
			spin_unlock_irqrestore(&xhci->lock, flags);
			xhci_warn(xhci, "Not enough host resources, "
					"active endpoint contexts = %u\n",
					xhci->num_active_eps);
			goto disable_slot;
		}
		spin_unlock_irqrestore(&xhci->lock, flags);
	}
	/* Use GFP_NOIO, since this function can be called from
	 * xhci_discover_or_reset_device(), which may be called as part of
	 * mass storage driver error handling.
	 */
	if (!xhci_alloc_virt_device(xhci, slot_id, udev, GFP_NOIO)) {
		xhci_warn(xhci, "Could not allocate xHCI USB device data structures\n");
		goto disable_slot;
	}
	vdev = xhci->devs[slot_id];
	slot_ctx = xhci_get_slot_ctx(xhci, vdev->out_ctx);
	trace_xhci_alloc_dev(slot_ctx);

	udev->slot_id = slot_id;

	xhci_debugfs_create_slot(xhci, slot_id);

	/*
	 * If resetting upon resume, we can't put the controller into runtime
	 * suspend if there is a device attached.
	 */
	if (xhci->quirks & XHCI_RESET_ON_RESUME)
		pm_runtime_get_noresume(hcd->self.controller);

	/* Is this a LS or FS device under a HS hub? */
	/* Hub or peripherial? */
	return 1;

disable_slot:
	xhci_disable_and_free_slot(xhci, udev->slot_id);

	return 0;
}

/**
 * xhci_setup_device - issues an Address Device command to assign a unique
 *			USB bus address.
 * @hcd: USB host controller data structure.
 * @udev: USB dev structure representing the connected device.
 * @setup: Enum specifying setup mode: address only or with context.
 * @timeout_ms: Max wait time (ms) for the command operation to complete.
 *
 * Return: 0 if successful; otherwise, negative error code.
 */
static int xhci_setup_device(struct usb_hcd *hcd, struct usb_device *udev,
			     enum xhci_setup_dev setup, unsigned int timeout_ms)
{
	const char *act = setup == SETUP_CONTEXT_ONLY ? "context" : "address";
	unsigned long flags;
	struct xhci_virt_device *virt_dev;
	int ret = 0;
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct xhci_slot_ctx *slot_ctx;
	struct xhci_input_control_ctx *ctrl_ctx;
	u64 temp_64;
	struct xhci_command *command = NULL;

	mutex_lock(&xhci->mutex);

	if (xhci->xhc_state) {	/* dying, removing or halted */
		ret = -ESHUTDOWN;
		goto out;
	}

	if (!udev->slot_id) {
		xhci_dbg_trace(xhci, trace_xhci_dbg_address,
				"Bad Slot ID %d", udev->slot_id);
		ret = -EINVAL;
		goto out;
	}

	virt_dev = xhci->devs[udev->slot_id];

	if (WARN_ON(!virt_dev)) {
		/*
		 * In plug/unplug torture test with an NEC controller,
		 * a zero-dereference was observed once due to virt_dev = 0.
		 * Print useful debug rather than crash if it is observed again!
		 */
		xhci_warn(xhci, "Virt dev invalid for slot_id 0x%x!\n",
			udev->slot_id);
		ret = -EINVAL;
		goto out;
	}
	slot_ctx = xhci_get_slot_ctx(xhci, virt_dev->out_ctx);
	trace_xhci_setup_device_slot(slot_ctx);

	if (setup == SETUP_CONTEXT_ONLY) {
		if (GET_SLOT_STATE(le32_to_cpu(slot_ctx->dev_state)) ==
		    SLOT_STATE_DEFAULT) {
			xhci_dbg(xhci, "Slot already in default state\n");
			goto out;
		}
	}

	command = xhci_alloc_command(xhci, true, GFP_KERNEL);
	if (!command) {
		ret = -ENOMEM;
		goto out;
	}

	command->in_ctx = virt_dev->in_ctx;
	command->timeout_ms = timeout_ms;

	slot_ctx = xhci_get_slot_ctx(xhci, virt_dev->in_ctx);
	ctrl_ctx = xhci_get_input_control_ctx(virt_dev->in_ctx);
	if (!ctrl_ctx) {
		xhci_warn(xhci, "%s: Could not get input context, bad type.\n",
				__func__);
		ret = -EINVAL;
		goto out;
	}
	/*
	 * If this is the first Set Address since device plug-in or
	 * virt_device realloaction after a resume with an xHCI power loss,
	 * then set up the slot context.
	 */
	if (!slot_ctx->dev_info)
		xhci_setup_addressable_virt_dev(xhci, udev);
	/* Otherwise, update the control endpoint ring enqueue pointer. */
	else
		xhci_copy_ep0_dequeue_into_input_ctx(xhci, udev);
	ctrl_ctx->add_flags = cpu_to_le32(SLOT_FLAG | EP0_FLAG);
	ctrl_ctx->drop_flags = 0;

	trace_xhci_address_ctx(xhci, virt_dev->in_ctx,
				le32_to_cpu(slot_ctx->dev_info) >> 27);

	trace_xhci_address_ctrl_ctx(ctrl_ctx);
	spin_lock_irqsave(&xhci->lock, flags);
	trace_xhci_setup_device(virt_dev);
	ret = xhci_queue_address_device(xhci, command, virt_dev->in_ctx->dma,
					udev->slot_id, setup);
	if (ret) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		xhci_dbg_trace(xhci, trace_xhci_dbg_address,
				"FIXME: allocate a command ring segment");
		goto out;
	}
	xhci_ring_cmd_db(xhci);
	spin_unlock_irqrestore(&xhci->lock, flags);

	/* ctrl tx can take up to 5 sec; XXX: need more time for xHC? */
	wait_for_completion(command->completion);

	/* FIXME: From section 4.3.4: "Software shall be responsible for timing
	 * the SetAddress() "recovery interval" required by USB and aborting the
	 * command on a timeout.
	 */
	switch (command->status) {
	case COMP_COMMAND_ABORTED:
	case COMP_COMMAND_RING_STOPPED:
		xhci_warn(xhci, "Timeout while waiting for setup device command\n");
		ret = -ETIME;
		break;
	case COMP_CONTEXT_STATE_ERROR:
	case COMP_SLOT_NOT_ENABLED_ERROR:
		xhci_err(xhci, "Setup ERROR: setup %s command for slot %d.\n",
			 act, udev->slot_id);
		ret = -EINVAL;
		break;
	case COMP_USB_TRANSACTION_ERROR:
		dev_warn(&udev->dev, "Device not responding to setup %s.\n", act);

		mutex_unlock(&xhci->mutex);
		ret = xhci_disable_and_free_slot(xhci, udev->slot_id);
		if (!ret) {
			if (xhci_alloc_dev(hcd, udev) == 1)
				xhci_setup_addressable_virt_dev(xhci, udev);
		}
		kfree(command->completion);
		kfree(command);
		return -EPROTO;
	case COMP_INCOMPATIBLE_DEVICE_ERROR:
		dev_warn(&udev->dev,
			 "ERROR: Incompatible device for setup %s command\n", act);
		ret = -ENODEV;
		break;
	case COMP_SUCCESS:
		xhci_dbg_trace(xhci, trace_xhci_dbg_address,
			       "Successful setup %s command", act);
		break;
	default:
		xhci_err(xhci,
			 "ERROR: unexpected setup %s command completion code 0x%x.\n",
			 act, command->status);
		trace_xhci_address_ctx(xhci, virt_dev->out_ctx, 1);
		ret = -EINVAL;
		break;
	}
	if (ret)
		goto out;
	temp_64 = xhci_read_64(xhci, &xhci->op_regs->dcbaa_ptr);
	xhci_dbg_trace(xhci, trace_xhci_dbg_address,
			"Op regs DCBAA ptr = %#016llx", temp_64);
	xhci_dbg_trace(xhci, trace_xhci_dbg_address,
		"Slot ID %d dcbaa entry @%p = %#016llx",
		udev->slot_id,
		&xhci->dcbaa->dev_context_ptrs[udev->slot_id],
		(unsigned long long)
		le64_to_cpu(xhci->dcbaa->dev_context_ptrs[udev->slot_id]));
	xhci_dbg_trace(xhci, trace_xhci_dbg_address,
			"Output Context DMA address = %#08llx",
			(unsigned long long)virt_dev->out_ctx->dma);
	trace_xhci_address_ctx(xhci, virt_dev->in_ctx,
				le32_to_cpu(slot_ctx->dev_info) >> 27);
	/*
	 * USB core uses address 1 for the roothubs, so we add one to the
	 * address given back to us by the HC.
	 */
	trace_xhci_address_ctx(xhci, virt_dev->out_ctx,
				le32_to_cpu(slot_ctx->dev_info) >> 27);
	/* Zero the input context control for later use */
	ctrl_ctx->add_flags = 0;
	ctrl_ctx->drop_flags = 0;
	slot_ctx = xhci_get_slot_ctx(xhci, virt_dev->out_ctx);
	udev->devaddr = (u8)(le32_to_cpu(slot_ctx->dev_state) & DEV_ADDR_MASK);

	xhci_dbg_trace(xhci, trace_xhci_dbg_address,
		       "Internal device address = %d",
		       le32_to_cpu(slot_ctx->dev_state) & DEV_ADDR_MASK);
out:
	mutex_unlock(&xhci->mutex);
	if (command) {
		kfree(command->completion);
		kfree(command);
	}
	return ret;
}

static int xhci_address_device(struct usb_hcd *hcd, struct usb_device *udev,
			       unsigned int timeout_ms)
{
	return xhci_setup_device(hcd, udev, SETUP_CONTEXT_ADDRESS, timeout_ms);
}

static int xhci_enable_device(struct usb_hcd *hcd, struct usb_device *udev)
{
	return xhci_setup_device(hcd, udev, SETUP_CONTEXT_ONLY,
				 XHCI_CMD_DEFAULT_TIMEOUT);
}

/*
 * Transfer the port index into real index in the HW port status
 * registers. Caculate offset between the port's PORTSC register
 * and port status base. Divide the number of per port register
 * to get the real index. The raw port number bases 1.
 */
int xhci_find_raw_port_number(struct usb_hcd *hcd, int port1)
{
	struct xhci_hub *rhub;

	rhub = xhci_get_rhub(hcd);
	return rhub->ports[port1 - 1]->hw_portnum + 1;
}

/*
 * Issue an Evaluate Context command to change the Maximum Exit Latency in the
 * slot context.  If that succeeds, store the new MEL in the xhci_virt_device.
 */
static int __maybe_unused xhci_change_max_exit_latency(struct xhci_hcd *xhci,
			struct usb_device *udev, u16 max_exit_latency)
{
	struct xhci_virt_device *virt_dev;
	struct xhci_command *command;
	struct xhci_input_control_ctx *ctrl_ctx;
	struct xhci_slot_ctx *slot_ctx;
	unsigned long flags;
	int ret;

	command = xhci_alloc_command_with_ctx(xhci, true, GFP_KERNEL);
	if (!command)
		return -ENOMEM;

	spin_lock_irqsave(&xhci->lock, flags);

	virt_dev = xhci->devs[udev->slot_id];

	/*
	 * virt_dev might not exists yet if xHC resumed from hibernate (S4) and
	 * xHC was re-initialized. Exit latency will be set later after
	 * hub_port_finish_reset() is done and xhci->devs[] are re-allocated
	 */

	if (!virt_dev || max_exit_latency == virt_dev->current_mel) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		xhci_free_command(xhci, command);
		return 0;
	}

	/* Attempt to issue an Evaluate Context command to change the MEL. */
	ctrl_ctx = xhci_get_input_control_ctx(command->in_ctx);
	if (!ctrl_ctx) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		xhci_free_command(xhci, command);
		xhci_warn(xhci, "%s: Could not get input context, bad type.\n",
				__func__);
		return -ENOMEM;
	}

	xhci_slot_copy(xhci, command->in_ctx, virt_dev->out_ctx);
	spin_unlock_irqrestore(&xhci->lock, flags);

	ctrl_ctx->add_flags |= cpu_to_le32(SLOT_FLAG);
	slot_ctx = xhci_get_slot_ctx(xhci, command->in_ctx);
	slot_ctx->dev_info2 &= cpu_to_le32(~((u32) MAX_EXIT));
	slot_ctx->dev_info2 |= cpu_to_le32(max_exit_latency);
	slot_ctx->dev_state = 0;

	xhci_dbg_trace(xhci, trace_xhci_dbg_context_change,
			"Set up evaluate context for LPM MEL change.");

	/* Issue and wait for the evaluate context command. */
	ret = xhci_configure_endpoint(xhci, udev, command,
			true, true);

	if (!ret) {
		spin_lock_irqsave(&xhci->lock, flags);
		virt_dev->current_mel = max_exit_latency;
		spin_unlock_irqrestore(&xhci->lock, flags);
	}

	xhci_free_command(xhci, command);

	return ret;
}

#ifdef CONFIG_PM

/* BESL to HIRD Encoding array for USB2 LPM */
static int xhci_besl_encoding[16] = {125, 150, 200, 300, 400, 500, 1000, 2000,
	3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000};

/* Calculate HIRD/BESL for USB2 PORTPMSC*/
static int xhci_calculate_hird_besl(struct xhci_hcd *xhci,
					struct usb_device *udev)
{
	int u2del, besl, besl_host;
	int besl_device = 0;
	u32 field;

	u2del = HCS_U2_LATENCY(xhci->hcs_params3);
	field = le32_to_cpu(udev->bos->ext_cap->bmAttributes);

	if (field & USB_BESL_SUPPORT) {
		for (besl_host = 0; besl_host < 16; besl_host++) {
			if (xhci_besl_encoding[besl_host] >= u2del)
				break;
		}
		/* Use baseline BESL value as default */
		if (field & USB_BESL_BASELINE_VALID)
			besl_device = USB_GET_BESL_BASELINE(field);
		else if (field & USB_BESL_DEEP_VALID)
			besl_device = USB_GET_BESL_DEEP(field);
	} else {
		if (u2del <= 50)
			besl_host = 0;
		else
			besl_host = (u2del - 51) / 75 + 1;
	}

	besl = besl_host + besl_device;
	if (besl > 15)
		besl = 15;

	return besl;
}

/* Calculate BESLD, L1 timeout and HIRDM for USB2 PORTHLPMC */
static int xhci_calculate_usb2_hw_lpm_params(struct usb_device *udev)
{
	u32 field;
	int l1;
	int besld = 0;
	int hirdm = 0;

	field = le32_to_cpu(udev->bos->ext_cap->bmAttributes);

	/* xHCI l1 is set in steps of 256us, xHCI 1.0 section 5.4.11.2 */
	l1 = udev->l1_params.timeout / 256;

	/* device has preferred BESLD */
	if (field & USB_BESL_DEEP_VALID) {
		besld = USB_GET_BESL_DEEP(field);
		hirdm = 1;
	}

	return PORT_BESLD(besld) | PORT_L1_TIMEOUT(l1) | PORT_HIRDM(hirdm);
}

static int xhci_set_usb2_hardware_lpm(struct usb_hcd *hcd,
			struct usb_device *udev, int enable)
{
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	struct xhci_port **ports;
	__le32 __iomem	*pm_addr, *hlpm_addr;
	u32		pm_val, hlpm_val, field;
	unsigned int	port_num;
	unsigned long	flags;
	int		hird, exit_latency;
	int		ret;

	if (xhci->quirks & XHCI_HW_LPM_DISABLE)
		return -EPERM;

	if (hcd->speed >= HCD_USB3 || !xhci->hw_lpm_support ||
			!udev->lpm_capable)
		return -EPERM;

	if (!udev->parent || udev->parent->parent ||
			udev->descriptor.bDeviceClass == USB_CLASS_HUB)
		return -EPERM;

	if (udev->usb2_hw_lpm_capable != 1)
		return -EPERM;

	spin_lock_irqsave(&xhci->lock, flags);

	ports = xhci->usb2_rhub.ports;
	port_num = udev->portnum - 1;
	pm_addr = ports[port_num]->addr + PORTPMSC;
	pm_val = readl(pm_addr);
	hlpm_addr = ports[port_num]->addr + PORTHLPMC;

	xhci_dbg(xhci, "%s port %d USB2 hardware LPM\n",
		 str_enable_disable(enable), port_num + 1);

	if (enable) {
		/* Host supports BESL timeout instead of HIRD */
		if (udev->usb2_hw_lpm_besl_capable) {
			/* if device doesn't have a preferred BESL value use a
			 * default one which works with mixed HIRD and BESL
			 * systems. See XHCI_DEFAULT_BESL definition in xhci.h
			 */
			field = le32_to_cpu(udev->bos->ext_cap->bmAttributes);
			if ((field & USB_BESL_SUPPORT) &&
			    (field & USB_BESL_BASELINE_VALID))
				hird = USB_GET_BESL_BASELINE(field);
			else
				hird = udev->l1_params.besl;

			exit_latency = xhci_besl_encoding[hird];
			spin_unlock_irqrestore(&xhci->lock, flags);

			ret = xhci_change_max_exit_latency(xhci, udev,
							   exit_latency);
			if (ret < 0)
				return ret;
			spin_lock_irqsave(&xhci->lock, flags);

			hlpm_val = xhci_calculate_usb2_hw_lpm_params(udev);
			writel(hlpm_val, hlpm_addr);
			/* flush write */
			readl(hlpm_addr);
		} else {
			hird = xhci_calculate_hird_besl(xhci, udev);
		}

		pm_val &= ~PORT_HIRD_MASK;
		pm_val |= PORT_HIRD(hird) | PORT_RWE | PORT_L1DS(udev->slot_id);
		writel(pm_val, pm_addr);
		pm_val = readl(pm_addr);
		pm_val |= PORT_HLE;
		writel(pm_val, pm_addr);
		/* flush write */
		readl(pm_addr);
	} else {
		pm_val &= ~(PORT_HLE | PORT_RWE | PORT_HIRD_MASK | PORT_L1DS_MASK);
		writel(pm_val, pm_addr);
		/* flush write */
		readl(pm_addr);
		if (udev->usb2_hw_lpm_besl_capable) {
			spin_unlock_irqrestore(&xhci->lock, flags);
			xhci_change_max_exit_latency(xhci, udev, 0);
			readl_poll_timeout(ports[port_num]->addr, pm_val,
					   (pm_val & PORT_PLS_MASK) == XDEV_U0,
					   100, 10000);
			return 0;
		}
	}

	spin_unlock_irqrestore(&xhci->lock, flags);
	return 0;
}

static int xhci_update_device(struct usb_hcd *hcd, struct usb_device *udev)
{
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	struct xhci_port *port;
	u32 capability;

	/* Check if USB3 device at root port is tunneled over USB4 */
	if (hcd->speed >= HCD_USB3 && !udev->parent->parent) {
		port = xhci->usb3_rhub.ports[udev->portnum - 1];

		udev->tunnel_mode = xhci_port_is_tunneled(xhci, port);
		if (udev->tunnel_mode == USB_LINK_UNKNOWN)
			dev_dbg(&udev->dev, "link tunnel state unknown\n");
		else if (udev->tunnel_mode == USB_LINK_TUNNELED)
			dev_dbg(&udev->dev, "tunneled over USB4 link\n");
		else if (udev->tunnel_mode == USB_LINK_NATIVE)
			dev_dbg(&udev->dev, "native USB 3.x link\n");
		return 0;
	}

	if (hcd->speed >= HCD_USB3 || !udev->lpm_capable || !xhci->hw_lpm_support)
		return 0;

	/* we only support lpm for non-hub device connected to root hub yet */
	if (!udev->parent || udev->parent->parent ||
			udev->descriptor.bDeviceClass == USB_CLASS_HUB)
		return 0;

	port = xhci->usb2_rhub.ports[udev->portnum - 1];
	capability = port->port_cap->protocol_caps;

	if (capability & XHCI_HLC) {
		udev->usb2_hw_lpm_capable = 1;
		udev->l1_params.timeout = XHCI_L1_TIMEOUT;
		udev->l1_params.besl = XHCI_DEFAULT_BESL;
		if (capability & XHCI_BLC)
			udev->usb2_hw_lpm_besl_capable = 1;
	}

	return 0;
}

/*---------------------- USB 3.0 Link PM functions ------------------------*/

/* Service interval in nanoseconds = 2^(bInterval - 1) * 125us * 1000ns / 1us */
static unsigned long long xhci_service_interval_to_ns(
		struct usb_endpoint_descriptor *desc)
{
	return (1ULL << (desc->bInterval - 1)) * 125 * 1000;
}

static u16 xhci_get_timeout_no_hub_lpm(struct usb_device *udev,
		enum usb3_link_state state)
{
	unsigned long long sel;
	unsigned long long pel;
	unsigned int max_sel_pel;
	char *state_name;

	switch (state) {
	case USB3_LPM_U1:
		/* Convert SEL and PEL stored in nanoseconds to microseconds */
		sel = DIV_ROUND_UP(udev->u1_params.sel, 1000);
		pel = DIV_ROUND_UP(udev->u1_params.pel, 1000);
		max_sel_pel = USB3_LPM_MAX_U1_SEL_PEL;
		state_name = "U1";
		break;
	case USB3_LPM_U2:
		sel = DIV_ROUND_UP(udev->u2_params.sel, 1000);
		pel = DIV_ROUND_UP(udev->u2_params.pel, 1000);
		max_sel_pel = USB3_LPM_MAX_U2_SEL_PEL;
		state_name = "U2";
		break;
	default:
		dev_warn(&udev->dev, "%s: Can't get timeout for non-U1 or U2 state.\n",
				__func__);
		return USB3_LPM_DISABLED;
	}

	if (sel <= max_sel_pel && pel <= max_sel_pel)
		return USB3_LPM_DEVICE_INITIATED;

	if (sel > max_sel_pel)
		dev_dbg(&udev->dev, "Device-initiated %s disabled "
				"due to long SEL %llu ms\n",
				state_name, sel);
	else
		dev_dbg(&udev->dev, "Device-initiated %s disabled "
				"due to long PEL %llu ms\n",
				state_name, pel);
	return USB3_LPM_DISABLED;
}

/* The U1 timeout should be the maximum of the following values:
 *  - For control endpoints, U1 system exit latency (SEL) * 3
 *  - For bulk endpoints, U1 SEL * 5
 *  - For interrupt endpoints:
 *    - Notification EPs, U1 SEL * 3
 *    - Periodic EPs, max(105% of bInterval, U1 SEL * 2)
 *  - For isochronous endpoints, max(105% of bInterval, U1 SEL * 2)
 */
static unsigned long long xhci_calculate_intel_u1_timeout(
		struct usb_device *udev,
		struct usb_endpoint_descriptor *desc)
{
	unsigned long long timeout_ns;
	int ep_type;
	int intr_type;

	ep_type = usb_endpoint_type(desc);
	switch (ep_type) {
	case USB_ENDPOINT_XFER_CONTROL:
		timeout_ns = udev->u1_params.sel * 3;
		break;
	case USB_ENDPOINT_XFER_BULK:
		timeout_ns = udev->u1_params.sel * 5;
		break;
	case USB_ENDPOINT_XFER_INT:
		intr_type = usb_endpoint_interrupt_type(desc);
		if (intr_type == USB_ENDPOINT_INTR_NOTIFICATION) {
			timeout_ns = udev->u1_params.sel * 3;
			break;
		}
		/* Otherwise the calculation is the same as isoc eps */
		fallthrough;
	case USB_ENDPOINT_XFER_ISOC:
		timeout_ns = xhci_service_interval_to_ns(desc);
		timeout_ns = DIV_ROUND_UP_ULL(timeout_ns * 105, 100);
		if (timeout_ns < udev->u1_params.sel * 2)
			timeout_ns = udev->u1_params.sel * 2;
		break;
	default:
		return 0;
	}

	return timeout_ns;
}

/* Returns the hub-encoded U1 timeout value. */
static u16 xhci_calculate_u1_timeout(struct xhci_hcd *xhci,
		struct usb_device *udev,
		struct usb_endpoint_descriptor *desc)
{
	unsigned long long timeout_ns;

	/* Prevent U1 if service interval is shorter than U1 exit latency */
	if (usb_endpoint_xfer_int(desc) || usb_endpoint_xfer_isoc(desc)) {
		if (xhci_service_interval_to_ns(desc) <= udev->u1_params.mel) {
			dev_dbg(&udev->dev, "Disable U1, ESIT shorter than exit latency\n");
			return USB3_LPM_DISABLED;
		}
	}

	if (xhci->quirks & (XHCI_INTEL_HOST | XHCI_ZHAOXIN_HOST))
		timeout_ns = xhci_calculate_intel_u1_timeout(udev, desc);
	else
		timeout_ns = udev->u1_params.sel;

	/* The U1 timeout is encoded in 1us intervals.
	 * Don't return a timeout of zero, because that's USB3_LPM_DISABLED.
	 */
	if (timeout_ns == USB3_LPM_DISABLED)
		timeout_ns = 1;
	else
		timeout_ns = DIV_ROUND_UP_ULL(timeout_ns, 1000);

	/* If the necessary timeout value is bigger than what we can set in the
	 * USB 3.0 hub, we have to disable hub-initiated U1.
	 */
	if (timeout_ns <= USB3_LPM_U1_MAX_TIMEOUT)
		return timeout_ns;
	dev_dbg(&udev->dev, "Hub-initiated U1 disabled due to long timeout %lluus\n",
		timeout_ns);
	return xhci_get_timeout_no_hub_lpm(udev, USB3_LPM_U1);
}

/* The U2 timeout should be the maximum of:
 *  - 10 ms (to avoid the bandwidth impact on the scheduler)
 *  - largest bInterval of any active periodic endpoint (to avoid going
 *    into lower power link states between intervals).
 *  - the U2 Exit Latency of the device
 */
static unsigned long long xhci_calculate_intel_u2_timeout(
		struct usb_device *udev,
		struct usb_endpoint_descriptor *desc)
{
	unsigned long long timeout_ns;
	unsigned long long u2_del_ns;

	timeout_ns = 10 * 1000 * 1000;

	if ((usb_endpoint_xfer_int(desc) || usb_endpoint_xfer_isoc(desc)) &&
			(xhci_service_interval_to_ns(desc) > timeout_ns))
		timeout_ns = xhci_service_interval_to_ns(desc);

	u2_del_ns = le16_to_cpu(udev->bos->ss_cap->bU2DevExitLat) * 1000ULL;
	if (u2_del_ns > timeout_ns)
		timeout_ns = u2_del_ns;

	return timeout_ns;
}

/* Returns the hub-encoded U2 timeout value. */
static u16 xhci_calculate_u2_timeout(struct xhci_hcd *xhci,
		struct usb_device *udev,
		struct usb_endpoint_descriptor *desc)
{
	unsigned long long timeout_ns;

	/* Prevent U2 if service interval is shorter than U2 exit latency */
	if (usb_endpoint_xfer_int(desc) || usb_endpoint_xfer_isoc(desc)) {
		if (xhci_service_interval_to_ns(desc) <= udev->u2_params.mel) {
			dev_dbg(&udev->dev, "Disable U2, ESIT shorter than exit latency\n");
			return USB3_LPM_DISABLED;
		}
	}

	if (xhci->quirks & (XHCI_INTEL_HOST | XHCI_ZHAOXIN_HOST))
		timeout_ns = xhci_calculate_intel_u2_timeout(udev, desc);
	else
		timeout_ns = udev->u2_params.sel;

	/* The U2 timeout is encoded in 256us intervals */
	timeout_ns = DIV_ROUND_UP_ULL(timeout_ns, 256 * 1000);
	/* If the necessary timeout value is bigger than what we can set in the
	 * USB 3.0 hub, we have to disable hub-initiated U2.
	 */
	if (timeout_ns <= USB3_LPM_U2_MAX_TIMEOUT)
		return timeout_ns;
	dev_dbg(&udev->dev, "Hub-initiated U2 disabled due to long timeout %lluus\n",
		timeout_ns * 256);
	return xhci_get_timeout_no_hub_lpm(udev, USB3_LPM_U2);
}

static u16 xhci_call_host_update_timeout_for_endpoint(struct xhci_hcd *xhci,
		struct usb_device *udev,
		struct usb_endpoint_descriptor *desc,
		enum usb3_link_state state,
		u16 *timeout)
{
	if (state == USB3_LPM_U1)
		return xhci_calculate_u1_timeout(xhci, udev, desc);
	else if (state == USB3_LPM_U2)
		return xhci_calculate_u2_timeout(xhci, udev, desc);

	return USB3_LPM_DISABLED;
}

static int xhci_update_timeout_for_endpoint(struct xhci_hcd *xhci,
		struct usb_device *udev,
		struct usb_endpoint_descriptor *desc,
		enum usb3_link_state state,
		u16 *timeout)
{
	u16 alt_timeout;

	alt_timeout = xhci_call_host_update_timeout_for_endpoint(xhci, udev,
		desc, state, timeout);

	/* If we found we can't enable hub-initiated LPM, and
	 * the U1 or U2 exit latency was too high to allow
	 * device-initiated LPM as well, then we will disable LPM
	 * for this device, so stop searching any further.
	 */
	if (alt_timeout == USB3_LPM_DISABLED) {
		*timeout = alt_timeout;
		return -E2BIG;
	}
	if (alt_timeout > *timeout)
		*timeout = alt_timeout;
	return 0;
}

static int xhci_update_timeout_for_interface(struct xhci_hcd *xhci,
		struct usb_device *udev,
		struct usb_host_interface *alt,
		enum usb3_link_state state,
		u16 *timeout)
{
	int j;

	for (j = 0; j < alt->desc.bNumEndpoints; j++) {
		if (xhci_update_timeout_for_endpoint(xhci, udev,
					&alt->endpoint[j].desc, state, timeout))
			return -E2BIG;
	}
	return 0;
}

static int xhci_check_tier_policy(struct xhci_hcd *xhci,
		struct usb_device *udev,
		enum usb3_link_state state)
{
	struct usb_device *parent = udev->parent;
	int tier = 1; /* roothub is tier1 */

	while (parent) {
		parent = parent->parent;
		tier++;
	}

	if (xhci->quirks & XHCI_INTEL_HOST && tier > 3)
		goto fail;
	if (xhci->quirks & XHCI_ZHAOXIN_HOST && tier > 2)
		goto fail;

	return 0;
fail:
	dev_dbg(&udev->dev, "Tier policy prevents U1/U2 LPM states for devices at tier %d\n",
			tier);
	return -E2BIG;
}

/* Returns the U1 or U2 timeout that should be enabled.
 * If the tier check or timeout setting functions return with a non-zero exit
 * code, that means the timeout value has been finalized and we shouldn't look
 * at any more endpoints.
 */
static u16 xhci_calculate_lpm_timeout(struct usb_hcd *hcd,
			struct usb_device *udev, enum usb3_link_state state)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct usb_host_config *config;
	char *state_name;
	int i;
	u16 timeout = USB3_LPM_DISABLED;

	if (state == USB3_LPM_U1)
		state_name = "U1";
	else if (state == USB3_LPM_U2)
		state_name = "U2";
	else {
		dev_warn(&udev->dev, "Can't enable unknown link state %i\n",
				state);
		return timeout;
	}

	/* Gather some information about the currently installed configuration
	 * and alternate interface settings.
	 */
	if (xhci_update_timeout_for_endpoint(xhci, udev, &udev->ep0.desc,
			state, &timeout))
		return timeout;

	config = udev->actconfig;
	if (!config)
		return timeout;

	for (i = 0; i < config->desc.bNumInterfaces; i++) {
		struct usb_driver *driver;
		struct usb_interface *intf = config->interface[i];

		if (!intf)
			continue;

		/* Check if any currently bound drivers want hub-initiated LPM
		 * disabled.
		 */
		if (intf->dev.driver) {
			driver = to_usb_driver(intf->dev.driver);
			if (driver && driver->disable_hub_initiated_lpm) {
				dev_dbg(&udev->dev, "Hub-initiated %s disabled at request of driver %s\n",
					state_name, driver->name);
				timeout = xhci_get_timeout_no_hub_lpm(udev,
								      state);
				if (timeout == USB3_LPM_DISABLED)
					return timeout;
			}
		}

		/* Not sure how this could happen... */
		if (!intf->cur_altsetting)
			continue;

		if (xhci_update_timeout_for_interface(xhci, udev,
					intf->cur_altsetting,
					state, &timeout))
			return timeout;
	}
	return timeout;
}

static int calculate_max_exit_latency(struct usb_device *udev,
		enum usb3_link_state state_changed,
		u16 hub_encoded_timeout)
{
	unsigned long long u1_mel_us = 0;
	unsigned long long u2_mel_us = 0;
	unsigned long long mel_us = 0;
	bool disabling_u1;
	bool disabling_u2;
	bool enabling_u1;
	bool enabling_u2;

	disabling_u1 = (state_changed == USB3_LPM_U1 &&
			hub_encoded_timeout == USB3_LPM_DISABLED);
	disabling_u2 = (state_changed == USB3_LPM_U2 &&
			hub_encoded_timeout == USB3_LPM_DISABLED);

	enabling_u1 = (state_changed == USB3_LPM_U1 &&
			hub_encoded_timeout != USB3_LPM_DISABLED);
	enabling_u2 = (state_changed == USB3_LPM_U2 &&
			hub_encoded_timeout != USB3_LPM_DISABLED);

	/* If U1 was already enabled and we're not disabling it,
	 * or we're going to enable U1, account for the U1 max exit latency.
	 */
	if ((udev->u1_params.timeout != USB3_LPM_DISABLED && !disabling_u1) ||
			enabling_u1)
		u1_mel_us = DIV_ROUND_UP(udev->u1_params.mel, 1000);
	if ((udev->u2_params.timeout != USB3_LPM_DISABLED && !disabling_u2) ||
			enabling_u2)
		u2_mel_us = DIV_ROUND_UP(udev->u2_params.mel, 1000);

	mel_us = max(u1_mel_us, u2_mel_us);

	/* xHCI host controller max exit latency field is only 16 bits wide. */
	if (mel_us > MAX_EXIT) {
		dev_warn(&udev->dev, "Link PM max exit latency of %lluus "
				"is too big.\n", mel_us);
		return -E2BIG;
	}
	return mel_us;
}

/* Returns the USB3 hub-encoded value for the U1/U2 timeout. */
static int xhci_enable_usb3_lpm_timeout(struct usb_hcd *hcd,
			struct usb_device *udev, enum usb3_link_state state)
{
	struct xhci_hcd	*xhci;
	struct xhci_port *port;
	u16 hub_encoded_timeout;
	int mel;
	int ret;

	xhci = hcd_to_xhci(hcd);
	/* The LPM timeout values are pretty host-controller specific, so don't
	 * enable hub-initiated timeouts unless the vendor has provided
	 * information about their timeout algorithm.
	 */
	if (!xhci || !(xhci->quirks & XHCI_LPM_SUPPORT) ||
			!xhci->devs[udev->slot_id])
		return USB3_LPM_DISABLED;

	if (xhci_check_tier_policy(xhci, udev, state) < 0)
		return USB3_LPM_DISABLED;

	/* If connected to root port then check port can handle lpm */
	if (udev->parent && !udev->parent->parent) {
		port = xhci->usb3_rhub.ports[udev->portnum - 1];
		if (port->lpm_incapable)
			return USB3_LPM_DISABLED;
	}

	hub_encoded_timeout = xhci_calculate_lpm_timeout(hcd, udev, state);
	mel = calculate_max_exit_latency(udev, state, hub_encoded_timeout);
	if (mel < 0) {
		/* Max Exit Latency is too big, disable LPM. */
		hub_encoded_timeout = USB3_LPM_DISABLED;
		mel = 0;
	}

	ret = xhci_change_max_exit_latency(xhci, udev, mel);
	if (ret)
		return ret;
	return hub_encoded_timeout;
}

static int xhci_disable_usb3_lpm_timeout(struct usb_hcd *hcd,
			struct usb_device *udev, enum usb3_link_state state)
{
	struct xhci_hcd	*xhci;
	u16 mel;

	xhci = hcd_to_xhci(hcd);
	if (!xhci || !(xhci->quirks & XHCI_LPM_SUPPORT) ||
			!xhci->devs[udev->slot_id])
		return 0;

	mel = calculate_max_exit_latency(udev, state, USB3_LPM_DISABLED);
	return xhci_change_max_exit_latency(xhci, udev, mel);
}
#else /* CONFIG_PM */

static int xhci_set_usb2_hardware_lpm(struct usb_hcd *hcd,
				struct usb_device *udev, int enable)
{
	return 0;
}

static int xhci_update_device(struct usb_hcd *hcd, struct usb_device *udev)
{
	return 0;
}

static int xhci_enable_usb3_lpm_timeout(struct usb_hcd *hcd,
			struct usb_device *udev, enum usb3_link_state state)
{
	return USB3_LPM_DISABLED;
}

static int xhci_disable_usb3_lpm_timeout(struct usb_hcd *hcd,
			struct usb_device *udev, enum usb3_link_state state)
{
	return 0;
}
#endif	/* CONFIG_PM */

/*-------------------------------------------------------------------------*/

/* Once a hub descriptor is fetched for a device, we need to update the xHC's
 * internal data structures for the device.
 */
int xhci_update_hub_device(struct usb_hcd *hcd, struct usb_device *hdev,
			struct usb_tt *tt, gfp_t mem_flags)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct xhci_virt_device *vdev;
	struct xhci_command *config_cmd;
	struct xhci_input_control_ctx *ctrl_ctx;
	struct xhci_slot_ctx *slot_ctx;
	unsigned long flags;
	unsigned think_time;
	int ret;

	/* Ignore root hubs */
	if (!hdev->parent)
		return 0;

	vdev = xhci->devs[hdev->slot_id];
	if (!vdev) {
		xhci_warn(xhci, "Cannot update hub desc for unknown device.\n");
		return -EINVAL;
	}

	config_cmd = xhci_alloc_command_with_ctx(xhci, true, mem_flags);
	if (!config_cmd)
		return -ENOMEM;

	ctrl_ctx = xhci_get_input_control_ctx(config_cmd->in_ctx);
	if (!ctrl_ctx) {
		xhci_warn(xhci, "%s: Could not get input context, bad type.\n",
				__func__);
		xhci_free_command(xhci, config_cmd);
		return -ENOMEM;
	}

	spin_lock_irqsave(&xhci->lock, flags);
	if (hdev->speed == USB_SPEED_HIGH &&
			xhci_alloc_tt_info(xhci, vdev, hdev, tt, GFP_ATOMIC)) {
		xhci_dbg(xhci, "Could not allocate xHCI TT structure.\n");
		xhci_free_command(xhci, config_cmd);
		spin_unlock_irqrestore(&xhci->lock, flags);
		return -ENOMEM;
	}

	xhci_slot_copy(xhci, config_cmd->in_ctx, vdev->out_ctx);
	ctrl_ctx->add_flags |= cpu_to_le32(SLOT_FLAG);
	slot_ctx = xhci_get_slot_ctx(xhci, config_cmd->in_ctx);
	slot_ctx->dev_info |= cpu_to_le32(DEV_HUB);
	/*
	 * refer to section 6.2.2: MTT should be 0 for full speed hub,
	 * but it may be already set to 1 when setup an xHCI virtual
	 * device, so clear it anyway.
	 */
	if (tt->multi)
		slot_ctx->dev_info |= cpu_to_le32(DEV_MTT);
	else if (hdev->speed == USB_SPEED_FULL)
		slot_ctx->dev_info &= cpu_to_le32(~DEV_MTT);

	if (xhci->hci_version > 0x95) {
		xhci_dbg(xhci, "xHCI version %x needs hub "
				"TT think time and number of ports\n",
				(unsigned int) xhci->hci_version);
		slot_ctx->dev_info2 |= cpu_to_le32(XHCI_MAX_PORTS(hdev->maxchild));
		/* Set TT think time - convert from ns to FS bit times.
		 * 0 = 8 FS bit times, 1 = 16 FS bit times,
		 * 2 = 24 FS bit times, 3 = 32 FS bit times.
		 *
		 * xHCI 1.0: this field shall be 0 if the device is not a
		 * High-spped hub.
		 */
		think_time = tt->think_time;
		if (think_time != 0)
			think_time = (think_time / 666) - 1;
		if (xhci->hci_version < 0x100 || hdev->speed == USB_SPEED_HIGH)
			slot_ctx->tt_info |=
				cpu_to_le32(TT_THINK_TIME(think_time));
	} else {
		xhci_dbg(xhci, "xHCI version %x doesn't need hub "
				"TT think time or number of ports\n",
				(unsigned int) xhci->hci_version);
	}
	slot_ctx->dev_state = 0;
	spin_unlock_irqrestore(&xhci->lock, flags);

	xhci_dbg(xhci, "Set up %s for hub device.\n",
			(xhci->hci_version > 0x95) ?
			"configure endpoint" : "evaluate context");

	/* Issue and wait for the configure endpoint or
	 * evaluate context command.
	 */
	if (xhci->hci_version > 0x95)
		ret = xhci_configure_endpoint(xhci, hdev, config_cmd,
				false, false);
	else
		ret = xhci_configure_endpoint(xhci, hdev, config_cmd,
				true, false);

	xhci_free_command(xhci, config_cmd);
	return ret;
}
EXPORT_SYMBOL_GPL(xhci_update_hub_device);

static int xhci_get_frame(struct usb_hcd *hcd)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	/* EHCI mods by the periodic size.  Why? */
	return readl(&xhci->run_regs->microframe_index) >> 3;
}

static void xhci_hcd_init_usb2_data(struct xhci_hcd *xhci, struct usb_hcd *hcd)
{
	xhci->usb2_rhub.hcd = hcd;
	hcd->speed = HCD_USB2;
	hcd->self.root_hub->speed = USB_SPEED_HIGH;
	/*
	 * USB 2.0 roothub under xHCI has an integrated TT,
	 * (rate matching hub) as opposed to having an OHCI/UHCI
	 * companion controller.
	 */
	hcd->has_tt = 1;
}

static void xhci_hcd_init_usb3_data(struct xhci_hcd *xhci, struct usb_hcd *hcd)
{
	unsigned int minor_rev;

	/*
	 * Early xHCI 1.1 spec did not mention USB 3.1 capable hosts
	 * should return 0x31 for sbrn, or that the minor revision
	 * is a two digit BCD containig minor and sub-minor numbers.
	 * This was later clarified in xHCI 1.2.
	 *
	 * Some USB 3.1 capable hosts therefore have sbrn 0x30, and
	 * minor revision set to 0x1 instead of 0x10.
	 */
	if (xhci->usb3_rhub.min_rev == 0x1)
		minor_rev = 1;
	else
		minor_rev = xhci->usb3_rhub.min_rev / 0x10;

	switch (minor_rev) {
	case 2:
		hcd->speed = HCD_USB32;
		hcd->self.root_hub->speed = USB_SPEED_SUPER_PLUS;
		hcd->self.root_hub->rx_lanes = 2;
		hcd->self.root_hub->tx_lanes = 2;
		hcd->self.root_hub->ssp_rate = USB_SSP_GEN_2x2;
		break;
	case 1:
		hcd->speed = HCD_USB31;
		hcd->self.root_hub->speed = USB_SPEED_SUPER_PLUS;
		hcd->self.root_hub->ssp_rate = USB_SSP_GEN_2x1;
		break;
	}
	xhci_info(xhci, "Host supports USB 3.%x %sSuperSpeed\n",
		  minor_rev, minor_rev ? "Enhanced " : "");

	xhci->usb3_rhub.hcd = hcd;
}

int xhci_gen_setup(struct usb_hcd *hcd, xhci_get_quirks_t get_quirks)
{
	struct xhci_hcd		*xhci;
	/*
	 * TODO: Check with DWC3 clients for sysdev according to
	 * quirks
	 */
	struct device		*dev = hcd->self.sysdev;
	int			retval;

	/* Accept arbitrarily long scatter-gather lists */
	hcd->self.sg_tablesize = ~0;

	/* support to build packet from discontinuous buffers */
	hcd->self.no_sg_constraint = 1;

	/* XHCI controllers don't stop the ep queue on short packets :| */
	hcd->self.no_stop_on_short = 1;

	xhci = hcd_to_xhci(hcd);

	if (!usb_hcd_is_primary_hcd(hcd)) {
		xhci_hcd_init_usb3_data(xhci, hcd);
		return 0;
	}

	mutex_init(&xhci->mutex);
	xhci->main_hcd = hcd;
	xhci->cap_regs = hcd->regs;
	xhci->op_regs = hcd->regs +
		HC_LENGTH(readl(&xhci->cap_regs->hc_capbase));
	xhci->run_regs = hcd->regs +
		(readl(&xhci->cap_regs->run_regs_off) & RTSOFF_MASK);
	/* Cache read-only capability registers */
	xhci->hcs_params1 = readl(&xhci->cap_regs->hcs_params1);
	xhci->hcs_params2 = readl(&xhci->cap_regs->hcs_params2);
	xhci->hcs_params3 = readl(&xhci->cap_regs->hcs_params3);
	xhci->hci_version = HC_VERSION(readl(&xhci->cap_regs->hc_capbase));
	xhci->hcc_params = readl(&xhci->cap_regs->hcc_params);
	if (xhci->hci_version > 0x100)
		xhci->hcc_params2 = readl(&xhci->cap_regs->hcc_params2);

	/* xhci-plat or xhci-pci might have set max_interrupters already */
	if ((!xhci->max_interrupters) ||
	    xhci->max_interrupters > HCS_MAX_INTRS(xhci->hcs_params1))
		xhci->max_interrupters = HCS_MAX_INTRS(xhci->hcs_params1);

	xhci->quirks |= quirks;

	if (get_quirks)
		get_quirks(dev, xhci);

	/* In xhci controllers which follow xhci 1.0 spec gives a spurious
	 * success event after a short transfer. This quirk will ignore such
	 * spurious event.
	 */
	if (xhci->hci_version > 0x96)
		xhci->quirks |= XHCI_SPURIOUS_SUCCESS;

	if (xhci->hci_version == 0x95 && link_quirk) {
		xhci_dbg(xhci, "QUIRK: Not clearing Link TRB chain bits");
		xhci->quirks |= XHCI_LINK_TRB_QUIRK;
	}

	/* Make sure the HC is halted. */
	retval = xhci_halt(xhci);
	if (retval)
		return retval;

	xhci_zero_64b_regs(xhci);

	xhci_dbg(xhci, "Resetting HCD\n");
	/* Reset the internal HC memory state and registers. */
	retval = xhci_reset(xhci, XHCI_RESET_LONG_USEC);
	if (retval)
		return retval;
	xhci_dbg(xhci, "Reset complete\n");

	/*
	 * On some xHCI controllers (e.g. R-Car SoCs), the AC64 bit (bit 0)
	 * of HCCPARAMS1 is set to 1. However, the xHCs don't support 64-bit
	 * address memory pointers actually. So, this driver clears the AC64
	 * bit of xhci->hcc_params to call dma_set_coherent_mask(dev,
	 * DMA_BIT_MASK(32)) in this xhci_gen_setup().
	 */
	if (xhci->quirks & XHCI_NO_64BIT_SUPPORT)
		xhci->hcc_params &= ~BIT(0);

	/* Set dma_mask and coherent_dma_mask to 64-bits,
	 * if xHC supports 64-bit addressing */
	if (HCC_64BIT_ADDR(xhci->hcc_params) &&
			!dma_set_mask(dev, DMA_BIT_MASK(64))) {
		xhci_dbg(xhci, "Enabling 64-bit DMA addresses.\n");
		dma_set_coherent_mask(dev, DMA_BIT_MASK(64));
	} else {
		/*
		 * This is to avoid error in cases where a 32-bit USB
		 * controller is used on a 64-bit capable system.
		 */
		retval = dma_set_mask(dev, DMA_BIT_MASK(32));
		if (retval)
			return retval;
		xhci_dbg(xhci, "Enabling 32-bit DMA addresses.\n");
		dma_set_coherent_mask(dev, DMA_BIT_MASK(32));
	}

	xhci_dbg(xhci, "Calling HCD init\n");
	/* Initialize HCD and host controller data structures. */
	retval = xhci_init(hcd);
	if (retval)
		return retval;
	xhci_dbg(xhci, "Called HCD init\n");

	if (xhci_hcd_is_usb3(hcd))
		xhci_hcd_init_usb3_data(xhci, hcd);
	else
		xhci_hcd_init_usb2_data(xhci, hcd);

	xhci_info(xhci, "hcc params 0x%08x hci version 0x%x quirks 0x%016llx\n",
		  xhci->hcc_params, xhci->hci_version, xhci->quirks);

	return 0;
}
EXPORT_SYMBOL_GPL(xhci_gen_setup);

static void xhci_clear_tt_buffer_complete(struct usb_hcd *hcd,
		struct usb_host_endpoint *ep)
{
	struct xhci_hcd *xhci;
	struct usb_device *udev;
	unsigned int slot_id;
	unsigned int ep_index;
	unsigned long flags;

	xhci = hcd_to_xhci(hcd);

	spin_lock_irqsave(&xhci->lock, flags);
	udev = (struct usb_device *)ep->hcpriv;
	slot_id = udev->slot_id;
	ep_index = xhci_get_endpoint_index(&ep->desc);

	xhci->devs[slot_id]->eps[ep_index].ep_state &= ~EP_CLEARING_TT;
	xhci_ring_doorbell_for_active_rings(xhci, slot_id, ep_index);
	spin_unlock_irqrestore(&xhci->lock, flags);
}

static const struct hc_driver xhci_hc_driver = {
	.description =		"xhci-hcd",
	.product_desc =		"xHCI Host Controller",
	.hcd_priv_size =	sizeof(struct xhci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq =			xhci_irq,
	.flags =		HCD_MEMORY | HCD_DMA | HCD_USB3 | HCD_SHARED |
				HCD_BH,

	/*
	 * basic lifecycle operations
	 */
	.reset =		NULL, /* set in xhci_init_driver() */
	.start =		xhci_run,
	.stop =			xhci_stop,
	.shutdown =		xhci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.map_urb_for_dma =      xhci_map_urb_for_dma,
	.unmap_urb_for_dma =    xhci_unmap_urb_for_dma,
	.urb_enqueue =		xhci_urb_enqueue,
	.urb_dequeue =		xhci_urb_dequeue,
	.alloc_dev =		xhci_alloc_dev,
	.free_dev =		xhci_free_dev,
	.alloc_streams =	xhci_alloc_streams,
	.free_streams =		xhci_free_streams,
	.add_endpoint =		xhci_add_endpoint,
	.drop_endpoint =	xhci_drop_endpoint,
	.endpoint_disable =	xhci_endpoint_disable,
	.endpoint_reset =	xhci_endpoint_reset,
	.check_bandwidth =	xhci_check_bandwidth,
	.reset_bandwidth =	xhci_reset_bandwidth,
	.address_device =	xhci_address_device,
	.enable_device =	xhci_enable_device,
	.update_hub_device =	xhci_update_hub_device,
	.reset_device =		xhci_discover_or_reset_device,

	/*
	 * scheduling support
	 */
	.get_frame_number =	xhci_get_frame,

	/*
	 * root hub support
	 */
	.hub_control =		xhci_hub_control,
	.hub_status_data =	xhci_hub_status_data,
	.bus_suspend =		xhci_bus_suspend,
	.bus_resume =		xhci_bus_resume,
	.get_resuming_ports =	xhci_get_resuming_ports,

	/*
	 * call back when device connected and addressed
	 */
	.update_device =        xhci_update_device,
	.set_usb2_hw_lpm =	xhci_set_usb2_hardware_lpm,
	.enable_usb3_lpm_timeout =	xhci_enable_usb3_lpm_timeout,
	.disable_usb3_lpm_timeout =	xhci_disable_usb3_lpm_timeout,
	.find_raw_port_number =	xhci_find_raw_port_number,
	.clear_tt_buffer_complete = xhci_clear_tt_buffer_complete,
};

void xhci_init_driver(struct hc_driver *drv,
		      const struct xhci_driver_overrides *over)
{
	BUG_ON(!over);

	/* Copy the generic table to drv then apply the overrides */
	*drv = xhci_hc_driver;

	if (over) {
		drv->hcd_priv_size += over->extra_priv_size;
		if (over->reset)
			drv->reset = over->reset;
		if (over->start)
			drv->start = over->start;
		if (over->add_endpoint)
			drv->add_endpoint = over->add_endpoint;
		if (over->drop_endpoint)
			drv->drop_endpoint = over->drop_endpoint;
		if (over->check_bandwidth)
			drv->check_bandwidth = over->check_bandwidth;
		if (over->reset_bandwidth)
			drv->reset_bandwidth = over->reset_bandwidth;
		if (over->update_hub_device)
			drv->update_hub_device = over->update_hub_device;
		if (over->hub_control)
			drv->hub_control = over->hub_control;
	}
}
EXPORT_SYMBOL_GPL(xhci_init_driver);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");

static int __init xhci_hcd_init(void)
{
	/*
	 * Check the compiler generated sizes of structures that must be laid
	 * out in specific ways for hardware access.
	 */
	BUILD_BUG_ON(sizeof(struct xhci_doorbell_array) != 256*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_slot_ctx) != 8*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_ep_ctx) != 8*32/8);
	/* xhci_device_control has eight fields, and also
	 * embeds one xhci_slot_ctx and 31 xhci_ep_ctx
	 */
	BUILD_BUG_ON(sizeof(struct xhci_stream_ctx) != 4*32/8);
	BUILD_BUG_ON(sizeof(union xhci_trb) != 4*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_erst_entry) != 4*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_cap_regs) != 8*32/8);
	BUILD_BUG_ON(sizeof(struct xhci_intr_reg) != 8*32/8);
	/* xhci_run_regs has eight fields and embeds 128 xhci_intr_regs */
	BUILD_BUG_ON(sizeof(struct xhci_run_regs) != (8+8*128)*32/8);

	if (usb_disabled())
		return -ENODEV;

	xhci_debugfs_create_root();
	xhci_dbc_init();

	return 0;
}

/*
 * If an init function is provided, an exit function must also be provided
 * to allow module unload.
 */
static void __exit xhci_hcd_fini(void)
{
	xhci_debugfs_remove_root();
	xhci_dbc_exit();
}

module_init(xhci_hcd_init);
module_exit(xhci_hcd_fini);