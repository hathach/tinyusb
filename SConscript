# RT-Thread building script for bridge

import os
from building import *

objs = []
cwd  = GetCurrentDir()

objs = objs + SConscript(cwd + '/lib/rt-thread/SConscript')

Return('objs')
