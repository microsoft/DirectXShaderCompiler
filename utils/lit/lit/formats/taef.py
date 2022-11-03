from __future__ import absolute_import
import os
import sys

import lit.Test
import lit.TestRunner
import lit.util
from .base import TestFormat

kIsWindows = sys.platform in ['win32', 'cygwin']

class TaefTest(TestFormat):
    def __init__(self, te_path, test_dll_path, hlsl_data_dir, test_path):
        self.te = te_path
        self.test_dll = test_dll_path
        self.hlsl_data_dir = hlsl_data_dir
        self.test_path = test_path

    def getTaefTests(self, litConfig, localConfig):
        """getTaefTests()

        Return the tests available in taef test dll.

        Args:
          litConfig: LitConfig instance
          localConfig: TestingConfig instance"""

        # TE:F:\repos\DxcGitHub\hlsl.bin\TAEF\x64\te.exe
        # test dll : F:\repos\DxcGitHub\hlsl.bin\Debug\test\clang-hlsl-tests.dll
        # /list

        try:
            lines = lit.util.capture([self.te, self.test_dll, '/list'],
                                     env=localConfig.environment)
            if kIsWindows:
              lines = lines.replace('\r', '')
            lines = lines.split('\n')
        except:
            litConfig.error("unable to discover taef in %r" % path)
            raise StopIteration

        nested_tests = []
        for ln in lines:
            # The test name is like VerifierTest::RunUnboundedResourceArrays.
            if ln.find('::') == -1:
                continue

            yield ''.join(nested_tests) + ln.strip()

    # Note: path_in_suite should not include the executable name.
    def getTestsInExecutable(self, testSuite, path_in_suite,
                             litConfig, localConfig):
        # Discover the tests in this executable.
        for testname in self.getTaefTests(litConfig, localConfig):
            yield lit.Test.Test(testSuite, testname, localConfig, file_path=testname)

    def getTestsInDirectory(self, testSuite, path_in_suite,
                            litConfig, localConfig):
        return self.getTestsInExecutable(testSuite, path_in_suite, litConfig, localConfig)

    def execute(self, test, litConfig):
        testName = test.getFilePath()
        param_hlsl_data_dir = str.format('/p:HlslDataDir={}', self.hlsl_data_dir)
        cmd = [self.te, self.test_dll, '/inproc',
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

