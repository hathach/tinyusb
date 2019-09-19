import visa
import time
import sys


def test_idn():
	idn = inst.query("*idn?");
	assert idn == "TinyUSB,ModelNumber,SerialNumber,FirmwareVer123456\r\n"

def test_echo():
	longstr = "0123456789abcdef" * 50

	#Next try echo from 1 to 175 characters (200 is max buffer size on DUT)
	for i in range(49,175):
		x = longstr[0:i]
		xt = x + inst.write_termination
		y = inst.query(x)
		assert(xt == y), f"echo {i}"

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

#print(idn);
inst.clear()

test_idn()
test_echo()
test_mav()
test_srq()

inst.close()
print("Test complete")
