#
# The MIT License (MIT)
#
# Copyright (c) 2023 HiFiPhile
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import os
import sys
import time
import serial
import subprocess

def flash_jlink(sn, dev, firmware):
    script = ['halt', 'r', f'loadfile {firmware}', 'r', 'go', 'exit']
    f = open('flash.jlink', 'w')
    f.writelines(f'{s}\n' for s in script)
    f.close()
    ret = subprocess.run(f'JLinkExe -USB {sn} -device {dev} -if swd -JTAGConf -1,-1 -speed auto -NoGui 1 -ExitOnError 1 -CommandFile flash.jlink',
                         shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    assert ret.returncode == 0, 'Flash failed'

def test_cdc_dual_ports(port1, port2):
    # Wait device enum
    timeout = 10
    while timeout:
        if os.path.exists(port1) and os.path.exists(port2):
            break
        time.sleep(1)
        timeout = timeout - 1

    assert os.path.exists(port1) and os.path.exists(port2), \
        'Port not available'
    
    ser1 = serial.Serial(port1)
    ser2 = serial.Serial(port2)

    ser1.timeout = 1
    ser2.timeout = 1

    str1 = b"test_no1"
    ser1.write(str1)
    ser1.flush()
    assert ser1.read(100) == str1.lower(), 'Port1 wrong data'
    assert ser2.read(100) == str1.upper(), 'Port2 wrong data'

    str2 = b"test_no2"
    ser2.write(str2)
    ser2.flush()
    assert ser1.read(100) == str2.lower(), 'Port1 wrong data'
    assert ser2.read(100) == str2.upper(), 'Port2 wrong data'

    print('cdc_dual_ports test done')




flash_jlink('774470029', 'stm32l412kb', 'examples/device/cdc_dual_ports/_build/stm32l412nucleo/cdc_dual_ports.elf')

test_cdc_dual_ports('/dev/ttyUSB_57323020.00', '/dev/ttyUSB_57323020.02')