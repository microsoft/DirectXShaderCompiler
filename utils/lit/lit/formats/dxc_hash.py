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

    def getCleanArgs(self, dxc_cmd):
        original_args = dxc_cmd.args
        args = []
        for i in range(len(original_args)):
            arg = original_args[i]
            if arg == "-Fo" or arg == "-validator-version" or arg == "-Fc":
                # remove "-Fo", "-validator-version" and things next to it from args
                i += 2
                continue
            elif arg == "-ast-dump" or arg == "/ast-dump" or arg == "-ast-dump-implicit" or \
                 arg == "-Zi" or arg == "-verify" or arg == "-M" or arg == "-H" or \
                 arg == "/Odump" or \
                 arg == "-fcgl" or arg.startswith("rootsig_1_") or arg.startswith("-Trootsig_1_"):
                # remove "-ast-dump" and "-Zi" from args
                i += 1
                continue
            elif arg == "lib_6_x" or arg == "structurize-returns":
                # FIXME: allow lib_6_x and structurize-returns
                i += 1
                continue

            args.append(arg)
        return args

    def executeHashTest(self, test, cmd):
        if isinstance(cmd, ShUtil.Seq):
            if cmd.op == '&':
                raise lit.TestRunner.InternalShellError(cmd,"unsupported shell operator: '&'")

            if cmd.op == ';' or cmd.op == '||' or cmd.op == '&&':
                res = self.executeHashTest(test, cmd.lhs)
                if res.code == lit.Test.FAIL:
                    return res
                return self.executeHashTest(test, cmd.rhs)

            raise ValueError('Unknown shell command: %r' % cmd.op)
        assert isinstance(cmd, ShUtil.Pipeline)

        status = lit.Test.PASS

        for i,j in enumerate(cmd.commands):
            args = self.getCleanArgs(j)

            if args[0] != "%dxc":
                continue

            if "-spirv" in args:
                continue

            if "/MD" in args or "/M" in args:
                continue

            # get test_name from test path for tmp file name.
            test_name = test.path_in_suite[0].replace('\\','_').replace('/','_').replace('.','_')

            # run hash stability test
            res, msg = run_hash_stablity_test(args, self.dxc_path, self.dxa_path, test_name, self.cwd, i)

            if not res:
                status = lit.Test.FAIL
                return lit.Test.Result(lit.Test.FAIL, msg)
        return lit.Test.Result(lit.Test.PASS)

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

        for cmd in cmds:
            res = self.executeHashTest(test, cmd)
            if res.code == lit.Test.FAIL:
                return res
        return lit.Test.Result(lit.Test.PASS)
