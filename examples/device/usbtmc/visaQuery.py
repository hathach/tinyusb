#!/usr/bin/env python3

import pyvisa
import time
import sys


def test_idn():
	idn = inst.query("*idn?");
	assert (idn == "TinyUSB,ModelNumber,SerialNumber,FirmwareVer123456\r\n")
	assert (inst.is_4882_compliant)

def test_echo(m,n):
	longstr = "0123456789abcdefghijklmnopqrstuvwxyz" * 50
	t0 = time.monotonic()

	#Next try echo from 1 to 175 characters (200 is max buffer size on DUT)
	for i in range(m,n):
		#print(i)
		x = longstr[0:i]
		xt = x + inst.write_termination
		y = inst.query(x)
		#print(x)
		#print (":".join("{:02x}".format(ord(c)) for c in xt))
		#print (":".join("{:02x}".format(ord(c)) for c in y))
		assert(xt == y), f"failed i={i}"
		#inst.read_stb();# Just to make USB logging easier by sending a control query (bad thing is that this made things slow)
	t = time.monotonic() - t0
	print(f"  elapsed: {t:0.3} sec")

def test_trig():
	# clear SRQ
	inst.read_stb()
	assert (inst.read_stb() == 0)
	inst.assert_trigger()
	time.sleep(0.3) # SRQ may have some delay
	assert (inst.read_stb() & 0x40), "SRQ not set after 0.3 seconds"
	assert (inst.read_stb() == 0)
	
	
def test_mav():
	inst.write("delay 50")
	inst.read_stb() # clear STB
	assert (inst.read_stb() == 0)
	inst.write("123")
	time.sleep(0.3)
	assert (inst.read_stb() & 0x10), "MAV not set after 0.5 seconds"
	
	rsp = inst.read()
	assert(rsp == "123\r\n")
	
	
def test_srq():
	assert (inst.read_stb() == 0)
	inst.write("123")
	
	#inst.enable_event(pyvisa.constants.VI_EVENT_SERVICE_REQ, pyvisa.constants.VI_QUEUE)
	#waitrsp = inst.wait_on_event(pyvisa.constants.VI_EVENT_SERVICE_REQ, 5000)
	#inst.discard_events(pyvisa.constants.VI_EVENT_SERVICE_REQ, pyvisa.constants.VI_QUEUE)
	#inst.wait_for_srq()
	time.sleep(0.3)
	stb = inst.read_stb()
	msg =  "SRQ not set after 0.5 seconds, was {:02x}".format(stb)
	assert (stb == 0x50),msg

	assert (inst.read_stb() == 0x10), "SRQ set at second read!"
	
	rsp = inst.read()
	assert(rsp == "123\r\n")

def test_read_timeout():
	inst.timeout = 500
	# First read with no MAV
	inst.read_stb()
	assert (inst.read_stb() == 0)
	inst.write("delay 500")
	t0 = time.monotonic()
	try:
		rsp = inst.read()
		assert(False), "Read should have resulted in timeout"
	except pyvisa.VisaIOError:
		print("  Got expected exception")
	t = time.monotonic() - t0
	assert ((t*1000.0) > (inst.timeout - 300))
	assert ((t*1000.0) < (inst.timeout + 300))
	print(f"Delay was {t:0.3}")
	# Response is still in queue, so send a clear (to be more helpful to the next test)
	inst.clear()
	time.sleep(0.3)
	assert(0 ==  (inst.read_stb() & 0x10)), "MAV not reset after clear"

def test_abort_in():
	inst.timeout = 200
	# First read with no MAV
	inst.read_stb()
	assert (inst.read_stb() == 0)
	inst.write("delay 500")
	inst.write("xxx")
	t0 = time.monotonic()
	try:
		rsp = inst.read()
		assert(False), "Read should have resulted in timeout"
	except pyvisa.VisaIOError:
		print("  Got expected exception")
	t = time.monotonic() - t0
	assert ((t*1000.0) > (inst.timeout - 300))
	assert ((t*1000.0) < (inst.timeout + 300))
	print(f"  Delay was {t:0.3}")
	# Response is still in queue, so read it out (to be more helpful to the next test)
	inst.timeout = 800
	y = inst.read()
	assert(y == "xxx\r\n")
	
def test_indicate():
	# perform indicator pulse
	usb_iface = inst.get_visa_attribute(pyvisa.constants.VI_ATTR_USB_INTFC_NUM)
	retv = inst.control_in(request_type_bitmap_field=0xA1, request_id=64, request_value=0x0000, index=usb_iface, length=0x0001)
	# pyvisa used to return (statuscode,bytes), but now only returns bytes, so we need to handle both cases
	if(isinstance(retv,bytes)):
		assert(retv == b'\x01')
	else:
		assert((retv[1] == pyvisa.constants.StatusCode(0)) and (retv[0] == b'\x01')), f"indicator pulse failed: retv={retv}"
	
	
def test_multi_read():
	old_chunk_size = inst.chunk_size
	longstr = "0123456789abcdefghijklmnopqrstuvwxyz" * 10
	timeout = 10
	x = longstr[0:174]
	inst.chunk_size = 50 # Seems chunk size only applies to read but not write
	inst.write(x)
	# I'm not sure how to request just the remaining bit using a max count... so just read it all.
	y = inst.read()
	assert (x + "\r\n" == y)
	#inst.chunk_size = old_chunk_size
	
def test_stall_ep0():
	usb_iface = inst.get_visa_attribute(pyvisa.constants.VI_ATTR_USB_INTFC_NUM)
	inst.read_stb()
	# This is an invalid request, should create stall.
	try:
		retv = inst.control_in(request_type_bitmap_field=0xA1, request_id=60, request_value=0x0000, index=usb_iface, length=0x0001)
		assert(False)
	except pyvisa.VisaIOError:
		pass
	
	assert (inst.read_stb() == 0)


rm = pyvisa.ResourceManager()
reslist = rm.list_resources("USB?::?*::INSTR")
print(reslist)

if (len(reslist) == 0):
    sys.exit()
	
inst = rm.open_resource(reslist[0]);
inst.timeout = 3000

inst.clear()

print("+ IDN")
test_idn()

print("+test abort in")
test_abort_in()


inst.timeout = 2000

print("+ multi read")
test_multi_read()

print("+ echo delay=0")
inst.write("delay 0")
test_echo(1,175)

print("+ echo delay=2")
inst.write("delay 2")
test_echo(1,175)

print("+ echo delay=150")
inst.write("delay 150")
test_echo(53,76)
test_echo(165,170)

print("+ Read timeout (no MAV)")
test_read_timeout()

print("+ Test EP0 stall recovery")
test_stall_ep0()

print("+ MAV")
test_mav()

print("+ SRQ")
test_srq()

print("+ indicate")
test_indicate()

print("+ TRIG")
test_trig()

# Untested:
#  abort bulk out
#  LLO, GTL, etc
#  Throughput rate?
#  Transmitting a message using multiple transfers

inst.close()
print("Test complete")
