#ifdef __cplusplus
	extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "midih.h"
#include "midiu.h"

#include "test.h" //!

//----------------------------------------------------------------------------- ----------------------------------------
// Port information
//
static midih_port_t*  midih_port    = NULL;  // Array of MIDI Ports
static int            midih_portMax = 0;     // Array Max
static int            midih_portCnt = 0;     // Array Count

//----------------------------------------------------------------------------- ----------------------------------------
// Error status fields
//
midih_err_t    midih_err     = MERR_OK;
const  char*   midih_errs[]  = { FOREACH_MERR(MERR_STRING) };

//+============================================================================ ========================================
// MIDI Host [CORE-API] : Driver - Start
//
bool  midih_start (int maxPorts)
{
	if (midih_port)  return MERR(RUNNING), false ;

	if ( !(midih_port = calloc((midih_portMax = maxPorts), sizeof(*midih_port))) )  return MERR(MALLOC), false ;
	midih_portCnt = 0;

	return MERR(OK), true;
}

//+============================================================================ ========================================
// MIDI Host [CORE-API] : Driver - Stop
//
bool  midih_stop (void)
{
	free(midih_port);
	midih_port    = NULL;  // Kill MIDI engine
	midih_portMax = 0;
	midih_portCnt = 0;

	return MERR(OK), true;
}

//+============================================================================ ========================================
// MIDI Host [CORE-API] : Set Port operating mode
//
bool  midih_portMode (midih_port_t* pp,  mtype_t type,  mdir_t dir,  mmode_t mode)
{
	if (!pp)  return MERR(NOT_FOUND), false ;

	pp->type = type;
	pp->dir  = dir;
	pp->mode = mode;

	return MERR(OK), true;
}

//+============================================================================ ========================================
// MIDI Host [CORE-API] : Set Port Local mode
//
// Called by my_localMode()
//
bool  midih_portLocal (midih_port_t* pp,  bool local)
{
	if (!pp)  return MERR(NOT_FOUND), false ;

	pp->mode = (pp->mode & ~MIN_LOCAL) | (local ? MIN_LOCAL : 0);

	return MERR(OK), true;
}

//+============================================================================ ========================================
// MIDI Host : Receive Message from USB port
//
bool  midih_rcvUSB (uint8_t dev_addr,  midimsg_t msg)
{
	midih_port_t*  pp = NULL;

	// https://www.midi.org/specifications-old/item/table-1-summary-of-midi-message
	static const bool(*rcvHook[16])(midih_port_t*, midimsg_t*) = {
		midih_rcvRsvd_cb,  // MIDI_CIN_MISC              : 0x0 [1...3] --> [RESERVED] for: Miscellaneous function codes.
		midih_rcvRsvd_cb,  // MIDI_CIN_CABLE_EVENT       : 0x1 [1...3] --> [RESERVED] for: Cable events.
		midih_rcvSC_cb  ,  // MIDI_CIN_SYSCOM_2BYTE      : 0x2 [  2  ] --> 2byte sys.common msg {MTC, SongSelect}
		midih_rcvSC_cb  ,  // MIDI_CIN_SYSCOM_3BYTE      : 0x3 [    3] --> 3byte sys.common msg {SPP}
		midih_rcvSyx    ,  // MIDI_CIN_SYSEX_START       : 0x4 [    3] --> SysEx start/continue
		NULL            ,  // MIDI_CIN_SYSEX_END_1BYTE   : 0x5 [1    ] --> 1byte sys.common msg {TuneRequest} ...overloaded
		midih_rcvSyx    ,  // MIDI_CIN_SYSEX_END_2BYTE   : 0x6 [  2  ] --> SysEx ends with 2 data
		midih_rcvSyx    ,  // MIDI_CIN_SYSEX_END_3BYTE   : 0x7 [    3] --> SysEx ends with 3 data
		midih_rcvSnd_cb ,  // MIDI_CIN_NOTE_OFF          : 0x8 [    3] --> Note off {0x8}
		midih_rcvSnd_cb ,  // MIDI_CIN_NOTE_ON           : 0x9 [    3] --> Note on {0x9}
		midih_rcvSnd_cb ,  // MIDI_CIN_POLY_KEYPRESS     : 0xA [    3] --> Key pressure ("aftertouch") {0xA}
		midih_rcvSnd_cb ,  // MIDI_CIN_CONTROL_CHANGE    : 0xB [    3] --> Control change [0..119] {0xB}
		midih_rcvSnd_cb ,  // MIDI_CIN_PROGRAM_CHANGE    : 0xC [  2  ] --> Program change {0xC}
		midih_rcvSnd_cb ,  // MIDI_CIN_CHANNEL_PRESSURE  : 0xD [  2  ] --> Channel pressure ("aftertouch") {0xD}
		midih_rcvSnd_cb ,  // MIDI_CIN_PITCH_BEND_CHANGE : 0xE [    3] --> Pitch bend {0xE}
		midih_rcvRT_cb     // MIDI_CIN_1BYTE_DATA        : 0xF [1    ] --> Realtime / Running Status bytes
	};                     //        __________________________/^^^^^\_____________
	static const int   rcvLen[16] = {3, 3, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1};  // Packet lengths (for GPIO ports)

	if (!midih_port)  return false ;                   // Driver not running

	pp = midih_dev2port(dev_addr, msg.cn);             // Find port

	if ((pp->dir != MDIR_IN) || (pp->type != MTYPE_USB))  return false ;  // Not a USB input port

	if (pp->mode == MIN_OFF)  return true ;            // Port is disabled

	msg.len = rcvLen[msg.cin];                         // Number of bytes in this packet type

	if (pp->mode & MIN_THRU)  midih_route(pp, &msg) ;  // Echo is enabled (virtual Thru)

	return rcvHook[msg.cin] ? rcvHook[msg.cin](pp, &msg) :                   // use array
	              (pp->syx) ? rcvHook[MIDI_CIN_SYSEX_END_2BYTE](pp, &msg)    // end of a sysex
	                        : rcvHook[MIDI_CIN_SYSCOM_2BYTE   ](pp, &msg) ;  // 1byte sys.common
}

//+============================================================================ ========================================
// MIDI Host : Receive Message from GPIO pin
//
#if MIDIH_ENABLE_GPIO
bool  midih_rcvGPIO (uint8_t dev_addr,  midimsg_t msg)
{
	return MERR(TODO), false; //!
}
#endif

//+============================================================================ ========================================
// MIDI Host : Send a packet from a MIDI-In port to all (routed &) thru-enabled MIDI-Out ports
//
// If you are designing a MIDI Router - this is where your (functional, not UI) code will live
//
// In this trivial example, every [thru-enabled] IN is simply routed to every [thru-enabled] OUT.
//
bool  midih_route (midih_port_t* from,  midimsg_t* mp)
{
	midih_port_t*  to = NULL;
	int            i;

	if (from->dir != MDIR_IN)  return MERR(NOT_IN), false ;      // Only input ports can pass data "thru"

	if (!(from->mode & MOUT_THRU))  return MERR(NOECHO), true ;  // Port echo is disabled

	// Routing code : Every IN goes to every OUT
	for (i = 0,  to = midih_port;  i < midih_portMax;  i++, to++) {
		if ((to->dir == MDIR_OUT) && (to->mode & MOUT_THRU)) {   // Output port IS enabled for "thru"
			midih_xmt2port(to, mp);
		}
	}

	return MERR(OK), true;
}

//+============================================================================ ========================================
// MIDI Host : Receive Sysex
//
//   * MIDI_CIN_SYSEX_START     --> SysEx start/continue
//   * MIDI_CIN_SYSEX_END_1BYTE --> 1byte sys.common msg {TuneRequest} ...overloaded
//   * MIDI_CIN_SYSEX_END_2BYTE --> SysEx ends with 2 data
//   * MIDI_CIN_SYSEX_END_3BYTE --> SysEx ends with 3 data
//
static
bool  midih_rcvSyx (midih_port_t* from,  midimsg_t* mp)
{
	switch (mp->cin) {
		case MIDI_CIN_SYSEX_START     :  // 4
			//if (from->syx)  detected_unclosed_sysex()
			from->syx = true;
			midih_rcvSyx_cb(from, mp);
			break;

		case MIDI_CIN_SYSEX_END_1BYTE :  // 5
		case MIDI_CIN_SYSEX_END_2BYTE :  // 6
		case MIDI_CIN_SYSEX_END_3BYTE :  // 7
			midih_rcvSyx_cb(from, mp);
			from->syx = false;
			break;
	}
	return MERR(OK), true;
}

//+============================================================================ ========================================
// MIDI Host [API] : Get the Port (Pointer) for the specified dev_addr
//
midih_port_t*  midih_dev2port (uint8_t dev_addr,  uint8_t dev_cn)
{
	midih_port_t*  pp;
	int            i;

	if (!midih_port)  return MERR(STOPPED), NULL ;

	// O(n) search for midi port
	for (i = 0,  pp = midih_port;  i < midih_portMax;  i++, pp++)
		if ((pp->dev_addr == dev_addr) && (pp->dev_cn == dev_cn))  return MERR(OK), pp ;

	return MERR(NOT_FOUND), NULL;
}

//+============================================================================ ========================================
// MIDI Host [API] : Send a packet to MIDI-Out port
//
bool  midih_xmt2port (midih_port_t* to,  midimsg_t* mp)
{
	if (!to)                  return MERR(NOT_FOUND), false ;
	if (to->dir != MDIR_OUT)  return MERR(NOT_OUT  ), false ;

	switch (to->type) {
		case MTYPE_USB   :    return doit_usb(to->dev_addr, mp) ;  //!
		case MTYPE_LOCAL :    return doit_virt(to, mp) ;           //!
#if ENABLE_GPIO
		case MTYPE_GPIO  :    return doit_gpio(to, mp) ;           //!
#endif
		default          :    return MERR(BAD_TYPE), false;
	}
}

//+============================================================================ ========================================
// MIDI Host [API] : Send message to port, by device_address
//
bool  midih_xmt2dev (uint8_t dev_addr,  midimsg_t* mp)
{
	midih_port_t*  pp = midih_dev2port(dev_addr, mp->cn);

	if (!pp)  return false ;        // merr set by dev2port()

	return midih_xmt2port(pp, mp);  // merr set by xmt2port()
}

//+============================================================================ ========================================
// MIDI Host [API] : Send a sysex to MIDI-Out
//
//   | Msg |
//   | Len | Messages
//   +-----+-----------------------------------------------------------
//   |  0  |                                           c6 F0 F7 --
//   |  1  |                                           c7 F0 m1 F7
//   |     |
//   |  2  | c4 F0 m1 m2,                              c5 F7 -- --
//   |  3  | c4 F0 m1 m2,                              c6 m3 F7 --
//   |  4  | c4 F0 m1 m2,                              c7 m3 m4 F7
//   |     |
//   |  5  | c4 F0 m1 m2,  c4 m3 m4 m5,                c5 F7 -- --
//   |  6  | c4 F0 m1 m2,  c4 m3 m4 m5,                c6 m6 F7 --
//   |  7  | c4 F0 m1 m2,  c4 m3 m4 m5,                c7 m6 m7 F7
//   |  8  | c4 F0 m1 m2,  c4 m3 m4 m5,  c4 m6 m7 m8,  c5 F7 -- --
//
bool  midih_sendSyx (midih_port_t* to,  uint8_t* buf,  int len)
{
	midimsg_t  msg;
	uint8_t*   bp  = NULL;
	int        i;
	int        sz;

	//if (!to)  send to all enabled, nonblocking, MIDI-Out ports"
	if (!to)                       return MERR(BAD_PORT ), false ;

	if (to->dir != MDIR_OUT)       return MERR(NOT_OUT  ), false ;
	if (!(to->mode & MOUT_LOCAL))  return MERR(NOT_LOCAL), false ;  // not accepting "local" messages

	msg.cn = to->dev_cn;

	//-----------------------------------------------------
	// User did NOT wrap the Sysex
	if ((len && (buf[0] != MIDI_STATUS_SYSEX_START)) || !len) {
		// Sanity check
		if (len)
			for (i = 0,  bp = buf;  i < len;  i++, bp++)
				if (*bp & 0x80)  return false ;  // found 8bit data

		// Start message
		switch (len) {
			case 0:
				msg.cin = MIDI_CIN_SYSEX_END_2BYTE;
				msg.len = 2;
				memcpy(&msg.data, (uint8_t[]){MIDI_STATUS_SYSEX_START, MIDI_STATUS_SYSEX_END, 0x00}, 3);
				sz = 0;
				break;
			case 1:
				msg.cin = MIDI_CIN_SYSEX_END_3BYTE;
				msg.len = 3;
				memcpy(&msg.data, (uint8_t[]){MIDI_STATUS_SYSEX_START, buf[0], MIDI_STATUS_SYSEX_END}, 3);
				sz = 1;
				break;
			default:
				msg.cin = MIDI_CIN_SYSEX_START;
				msg.len = 3;
				memcpy(&msg.data, (uint8_t[]){MIDI_STATUS_SYSEX_START, buf[0], buf[1]}, 3);
				sz = 2;
				break;
		}
		to->syx = true;
		midih_xmt2port(to, &msg);

		// Middle message(s)
		msg.cin = MIDI_CIN_SYSEX_START;
		for (msg.len = 3;  (len - sz) >= 3;  sz += 3) {
			memcpy(&msg.data, (uint8_t[]){buf[sz], buf[sz+1], buf[sz+2]}, 3);
			midih_xmt2port(to, &msg);
		}

		// End message
		switch (len - sz) {
			case 0:
				msg.cin = MIDI_CIN_SYSEX_END_1BYTE;
				msg.len = 1;
				memcpy(msg.data, (uint8_t[]){MIDI_STATUS_SYSEX_END, 0x00, 0x00}, 3);
				break;
			case 1:
				msg.cin = MIDI_CIN_SYSEX_END_2BYTE;
				msg.len = 2;
				memcpy(msg.data, (uint8_t[]){buf[sz], MIDI_STATUS_SYSEX_END, 0x00}, 3);
				break;
			case 2:
				msg.cin = MIDI_CIN_SYSEX_END_3BYTE;
				//msg.len = 3;
				memcpy(msg.data, (uint8_t[]){buf[sz], buf[sz+1], MIDI_STATUS_SYSEX_END}, 3);
				break;
		}
		midih_xmt2port(to, &msg);
		to->syx = false;

	//-----------------------------------------------------
	// User DID wrap the Sysex [sysex header detected]
	} else {
		if (buf[len-1] != MIDI_STATUS_SYSEX_END)  return false ;  // Missing footer

		// Sanity check
		for (i = 1,  bp = buf;  i < len-1;  i++, bp++)
			if (*bp & 0x80)  return false ;  // found 8bit data

		// Start & Middle message(s)
		to->syx = true;
		msg.cin = MIDI_CIN_SYSEX_START;
		msg.len = 3;
		for (sz = 0;  (len - sz) > 3;  sz += 3) {
			memcpy(msg.data, (uint8_t[]){buf[sz], buf[sz+1], buf[sz+2]}, 3);
			midih_xmt2port(to, &msg);
		}

		// End message
		switch (len - sz) {
			case 1:
				msg.cin = MIDI_CIN_SYSEX_END_2BYTE;
				msg.len = 1;
				memcpy(msg.data, (uint8_t[]){buf[sz], 0x00, 0x00}, 3);
				break;
			case 2:
				msg.cin = MIDI_CIN_SYSEX_END_2BYTE;
				msg.len = 2;
				memcpy(msg.data, (uint8_t[]){buf[sz], buf[sz+1], 0x00}, 3);
				break;
			case 3:
				msg.cin = MIDI_CIN_SYSEX_END_3BYTE;
				msg.len = 3;
				memcpy(msg.data, (uint8_t[]){buf[sz], buf[sz+1], buf[sz+2]}, 3);
				break;
		}
		midih_xmt2port(to, &msg);
		to->syx = false;
	}

	return MERR(OK), true;
}

//----------------------------------------------------------------------------- ----------------------------------------
#ifdef __cplusplus
	}
#endif
