#ifndef  MIDIH_H_
#define  MIDIH_H_

#ifdef __cplusplus
	extern "C" {
#endif

//----------------------------------------------------------------------------- ----------------------------------------
#include "midi.h"  /// Stuff we are inheriting from the TinyUSB Device code

//----------------------------------------------------------------------------- ----------------------------------------
// Features
//
#ifndef MIDIH_ENABLE_GPIO
#	define MIDIH_ENABLE_GPIO  0  // default: Disabled
#endif

//----------------------------------------------------------------------------- ----------------------------------------
// MIDI Host [API] : Messages
//
//  +-----------------------------------------+
//  |     0     |     1     |   2    |   3    |  byte (little endian data)
//  | 7:4 | 3:0 | 7:4 | 3:0 |  7:0   |  7:0   |  bits
//  +-----------------------------------------+
//  |  USB Hdr  | MIDI Cmd  | MIDI_1 | MIDI_2 |
//  +-----------------------------------------+
//  | CN  | CIN |   Event   | Pram-1 | Pram-2 |
//  +-----------------------------------------+
//  CN     : Virtual cable number [per USB endpoint]
//  CIN    : MIDI Cmd/event category [why?]
//
typedef
	struct TU_ATTR_PACKED {
		union {
			uint32_t  msg;                          // USB MIDI Message
			struct {
				union {
					uint8_t  hdr;                   // USB Header
					struct {
						uint8_t  cn  : 4;           // USB Cable Number
						uint8_t  cin : 4;           // USB Category
					};
				};
				union {
					uint8_t  data[3];               // USB Payload
					struct {
						union {
							uint8_t  d0;            // USB Data byte 0
							uint8_t  event;         // MIDI event
							struct {
								uint8_t  vevt : 4;  // MIDI 
								uint8_t  chan : 4;  // MIDI Channel
							};
						};
						uint8_t  d1;                // Data byte 1
						uint8_t  d2;                // Data byte 2
					};
				};
			};
		};
		uint8_t  len;  // This is present for GPIO ports
	}
midimsg_t;

//----------------------------------------------------------------------------- ----------------------------------------
// MIDI Host [API] : MIDI Events
//
enum {
	MIDI_EVENT_VOICE_NOTE_OFF          = 0x8,
	MIDI_EVENT_VOICE_NOTE_ON           = 0x9,
	MIDI_EVENT_VOICE_KEY_AFTERTOUCH    = 0xA,  // AKA. Polyphonic Key Pressure
	MIDI_EVENT_VOICE_CONTROLLER_CHANGE = 0xB,
	MIDI_EVENT_VOICE_PROGRAM_CHANGE    = 0xC,
	MIDI_EVENT_VOICE_CHAN_AFTERTOUCH   = 0xD,  // AKA. Channel Pressure
	MIDI_EVENT_VOICE_PITCH_BEND        = 0xE,
	MIDI_EVENT_VOICE_MASK              = 0xF,
};

//----------------------------------------------------------------------------- ----------------------------------------
// MIDI Host [API] : MIDI Port modes
//
// Ports will be (implicitly) disabled by the midih_init() (by use of bzero())
//
// NB "True" MIDI-Thru is implemented in the hardware layer
//
typedef
	enum {
		MIN_OFF    = 0x00,                     // In : Virtual off switch
		MIN_LOCAL  = 0x01,                     // In : Only process messages locally
		MIN_THRU   = 0x02,                     // In : Only pass messages to MIDI-Out (do not process locally)
		MIN_FULL   = MIN_LOCAL | MIN_THRU,     // In : Process messages locally AND forward them to MIDI-Out

		MOUT_OFF   = 0x00,                     // Out: Virtual off switch
		MOUT_LOCAL = 0x10,                     // Out: Accept only Local messages
		MOUT_THRU  = 0x20,                     // Out: Accept only "Thru" messages
		MOUT_MIX   = MOUT_LOCAL | MOUT_THRU,   // Out: Accept Local and "Thru" messages
	}
mmode_t;

//----------------------------------------------------------------------------- ----------------------------------------
// MIDI Host [API] : Port direction
//
// Ports will be (implicitly) set to OUT (not IN) mode by the midih_init() (by use of bzero())
//
typedef
	enum {
		MDIR_OUT  = 0,  // Transmit
		MDIR_IN   = 1,  // Receive
	}
mdir_t;

//----------------------------------------------------------------------------- ----------------------------------------
// MIDI Host [API] : Port type
//
// Ports will be (implicitly) set to USB (not GPIO) mode by the midih_init() (by use of bzero())
//
typedef
	enum {
		MTYPE_USB   = 0,  // USB MIDI Port
		MTYPE_GPIO  = 1,  // GPIO MIDI Port
		MTYPE_LOCAL = 2,  // Local MIDI Port
	}
mtype_t;

//----------------------------------------------------------------------------- ----------------------------------------
// MIDI Host [API] : Port information
//
typedef
	struct {
		uint8_t    dev_addr;  // USB Device Address
		uint8_t    dev_cn;    // Cable Number; Each USB MIDI-In|Out Endpoint can have up to 16 "Cables"

		mtype_t    type;      // USB|GPIO|LOCAL
		mdir_t     dir;       // Out|In
		mmode_t    mode;      // In:Off|Local|Thru|Full, Out:Off|Local|Thru|Mix

		bool       syx;       // SysEx has started
	}
midih_port_t;

//----------------------------------------------------------------------------- ----------------------------------------
// MIDI Host [API] : Error codes
//
// https://stackoverflow.com/a/10966395/2476535.
//
#define  MERR_ENUMz(name, desc)    MERR_##name,
#define  MERR_STRING(name, desc)  desc,

// {enum, string} pairs
#define  FOREACH_MERR(pair) \
	/* Success conditions */ \
	pair(OK,        "OK"                     ) \
	pair(NOECHO,    "Port echo disabled"     ) \
	/* Failure conditions */ \
	pair(MALLOC,    "malloc() fail"          ) \
	pair(STOPPED,   "MIDIhost not running"   ) \
	pair(NOT_FOUND, "Port not found"         ) \
	pair(NOT_OUT,   "Port != MIDI-Out"       ) \
	pair(NOT_IN,    "Port != MIDI-In"        ) \
	pair(NOT_LOCAL, "Local Messages disabled") \
	pair(BAD_TYPE,  "Invalid Port type"      ) \
	pair(BAD_PORT,  "Invalid Port"           ) \
	pair(RUNNING,   "Already running!"       ) \
	pair(TODO,      "Feature not implemented") \

// enums
typedef
	enum {
		FOREACH_MERR(MERR_ENUMz)
	}
midih_err_t;

#define  MERR(merr)  (midih_err = MERR_##merr)  // Set midih error status. EG. MERR(MALLOC)

extern midih_err_t  midih_err;     // printf("error %d: %s", midih_err,
extern const char*  midih_errs[];  //                        midih_errs[midih_err]);

//----------------------------------------------------------------------------- ----------------------------------------
// Function protoypes
//
// cat *.c | grep '^{' -B 2 | sed '/^{/d;/^--$/d;/\/\//d'
//

//-[ midih_core.c ]------------------------------------------------------------ 
bool           midih_start        (int maxPorts) ;                               // [CORE-API] Driver - Start
bool           midih_stop         (void) ;                                       // [CORE-API] Driver - Stop

bool           midih_portMode     (midih_port_t* pp,  mtype_t type,  mdir_t dir,  mmode_t mode) ;
                                                                                 // [CORE-API] Set port operating mode
bool           midih_portLocal    (midih_port_t* pp,  bool local) ;              // [CORE-API] Set Local mode

bool           midih_rcvUSB       (uint8_t dev_addr,  midimsg_t msg) ;           // [CORE] Receive Message from USB port

#if MIDIH_ENABLE_GPIO
bool           midih_rcvGPIO      (uint8_t dev_addr,  midimsg_t msg) ;           // [CORE] Receive Message from GPIO pin
#endif

bool           midih_route        (midih_port_t* from,  midimsg_t* mp) ;         // [CORE] Message Router (In->Out)
static bool    midih_rcvSyx       (midih_port_t* from,  midimsg_t* mp) ;         // [CORE] Receive Sysex

midih_port_t*  midih_dev2port     (uint8_t dev_addr,  uint8_t dev_cn) ;          // [API] Get the Port (Pointer) by dev_addr
bool           midih_xmt2port     (midih_port_t* to,  midimsg_t* mp) ;           // [API] Send message to port
bool           midih_xmt2dev      (uint8_t dev_addr,  midimsg_t* mp) ;           // [API] Send message to port, by device_address
bool           midih_sendSyx      (midih_port_t* to,  uint8_t* buf,  int len) ;  // [API] Send SysEx to port

//----------------------------------------------------------------------------- ----------------------------------------
#ifdef __cplusplus
	}
#endif

#endif // MIDIH_H_
