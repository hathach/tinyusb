# MIDI HOST DRIVER
This README file contains the design notes and limitations of the
MIDI host driver.

# MAXIMUM NUMBER OF MIDI DEVICES ATTACHED TO HOST
In this version of the driver, only one MIDI device is supported. This
constraint may change in the future.

# MAXIMUM NUMBER OF ENDPOINTS
Although the USB MIDI 1.0 Class specification allows an arbitrary number
of endpoints, this driver supports at most one USB BULK DATA IN endpoint
and one USB BULK DATA OUT endpoint. Each endpoint can support up to 16
virtual cables. If a device has multiple IN endpoints or multiple OUT
endpoints, it will fail to enumerate.

Most USB MIDI devices contain both an IN endpoint and an OUT endpoint,
but not all do. For example, some USB pedals only support an OUT endpoint.
This driver allows that.

# PUBLIC API
Applications interact with this driver via 8-bit buffers of MIDI messages
formed using the rules for sending bytes on a 5-pin DIN cable per the
original MIDI 1.0 specification.

To send a message to a device, the Host application composes a sequence
of status and data bytes in a byte array and calls the API function.
The arguments of the function are a pointer to the byte array, the number
of bytes in the array, and the target virtual cable number 0-15.

When the host driver receives a message from the device, the host driver
will call a callback function that the host application registers. This
callback function contains a pointer to a message buffer, a message length,
and the virtual cable number of the message buffer. One complete bulk IN
endpoint transfer might contain multiple messages targeted to different
virtual cables.

# SUBCLASS AUDIO CONTROL
A MIDI device does not absolutely need to have an Audio Control Interface,
unless it adheres to the USB Audio Class 2 spec, but many devices
have them even if the devices do not have an audio streaming interface.
Because this driver does not support audio streaming, the descriptor parser
will skip past any audio control interface and audio streaming interface
and open only the MIDI interface.

An audio streaming host driver can use this driver by passing a pointer
to the MIDI interface descriptor that is found after the audio streaming
interface to the midih_open() function. That is, an audio streaming host
driver would parse the audio control interface descriptor and then the
audio streaming interface and endpoint descriptors. When the next descriptor
pointer points to a MIDI interface descriptor, call midih_open() with that
descriptor pointer.

# CLASS SPECIFIC INTERFACE AND REQUESTS
The host driver does not make use of the information in the class specific
interface descriptors. In the future, a public API could be created to
retrieve the string descriptors for the names of each ELEMENT,
IN JACK and OUT JACK, and how the device describes the connections.

This driver also does not support class specific requests to control
ELEMENT items, nor does it support non-MIDI Streaming bulk endpoints.

# MIDI CLASS SPECIFIC DESCRIPTOR TOTAL LENGTH FIELD IGNORED
I have observed at least one keyboard by a leading manufacturer that
sets the wTotalLength field of the Class-Specific MS Interface Header
Descriptor to include the length of the MIDIStreaming Endpoint
Descriptors. This is wrong per my reading of the specification.

# MESSAGE BUFFER DETAILS
Messages buffers composed from USB data received on the IN endpoint will never contain
running status because USB MIDI 1.0 class does not support that. Messages
buffers to be sent to the device on the OUT endpoint may contain running status
(the message might come from a UART data stream from a 5-pin DIN MIDI IN
cable on the host, for example). The driver may in the future correctly compose
4-byte USB MIDI Class packets using the running status if need be. However,
it does not currently do that. Also, use of running status is not a good idea
overall because a single byte error can really mess up the data stream with no
way to recover until the next non-real time status byte is in the message buffer.

Message buffers to be sent to the device may contain Real time messages
such as MIDI clock. Real time messages may be inserted in the message
byte stream between status and data bytes of another message without disrupting
the running status. However, because MIDI 1.0 class messages are sent
as four byte packets, a real-time message so inserted will be re-ordered
to be sent to the device in a new 4-byte packet immediately before the
interrupted data stream.

Real time messages the device sends to the host can only appear between
the status byte and data bytes of the message in System Exclusive messages
that are longer than 3 bytes.

# POORLY FORMED USB MIDI DATA PACKETS FROM THE DEVICE
Some devices do not properly encode the code index number (CIN) for the
MIDI message status byte even though the 3-byte data payload correctly encodes
the MIDI message. This driver looks to the byte after the CIN byte to decide
how many bytes to place in the message buffer.

Some devices do not properly encode the virtual cable number. If the virtual
cable number in the CIN data byte of the packet is not less than bNumEmbMIDIJack
for that endpoint, then the host driver assumes virtual cable 0 and does not
report an error.

Some MIDI devices will always send back exactly wMaxPacketSize bytes on
every endpoint even if only one 4-byte packet is required (e.g., NOTE ON).
These devices send packets with 4 packet bytes 0. This driver ignores all
zero packets without reporting an error.

# ENUMERATION FAILURES
The host may fail to enumerate a device if it has too many endpoints, if it has
if it has a Standard MS Transfer Bulk Data Endpoint Descriptor (not supported),
if it has a poorly formed descriptor, or if the descriptor is too long for
the host to read the whole thing.
