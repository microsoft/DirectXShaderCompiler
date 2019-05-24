import argparse
import calendar
import datetime
import os
import re
import subprocess
import time

#globals
dxilver_major=0
dxilver_minor=0

def rc_version_field_1(t):
    return dxilver_major


def rc_version_field_2(t):
    return dxilver_minor


def rc_version_field_3(t):
    return t >> 16


def rc_version_field_4(t):
    return t & 0xFFFF

def last_commit_sha():
    enlistment_root=os.path.dirname(os.path.abspath( __file__ ))
    output = subprocess.check_output([ "git.exe", "describe", "--tags", "--always", "--dirty" ], cwd=enlistment_root)
    return output.decode('ASCII').strip()

def branch_name():
    enlistment_root=os.path.dirname(os.path.abspath( __file__ ))
    output = subprocess.check_output([ "git.exe", "rev-parse", "--abbrev-ref", "HEAD" ], cwd=enlistment_root)
    return output.decode('ASCII').strip()

def quoted_version_str(t):
    return '"{}.{}.{}.{}"'.format(
        rc_version_field_1(t),
        rc_version_field_2(t),
        rc_version_field_3(t),
        rc_version_field_4(t))

def product_version_str(t):
    if (args.no_commit_sha):
        return quoted_version_str(t)
    else:
        return '"{}.{}.{}.{} ({}, {})"'.format(
            rc_version_field_1(t),
            rc_version_field_2(t),
            rc_version_field_3(t),
            rc_version_field_4(t),
            branch_name(),
            last_commit_sha()
            )

def print_define(name, value):
    print('#ifdef {}'.format(name))
    print('#undef {}'.format(name))
    print('#endif')
    print('#define {} {}'.format(name, value))
    print()


def fetch_dxil_version():
    global dxilver_major
    global dxilver_minor

    dxil_const_filename=os.path.join(os.environ['HLSL_SRC_DIR'], "include", "dxc", "DXIL", "DxilConstants.h")
    with open(dxil_const_filename) as f:
        lines = f.readlines()

    for line in lines:
        match = re.search("const\s+unsigned\s+kDxilMajor\s*=\s*(\d+)\s*;", line)
        if match: 
            dxilver_major = match.group(1)
        else:
            match = re.search("const\s+unsigned\s+kDxilMinor\s*=\s*(\d+)\s*;", line)
            if match: dxilver_minor = match.group(1)

        if (dxilver_major  != 0 and dxilver_minor != 0):
            break

    if dxilver_major == 0 and dxilver_minor == 0:
        raise Exception("Could not parse kDxilMajor/kDxilMinor from {}".format(dxil_const_filename))


# main
p = argparse.ArgumentParser("gen_version")
p.add_argument("--no-commit-sha", action='store_true')
args = p.parse_args()

# parse DXIL version from include\dxc\DXIL\DxilConstants.h
fetch_dxil_version()

t = int(time.time())
print("// Version generated based on current time")
print("// UTC:   {}".format(time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime(t))))
print("// Local: {}".format(time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime(t))))

print('#pragma once')
print()
print_define('RC_COMPANY_NAME',      '"Microsoft(r) Corporation"')
print_define('RC_VERSION_FIELD_1',   rc_version_field_1(t))
print_define('RC_VERSION_FIELD_2',   rc_version_field_2(t))
print_define('RC_VERSION_FIELD_3',   rc_version_field_3(t))
print_define('RC_VERSION_FIELD_4',   rc_version_field_4(t))
print_define('RC_FILE_VERSION',      quoted_version_str(t))
print_define('RC_FILE_DESCRIPTION', '"DirectX Compiler - Out Of Band"')
print_define('RC_COPYRIGHT',        '"(c) Microsoft Corporation. All rights reserved."')
print_define('RC_PRODUCT_NAME',     '"Microsoft(r) DirectX for Windows(r) - Out Of Band"')
print_define('RC_PRODUCT_VERSION',   product_version_str(t))
