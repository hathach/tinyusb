#ifdef __cplusplus
	extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "midih.h"
#include "midiu.h"

//+============================================================================ ========================================
// MIDI Host [SYNTH-SYS] - Initialise Synth
//     ** There should be NO need to edit this function **
//
bool  my_synthInit (uint8_t dev_addr,  uint8_t chan)
{
	return
		!!(mySynth.port = midih_dev2port(dev_addr, chan)) &

		my_allNotesOff()         &  // All Notes off
		my_rstAllCtrls()         &  // Reset all controllers  //! should this reset the RPNs and NPRNs?
//!		my_rstAllRPNs()          &  // Reset all RPNs
//!		my_rstAllNRPNs()         &  // Reset all NRPNs

		my_localMode(true)       &  // Accept local message
		my_omniMode(true)        &  // Listen on all channels
		my_omniChan(0)           &  // (My) default non-omni channel is (0 +1) ==1
		my_polyMode(true)        &  // Polyphonic

		// RPNs
		my_bendRange(chan, 2, 0) &  // Bend range 2 semitones and 0 cents
		my_tune(chan, 0, 0)      &  // Bend range 2 semitones and 0 cents

		// NRPNs
	true;

}

//+============================================================================ ========================================
// MIDI Host [SYNTH-SYS] - Set Local Mode {true, false}
//     ** There should be NO need to edit this function **
//
bool  my_localMode (bool state)
{
	mySynth.local = state;
	midih_portLocal(mySynth.port,  state);  // Tell device driver (router) of the mode change
	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-SYS] - Set Omni mode  {true, false}
//     ** There should be NO need to edit this function **
//
// OmniMode = true  : Listen on ALL MIDI Channels
// OmniMode = false : Listen only on MIDI Channels 'omniChan'
//
bool  my_omniMode  (bool state)
{
	mySynth.omni = state;
	return my_allNotesOff();
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-SYS] - Set Omni mode Channel
//     ** There should be NO need to edit this function **
//
bool  my_omniChan (uint8_t ochn)
{
	if (ochn > 15)  return false ;  // invalid channel
	mySynth.ochn = ochn;
	return my_allNotesOff() ;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-SYS] - Set Polyphonic mode  {true, false}
//     ** There should be NO need to edit this function **
//
bool  my_polyMode  (bool state)
{
	mySynth.poly = state;
	return my_allNotesOff();
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-SYS] - Handle Channel Mode-Controllers
//     ** There should be NO need to edit this function **
//
// https://www.voidaudio.net/controller.html
// ,--------------------------------------------------------------------------------------------------.
// | Num | Name                              | Range  | GM  | GM2 | AWE32/ |    Roland/GS    | Yam/XG |
// |     |                                   |        |     |     | Live!  | SC55  | SC88Pro | SW1000 |
// +-----+-----------------------------------+--------+-----+-----+--------+-------+---------+--------|
// | 120 | All Sounds Off                    | ANY    |  -  |  *  |   *    |   *   |    *    |    *   |
// | 121 | Reset All Controllers             | ANY    |  *  |  *  |   *    |   *   |    *    |    *   |
// | 122 | Local Control On/Off              | ON_OFF |  -  |  -  |   -    |   -   |    -    |    -   |
// | 123 | All Notes Off                     | ANY    |  *  |  *  |   *    |   *   |    *    |    *   |
// |     |                                   |        |     |     |        |       |         |        |
// | 124 | Omni Mode Off    + All Notes Off  | ANY    |  -  |  -  |   -    |   *   |    *    |    *   |
// | 125 | Omni Mode On     + All Notes Off  | ANY    |  -  |  -  |   -    |   *   |    *    |    *   |
// | 126 | Poly Mode On/Off + All Sounds Off | ANY    |  -  |  -  |   -    |   *   |    *    |    *   |
// | 127 | Poly Mode On     + All Sounds Off | ANY    |  -  |  -  |   -    |   *   |    *    |    *   |
// `--------------------------------------------------------------------------------------------------'
//   Key: ANY    : 0..127 - Any value to trigger the controller
//        ON_OFF : 0, 127 - 0=off, 127=on
//
// https://www.midi.org/specifications-old/item/table-1-summary-of-midi-message
//   The code below is written to ^this^ document
//
// 120      http://midi.teragonaudio.com/tech/midispec/sndoff.htm
// 121      http://midi.teragonaudio.com/tech/midispec/ctloff.htm
// 122      http://midi.teragonaudio.com/tech/midispec/local.htm
// 123      http://midi.teragonaudio.com/tech/midispec/ntnoff.htm
// 124, 125 http://midi.teragonaudio.com/tech/midispec/modes.htm
// 126      http://midi.teragonaudio.com/tech/midispec/mono.htm
// 127      http://midi.teragonaudio.com/tech/midispec/poly.htm
//
bool  my_chnMode (uint8_t mode,  uint8_t val)
{
	switch (mode) {
		case MIDIH_CTRL_ALL_SOUND_OFF :  // 120
			return (val == 0) ? my_allSoundOff() : false ;  // invalid value

		case MIDIH_CTRL_ALL_CTRL_OFF  :  // 121 [GM][GM2]
			return (val == 0) ? my_rstAllCtrls() : false ;  // unexpected value

		case MIDIH_CTRL_LOCAL         :  // 122
			return  ((val == 0) || (val == 127)) ? my_localMode(!!val) : false ;  // invalid value

		case MIDIH_CTRL_ALL_NOTES_OFF :  // 123 [GM][GM2]
			return (val == 0) ? my_allNotesOff() : false ;  // invalid value

		case MIDIH_CTRL_OMNI_OFF      :  // 124
		case MIDIH_CTRL_OMNI_ON       :  // 125
			return (val == 0) ? my_omniMode(mode == MIDIH_CTRL_OMNI_ON) : false ;  // invalid value

		case MIDIH_CTRL_MONO          :  // 126
			if (mySynth.omni = false)  // single-channel mode
				return my_omniChan(val);
			else
				return (val == 0) ? my_polyMode(false) : false ;  // invalid value

		case MIDIH_CTRL_POLY          :  // 127
			return (val == 0) ? my_polyMode(true) : false ;  // invalid value
	}
	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-SYS] - Turn off all sound
//
bool  my_allSoundOff (void)
{
	//** Turn off all synth sound
	//   http://midi.teragonaudio.com/tech/midispec/sndoff.htm

	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-RPN] - Reset all RPNs
//
bool  my_rstAllRPNs (void)
{
//! TODO
	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-RPN] - Reset all NRPNs
//
bool  my_rstAllNRPNs (void)
{
//! TODO
	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-RPN] - Set RPN controller
//     ** There should be NO need to edit this function **
//
bool  my_rpnSet (uint8_t chan,  midi14_t rpn)
{
	return
		my_ctrlChg(chan, MIDIH_CTRL_RPN_LSB, rpn.lsb) &
		my_ctrlChg(chan, MIDIH_CTRL_RPN_MSB, rpn.msb)  ;
}

//+============================================================================
// MIDI Host [SYNTH-RPN] - Deselect RPN controller
//     ** There should be NO need to edit this function **
//
bool  my_rpnNull (uint8_t chan)
{
	return my_rpnSet(chan, RPN_NULL);  // [127,127] = RPN: Null
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-RPN] - Set NRPN controller
//     ** There should be NO need to edit this function **
//
bool  my_nrpnSet (uint8_t chan,  midi14_t nrpn)
{
	return
		my_ctrlChg(chan, MIDIH_CTRL_NRPN_LSB, nrpn.lsb) &
		my_ctrlChg(chan, MIDIH_CTRL_NRPN_MSB, nrpn.msb)  ;
}

//+============================================================================
// MIDI Host [SYNTH-RPN] - Deselect NRPN controller
//     ** There should be NO need to edit this function **
//
bool  my_nrpnNull (uint8_t chan)
{
	return my_nrpnSet(chan, RPN_NULL);  // [127,127] = RPN: Null
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-RPN] - Set 14bit (N)PRN data value
//     ** There should be NO need to edit this function **
//
bool  my_rpnData14 (uint8_t chan,  midi14_t val)
{
	return
		my_rpnDataMSB(chan, val.msb) &
		my_rpnDataLSB(chan, val.lsb)  ;
}

//+============================================================================
// MIDI Host [SYNTH-RPN] - Set 7bit LSB (N)PRN data value
//     ** There should be NO need to edit this function **
//
bool  my_rpnDataLSB (uint8_t chan,  uint8_t val)
{
	return my_rpnChg(chan, MIDIH_CTRL_DATA_LSB, val);
}

//+============================================================================
// MIDI Host [SYNTH-RPN] - Set 7bit MSB (N)PRN data value
//     ** There should be NO need to edit this function **
//
bool  my_rpnDataMSB (uint8_t chan,  uint8_t val)
{
	return my_rpnChg(chan, MIDIH_CTRL_DATA_MSB, val);
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-RPN] - Increment (N)PRN data value
//     ** There should be NO need to edit this function **
//
bool  my_rpnInc (uint8_t chan)
{
	return my_rpnChg(chan, MIDIH_CTRL_DATA_INC, 1);
}

//+============================================================================
// MIDI Host [SYNTH-RPN] - Decrement (N)PRN data value
//     ** There should be NO need to edit this function **
//
bool  my_rpnDec (uint8_t chan)
{
	return my_rpnChg(chan, MIDIH_CTRL_DATA_DEC, 1);
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-RPN] - Modify the active (N)RPN
//
// This will apply a DATA value or INC/DEC command to
//        the active RPN (if there is one)
// and/or the active NRPN (if there is one)
//
bool  my_rpnChg (uint8_t chan,  uint8_t ctrl,  uint8_t val)
{
	if ((mySynth.chan[chan].ctrl[MIDIH_CTRL_RPN_LSB] != 127) || 
	    (mySynth.chan[chan].ctrl[MIDIH_CTRL_RPN_MSB] != 127)) {

		//** Modify the active RPN

	}

	if ((mySynth.chan[chan].ctrl[MIDIH_CTRL_NRPN_LSB] != 127) || 
	    (mySynth.chan[chan].ctrl[MIDIH_CTRL_NRPN_MSB] != 127)) {

		//** Modify the active NRPN

	}

	return true;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-META] - Set Pitch Bend Range
//     ** There should be NO need to edit this function **
//
// Set how many semitones:cents will be covered in ONE DIRECTION by the bend controller
//
// If you wish to only modify the semitone value, specify cents     as -1
// If you wish to only modify the cents    value, specify semitones as -1
//
// The semitone limit is specified to be 24 semitones
//    (2 octaves each way, ie. a total range of 48 semitones/4 octaves)
//    But will realistically be limited by the synth (like everything else)
//
// I *assume* >99cents is illogical - but I can't find it written anywhere
//    Theoretically, the protocal can handle 0..127
//
// The default value is specified as: 2 semitones and 0 cents
//
// Setting a distance of 0:0 (semitones:cents) effectively disables pitch bend
//
bool  my_bendRange (uint8_t chan,  int8_t semi,  int8_t cents)
{
#if (BEND_MAX_SEMI < 127)
	if (semi  > BEND_MAX_SEMI )  return false ;  // out of range - bad prm
#endif

#if (BEND_MAX_CENTS < 127)
	if (cents > BEND_MAX_CENTS)  return false ;  // out of range - bad prm
#endif

	return
		my_rpnSet(chan, RPN_BEND) &   // [0,0] = RPN: Pitch Bend range
		((semi  >= 0) && my_rpnDataMSB(chan, semi))  &
		((cents >= 0) && my_rpnDataLSB(chan, cents))  ;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-META] - Set Tuning Coarse+Fine with one call
//     ** There should be NO need to edit this function **
//
// See semitone/cent notes in my_bendRange()
//
// Not all synths allow you to set Fine tuning
// ...Even fewer allow you to set Fine at the same time as Coarse
//
bool  my_tune (uint8_t chan,  uint8_t semi,  uint8_t cents)
{
#if (TUNE_MAX_SEMI < 127)
	if (semi  > TUNE_MAX_SEMI)  return false ;  // out of range - bad prm
#endif

#if (TUNE_MAX_CENTS < 127)
	if (cents > TUNE_MAX_CENTS)  return false ;  // out of range - bad prm
#endif

	return
		my_rpnSet(chan, RPN_TUNE_COARSE)  &
		my_rpnDataMSB(chan, semi)  &
		my_rpnDataLSB(chan, cents)  ;  // not recognised by all synths (is anything?)
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-META] - Set Tuning Coarse
//     ** There should be NO need to edit this function **
//
// See semitone/cent notes in my_bendRange()
//
bool  my_tuneCoarse (uint8_t chan,  uint8_t semi)
{
#if (TUNE_MAX_SEMI < 127)
	if (semi  > TUNE_MAX_SEMI)  return false ;  // out of range - bad prm
#endif

	return
		my_rpnSet(chan, RPN_TUNE_COARSE)  &
		my_rpnDataMSB(chan, semi) ;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-META] - Set Tuning Fine
//     ** There should be NO need to edit this function **
//
// See semitone/cent notes in my_bendRange()
//
// Not all synths allow you to set Fine tuning
//
bool  my_tuneFine (uint8_t chan,  uint8_t cents)
{
#if (TUNE_MAX_CENTS < 127)
	if (cents > TUNE_MAX_CENTS)  return false ;  // out of range - bad prm
#endif

	return
		my_rpnSet(chan, RPN_TUNE_FINE)  &
		my_rpnDataMSB(chan, cents) ;
}

//+============================================================================ ========================================
// MIDI Host [SYNTH-META] - Select Program/Patch Bank
//     ** There should be NO need to edit this function **
//
bool  my_bankChg (uint8_t chan,  midi14_t val)
{
	return
		my_ctrlChg(chan, MIDIH_CTRL_BANK_MSB, val.msb) &
		my_ctrlChg(chan, MIDIH_CTRL_BANK_LSB, val.lsb)  ;
}

//----------------------------------------------------------------------------- ----------------------------------------
#ifdef __cplusplus
	}
#endif
