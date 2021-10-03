#ifndef  MIDIH_USER_H_
#define  MIDIH_USER_H_

#ifdef __cplusplus
	extern "C" {
#endif

//----------------------------------------------------------------------------- ----------------------------------------
#include "midih.h"
#include "midiu_vid.h"

//----------------------------------------------------------------------------- ----------------------------------------
// I have taken my controller list from here:
//    http://midi.teragonaudio.com/tech/midispec.htm
//    https://en.wikipedia.org/wiki/General_MIDI
//    https://en.wikipedia.org/wiki/General_MIDI_Level_2
// Note that, generally, LSB = (MSB +  32 ) ...  MSB = (LSB -   32 )
//    or, if you prefer: LSB = (MSB | 0x20) ...  MSB = (LSB & ~0x20)
//
// A [trigger] controller triggers/happens when ANY value is written to it
//
// The six Pedal controllers {64..69} are:
//     0 <= n <=  63 : Pedal off
//    64 <= n <= 127 : Pedal on
//  ie. bool pedalActive = !!(n & 0x40)
//
// ...I have grouped them logically, not numerically
//

// -- PatchBank selection
#define  MIDIH_CTRL_BANK_MSB            0                       // [GS][GM2] Bank Select MSB
#define  MIDIH_CTRL_BANK_LSB           32                       // [GS][GM2] Bank Select LSB

// -- Basic mixing - GM/GM2/GS/XG only specify Coarse/MSB tuning
#define  MIDIH_CTRL_BAL_MSB             8                       // Stereo Balance MSB
#define  MIDIH_CTRL_BAL_LSB            40                       // Stereo Balance LSB
#define  MIDIH_CTRL_BAL               MIDIH_CTRL_BAL_MSB        // Stereo Balance

#define  MIDIH_CTRL_PAN_MSB            10                       // Mono Position MSB
#define  MIDIH_CTRL_PAN_LSB            42                       // Mono Position LSB
#define  MIDIH_CTRL_PAN               MIDIH_CTRL_PAN_MSB        // [GM][GM2] Mono Position

#define  MIDIH_CTRL_VOLUME_MSB          7                       // Volume MSB
#define  MIDIH_CTRL_VOLUME_LSB         39                       // Volume LSB
#define  MIDIH_CTRL_VOL               MIDIH_CTRL_VOLUME_MSB     // [GM][GM2] Volume

#define  MIDIH_CTRL_EXPR_MSB           11                       // Expression (fade) MSB
#define  MIDIH_CTRL_EXPR_LSB           43                       // Expression (fade) LSB
#define  MIDIH_CTRL_EXPR              MIDIH_CTRL_EXPR_MSB       // [GM] Expression (fade)

// -- Extended effects
#define  MIDIH_CTRL_USRFX1_MSB         12                       // Effects Controller #1 MSB
#define  MIDIH_CTRL_USRFX1_LSB         44                       // Effects Controller #1 LSB

#define  MIDIH_CTRL_USRFX2_MSB         13                       // Effects Controller #2 MSB
#define  MIDIH_CTRL_USRFX2_LSB         45                       // Effects Controller #2 LSB

// - Control sliders
#define  MIDIH_CTRL_SLIDER1            16                       // Slider #1
#define  MIDIH_CTRL_SLIDER2            17                       // Slider #2
#define  MIDIH_CTRL_SLIDER3            18                       // Slider #3
#define  MIDIH_CTRL_SLIDER4            19                       // Slider #4

//- Control buttons
#define  MIDIH_CTRL_BUTTON1            80                       // Button #1
#define  MIDIH_CTRL_BUTTON2            81                       // Button #2
#define  MIDIH_CTRL_BUTTON3            82                       // Button #3
#define  MIDIH_CTRL_BUTTON4            83                       // Button #4

// -- Piano pedals : https://www.flowkey.com/en/piano-guide/piano-pedals
// -- 0 <= off < 64 <= on <= 127
#define  MIDIH_CTRL_PDL_DAMPER         64                       // [GM][GM2] Damper Pedal (aka. "sustain" or "hold-1")
#define  MIDIH_CTRL_PDL_PORT           65                       // [GS][GM2] Portamento Pedal
#define  MIDIH_CTRL_PDL_SUSTENUTO      66                       // [GS][GM2] Sustenuto Pedal
#define  MIDIH_CTRL_PDL_SOFT           67                       // [GS][GM2] Soft Pedal
#define  MIDIH_CTRL_PDL_LEGATO         68                       // Legato Pedal
#define  MIDIH_CTRL_PDL_HOLD           69                       // Hold Pedal (longer decay) (aka "hold-2")

// - Basic sound controllers - GM/GM2/GS/XG only specify Coarse/MSB tuning
#define  MIDIH_CTRL_MOD_MSB             1                       // Modulation wheel MSB
#define  MIDIH_CTRL_MOD_LSB            33                       // Modulation wheel LSB
#define  MIDIH_CTRL_MOD               MIDIH_CTRL_MOD            // [GM][GM2] Modulation wheel

#define  MIDIH_CTRL_BREATH_MSB          2                       // Breath Control MSB
#define  MIDIH_CTRL_BREATH_LSB         34                       // Breath Control LSB
#define  MIDIH_CTRL_BREATH            MIDIH_CTRL_BREATH_MSB     // [GM2] Breath Control

#define  MIDIH_CTRL_FOOT_MSB            4                       // Foot Control MSB
#define  MIDIH_CTRL_FOOT_LSB           36                       // Foot Control LSB
#define  MIDIH_CTRL_FOOT              MIDIH_CTRL_FOOT_MSB       // [GM2] Foot Control

// - Portamento controls
#define  MIDIH_CTRL_PORT_TIME         MIDIH_CTRL_PORT_TIME_MSB  // [GM2] Portamento Time
#define  MIDIH_CTRL_PORT_TIME_MSB       5                       // Portamento Time
#define  MIDIH_CTRL_PORT_TIME_LSB      37                       // Portamento Time
#define  MIDIH_CTRL_PORT_CTRL          84                       // [GS][XG] Portamento Control [trigger]

// - Basic Effects
#define  MIDIH_CTRL_FX1                91                       // FX-1 Send level
#define  MIDIH_CTRL_REVERB            MIDIH_CTRL_FX1            // FX-1 : Reverb

#define  MIDIH_CTRL_FX2                92                       // [GS] FX-2 Send level
#define  MIDIH_CTRL_TREMOLO           MIDIH_CTRL_FX2            // [GS] FX-2 : Termulo/tremolo
#define  MIDIH_CTRL_TREMULO           MIDIH_CTRL_FX2            // [GS] FX-2 : Termulo/tremolo

#define  MIDIH_CTRL_FX3                93                       // [GS] FX-2 Send level
#define  MIDIH_CTRL_CHORUS            MIDIH_CTRL_FX3            // [GS] FX-3 Send: Chorus

#define  MIDIH_CTRL_FX4                94                       //  FX-4 Send level
#define  MIDIH_CTRL_DELAY             MIDIH_CTRL_FX4            // [GS] FX-4 Send: Delay/Variation
#define  MIDIH_CTRL_DETUNE            MIDIH_CTRL_FX4            // [GM] FX-4 Send: Detune
#define  MIDIH_CTRL_CELESTE           MIDIH_CTRL_FX4            // FX-4 Send: Celeste

#define  MIDIH_CTRL_FX5                95                       //  FX-5 Send level
#define  MIDIH_CTRL_PHASER            MIDIH_CTRL_FX5            //  [GM] FX-5 Send level

// - Sound Controllers
#define  MIDIH_CTRL_SOUND1             70                       // Sound controller #1
#define  MIDIH_CTRL_SOUND2             71                       // Sound controller #2
#define  MIDIH_CTRL_SOUND3             72                       // Sound controller #3
#define  MIDIH_CTRL_SOUND4             73                       // Sound controller #4
#define  MIDIH_CTRL_SOUND5             74                       // Sound controller #5
#define  MIDIH_CTRL_SOUND6             75                       // Sound controller #6
#define  MIDIH_CTRL_SOUND7             76                       // Sound controller #7
#define  MIDIH_CTRL_SOUND8             77                       // Sound controller #8
#define  MIDIH_CTRL_SOUND9             78                       // Sound controller #9
#define  MIDIH_CTRL_SOUND10            79                       // Sound controller #10

// - GM2
#define  MIDIH_CTRL_GM_VARIATION      MIDIH_CTRL_SOUND1         // [GM2] Sound variaion

#define  MIDIH_CTRL_TIMBRE            MIDIH_CTRL_SOUND2         // [GM2] Timbre (or FILTER_CUTOFF)
#define  MIDIH_CTRL_FILTER_RESONANCE  MIDIH_CTRL_SOUND2         // [GM2] Filter Resonance

#define  MIDIH_CTRL_BRIGHTNESS        MIDIH_CTRL_SOUND5         // [GM2] BRIGHTNESS (or FILTER_RESONANCE)
#define  MIDIH_CTRL_FILTER_CUTOFF     MIDIH_CTRL_SOUND5         // [GM2] Filter Cutoff (Timbre)

#define  MIDIH_CTRL_VIBRATO_DEPTH     MIDIH_CTRL_SOUND8         // [GM2] Vibrato Depth
#define  MIDIH_CTRL_VIBRATO_RATE      MIDIH_CTRL_SOUND7         // [GM2] Vibrato Rate
#define  MIDIH_CTRL_VIBRATO_DELAY     MIDIH_CTRL_SOUND9         // [GM2] Vibrato Delay

#define  MIDIH_CTRL_ATTACK            MIDIH_CTRL_SOUND4         // [GM2] Attack Time
#define  MIDIH_CTRL_DECAY             MIDIH_CTRL_SOUND6         // [GM2] Decay Time   - err, where is "Sustain"?
#define  MIDIH_CTRL_RELEASE           MIDIH_CTRL_SOUND3         // [GM2] Release Time

// - (N)RPN controls
#define  MIDIH_CTRL_RPN_LSB           100                       // [GS][GM][GM2] RPN LSB
#define  MIDIH_CTRL_RPN_MSB           101                       // [GS][GM][GM2] RPN MSB

#define  MIDIH_CTRL_NRPN_LSB           98                       // [GS] NRPN LSB [0lll'llll]
#define  MIDIH_CTRL_NRPN_MSB           99                       // [GS] NRPN MSB [0mmm'mmmm]

#define  MIDIH_CTRL_DATA_MSB            6                       // [GM2] Data Entry MSB
#define  MIDIH_CTRL_DATA_LSB           38                       // [GM2] Data Entry LSB

#define  MIDIH_CTRL_DATA_INC           96                       // Data Increment (+1) [trigger]
#define  MIDIH_CTRL_DATA_DEC           97                       // Data Decrement (-1) [trigger]

// - System Controllers
#define  MIDIH_CTRL_ALL_SOUND_OFF      120                      // http://midi.teragonaudio.com/tech/midispec/sndoff.htm
#define  MIDIH_CTRL_ALL_CTRL_OFF       121                      // http://midi.teragonaudio.com/tech/midispec/ctloff.htm
#define  MIDIH_CTRL_LOCAL              122                      // http://midi.teragonaudio.com/tech/midispec/local.htm
#define  MIDIH_CTRL_ALL_NOTES_OFF      123                      // http://midi.teragonaudio.com/tech/midispec/ntnoff.htm
#define  MIDIH_CTRL_OMNI_OFF           124                      // http://midi.teragonaudio.com/tech/midispec/modes.htm
#define  MIDIH_CTRL_OMNI_ON            125                      // http://midi.teragonaudio.com/tech/midispec/modes.htm
#define  MIDIH_CTRL_MONO               126                      // http://midi.teragonaudio.com/tech/midispec/mono.htm
#define  MIDIH_CTRL_POLY               127                      // http://midi.teragonaudio.com/tech/midispec/poly.htm

//----------------------------------------------------------------------------- ----------------------------------------
// ALL MIDI *data* uses 7-bit Bytes (bit-8 is used to flag a command byte)
// As such a "Word" is 14-bits, laid out like this: [--mm mmmm mlll llll]
// So if mmmmmmm ==   1010101           == 0x55   == 85
//   and lllllll ==             0110111 == 0x  37 == 55
//  then word14  ==   1010101   0110111 == 0x2AB7 == 10935
//                 [--10 1010 1011 0111]
//
typedef
	union TU_ATTR_PACKED {
		struct TU_ATTR_PACKED {   // uint14_t
			uint16_t  val    : 14 ; // [13: 0]
			uint16_t         :  2 ; // [15:14]
		};
		struct TU_ATTR_PACKED {  // (N)RPNs
			uint16_t  lsb    : 7 ; // [ 6: 0]
			uint16_t  msb    : 7 ; // [13: 7]
			uint16_t         : 2 ; // [15:14]
		};
		struct TU_ATTR_PACKED {  // RPN:Tuning
			uint16_t  cent   : 7 ; // [ 6: 0]
			uint16_t  semi   : 7 ; // [13: 7]
			uint16_t         : 2 ; // [15:14]
		};
		struct TU_ATTR_PACKED {  // RPN:Tuning/Bank
			uint16_t  fine   : 7 ; // [ 6: 0]
			uint16_t  coarse : 7 ; // [13: 7]
			uint16_t         : 2 ; // [15:14]
		};
	}
midi14_t;

#define  MIDI14_MIN   (-8192)
#define  MIDI14_MAX   ( 8191)
#define  MIDI14_MID   (0x2000)
#define  MIDI14_MASK  (0xC000)

#define  UINT7_MIN    (0)
#define  UINT7_MAX    (127)

//----------------------------------------------------------------------------- ----------------------------------------
// Notes can be turned on or off, and (in either case) with a velocity
// Keys may also have an Aftertouch value (generally pressure applied to the key after it has bottomed out)
//
typedef
	struct {
		struct TU_ATTR_PACKED {
			uint8_t  vel : 7;  // [6:0] Velocity 0..127
			bool     on  : 1;  // [7:7] 1=on, 0=off  ... on(n, 0) -> off(n, 127)
		};
		uint8_t  aft : 7;      // [7:0] Key Aftertouch (aka "Pressure")
	}
myKey_t;

//----------------------------------------------------------------------------- ----------------------------------------
// This is what we know about the Settings and State for a given MIDI channel
//
// [*] C has no syntax to define an array of bitfields, eg. "uint8_t ctrl:7[120]"
//
#define KEY_MAX          (127)  // Maximum number of keys
#define KEY_VEL_MAX      (127)  // Maximum velocity

#define RPN_BEND         ((midi14_t){ .msb =   0,  .lsb =   0 })
#define BEND_MAX_SEMI    (24)   // {0..24..127}
#define BEND_MAX_CENTS   (99)   // {0..99..127}

#define RPN_TUNE_FINE    ((midi14_t){ .msb =   1,  .lsb =   0 })
#define TUNE_MAX_CENTS   (99)   // {0..99..127}

#define RPN_TUNE_COARSE  ((midi14_t){ .msb =   2,  .lsb =   0 })
#define TUNE_MAX_SEMI    (24)   // {0..24..127}

#define RPN_NULL         ((midi14_t){ .msb = 127,  .lsb = 127 })

typedef
	struct {
		midi14_t  bank;             // Program Bank number
		uint8_t   patch : 7;        // Program Patch number
		midi14_t  bend;             // Pitch bend ... .val={0x0000 .. 0x2000 .. 0x3FFF}
		uint8_t   chnAft : 7;       // Channel aftertouch (aka "Pressure")

		myKey_t   note[KEY_MAX];    // Notes
		uint8_t   ctrl[120];        // [*] Channel controller values {0.. 119}

		// --MIDI/GM RPN--
		midi14_t  range;            // Bend range [semi:cent]
		midi14_t  tune;             // Tuning     [semi:cent]
	}
myChan_t;

//----------------------------------------------------------------------------- ----------------------------------------
// Everything we know about the state of the synth
//
// The machine numbers the channels/musicians {0..15}, eg. GM drums on chan 9
//       users number  the channels/musicians {1..16}, eg. GM drums on chan 10
//
typedef
	struct {
		// -- Device information --
		midih_port_t*  port;      // Port pointer

		// -- Global Settings --
		bool           local;     // 1=route, 0=ignore .. local messages  //! TODO
		bool           poly;      // 1=polyphonic, 0=monophonic
		bool           omni;      // Listen on 1=omni(AllChannels), 0=ochn
		uint8_t        ochn : 4;  // Omni Channel number

		// -- Per-Channel Settings --
		myChan_t       chan[16];  // 16 channels {0..15}
	}
mySynth_t;

mySynth_t  mySynth;

//----------------------------------------------------------------------------- ----------------------------------------
// Function protoypes
//
// cat *.c | grep '^{' -B 2 | sed '/^{/d;/^--$/d;/\/\//d'
//

//-[ midih_user_cb.c ]--------------------------------------------------------- 
bool           midih_rcvRsvd_cb   (midih_port_t* from,  midimsg_t* mp) ;            // [USER-CB] External Message - Reserved
bool           midih_rcvSC_cb     (midih_port_t* from,  midimsg_t* mp) ;            // [USER-CB] External Message - System Common
bool           midih_rcvRT_cb     (midih_port_t* from,  midimsg_t* mp) ;            // [USER-CB] External Message - Running Status
bool           midih_rcvSnd_cb    (midih_port_t* from,  midimsg_t* mp) ;            // [USER-CB] External Message - Sound Event
bool           midih_rcvSyx_cb    (midih_port_t* from,  midimsg_t* mp) ;            // [USER-CB] External Message - SysEx (collect)

//-[ midih_user_synth_sys.c ]-------------------------------------------------- 
bool           my_synthInit       (uint8_t dev_addr,  uint8_t chan) ;               // [SYNTH-SYS] Synth Init
bool           my_localMode       (bool state) ;                                    // [SYNTH-SYS] Local mode
bool           my_omniMode        (bool state) ;                                    // [SYNTH-SYS] Omni mode (on/off)
bool           my_omniChan        (uint8_t ochn) ;                                  // [SYNTH-SYS] Omni Channel [omni=off]
bool           my_polyMode        (bool state) ;                                    // [SYNTH-SYS] Polyphonic mode (on/off)
bool           my_chnMode         (uint8_t mode,  uint8_t val) ;                    // [SYNTH-SYS] Channel Mode-Controllers (ctrl>=120)
bool           my_allSoundOff     (void) ;                                          // [SYNTH-SYS] All sound off

bool           my_rstAllRPNs      (void) ;                                          // [SYNTH-RPN] Reset all RPNs
bool           my_rstAllNRPNs     (void) ;                                          // [SYNTH-RPN] Reset all NRPNs

bool           my_rpnSet          (uint8_t chan,  midi14_t rpn) ;                   // [SYNTH-RPN] Set RPN controller
bool           my_rpnNull         (uint8_t chan) ;                                  // [SYNTH-RPN] Deselect RPN controller

bool           my_nrpnSet         (uint8_t chan,  midi14_t nrpn) ;                  // [SYNTH-RPN] Set NRPN controller
bool           my_nrpnNull        (uint8_t chan) ;                                  // [SYNTH-RPN] Deselect NRPN controller

bool           my_rpnData14       (uint8_t chan,  midi14_t val) ;                   // [SYNTH-RPN] Set 14bit (N)PRN data value
bool           my_rpnDataLSB      (uint8_t chan,  uint8_t val) ;                    // [SYNTH-RPN] Set 7bit LSB (N)PRN data value
bool           my_rpnDataMSB      (uint8_t chan,  uint8_t val) ;                    // [SYNTH-RPN] Set 7bit MSB (N)PRN data value
bool           my_rpnInc          (uint8_t chan) ;                                  // [SYNTH-RPN] Increment (N)PRN data value
bool           my_rpnDec          (uint8_t chan) ;                                  // [SYNTH-RPN] Decrement (N)PRN data value
bool           my_rpnChg          (uint8_t chan,  uint8_t ctrl,  uint8_t val) ;     // [SYNTH-RPN] Modify active (N)RPNs


bool           my_bendRange       (uint8_t chan,  int8_t semi,  int8_t cents) ;     // [SYNTH-META] Set PitchBend range
bool           my_tune            (uint8_t chan,  uint8_t semi,  uint8_t cents) ;   // [SYNTH-META] Set Tuning
bool           my_tuneCoarse      (uint8_t chan,  uint8_t semi) ;                   // [SYNTH-META] Set Tuning - Coarse
bool           my_tuneFine        (uint8_t chan,  uint8_t cents) ;                  // [SYNTH-META] Set Tuning - Fine

bool           my_bankChg         (uint8_t chan,  midi14_t val) ;                   // [SYNTH-META] Bank Change

//-[ midih_user_synth_event.c ]------------------------------------------------ 
bool           my_chnNotesOff     (uint8_t chan) ;                                  // [SYNTH-EVENT] All notes off on this channel
bool           my_allNotesOff     (void) ;                                          // [SYNTH-EVENT] All notes off on all channels
bool           my_rstAllCtrls     (void) ;                                          // [SYNTH-EVENT] Reset all controllers

bool           my_syxHandler      (midih_port_t* from,  uint8_t* buf,  int cnt) ;   // [SYNTH-EVENT] Process SysEx message
bool           my_syxNonRealitime (midih_port_t* from,  void* buf,  int cnt) ;      // [SYNTH-SYX-UNI] SysEx Non-Realtime Message
bool           my_syxRealitime    (midih_port_t* from,  void* buf,  int cnt) ;      // [SYNTH-SYX-UNI] SysEx Non-Realtime Message
bool           my_syxGmEnable     (void* buf) ;                // [SYNTH-SYX-UNI] GM Enable (on/off)
bool           my_syxMasterVol    (void* buf) ;                // [SYNTH-SYX-UNI] Master Volume
bool           my_syxIdReq        (midih_port_t* from,  void* buf) ;                // [SYNTH-SYX-UNI] Identity Request
bool           my_syxSdsDumpReq   (midih_port_t* from,  void* buf) ;                // [SYNTH-SYX-UNI] SDS Dump Request

bool           my_noteOff         (uint8_t chan,  uint8_t key,  uint8_t vel) ;      // [SYNTH-EVENT] Note On
bool           my_noteOn          (uint8_t chan,  uint8_t key,  uint8_t vel) ;      // [SYNTH-EVENT] Note Off
bool           my_keyAft          (uint8_t chan,  uint8_t key,  uint8_t pressure) ; // [SYNTH-EVENT] Key Aftertouch
bool           my_ctrlChg         (uint8_t chan,  uint8_t ctrl,  uint8_t val) ;     // [SYNTH-EVENT] Controller Change (ctrl<120)
bool           my_patchChg        (uint8_t chan,  uint8_t patch) ;                  // [SYNTH-EVENT] Patch Change
bool           my_chnAft          (uint8_t chan,  uint8_t pressure) ;               // [SYNTH-EVENT] Channel Aftertouch

//static bool  my_bend_           (uint8_t chan,  uint16_t bend) ;                  // [SYNTH-EVENT] Bend note
bool           my_bendAbs         (uint8_t chan,  uint16_t bend) ;                  // [SYNTH-EVENT] {0x0000 .. 0x3FFF}
bool           my_bend            (uint8_t chan,  int16_t bend) ;                   // [SYNTH-EVENT] {-8192 .. +8191}
//----------------------------------------------------------------------------- ----------------------------------------
#ifdef __cplusplus
	}
#endif

#endif // MIDIH_USER_H_
