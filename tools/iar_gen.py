#!/usr/bin/python3

import os
import xml.dom.minidom as XML

# Read base configuration
base = ""
with open("iar_template.ipcf") as f:
    base = f.read()

# Enumerate all device/host examples
dir_1 = os.listdir("../examples")
for dir_2 in dir_1:
    if os.path.isdir("../examples/{}".format(dir_2)):
        print(dir_2)
        examples = os.listdir("../examples/{}".format(dir_2))
        for example in examples:
            if os.path.isdir("../examples/{}/{}".format(dir_2, example)):
                print("../examples/{}/{}".format(dir_2, example))
                conf = XML.parseString(base)
                files = conf.getElementsByTagName("files")[0]
                inc   = conf.getElementsByTagName("includePath")[0]
                # Add bsp inc
                path = conf.createElement('path')
                path_txt = conf.createTextNode("$TUSB_DIR$/hw")
                path.appendChild(path_txt)
                inc.appendChild(path)
                # Add board.c/.h
                grp = conf.createElement('group')
                grp.setAttribute("name", "bsp")
                path = conf.createElement('path')
                path_txt = conf.createTextNode("$TUSB_DIR$/hw/bsp/board.c")
                path.appendChild(path_txt)
                grp.appendChild(path)
                files.appendChild(grp)
                # Add example's .c/.h
                grp = conf.createElement('group')
                grp.setAttribute("name", "example")
                for file in os.listdir("../examples/{}/{}/src".format(dir_2, example)):
                    if file.endswith(".c") or file.endswith(".h"):
                        path = conf.createElement('path')
                        path.setAttribute("copyTo", "$PROJ_DIR$/{}".format(file))
                        path_txt = conf.createTextNode("$TUSB_DIR$/examples/{0}/{1}/src/{2}".format(dir_2, example, file))
                        path.appendChild(path_txt)
                        grp.appendChild(path)
                files.appendChild(grp)
                cfg_str = conf.toprettyxml()
                cfg_str = '\n'.join([s for s in cfg_str.splitlines() if s.strip()])
                #print(cfg_str)
                with open("../examples/{0}/{1}/iar_{1}.ipcf".format(dir_2, example), 'w') as f:
                    f.write(cfg_str)
