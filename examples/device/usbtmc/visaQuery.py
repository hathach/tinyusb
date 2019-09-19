import visa
import time
import sys


def test_idn():
	idn = inst.query("*idn?");
	assert idn == "TinyUSB,ModelNumber,SerialNumber,FirmwareVer123456\r\n"

def test_echo(m,n):
	longstr = "0123456789abcdefghijklmnopqrstuvwxyz" * 50

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
		inst.read_stb();# Just to make USB logging easier by sending a control query

def test_trig():
	# clear SRQ
	inst.read_stb()
	assert (inst.read_stb() == 0)
	inst.assert_trigger()
	time.sleep(0.3) # SRQ may have some delay
	assert (inst.read_stb() & 0x40), "SRQ not set after 0.3 seconds"
	assert (inst.read_stb() == 0)
	
	
def test_mav():
	assert (inst.read_stb() == 0)
	inst.write("123")
	time.sleep(0.3)
	assert (inst.read_stb() & 0x10), "MAV not set after 0.5 seconds"
	
	rsp = inst.read()
	assert(rsp == "123\r\n")
	
	
def test_srq():
	assert (inst.read_stb() == 0)
	inst.write("123")
	
	#inst.enable_event(visa.constants.VI_EVENT_SERVICE_REQ, visa.constants.VI_QUEUE)
	#waitrsp = inst.wait_on_event(visa.constants.VI_EVENT_SERVICE_REQ, 5000)
	#inst.discard_events(visa.constants.VI_EVENT_SERVICE_REQ, visa.constants.VI_QUEUE)
	#inst.wait_for_srq()
	time.sleep(0.3)
	stb = inst.read_stb()
	msg =  "SRQ not set after 0.5 seconds, was {:02x}".format(stb)
	assert (stb == 0x50),msg

	assert (inst.read_stb() == 0x10), "SRQ set at second read!"
	
	rsp = inst.read()
	assert(rsp == "123\r\n")
	

rm = visa.ResourceManager("/c/Windows/system32/visa64.dll")
reslist = rm.list_resources("USB?::?*::INSTR")
print(reslist)

if (len(reslist) == 0):
    sys.exit()
	
inst = rm.open_resource(reslist[0]);
inst.timeout = 3000
inst.clear()

#print("+ IDN")
#test_idn()

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

print("+ MAV")
test_mav()

print("+ SRQ")
test_srq()

print("+ TRIG")
test_trig()

inst.close()
print("Test complete")
