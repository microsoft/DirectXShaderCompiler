from __future__ import absolute_import
import os
import subprocess
import platform
import filecmp
from glob import iglob

import lit.Test
import lit.TestRunner
import lit.util
import lit.ShUtil as ShUtil
from .base import TestFormat

# import HashStability.py
import importlib.util
hs_path = os.path.join(os.path.dirname(os.path.realpath(__file__)),
             '..', '..', '..',
             'hct', 'HashStability.py')
spec = importlib.util.spec_from_file_location("HashStability", hs_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
run_hash_stablity_test = module.run_hash_stablity_test


class DxcHashTest(TestFormat):
    def __init__(self, test_path, dxc_path, dxa_path, working_dir):
        self.test_path = test_path
        self.dxc_path = dxc_path
        self.dxa_path = dxa_path
        self.cwd = working_dir

    def getTestsInDirectory(self, testSuite, path_in_suite,
                            litConfig, localConfig):
        if not os.path.isdir(self.test_path):
            hlsl_list = [self.test_path]
        else:
            # Scan all sub directories in self.test_path
            rootdir = self.test_path.replace('\\','/')
            rootdir_glob = f"{rootdir}/**/*.hlsl"

            # This will return absolute paths
            hlsl_list = [f for f in iglob(rootdir_glob, recursive=True) if os.path.isfile(f)]

        # add hlsl files which has first line as %dxc test as tests.
        for filename in hlsl_list:
            with open(filename, 'r') as f:
                first_line = f.readline()
                if (first_line.find("%dxc") == -1 or first_line.find("RUN:") == -1 or
                    first_line.find(" not ") != -1 or
                    first_line.find(" -M ") != -1 or
                    first_line.find(" -H ") != -1 or
                    first_line.find(" -Fi ") != -1 or
                    first_line.find(" -exports ") != -1 or
                    first_line.find(" -ast-dump-implicit ") != -1 or
                    first_line.find(" -fcgl ") != -1 or
                    first_line.find("rootsig_1_") != -1 or
                    first_line.find(" -verify ") != -1 or
                    # must have a target profile
                    first_line.find(" -T") == -1 or
                    # skip spirv.
                    first_line.find("spirv") != -1):
                    continue
            rel_dir = os.path.relpath(filename, testSuite.source_root)

            yield lit.Test.Test(testSuite, [rel_dir], localConfig, filename)

    def getRunLine(self, test):
        # Read first line of the test file
        f = open(test.getFilePath(), 'r')
        first_line = f.readline().rstrip()
        f.close()
        # remove things before RUN: from first line
        first_line = first_line[first_line.find("RUN:") + 4:]

        sourcepath = test.getFilePath()
        sourcedir = os.path.dirname(sourcepath)
        execpath = test.getExecPath()
        execdir,execbase = os.path.split(execpath)
        tmpDir = os.path.join(self.cwd, 'Output')
        tmpBase = os.path.join(tmpDir, execbase)
        tmpFile = tmpBase + '.tmp'
        # subsitute %s with sourcepath
        first_line = first_line.replace("%s", sourcepath)
        # subsitute %t with tmpDir
        first_line = first_line.replace("%t", tmpFile)
        # subsitute %S with sourcedir
        first_line = first_line.replace("%S", sourcedir)
        # subsitute %T with execdir
        first_line = first_line.replace("%T", tmpDir)

        return first_line

    def getCleanArgs(self, dxc_cmd):
        original_args = dxc_cmd.args
        args = []
        for i in range(len(original_args)):
            arg = original_args[i]
            if arg == "-Fo" or arg == "-validator-version":
                # remove "-Fo", "-validator-version" and things next to it from args
                i += 2
                continue
            elif arg == "-ast-dump" or arg == "-Zi" or arg == "-verify" or arg == "-M" or arg == "-H":
                # remove "-ast-dump" and "-Zi" from args
                i += 1
                continue
            args.append(arg)
        return args

    def execute(self, test, litConfig):
        first_line = self.getRunLine(test)
        cmds = []
        try:
            cmds.append(ShUtil.ShParser(first_line, litConfig.isWindows,
                                        test.config.pipefail).parse())
        except ValueError as e:
            return lit.Test.Result(lit.Test.FAIL, "shell parser error: %s" % e)
        except Exception as e:
            return lit.Test.Result(lit.Test.FAIL, "shell parser error: %s" % e)
        except:
            return lit.Test.Result(lit.Test.FAIL, "shell parser error on: %r" % first_line)

        # get first command
        cmd = cmds[0]
        while isinstance(cmd, ShUtil.Seq):
            cmd = cmd.lhs
        dxc_cmd = cmd.commands[0]

        args = self.getCleanArgs(dxc_cmd)

        if args[0] != "%dxc":
            return lit.Test.Result(lit.Test.PASS, "Not a dxc test. Skip.")

        # get test_name from test path for tmp file name.
        test_name = test.path_in_suite[0].replace('\\','_').replace('/','_').replace('.','_')

        # run hash stability test
        res, msg = run_hash_stablity_test(args, self.dxc_path, self.dxa_path, test_name, self.cwd)

        if res:
            return lit.Test.Result(lit.Test.PASS, msg)
        else:
            return lit.Test.Result(lit.Test.FAIL, msg)
