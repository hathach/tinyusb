# RT-Thread building script for bridge

import os
from building import *

objs = []
cwd  = GetCurrentDir()

objs = objs + SConscript(cwd + '/sys/rt-thread/SConscript')

Return('objs')
