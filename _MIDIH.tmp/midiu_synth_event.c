#ifdef __cplusplus
	extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "midih.h"
#include "midiu.h"

//+============================================================================ ========================================
// MIDI Host [SYNTH-EVENT] - Stop all notes (aka "panic")
//     ** It is UNLIKELY that you will edit this function **
//
bool  my_chnNotesOff (uint8_t chan)
{
	bool  rv = true;

	for (uint8_t key = 0;  key <= KEY_MAX;  key++) {
		rv &= (mySynth.chan[chan].note[key].on  ? my_noteOff(chan, key, KEY_VEL_MAX) : true) ;
		rv &= (mySynth.chan[chan].note[key].aft ? my_keyAft( chan, key,           0) : true) ;  //! should I be doing this?
	}

	return rv;  // false if ANYTHING failed
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-EVENT] - Stop all notes (aka "panic")
//     ** It is UNLIKELY that you will edit this function **
//
bool  my_allNotesOff (void)
{
	bool  rv = true;

	for (uint8_t chan = 0;  chan <= 15;  chan++)
		rv &= my_chnNotesOff(chan);

	return rv;  // false if ANYTHING failed
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-EVENT] - Reset all Controllers to their default values
//
bool  my_rstAllCtrls (void)
{
	//** Choose some good defaults for your synth

	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-EVENT] - Process SysEx Message
//
// We assume the SysEx has been validated before getting here
//
bool  my_syxHandler (midih_port_t* from,  uint8_t* buf,  int cnt)
{
	struct TU_ATTR_PACKED {
		uint8_t  syxSt;   // 0xF0
		uint8_t  rt;      // 0x7E=Non-RealTime; 0x7F=RealTime
		//...
	}* pMsg = (void*)buf;

	if (pMsg->rt == MIDH_VID_NON_REALTIME)  return my_syxNonRealitime(from, buf, cnt) ;
	if (pMsg->rt == MIDH_VID_REALTIME)      return my_syxRealitime(   from, buf, cnt) ;

	//** Act on the received SysEx

	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-SYX-UNI] - Process SysEx Non-Realtime Message
//    http://midi.teragonaudio.com/tech/midispec.htm
//    http://midi.teragonaudio.com/tech/midispec/uni.htm
//
// Currently, this treats Non-Realtime Messages as Realtime Messages
//
// We assume the SysEx has been validated before getting here
//
bool  my_syxNonRealitime (midih_port_t* from,  void* buf,  int cnt)
{
	return my_syxRealitime(from, buf, cnt) ;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-SYX-UNI] - Process SysEx Non-Realtime Message
//    http://midi.teragonaudio.com/tech/midispec.htm
//    http://midi.teragonaudio.com/tech/midispec/uni.htm
//
// We (currently) treat Non-Realtime Messages as Realtime Messages
//
// We assume the SysEx has been validated before getting here
//
bool  my_syxRealitime (midih_port_t* from,  void* buf,  int cnt)
{
	struct TU_ATTR_PACKED {
		uint8_t  syxSt;   // 0xF0
		uint8_t  rt;      // 0x7E=Non-RealTime; 0x7F=RealTime
		uint8_t  schan;   // 0 <= SysEx_Channel <= 126; 127=disregard
		uint8_t  subID;   //
		uint8_t  subID2;  //
		//...
	}* pMsg = buf;

	if (pMsg->schan != 127)  { // Disregard channel
		// ** Apply additional SysEx Channel filtering
	}

	if ((cnt == 6) && (pMsg->subID == 0x09)                          )  return my_syxGmEnable(buf) ;
	if ((cnt == 8) && (pMsg->subID == 0x04) && (pMsg->subID2 == 0x01))  return my_syxMasterVol(buf) ;
	if ((cnt == 6) && (pMsg->subID == 0x06) && (pMsg->subID2 == 0x01))  return my_syxIdReq(from, buf) ;
	if ((cnt == 7) && (pMsg->subID == 0x03)                          )  return my_syxSdsDumpReq(from, buf) ;
	return false;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-SYX-UNI] - GM Enable (on/off)
//
// We assume the SysEx has been validated before getting here
//
bool  my_syxGmEnable (void* buf)
{
	struct TU_ATTR_PACKED {
		uint8_t  syxSt;   // 0xF0
		uint8_t  nrt;     // 0x7E
		uint8_t  schan;   // 0 <= SysEx_Channel <= 126
		uint8_t  subID;   // 0x09 - GM Enable
		uint8_t  onOff;   // 0=Off; 1=On
		uint8_t  syxNd;   // 0xF7
	}* pMsg = buf;

	if (pMsg->onOff == 0) {

		//** Disable GM Mode

		return true;
	}

	if (pMsg->onOff == 1) {

		//** Enable GM Mode

		return true;
	}

	return false;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-SYX-UNI] - Master Volume
//
// We assume the SysEx has been validated before getting here
//
bool  my_syxMasterVol (void* buf)
{
	struct TU_ATTR_PACKED {
		uint8_t  syxSt;   // 0xF0
		uint8_t  rt;      // 0x7F
		uint8_t  schan;   // 0 <= SysEx_Channel <= 126
		uint8_t  subID;   // 0x04 - Device Control
		uint8_t  subID2;  // 0x09 - Master Volume
		uint8_t  volLSB;  // [ 6: 0]
		uint8_t  volMSB;  // [13: 7]
		uint8_t  syxNd;   // 0xF7
	}* pMsg = buf;

	midi14_t  vol = (midi14_t){ .lsb = pMsg->volLSB,  .msb = pMsg->volMSB };

	//** Set Master Volume

	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-SYX-UNI] - Identity Request
//
// We assume the SysEx has been validated before getting here
//
bool  my_syxIdReq (midih_port_t* from,  void* buf)
{
// We don't actually need anything from the request packet
//	struct TU_ATTR_PACKED {
//		uint8_t  syxSt;   // 0xF0
//		uint8_t  nrt;     // 0x7E
//		uint8_t  schan;   // 0 <= SysEx_Channel <= 126
//		uint8_t  subID;   // 0x06 - General Information
//		uint8_t  subID2;  // 0x01 - Identity Request
//		uint8_t  syxNd;   // 0xF7
//	}* pMsg = buf;

	struct TU_ATTR_PACKED {
		uint8_t  syxSt;
		uint8_t  nrt;     
		uint8_t  schan;   
		uint8_t  subID;   
		uint8_t  subID2;  
		uint8_t  vid;     
		uint8_t  fam1;
		uint8_t  fam2;
		uint8_t  pid1;
		uint8_t  pid2;
		uint8_t  ver1;
		uint8_t  ver2;
		uint8_t  ver3;
		uint8_t  ver4;
		uint8_t  syxNd;
	} reply = {
		.syxSt  = MIDI_STATUS_SYSEX_START,
		.nrt    = MIDH_VID_NON_REALTIME,
		.schan  = 127,                   // We may want this to be something else!?
		.subID  = 0x06,                  // 0x06 - General Information
		.subID2 = 0x02,                  // 0x02 - Identity Reply
		.vid    = MIDH_VID_EDUCATIONAL,  //** Manufacturer/Vendor ID ...what happens for 3 byte VIDs?? //!
		.fam1   = 0,                     //** Device Family
		.fam2   = 0,                     //** ...
		.pid1   = 0,                     //** Device Product ID
		.pid2   = 0,                     //** ...
		.ver1   = 0,                     //** Device Version
		.ver2   = 0,                     //** ...
		.ver3   = 0,                     //** ...
		.ver4   = 0,                     //** ...
		.syxNd  = MIDI_STATUS_SYSEX_END
	};
	
	return midih_sendSyx(from, (uint8_t*)&reply, sizeof(reply));
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-SYX-UNI] - SDS Dump Request
//    http://midi.teragonaudio.com/tech/sds.htm
//
// We assume the SysEx has been validated before getting here
//
bool  my_syxSdsDumpReq (midih_port_t* from,  void* buf)
{
	struct TU_ATTR_PACKED {
		uint8_t  syxSt;    // 0xF0
		uint8_t  nrt;      // 0x7E
		uint8_t  schan;    // 0 <= SysEx_Channel <= 126
		uint8_t  sampLSB;  // [ 6: 0] - Sample ID
		uint8_t  sampMSB;  // [13: 7] - ...
		uint8_t  syxNd;    // 0xF7
	}* pMsg = buf;
	
	midi14_t  sid = (midi14_t){ .lsb = pMsg->sampLSB,  .msb = pMsg->sampMSB };
	
	//** Dump the requested sample: http://midi.teragonaudio.com/tech/sds.htm
	(void)from;
	
	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-EVENT] - Note Off
//
bool  my_noteOff (uint8_t chan,  uint8_t key,  uint8_t vel)
{
	if ((key > UINT7_MAX) || (vel > UINT7_MAX))  return false ;  // bad prams

	mySynth.chan[chan].note[key] = (myKey_t){.on = false,  .vel = vel} ;

	//** Stop note playing

	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-EVENT] - Note On
//
bool  my_noteOn (uint8_t chan,  uint8_t key,  uint8_t vel)
{
	if (vel == 0)  return my_noteOff(chan, key, KEY_VEL_MAX) ;  // Running Status !?

	if ((key > UINT7_MAX) || (vel > UINT7_MAX))  return false ;  // bad prams

	mySynth.chan[chan].note[key] = (myKey_t){.on = true,  .vel = vel};

	//** Start note playing

	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-EVENT] - Key aftertouch
//
bool  my_keyAft (uint8_t chan,  uint8_t key,  uint8_t pressure)
{
	if ((key > UINT7_MAX) || (pressure > UINT7_MAX))  return false ;  // bad prams

	mySynth.chan[chan].note[key].aft = pressure ;

	//** Apply aftertouch effect to specified note

	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-EVENT] - Change controller
//
//  http://www.studio4all.de/htmle/main91.html
//  https://www.voidaudio.net/controller.html
// ,-------------------------------------------------------------------------------------------.
// | Num | Name                       | Range  | GM  | GM2 | AWE32/ |    Roland/GS    | Yam/XG |
// |     |                            |        |     |     | Live!  | SC55  | SC88Pro | SW1000 |
// +-----+----------------------------+--------+-----+-----+--------+-------+---------+--------|
// |   0 | Bank Select MSB ---.       | SELECT |  -  |  *  |   *    |   *   |    *    |    *   |
// |   1 | Modulation         |       | SELECT |  *  |  *  |   *    |   *   |    *    |    *   |
// |   2 | Breath Control     |       | SELECT |  -  |  -  |   -    |   -   |    -    |    -   |
// |     |                    |       |        |     |     |        |       |         |        |
// |   4 | Foot Control       |       | SELECT |  -  |  -  |   -    |   -   |    -    |    -   |
// |   5 | Portamento Time    |       | SELECT |  -  |  *  |   -    |   *   |    *    |    *   |
// |   6 | Data Entry MSB ----)---,   | SELECT |  *  |  *  |   *    |   *   |    *    |    *   |
// |   7 | Volume             |   |   | SELECT |  *  |  *  |   *    |   *   |    *    |    *   |
// |     |                    |   |   |        |     |     |        |       |         |        |
// |  10 | Panpot (Stereo)    |   |   | SELECT |  *  |  *  |   *    |   *   |    *    |    *   |
// |  11 | Expression         |   |   | SELECT |  *  |  *  |   *    |   *   |    *    |    *   |
// |     |                    |   |   |        |     |     |        |       |         |        |
// |  32 | Bank Select LSB ---'   |   | SELECT |  -  |  *  |   *    |   *   |    *    |    *   |
// |     |                        |   |        |     |     |        |       |         |        |
// |  38 | Data Entry LSB --------'   | SELECT |  *  |  *  |   *    |   *   |    *    |    *   |
// |     |                            |        |     |     |        |       |         |        |
// |  64 | Damper Pedal    ---.       | PEDAL  |  *  |  *  |   *    |   *   |    *    |    *   |
// |  65 | Portamento        -|       | PEDAL  |  -  |  *  |   -    |   *   |    *    |    *   |
// |  66 | Sustenuto Pedal   -|       | PEDAL  |  -  |  *  |   -    |   *   |    *    |    *   |
// |  67 | Soft Pedal      ---'       | PEDAL  |  -  |  *  |   -    |   -   |    *    |    *   |
// |     |                            |        |     |     |        |       |         |        |
// |  71 | Filter Cutoff              | SELECT |  -  |  *  |   -    |   -   |    -    |    *   |
// |  72 | Release Time               | SELECT |  -  |  *  |   -    |   -   |    -    |    *   |
// |  73 | Attack Time                | SELECT |  -  |  *  |   -    |   -   |    -    |    *   |
// |  74 | Filter Resonance           | SELECT |  -  |  *  |   -    |   -   |    -    |    *   |
// |  75 | Decay Time                 | SELECT |  -  |  *  |   -    |   -   |    -    |    -   |
// |     |                            |        |     |     |        |       |         |        |
// |  76 | Vibrato Depth              | SELECT |  -  |  *  |   -    |   -   |    -    |    -   |
// |  77 | Vibrato Rate               | SELECT |  -  |  *  |   -    |   -   |    -    |    -   |
// |  78 | Vibrato Delay              | SELECT |  -  |  *  |   -    |   -   |    -    |    -   |
// |     |                            |        |     |     |        |       |         |        |
// |  84 | Portamento Control         | ANY    |  -  |  -  |   -    |   *   |    *    |    *   |
// |     |                            |        |     |     |        |       |         |        |
// |  91 | FX-1 Send: Reverb          | SELECT |  -  |  *  |   *    |   *   |    *    |    *   |
// |     |                            |        |     |     |        |       |         |        |
// |  93 | FX-3 Send: Chorus          | SELECT |  -  |  *  |   *    |   *   |    *    |    *   |
// |  94 | FX-4 Send: Delay/Variation | SELECT |  -  |  -  |   -    |   -   |    *    |    *   |
// |     |                            |        |     |     |        |       |         |        |
// |  96 | Data Increment (+1)        | ANY    |  *  |  *  |   *    |   *   |    *    |    *   |
// |  97 | Data Decrement (-1)        | ANY    |  *  |  *  |   *    |   *   |    *    |    *   |
// |     |                            |        |     |     |        |       |         |        |
// |  98 | NRPN LSB [0lll'llll] --.   | SELECT |  -  |  -  |   *    |   *   |    *    |    *   |
// |  99 | NRPN MSB [0mmm'mmmm] --'   | SELECT |  -  |  -  |   *    |   *   |    *    |    *   |
// |     |                            |        |     |     |        |       |         |        |
// | 100 | RPN LSB  [0lll'llll] --.   | SELECT |  *  |  *  |   *    |   *   |    *    |    *   |
// | 101 | RPN MSB  [0mmm'mmmm] --'   | SELECT |  *  |  *  |   *    |   *   |    *    |    *   |
// `-------------------------------------------------------------------------------------------'
//   Key: SELECT : 0..127 - Set value for controller
//        ANY    : 0..127 - Any value to trigger the controller
//        PEDAL  : 0 <= off < 64 <= on <= 127
//
bool  my_ctrlChg (uint8_t chan,  uint8_t ctrl,  uint8_t val)
{
	if (ctrl >= 120)  return false ;  // bad user-controller

	mySynth.chan[chan].ctrl[ctrl] = val;

	// RPN Controllers
	if ( (ctrl == MIDIH_CTRL_DATA_MSB) || (ctrl == MIDIH_CTRL_DATA_LSB) ||
	     (ctrl == MIDIH_CTRL_DATA_INC) || (ctrl == MIDIH_CTRL_DATA_DEC) )
		return my_rpnChg(chan, ctrl, val);

	//** Handle Controller Messages

	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-EVENT] - Patch/Program change
//
bool  my_patchChg (uint8_t chan,  uint8_t patch)
{
	if (patch >= UINT7_MAX)  return false ;  // bad patch

	my_chnNotesOff(chan);
	mySynth.chan[chan].patch = patch;

	//** Set the patch on this channel of your synth to:
	//     bank  = mySynth.chan[chan].bank.val  // you may split .val with {.msb|.lsb|.coarse|.fine}
	//     patch = mySynth.chan[chan].patch

	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-EVENT] - Channel aftertouch
//
bool  my_chnAft (uint8_t chan,  uint8_t pressure)
{
	if (pressure >= UINT7_MAX)  return false ;  // bad pressure

	mySynth.chan[chan].chnAft = pressure;

	//** Apply aftertouch to all notes playing on this channel

	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-EVENT] - Bend note
//
// bend is a 14bit value in the range
//   Machine : 0x0000 .. 0x2000 .. 0x3FFF
//   User    :  -8192 ..    0   .. +8191
//   Tone    :  Lower .. Centre .. Higher
//
// userNotation    = machineNotation - 0x2000
// machineNotation = userNotation    + 8192
//
// At the extremes, a pitch shift will occur that matches the number of
// semitones set by my_bendRange()
// EG. If 'bend' is configured to 12 semitones (1 octave)
//       then  -8192 will be 12 semitones down
//        and  +8191 will be 12 semitones up
// ...a total range of 24 semitones (2 octaves)
//
static
bool  my_bend_ (uint8_t chan,  uint16_t bend)
{
	mySynth.chan[chan].bend.val = bend;

	//** Apply bend to all notes playing on this channel

	return true;
}

//+============================================================================
// MIDI Host [SYNTH-EVENT] - Bend note {-0x0000 .. 0x2000 .. 0x3FFF)
//     ** There should be NO need to edit this function **
//
bool  my_bendAbs (uint8_t chan,  uint16_t bend)
{
	return (bend & MIDI14_MASK) ? false : my_bend_(chan, bend) ;  // out of range - bad prm
}

//+============================================================================
// MIDI Host [SYNTH-EVENT] - Bend note {-8192 .. 0 .. +8191)
//     ** There should be NO need to edit this function **
//
bool  my_bend (uint8_t chan,  int16_t bend)
{
	return ((bend < MIDI14_MIN) || (bend > MIDI14_MAX)) ? false : my_bend_(chan, (uint16_t)(bend - MIDI14_MIN)) ;  // out of range - bad prm
}

//----------------------------------------------------------------------------- ----------------------------------------
#ifdef __cplusplus
	}
#endif
