| ¦                                                                                      ¦ |
|o¦                                                                                      ¦o|
| ¦__ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __¦ |
|o¦                  ___ _ _    _ _                         _                            ¦o|
| ¦                 | _ |_) |__| (_)___  __ _ _ _ __ _ _ __| |_ _  _                     ¦ |
|o¦                 | _ \ | '_ \ | / _ \/ _` | '_/ _` | '_ \ ' \ || |                    ¦o|
| ¦                 |___/_|_.__/_|_\___/\__, |_| \__,_| .__/_||_\_, |                    ¦ |
|o¦               ======================|___/=========|_|=======|__/===                  ¦o|
| ¦                                                                                      ¦ |
|o¦ Bad USB devices                                                                      ¦o|
| ¦     https://docs.huihoo.com/doxygen/linux/kernel/3.7/sound_2usb_2midi_8c_source.html ¦ |
|o¦ USB Spec                                                                             ¦o|
| ¦     https://www.usb.org/sites/default/files/midi10.pdf                               ¦ |
|o¦     https://www.usb.org/sites/default/files/USB%20MIDI%20v2_0.pdf                    ¦o|
| ¦ MIDI Messages                                                                        ¦ |
|o¦     https://www.midi.org/specifications-old/item/table-1-summary-of-midi-message     ¦o|
| ¦     https://stackoverflow.com/questions/29481090/explanation-of-midi-messages        ¦ |
|o¦     http://midi.teragonaudio.com/tech/midispec.htm                                   ¦o|
| ¦     https://studiocode.dev/resources/midi-pitch-bend/                                ¦ |
|o¦     https://www.voidaudio.net/controller.html   (Audio card controllers)             ¦o|
| ¦     http://www.studio4all.de/htmle/main91.html  (XG Messages, incl NRPNs)            ¦ |
|o¦ Misc.                                                                                ¦o|
| ¦     https://www.flowkey.com/en/piano-guide/piano-pedals                              ¦ |
|o¦ ASCII Art                                                                            ¦o|
| ¦     https://patorjk.com/software/taag/#p=display&f=Small&t=ASCII%20ART               ¦ |
|o¦     Big shout out to JGS/Spunk ...the best ASCII artist there ever was!              ¦o|
| ¦__ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __¦ |
|o¦                   ___ _         _     ___  _                                         ¦o|
| ¦                  | _ ) |___  __| |__ |   \(_)__ _ __ _ _ _ __ _ _ __                 ¦ |
|o¦                  | _ \ / _ \/ _| / / | |) | / _` / _` | '_/ _` | '  \                ¦o|
| ¦                  |___/_\___/\__|_\_\ |___/|_\__,_\__, |_| \__,_|_|_|_|               ¦ |
|o¦                ==================================|___/==================             ¦o|
| ¦                                                                                      ¦ |
|o¦          +-------+=============+     +-------+=============+                         ¦o|
| ¦          | ,---. | 01:Piano    |     |  ,-.  | 10:Drums    |                         ¦ |
|o¦          | | # | |-------------|     | (:.:) |-------------|                         ¦o|
| ¦          | +---+ | (@)  (O)(O) |     |  `-'  | (@)  (O)(O) |                         ¦ |
|o¦          |  I/O  | vol   L  R  |     |  IN   | vol   L  R  |                         ¦o|
| ¦          +--^-v--+=============+     +---^---+=============+        /|  \            ¦ |
|o¦             | |                          |                       __/ | \ \           ¦o|
| ¦             | |                          |                 .--->[__  |) ) )          ¦ |
|o¦         USB | |                          |                 |       \ | / /           ¦o|
| ¦         1.0 | |             31250/8N1    |                 |        \|  /            ¦ |
|o¦             | |       ,------------------'                 |                         ¦o|
| ¦             | |       |                                    |                         ¦ |
|o¦      #######|#|#######|####################################|###############          ¦o|
| ¦      #      | |       |                                    |              #          ¦ |
|o¦      #      | |       |                            +=======|===========+  #          ¦o|
| ¦      #      | |       |                            [  ,----^--------.  ]  #          ¦ |
|o¦      #  +===|=|=======|=====+                   .--[->|    SYNTH    |  ]  #          ¦o|
| ¦      #  [   | | I/O   |     ]     +==========+  |  [  `-------------'  ]  #          ¦ |
|o¦      #  [ ,-^-v--. ,--^---. ]<---<[ MIDI     ]>-'  [   LOCAL DEVICES   ]  #          ¦o|
| ¦      #  [ | TUSB | | GPIO | ]>--->[   ENGINE ]<-.  [  ,-------------.  ]  #          ¦ |
|o¦      #  [ `-^-v--' `--^---' ]     +==========+  |  [  | H H H | H H |  ]  #          ¦o|
| ¦      #  +===|=|=======|=====+                   `--[-<| | | | | | | |  ]  #          ¦ |
|o¦      #      | |       |                            [  `-^-^-^-^-^-^-'  ]  #          ¦o|
| ¦      #      | |       |                            +===^===============+  #          ¦ |
|o¦      #      | |       |                                |                  #          ¦o|
| ¦      #######|#|#######|################################|###################          ¦ |
|o¦             | |       |                                |                             ¦o|
| ¦             | |       `------------------.             |        .------------.       ¦ |
|o¦         USB | |             31250/8N1    |             |        |#.--------. |       ¦o|
| ¦         1.0 | |                          |             `-------<| | User   | |       ¦ |
|o¦             | |                          |                      | |  Iface | |       ¦o|
| ¦             | |                          |                      | `--------' |       ¦ |
|o¦          +--^-v--+=============+     +---^---+=============+    |   _____ _  |       ¦o|
| ¦          | ,---. | H H H | H H |     |  ,-.  | H H H | H H |    |  | #   | | |       ¦ |
|o¦          | | # | | H H H | H H |     | (:.:) | H H H | H H |     \_|_#___|_|_|       ¦o|
| ¦          | +---+ | | | | | | | |     |  `-'  | | | | | | | |                         ¦ |
|o¦          |  I/O  | | | | | | | |     |  OUT  | | | | | | | |                         ¦o|
| ¦          +-------`-^-^-^-^-^-^-^     +-------`-^-^-^-^-^-^-'                         ¦ |
|o¦__ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __¦o|
| ¦                           _   ___  ___ ___ ___     _   ___ _____                     ¦ |
|o¦                          /_\ / __|/ __|_ _|_ _|   /_\ | _ \_   _|                    ¦o|
| ¦                         / _ \\__ \ (__ | | | |   / _ \|   / | |                      ¦ |
|o¦                        /_/ \_\___/\___|___|___| /_/ \_\_|_\ |_|                      ¦o|
| ¦                      ==============================================                  ¦ |
|o¦                                                                                      ¦o|
| ¦              ,-.                              ,-.                                    ¦ |
|o¦             ( 0 )  Mono Jack                 (:.:)   MIDI (5-pin DIN)                ¦o|
| ¦              `-'                              `-'                                    ¦ |
|o¦              ,-.                             ,---.                                   ¦o|
| ¦             ( @ )  Stereo Jack (TRS)         | # |   USB-B (f)                       ¦ |
|o¦              `-'                             +---+                                   ¦o|
| ¦                                                                                      ¦ |
|o¦             (@)    Knob (Potentiometer)      (O)(O)  Wht/Red phono pair (RCA)        ¦o|
| ¦                                                                                      ¦ |
|o¦             .------------.                                                           ¦o|
| ¦             |#.--------. |  Floppy Disk          /|  \                               ¦ |
|o¦             | | User   | |                    __/ | \ \                              ¦o|
| ¦             | |  Iface | |                   [__  |) ) )  Speaker                    ¦ |
|o¦             | `--------' |                      \ | / /                              ¦o|
| ¦             |   _____ _  |                       \|  /                               ¦ |
|o¦             |  | #   | | |                                                           ¦o|
| ¦              \_|_#___|_|_|                                                           ¦ |
|o¦                                                                                      ¦o|
| ¦             +=============+-------+          +=============+-------+                 ¦ |
|o¦             | H H H | H H | ,---. |          | H H H | H H |  ,-.  |                 ¦o|
| ¦             | H H H | H H | | # | |          | H H H | H H | (:.:) |                 ¦ |
|o¦             | | | | | | | | +---+ |          | | | | | | | |  `-'  |                 ¦o|
| ¦             | | | | | | | |  I/O  |          | | | | | | | |  OUT  |                 ¦ |
|o¦             `-^-^-^-^-^-^-'-------+          `-^-^-^-^-^-^-'-------+                 ¦o|
| ¦              Device: MIDI Keyboard            Device: MIDI Keyboard                  ¦ |
|o¦                     (USB)                       (5-pin DIN / GPIO)                   ¦o|
| ¦                                                                                      ¦ |
|o¦             +=============+-------+          +=============+-------+                 ¦o|
| ¦             | 01:Piano    | ,---. |          | 01:Piano    |  ,-.  |                 ¦ |
|o¦             |-------------| | # | |          |-------------| (:.:) |                 ¦o|
| ¦             | (@)  (O)(O) | +---+ |          | (@)  (O)(O) |  `-'  |                 ¦ |
|o¦             | vol   L  R  |  I/O  |          | vol   L  R  |  IN   |                 ¦o|
| ¦             +=============+-------+          +=============+-------+                 ¦ |
|o¦                Device: USB Synth                Device: MIDI Synth                   ¦o|
| ¦                                              (5-pin DIN / GPIO)                      ¦ |
|o¦__ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __¦o|
| ¦                      ___          _   ___          _                                 ¦ |
|o¦                     | _ ) __ _ __| | |   \ _____ _(_)__ ___ ___                      ¦o|
| ¦                     | _ \/ _` / _` | | |) / -_) V / / _/ -_|_-<                      ¦ |
|o¦                     |___/\__,_\__,_| |___/\___|\_/|_\__\___/__/                      ¦o|
| ¦                   ===============================================                    ¦ |
|o¦                                                                                      ¦o|
| ¦ https://docs.huihoo.com/doxygen/linux/kernel/3.7/sound_2usb_2midi_8c_source.html     ¦ |
|o¦                                                                                      ¦o|
| ¦ 1282 /*                                                                              ¦ |
|o¦ 1283  * Creates an output endpoint, and initializes output ports.                    ¦o|
| ¦ 1284  */                                                                             ¦ |
|o¦ 1285 static int snd_usbmidi_out_endpoint_create(struct snd_usb_midi* umidi,          ¦o|
| ¦ 1286                        struct snd_usb_midi_endpoint_info* ep_info,              ¦ |
|o¦ 1287                        struct snd_usb_midi_endpoint* rep)                       ¦o|
| ¦ 1288 {                                                                               ¦ |
|o¦ 1312     switch (umidi->usb_id) {                                                    ¦o|
| ¦ 1313     default:                                                                    ¦ |
|o¦ 1314         ep->max_transfer = usb_maxpacket(umidi->dev, pipe, 1);                  ¦o|
| ¦ 1315         break;                                                                  ¦ |
|o¦ 1316         /*                                                                      ¦o|
| ¦ 1317          * Various chips declare a packet size larger than 4 bytes, but         ¦ |
|o¦ 1318          * do not actually work with larger packets:                            ¦o|
| ¦ 1319          */                                                                     ¦ |
|o¦ 1320     case USB_ID(0x0a92, 0x1020): /* ESI M4U */                                  ¦o|
| ¦ 1321     case USB_ID(0x1430, 0x474b): /* RedOctane GH MIDI INTERFACE */              ¦ |
|o¦ 1322     case USB_ID(0x15ca, 0x0101): /* Textech USB Midi Cable */                   ¦o|
| ¦ 1323     case USB_ID(0x15ca, 0x1806): /* Textech USB Midi Cable */                   ¦ |
|o¦ 1324     case USB_ID(0x1a86, 0x752d): /* QinHeng CH345 "USB2.0-MIDI" */              ¦o|
| ¦ 1325     case USB_ID(0xfc08, 0x0101): /* Unknown vendor Cable */                     ¦ |
|o¦ 1326         ep->max_transfer = 4;                                                   ¦o|
| ¦ 1327         break;                                                                  ¦ |
|o¦ 1328         /*                                                                      ¦o|
| ¦ 1329          * Some devices only work with 9 bytes packet size:                     ¦ |
|o¦ 1330          */                                                                     ¦o|
| ¦ 1331     case USB_ID(0x0644, 0x800E): /* Tascam US-122L */                           ¦ |
|o¦ 1332     case USB_ID(0x0644, 0x800F): /* Tascam US-144 */                            ¦o|
| ¦ 1333         ep->max_transfer = 9;                                                   ¦ |
|o¦ 1334         break;                                                                  ¦o|
| ¦ 1335     }                                                                           ¦ |
|o¦__ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __¦o|
| ¦                                 _  _     _                                           ¦ |
|o¦                                | \| |___| |_ ___ ___                                 ¦o|
| ¦                                | .` / _ \  _/ -_|_-<                                 ¦ |
|o¦                                |_|\_\___/\__\___/__/                                 ¦o|
| ¦                              =========================                               ¦ |
|o¦                                                                                      ¦o|
| ¦ USB MIDI 2.0:                                                                        ¦ |
|o¦     Not gonna support this:                                                          ¦o|
| ¦     https://www.usb.org/sites/default/files/USB%20MIDI%20v2_0.pdf                    ¦ |
|o¦         mostly cos I've no kit to test against :/                                    ¦o|
| ¦                                                                                      ¦ |
|o¦ Ref:                                                                                 ¦o|
| ¦ BR's primer:                                                                         ¦ |
|o¦      Each "Audio Function" has precisely ONE AudioControl (AC) interface             ¦o|
| ¦      and MAY have MANY AudioStreaming (AS), and MANY MIDIStreaming (MS) interfaces.  ¦ |
|o¦      A MIDIStreaming interface is used to transport                                  ¦o|
| ¦        USB-MIDI data into and out of the "audio function".                           ¦ |
|o¦                                                                                      ¦o|
| ¦ Ref:                                                                                 ¦ |
|o¦     https://www.usb.org/sites/default/files/midi10.pdf                               ¦o|
| ¦         3.1, Fig 4                                                                   ¦ |
|o¦                 4 endpoints : MIDI In, MIDI Out, Xfr In, Xfr Out                     ¦o|
| ¦                                                                                      ¦ |
|o¦         3.2.1                                                                        ¦o|
| ¦                 Each MIDI Endpoint has (up to) 16 "virtual cables"                   ¦ |
|o¦                   identified by their "cable number" ["cn"] {0..15}                  ¦o|
| ¦                 Each cable may be IN xor OUT                                         ¦ |
|o¦                 MIDI Data is sent (over USB) in 32-bit "messages"                    ¦o|
| ¦                   The first 4 bits are used to specify the target 'cable'            ¦ |
|o¦                                                                                      ¦o|
| ¦         3.2.2 Transfer Endpoints                                                     ¦ |
|o¦                 no test kit                                                          ¦o|
| ¦                                                                                      ¦ |
|o¦__ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __¦o|
| ¦                                                                                      ¦ |

Descriptors
===========

https://www.beyondlogic.org/usbnutshell/usb5.shtml

-< one-to-many

Device.bNumConfigurations
+--< Configuration.bNumInterfaces
      +--< Interface.bNumEndpoints
           +--< Endpoint















