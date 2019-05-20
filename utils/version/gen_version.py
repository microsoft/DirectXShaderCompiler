import argparse
import calendar
import datetime
import os
import subprocess
import time

p = argparse.ArgumentParser("gen_version")
p.add_argument("--no-commit-sha", action='store_true')
args = p.parse_args()

d = datetime.datetime.now(datetime.timezone.utc)

def rc_version_field_1(d):
    return '14'     # DXIL version 1.4 (should match kDxilMajor/kDxilMinor in dxc\DXIL\DxilConstants.h)


def rc_version_field_2(d):
    return d.year


def rc_version_field_3(d):
    return '1{:02}{:02}'.format(d.month, d.day)


def rc_version_field_4(d):
    return '1{:02}{:02}'.format(d.hour, d.minute)

def last_commit_sha():
    enlistment_root=os.path.dirname(os.path.abspath( __file__ ))
    output = subprocess.check_output([ "git.exe", "describe", "--tags", "--always", "--dirty" ], cwd=enlistment_root)
    return output.decode('ASCII').strip()

def branch_name():
    enlistment_root=os.path.dirname(os.path.abspath( __file__ ))
    output = subprocess.check_output([ "git.exe", "rev-parse", "--abbrev-ref", "HEAD" ], cwd=enlistment_root)
    return output.decode('ASCII').strip()

def quoted_version_str(d):
    return '"{}.{}.{}.{}"'.format(
        rc_version_field_1(d),
        rc_version_field_2(d),
        rc_version_field_3(d),
        rc_version_field_4(d))

def product_version_str(d):
    if (args.no_commit_sha):
        return quoted_version_str(d)
    else:
        return '"{}.{}.{}.{} ({}, {})"'.format(
            rc_version_field_1(d),
            rc_version_field_2(d),
            rc_version_field_3(d),
            rc_version_field_4(d),
            branch_name(),
            last_commit_sha()
            )

def print_define(name, value):
    print('#ifdef {}'.format(name))
    print('#undef {}'.format(name))
    print('#endif')
    print('#define {} {}'.format(name, value))
    print()


print('#pragma once')
print()
print_define('RC_COMPANY_NAME',      '"Microsoft(r) Corporation"')
print_define('RC_VERSION_FIELD_1',   rc_version_field_1(d))
print_define('RC_VERSION_FIELD_2',   rc_version_field_2(d))
print_define('RC_VERSION_FIELD_3',   rc_version_field_3(d))
print_define('RC_VERSION_FIELD_4',   rc_version_field_4(d))
print_define('RC_FILE_VERSION',      quoted_version_str(d))
print_define('RC_FILE_DESCRIPTION', '"DirectX Compiler - Out Of Band"')
print_define('RC_COPYRIGHT',        '"(c) Microsoft Corporation. All rights reserved."')
print_define('RC_PRODUCT_NAME',     '"Microsoft(r) DirectX for Windows(r) - Out Of Band"')
print_define('RC_PRODUCT_VERSION',   product_version_str(d))
