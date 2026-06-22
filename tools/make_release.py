#!/usr/bin/env python3
import re
import gen_doc
import gen_presets

version = '0.20.0'

print('version {}'.format(version))
ver_id = version.split('.')

###################
# src/tusb_option.h
###################
f_option_h = 'src/tusb_option.h'
with open(f_option_h) as f:
    fdata = f.read()
    fdata = re.sub(r'(#define TUSB_VERSION_MAJOR *) \d+', r"\1 {}".format(ver_id[0]), fdata)
    fdata = re.sub(r'(#define TUSB_VERSION_MINOR *) \d+', r"\1 {}".format(ver_id[1]), fdata)
    fdata = re.sub(r'(#define TUSB_VERSION_REVISION *) \d+', r"\1 {}".format(ver_id[2]), fdata)

# Write the file out again
with open(f_option_h, 'w') as f:
    f.write(fdata)

###################
# repository.yml
###################
f_repository_yml = 'repository.yml'
with open(f_repository_yml) as f:
    fdata = f.read()

if fdata.find(version) < 0:
    fdata = re.sub(r'("0-latest"): "\d+\.\d+\.\d+"', r'"{}": "{}"\r\n    \1: "{}"'.format(version, version, version), fdata)
    with open(f_repository_yml, 'w') as f:
        f.write(fdata)

###################
# library.json
###################
f_library_json = 'library.json'
with open(f_library_json) as f:
    fdata = f.read()
    fdata = re.sub(r'( {4}"version":) "\d+\.\d+\.\d+"', rf'\1 "{version}"', fdata)

with open(f_library_json, 'w') as f:
    f.write(fdata)

###################
# sonar-project.properties
###################
f_sonar_properties = 'sonar-project.properties'
with open(f_sonar_properties) as f:
    fdata = f.read()
fdata = re.sub(r'(sonar\.projectVersion=)\d+\.\d+\.\d+', r'\g<1>{}'.format(version), fdata)

with open(f_sonar_properties, 'w') as f:
    f.write(fdata)

# gen docs
gen_doc.gen_deps_doc()
gen_doc.gen_boards_doc()

# gen presets
gen_presets.main()

##################(ver#
# docs/info/changelog.rst
###################
print("Update docs/info/changelog.rst")
