#ifdef __cplusplus
	extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "midih.h"
#include "midiu.h"

//+============================================================================ ========================================
// MIDI Host [USER] : Receive Message - Reserved
//   * MIDI_CIN_MISC               0x0[1...3] --> [RESERVED] for: Miscellaneous function codes.
//   * MIDI_CIN_CABLE_EVENT        0x1[1...3] --> [RESERVED] for: Cable events.
//
// The current driver always claims THREE bytes of data - but <shrug>
//
// You should probably never receive any of these - documentation is curiously sparse
//
bool  midih_rcvRsvd_cb (midih_port_t* from,  midimsg_t* mp)
{
	return true;
}

//+============================================================================ ========================================
// MIDI Host [USER] : Receive Message - System Common
//   * MIDI_CIN_SYSCOM_2BYTE       0x2[  2  ] --> 2byte sys.common msg {MTC, SongSelect}
//   * MIDI_CIN_SYSCOM_3BYTE       0x3[    3] --> 3byte sys.common msg {SPP}
//   * MIDI_CIN_SYSEX_END_1BYTE    0x5[1    ] --> 1byte sys.common msg {TuneRequest} ...overloaded
//
//   + MIDI_STATUS_SYSCOM_TIME_CODE_QUARTER_FRAME = 0xF1,
//   + MIDI_STATUS_SYSCOM_SONG_POSITION_POINTER   = 0xF2,
//   + MIDI_STATUS_SYSCOM_SONG_SELECT             = 0xF3,
//   + --undefined--                         = 0xF4
//   + --undefined--                         = 0xF5
//   + MIDI_STATUS_SYSCOM_TUNE_REQUEST            = 0xF6,
//
bool  midih_rcvSC_cb (midih_port_t* from,  midimsg_t* mp)
{
	switch (mp->event) {
		case MIDI_STATUS_SYSCOM_TIME_CODE_QUARTER_FRAME :  // 0xF1 - http://midi.teragonaudio.com/tech/midispec/frame.htm
		case MIDI_STATUS_SYSCOM_SONG_POSITION_POINTER   :  // 0xF2 - http://midi.teragonaudio.com/tech/midispec/ssp.htm
		case MIDI_STATUS_SYSCOM_SONG_SELECT             :  // 0xF3 - http://midi.teragonaudio.com/tech/midispec/songsel.htm
		case MIDI_STATUS_SYSCOM_TUNE_REQUEST            :  // 0xF6 - http://midi.teragonaudio.com/tech/midispec/tune.htm
			break;

		case 0xF4 :
		case 0xF5 :
			return false;  // undefined message type

		default:
			return false;  // unknown message type
	}
	return true;
}

//+============================================================================ ========================================
// MIDI Host [USER] : Receive Message - Running Status
//   * MIDI_CIN_1BYTE_DATA         0xF[1    ] --> Running Status bytes
//
//   + MIDI_STATUS_SYSREAL_TIMING_CLOCK           = 0xF8,
//   + MIDI_STATUS_SYSREAL_TICK                   = 0xF9,
//   + MIDI_STATUS_SYSREAL_START                  = 0xFA,
//   + MIDI_STATUS_SYSREAL_CONTINUE               = 0xFB,
//   + MIDI_STATUS_SYSREAL_STOP                   = 0xFC,
//   + --undefined--                         = 0xFD
//   + MIDI_STATUS_SYSREAL_ACTIVE_SENSING         = 0xFE,
//   + MIDI_STATUS_SYSREAL_SYSTEM_RESET           = 0xFF,
//
bool  midih_rcvRT_cb (midih_port_t* from,  midimsg_t* mp)
{
	switch (mp->event) {
		case MIDI_STATUS_SYSREAL_TIMING_CLOCK   :  // 0xF8 - http://midi.teragonaudio.com/tech/midispec/clock.htm
		case MIDI_STATUS_SYSREAL_TICK           :  // 0xF9 - http://midi.teragonaudio.com/tech/midispec/tick.htm
		case MIDI_STATUS_SYSREAL_START          :  // 0xFA - http://midi.teragonaudio.com/tech/midispec/start.htm
		case MIDI_STATUS_SYSREAL_CONTINUE       :  // 0xFB - http://midi.teragonaudio.com/tech/midispec/continue.htm
		case MIDI_STATUS_SYSREAL_STOP           :  // 0xFC - http://midi.teragonaudio.com/tech/midispec/stop.htm
		case MIDI_STATUS_SYSREAL_ACTIVE_SENSING :  // 0xFE - http://midi.teragonaudio.com/tech/midispec/sense.htm
		case MIDI_STATUS_SYSREAL_SYSTEM_RESET   :  // 0xFF - http://midi.teragonaudio.com/tech/midispec/reset.htm
			break;

		//case 0xF9 :
		case 0xFD :
			return false;  // undefined message type

		default:
			return false;  // unknown message type
	}
	return true;
}

//+============================================================================ ========================================
// MIDI Host [USER] : Receive Message - Sound Event
//   * MIDI_CIN_NOTE_OFF           0x8[    3] --> Note off {0x8}
//   * MIDI_CIN_NOTE_ON            0x9[    3] --> Note on {0x9}
//   * MIDI_CIN_POLY_KEYPRESS      0xA[    3] --> Key pressure ("aftertouch") {0xA}
//   * MIDI_CIN_CONTROL_CHANGE     0xB[    3] --> Control change [0..119] {0xB}
//   * MIDI_CIN_PROGRAM_CHANGE     0xC[  2  ] --> Program change {0xC}
//   * MIDI_CIN_CHANNEL_PRESSURE   0xD[  2  ] --> Channel pressure ("aftertouch") {0xD}
//   * MIDI_CIN_PITCH_BEND_CHANGE  0xE[    3] --> Pitch bend {0xE}
//
bool  midih_rcvSnd_cb (midih_port_t* from,  midimsg_t* mp)
{
	// (void) : The USB stack doesn't care if your synth fails to make sounds (or such)
	switch (mp->vevt) {
		case MIDI_EVENT_VOICE_NOTE_OFF          :
			(void) my_noteOff(mp->chan, mp->d1, mp->d2);
			break;
		case MIDI_EVENT_VOICE_NOTE_ON           :
			(void) my_noteOn(mp->chan, mp->d1, mp->d2);
			break;
		case MIDI_EVENT_VOICE_KEY_AFTERTOUCH    :
			(void) my_keyAft(mp->chan, mp->d1, mp->d2);
			break;
		case MIDI_EVENT_VOICE_CONTROLLER_CHANGE :
			if (mp->d1 >= 120)  (void) my_chnMode(mp->d1, mp->d2);
			else                (void) my_ctrlChg(mp->chan, mp->d1, mp->d2);
			break;
		case MIDI_EVENT_VOICE_PROGRAM_CHANGE    :
			(void) my_patchChg(mp->chan, mp->d1);
			break;
		case MIDI_EVENT_VOICE_CHAN_AFTERTOUCH   :
			(void) my_chnAft(mp->chan, mp->d1);
			break;
		case MIDI_EVENT_VOICE_PITCH_BEND        :
			(void) my_bend(mp->chan, mp->d1 | ((uint16_t)mp->d2 << 7));  // 0x2000 = centre
			break;

		default:
			return false;  // unknown message type
	}
	return true;
}

//+============================================================================ ========================================
// MIDI Host [USER] : Receive Message - SysEx
//   * MIDI_CIN_SYSEX_START        0x4[    3] --> SysEx start/continue
//   * MIDI_CIN_SYSEX_END_1BYTE    0x5[1    ] --> 1byte sys.common msg {TuneRequest} ...overloaded
//   * MIDI_CIN_SYSEX_END_2BYTE    0x6[  2  ] --> SysEx ends with 2 data
//   * MIDI_CIN_SYSEX_END_3BYTE    0x7[    3] --> SysEx ends with 3 data
//
//   + MIDI_STATUS_SYSEX_START                    = 0xF0,
//   + MIDI_STATUS_SYSEX_END                      = 0xF7,
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
// This is just some code that will get SysEx's working
// ...it is not intended to be clever, or optimal, or flexible
// ...it's JUST some example code
//
bool  midih_rcvSyx_cb (midih_port_t* from,  midimsg_t* mp)
{
	static uint8_t  buf[2048];
	static int      cnt       = 0;
	       int      n         = 3;  // assume type 4 or 7

	switch (mp->cin) {
		case MIDI_CIN_SYSEX_START     :  // 4
		case MIDI_CIN_SYSEX_END_3BYTE :  // 7
			//n = 3;
			break;

		case MIDI_CIN_SYSEX_END_1BYTE :  // 5
		case MIDI_CIN_SYSEX_END_2BYTE :  // 6
			n = mp->cin -4;
			break;

		default:
			return false;  // unknown type
	}

	// Append to SysEx buffer
	if (cnt + n > sizeof(buf))  return cnt = 0,  false ;  // buffer overflow
	else                        memcpy(buf +cnt, mp->data, n),  cnt += n ;

	// Call synth handler
	if (mp->cin != MIDI_CIN_SYSEX_START)  (void) my_syxHandler(from, buf, cnt),  cnt = 0 ;

	return true;
}

//----------------------------------------------------------------------------- ----------------------------------------
#ifdef __cplusplus
	}
#endif
