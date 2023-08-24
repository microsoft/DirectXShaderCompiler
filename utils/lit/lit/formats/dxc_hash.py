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

kIsWindows = platform.system() == 'Windows'

# Don't use close_fds on Windows.
kUseCloseFDs = not kIsWindows

class ShellEnvironment(object):

    """Mutable shell environment containing things like CWD and env vars.

    Environment variables are not implemented, but cwd tracking is.
    """

    def __init__(self, cwd, env):
        self.cwd = cwd
        self.env = env

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

    def get_run_line(self, test):
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

    def extract_hash(self, dxa_path, dx_container):
        # extract hash from dxa_path
        hash_file = f"{dx_container}.hash"
        args = [dxa_path, "-extractpart", "HASH", dx_container, "-o", hash_file]
        stdin, stdout, stderr = 0, subprocess.PIPE, subprocess.PIPE

        proc = subprocess.Popen(args, cwd=self.cwd,
                                                executable = dxa_path,
                                        stdin = stdin,
                                        stdout = stdout,
                                        stderr = stderr,
                                                close_fds = kUseCloseFDs)
        proc.communicate()
        res = proc.returncode
        if res != 0:
            print(f"extract hash failed {args}")
            # extract hash failed, return fail.
            return None
        return hash_file

    def normal_compile(self, args, shenv, test_name, output_file):
        normal_args = args
        normal_args.append("-Qstrip_reflect")
        normal_args.append("-Zsb")
        normal_args.append("-Fo")
        normal_args.append(output_file)
        proc = subprocess.Popen(normal_args, cwd=shenv.cwd,
                                # don't writ output to stdout
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                env=shenv.env,
                                close_fds=kUseCloseFDs)
        proc.communicate()
        return proc.returncode

    def debug_compile(self, args, shenv, test_name, output_file):
        debug_args = args
        debug_args.append("-Zi")
        debug_args.append("-Qstrip_reflect")
        debug_args.append("-Zsb")
        debug_args.append("-Fo")
        debug_args.append(output_file)

        proc = subprocess.Popen(debug_args, cwd=shenv.cwd,
                                # don't writ output to stdout
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                env=shenv.env,
                                close_fds=kUseCloseFDs)
        proc.communicate()
        return proc.returncode

    def execute(self, test, litConfig):
        first_line = self.get_run_line(test)
        cwd = self.cwd
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

        shenv = ShellEnvironment(cwd, test.config.environment)

        # get first command
        cmd = cmds[0]
        while isinstance(cmd, ShUtil.Seq):
            cmd = cmd.lhs
        dxc_cmd = cmd.commands[0]

        if dxc_cmd.args[0] != "%dxc":
            return lit.Test.Result(lit.Test.PASS)
        original_args = dxc_cmd.args
        original_args[0] = self.dxc_path
        executable = self.dxc_path
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

        # run original compilation
        proc = subprocess.Popen(args, cwd=shenv.cwd,
                                executable=executable,
                                # don't writ output to stdout
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                env=shenv.env,
                                close_fds=kUseCloseFDs)
        proc.communicate()
        res = proc.returncode
        if res != 0:
            # original compilation failed, don't run hash test.
            return lit.Test.Result(lit.Test.PASS, "Original compilation failed.")
        # get test_name from test path for tmp file name.
        test_name = test.path_in_suite[0].replace('\\','_').replace('/','_').replace('.','_')
        # run normal compile
        normal_out = os.path.join(self.cwd, 'Output', test_name+'.normal.out')
        res = self.normal_compile(args, shenv, test_name, normal_out)
        if res != 0:
            # strip_reflect failed, return fail.
            return lit.Test.Result(lit.Test.FAIL, "Adding Qstrip_reflect failed compilation.")

        normal_hash = self.extract_hash(self.dxa_path, normal_out)
        if normal_hash is None:
            return lit.Test.Result(lit.Test.FAIL, "Fail to get hash for normal compilation.")


        # run debug compilation
        debug_out = os.path.join(self.cwd, 'Output', test_name+'.dbg.out')
        res = self.debug_compile(args, shenv, test_name, debug_out)
        if res != 0:
            # strip_reflect failed, return fail.
            return lit.Test.Result(lit.Test.FAIL, "Adding Qstrip_reflect and Zi failed compilation.")

        debug_hash = self.extract_hash(self.dxa_path, debug_out)
        if debug_hash is None:
            return lit.Test.Result(lit.Test.FAIL, "Fail to get hash for normal compilation.")

        # compare normal_hash and debug_hash.
        if filecmp.cmp(normal_hash, debug_hash):
            # hash match, return pass.
            return lit.Test.Result(lit.Test.PASS)
        else:
            # hash mismatch
            return lit.Test.Result(lit.Test.FAIL, "Hash mismatch.")
