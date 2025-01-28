from __future__ import absolute_import
import os
import subprocess
import platform
import filecmp
from glob import iglob

import lit.Test
import lit.TestRunner
from lit.TestRunner import parseIntegratedTestScript
import lit.util
import lit.ShUtil as ShUtil
from lit.discovery import getLocalConfig
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

def split_path(path):
    all_parts = []
    while True:
        parts = os.path.split(path)
        if parts[0] == path:  # sentinel for absolute paths
            all_parts.insert(0, parts[0])
            break
        elif parts[1] == path: # sentinel for relative paths
            all_parts.insert(0, parts[1])
            break
        else:
            path = parts[0]
            all_parts.insert(0, parts[1])
    return tuple(all_parts)

class DxcHashTest(TestFormat):
    def __init__(self, test_path, dxc_path, dxa_path, working_dir):
        self.test_path = test_path
        self.dxc_path = dxc_path
        self.dxa_path = dxa_path
        self.cwd = working_dir

    def addTest(self, testSuite, rel_dir, localConfig, filename):
        return lit.Test.Test(testSuite, [rel_dir], localConfig, filename)

    def discoverTests(self, testSuite, path, litConfig, local_config_cache):
        lc = getLocalConfig(testSuite, split_path(path), litConfig, local_config_cache)
        for name in os.listdir(path):
            # Ignore dot files and excluded tests.
            if (name.startswith('.') or
                name in lc.excludes):
                continue

            full_name = os.path.join(path, name)
            if os.path.isfile(full_name):
                base,ext = os.path.splitext(full_name)
                if ext not in lc.suffixes:
                    continue

                if not name.endswith(".hlsl") and not name.endswith(".test"):
                    continue
                rel_dir = os.path.relpath(full_name, testSuite.source_root)
                yield self.addTest(testSuite, rel_dir, lc, full_name)
            else:
                for test in self.discoverTests(testSuite, full_name, litConfig, local_config_cache):
                    yield test
                #yield self.discoverTests(testSuite, os.path.join(path, name), litConfig, local_config_cache)


    def getTestsInDirectory(self, testSuite, path_in_suite,
                            litConfig, localConfig):
        # If not running from HashStability directory directly, update source_root to HashStability
        # This is for temp dir for the original RUN line output file like Fi/Fe.
        rel_dir = os.path.relpath(testSuite.source_root, self.cwd)
        if rel_dir == ".": # testSuite.source_root is self.test_path
            testSuite.source_root = os.path.join(self.test_path, 'HashStability')
        # update exec_root to cwd
        testSuite.exec_root = self.cwd

        if not os.path.isdir(self.test_path):
            rel_dir = os.path.relpath(self.test_path, testSuite.source_root)
            yield self.addTest(testSuite, rel_dir, localConfig, self.test_path)
        else:
            local_config_cache = {}
            localConfig.suffixes = {'.hlsl', '.test'}
            # add localConfig to cache
            key = (testSuite, path_in_suite)
            local_config_cache[key] = localConfig
            for test in self.discoverTests(testSuite, self.test_path, litConfig, local_config_cache):
                yield test
            #yield self.discoverTests(testSuite, self.test_path, litConfig, local_config_cache)

    def isUselessArgWithValue(self, arg):
        useless_arg_with_val = ["-Fo" , "/Fo", # Fo will be removed for use different output file
                 "-P", "/P", # P will make dxc only preprocess
                 "-Fd", "/Fd", # Fd is depend on /Zi which will be removed for normal compile
                 ]
        return arg in useless_arg_with_val
    
    def isUselessArgFlag(self, arg):
        useless_arg_flag = [
                            "-Zi", "/Zi", # skip Zi for normal compile
                            "-M", "/M", # skip M which dump dependency
                            "-MD", "/MD", # skip MD which dump dependency
                            "-H", # skip H which only show header includes
                            "-Odump", "/Odump", # skip Odump which only dump optimizer commands
                            "-fcgl", "/fcgl", # skip fcgl which doesn't generate container
                            # skip ast options which doesn't generate container
                            "-ast-dump", "/ast-dump",
                            "-ast-dump-implicit", "/ast-dump-implicit",
                            # skip debug related args for normal compile
                            "-Zsb", "/Zsb",
                            "-Zss", "/Zss",
                            "-Zpr", "/Zpr",
                            "-Qembed_debug", "/Qembed_debug", 
                            # skip Zs for conflict with Zi
                            "-Zs", "/Zs"]
        return arg in useless_arg_flag

    def getCleanArgs(self, dxc_cmd):
        original_args = dxc_cmd.args
        args = []
        skip_val = False
        for i in range(len(original_args)):
            arg = original_args[i]
            if self.isUselessArgWithValue(arg):
                if arg == "/P" or arg == "-P":
                    # Check if next arg is a file name
                    if i + 1 < len(original_args):
                        next_arg = original_args[i + 1]
                        # If next arg is not a file name, leave it out
                        if not os.path.isfile(next_arg) and not next_arg.endswith(".hlsl"):
                            continue
                # remove "-Fo", "-Fe", "-Fi" and things next to it from args
                skip_val = True
                continue
            elif self.isUselessArgFlag(arg):
                # remove "-ast-dump" and "-Zi" from args
                continue
            elif skip_val:
                skip_val = False
                continue

            args.append(arg)

        return args

    def hasIllegalArgs(self, args):
        # skip RUN lines without %dxc
        if args[0] != "%dxc":
            return True

        # skip RUN lines with illegal args
        illegal_args = {
                        # skip spirv which not generate DXIL and might have other spirv only options.
                        "-spirv", "/spirv",
                        # skip options which don't need set target profile.
                        "-dumpbin", "/dumpbin",
                        "-recompile", "/recompile",
                        # skip verify for the shader might fail to compile.
                        "-verify"}
        if not illegal_args.isdisjoint(args):
            return True

        for arg in args:
            # root signature profile doesn't generate hash part
            if (arg.startswith("rootsig_1_") and "-force_rootsig_ver" not in args) or \
                arg.startswith("-Trootsig_1_"):
                return True

        return False

    def executeHashTest(self, test, cmd, counter):
        if isinstance(cmd, ShUtil.Seq):
            if cmd.op == '&':
                raise lit.TestRunner.InternalShellError(cmd,"unsupported shell operator: '&'")

            if cmd.op == ';' or cmd.op == '||' or cmd.op == '&&':
                res, counter = self.executeHashTest(test, cmd.lhs, counter)
                if res.code == lit.Test.FAIL:
                    return res, counter
                return self.executeHashTest(test, cmd.rhs, counter)

            raise ValueError('Unknown shell command: %r' % cmd.op)
        assert isinstance(cmd, ShUtil.Pipeline)

        status = lit.Test.PASS

        for i,j in enumerate(cmd.commands):
            if self.hasIllegalArgs(j.args):
                continue

            args = self.getCleanArgs(j)

            # get test_name from test path for tmp file name.
            test_name = test.path_in_suite[0].replace('\\','_').replace('/','_').replace('.','_')

            # run hash stability test
            res, msg = run_hash_stablity_test(args, self.dxc_path, self.dxa_path, test_name, self.cwd, counter)
            counter += 1
            if not res:
                status = lit.Test.FAIL
                return lit.Test.Result(lit.Test.FAIL, msg), counter
        return lit.Test.Result(lit.Test.PASS), counter

    def execute(self, test, litConfig):
        if test.config.unsupported:
            return (lit.Test.UNSUPPORTED, 'Test is unsupported')

        res = parseIntegratedTestScript(test)
        if isinstance(res, lit.Test.Result):
            if res.code == lit.Test.UNRESOLVED:
                return lit.Test.Result(lit.Test.PASS)
            return res
        if litConfig.noExecute:
            return lit.Test.Result(lit.Test.PASS)
        script, tmpBase, execdir = res
        cmds = []
        for ln in script:
            try:
                cmds.append(ShUtil.ShParser(ln, litConfig.isWindows,
                                            test.config.pipefail).parse())
            except:
                return lit.Test.Result(lit.Test.FAIL, "shell parser error on: %r" % ln)

        # Create the output directory for original RUN line output file if it does not already exist.
        # This is for output like Fi/Fe.
        execpath = test.getExecPath()
        execdir,execbase = os.path.split(execpath)
        tmpDir = os.path.join(execdir, 'Output')
        lit.util.mkdir_p(os.path.dirname(tmpDir))

        counter = 0
        for cmd in cmds:
            res, counter = self.executeHashTest(test, cmd, counter)
            if res.code == lit.Test.FAIL:
                return res
        if counter == 0:
            return lit.Test.Result(lit.Test.FAIL, "no RUN lines for hash stability found. If this is expected, add 'UNSUPPORTED: hash_stability' to the test")
        return lit.Test.Result(lit.Test.PASS)
