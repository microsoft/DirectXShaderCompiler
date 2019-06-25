import argparse
import json
import os
import re
import subprocess

def get_output_of(cmd):
    enlistment_root=os.path.dirname(os.path.abspath( __file__ ))
    output = subprocess.check_output(cmd, cwd=enlistment_root)
    return output.decode('ASCII').strip()

def get_last_commit_sha():
    return get_output_of([ "git.exe", "describe", "--always", "--dirty" ])

def get_current_branch():
    return get_output_of([ "git.exe", "rev-parse", "--abbrev-ref", "HEAD" ])

def get_commit_count(sha):
    return get_output_of([ "git.exe", "rev-list", "--count", sha ])

def read_latest_release_info():
    latest_release_file = os.path.join(os.path.dirname(os.path.abspath( __file__)), "latest-release.json")
    with open(latest_release_file, 'r') as f:
        return json.load(f)

class VersionGen():
    def __init__(self, options):
        self.options = options
        self.latest_release_info = read_latest_release_info()
        self.current_branch = get_current_branch()
        self.rc_version_field_4_cache = None

    def rc_version_field_1(self):
        return self.latest_release_info["version"]["major"]

    def rc_version_field_2(self):
        return self.latest_release_info["version"]["minor"]

    def rc_version_field_3(self):
        return self.latest_release_info["version"]["rev"] if self.options.official else "0"

    def rc_version_field_4(self):
        if self.rc_version_field_4_cache is None:
            base_commit_count = 0
            if self.options.official:
                base_commit_count = int(get_commit_count(self.latest_release_info["sha"]))
            current_commit_count = int(get_commit_count("HEAD"))
            distance_from_base = current_commit_count - base_commit_count
            if (self.current_branch is "master"):
                distance_from_base += 10000
            self.rc_version_field_4_cache = str(distance_from_base)
        return self.rc_version_field_4_cache

    def quoted_version_str(self):
        return '"{}.{}.{}.{}"'.format(
            self.rc_version_field_1(),
            self.rc_version_field_2(),
            self.rc_version_field_3(),
            self.rc_version_field_4())

    def product_version_str(self):
        if (self.options.no_commit_sha):
            return self.quoted_version_str()
        else:
            return '"{}.{}.{}.{} ({}, {})"'.format(
                self.rc_version_field_1(),
                self.rc_version_field_2(),
                self.rc_version_field_3(),
                self.rc_version_field_4(),
                self.current_branch,
                get_last_commit_sha()
                )

    def print_define(self, name, value):
        print('#ifdef {}'.format(name))
        print('#undef {}'.format(name))
        print('#endif')
        print('#define {} {}'.format(name, value))
        print()

    def print_version(self):
        print('#pragma once')
        print()
        self.print_define('RC_COMPANY_NAME',      '"Microsoft(r) Corporation"')
        self.print_define('RC_VERSION_FIELD_1',   self.rc_version_field_1())
        self.print_define('RC_VERSION_FIELD_2',   self.rc_version_field_2())
        self.print_define('RC_VERSION_FIELD_3',   self.rc_version_field_3() if self.options.official else "0")
        self.print_define('RC_VERSION_FIELD_4',   self.rc_version_field_4())
        self.print_define('RC_FILE_VERSION',      self.quoted_version_str())
        self.print_define('RC_FILE_DESCRIPTION', '"DirectX Compiler - Out Of Band"')
        self.print_define('RC_COPYRIGHT',        '"(c) Microsoft Corporation. All rights reserved."')
        self.print_define('RC_PRODUCT_NAME',     '"Microsoft(r) DirectX for Windows(r) - Out Of Band"')
        self.print_define('RC_PRODUCT_VERSION',   self.product_version_str())


def main():
    p = argparse.ArgumentParser("gen_version")
    p.add_argument("--no-commit-sha", action='store_true')
    p.add_argument("--official", action="store_true")
    args = p.parse_args()

    VersionGen(args).print_version() 


if __name__ == '__main__':
    main()

