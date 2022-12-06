from __future__ import absolute_import
import os
import sys

import lit.Test
import lit.TestRunner
import lit.util
from .base import TestFormat

class TaefTest(TestFormat):
    def __init__(self, te_path, test_dll, hlsl_data_dir, test_path):
        self.te = te_path
        self.test_dll = test_dll
        self.hlsl_data_dir = hlsl_data_dir
        self.test_path = test_path
        # NOTE: when search test, always running on test_dll,
        #       use test_searched to make sure only add test once.
        #       If TaeftTest is created in directory with sub directory,
        #       getTestsInDirectory will be called more than once.
        self.test_searched = False

    def getTaefTests(self, dll_path, litConfig, localConfig):
        """getTaefTests()

        Return the tests available in taef test dll.

        Args:
          litConfig: LitConfig instance
          localConfig: TestingConfig instance"""

        # TE:F:\repos\DxcGitHub\hlsl.bin\TAEF\x64\te.exe
        # test dll : F:\repos\DxcGitHub\hlsl.bin\Debug\test\clang-hlsl-tests.dll
        # /list

        if litConfig.debug:
            litConfig.note('searching taef test in %r' % dll_path)

        try:
            lines = lit.util.capture([self.te, dll_path, '/list'],
                                     env=localConfig.environment)
            # this is for windows
            lines = lines.replace('\r', '')
            lines = lines.split('\n')
        except:
            litConfig.error("unable to discover taef in %r" % dll_path)
            raise StopIteration

        for ln in lines:
            # The test name is like VerifierTest::RunUnboundedResourceArrays.
            if ln.find('::') == -1:
                continue

            yield ln.strip()

    # Note: path_in_suite should not include the executable name.
    def getTestsInExecutable(self, testSuite, path_in_suite, execpath,
                             litConfig, localConfig):

        # taef test should be dll.
        if not execpath.endswith('dll'):
            return

        (dirname, basename) = os.path.split(execpath)
        # Discover the tests in this executable.
        for testname in self.getTaefTests(execpath, litConfig, localConfig):
            testPath = path_in_suite + (basename, testname)
            yield lit.Test.Test(testSuite, testPath, localConfig, file_path=execpath)

    def getTestsInDirectory(self, testSuite, path_in_suite,
                            litConfig, localConfig):
        # Make sure self.test_dll only search once.
        if self.test_searched:
            return

        self.test_searched = True

        filepath = self.test_dll
        for test in self.getTestsInExecutable(
                testSuite, path_in_suite, filepath,
                litConfig, localConfig):
            yield test

    def execute(self, test, litConfig):
        test_dll = test.getFilePath()

        testPath,testName = os.path.split(test.getSourcePath())

        param_hlsl_data_dir = str.format('/p:HlslDataDir={}', self.hlsl_data_dir)
        cmd = [self.te, test_dll, '/inproc',
                param_hlsl_data_dir,
                '/miniDumpOnCrash', '/unicodeOutput:false',
                '/logOutput:LowWithConsoleBuffering',
                str.format('/outputFolder:{}', self.test_path),
                str.format('/name:{}', testName)]

        if litConfig.useValgrind:
            cmd = litConfig.valgrindArgs + cmd

        if litConfig.noExecute:
            return lit.Test.PASS, ''

        out, err, exitCode = lit.util.executeCommand(
            cmd, env=test.config.environment)

        if exitCode:
            return lit.Test.FAIL, out + err

        summary = 'Summary: Total='
        if summary not in out:
            msg = ('Unable to find %r in taef output:\n\n%s%s' %
                   (summary, out, err))
            return lit.Test.UNRESOLVED, msg
        no_fail = 'Failed=0, Blocked=0, Not Run=0, Skipped=0'
        if no_fail not in out == -1:
            msg = ('Unable to find %r in taef output:\n\n%s%s' %
                   (no_fail, out, err))
            return lit.Test.UNRESOLVED, msg

        return lit.Test.PASS,''

