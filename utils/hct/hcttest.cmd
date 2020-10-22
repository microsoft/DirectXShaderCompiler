@echo off
setlocal ENABLEDELAYEDEXPANSION

if "%BUILD_CONFIG%"=="" (
  set BUILD_CONFIG=Debug
)

rem Whether we built the project using ninja as the generator.
set GENERATOR_NINJA=0

set TEST_ALL=1
set TEST_CLANG=0
set TEST_CMD=0
set TEST_EXEC=0
set TEST_DXILCONV=0
set TEST_DXILCONV_FILTER=
set TEST_EXEC_FUTURE=0
set TEST_EXTRAS=0
set TEST_EXEC_REQUIRED=0
set TEST_CLANG_FILTER= /select: "@Priority<1"
set TEST_EXEC_FILTER=ExecutionTest::*
set LOG_FILTER=/logOutput:LowWithConsoleBuffering
set TEST_COMPAT_SUITE=0
set MANUAL_FILE_CHECK_PATH=
set TEST_MANUAL_FILE_CHECK=0
set SINGLE_FILE_CHECK_NAME=0
set CUSTOM_BIN_SET=

rem Begin SPIRV change
set TEST_SPIRV=0
set TEST_SPIRV_ONLY=0
rem End SPIRV change

set HCT_DIR=%~dp0

if "%NUMBER_OF_PROCESSORS%"=="" (
  set PARALLEL_OPTION=
) else if %NUMBER_OF_PROCESSORS% LEQ 1 (
  set PARALLEL_OPTION=
) else if %NUMBER_OF_PROCESSORS% LEQ 4 (
  set PARALLEL_OPTION=/parallel:%NUMBER_OF_PROCESSORS%
) else (
  rem Sweet spot for /parallel seems to be NUMBER_OF_PROCESSORS - 1
  set /a PARALLEL_COUNT=%NUMBER_OF_PROCESSORS%-1
  set PARALLEL_OPTION=/parallel:!PARALLEL_COUNT!
)

:opt_loop
if "%1"=="" (goto :done_opt)

if "%1"=="/?" goto :showhelp
if "%1"=="-?" goto :showhelp
if "%1"=="-h" goto :showhelp
if "%1"=="-help" goto :showhelp
if "%1"=="--help" goto :showhelp

rem Begin SPIRV change
if "%1"=="spirv" (
  set TEST_SPIRV=1
  shift /1
)

if "%1"=="spirv_only" (
  set TEST_SPIRV=1
  set TEST_SPIRV_ONLY=1
  shift /1
)
rem End SPIRV change

if "%1"=="-clean" (
  set TEST_CLEAN=1
) else if "%1"=="clean" (
  set TEST_CLEAN=1
  set TEST_CLEAN_ONLY=1
) else if "%1"=="none" (
  set TEST_ALL=0
) else if "%1"=="clang" (
  set TEST_ALL=0
  set TEST_CLANG=1
) else if "%1"=="clang-filter" (
  set TEST_ALL=0
  set TEST_CLANG=1
  set TEST_CLANG_FILTER= /name:%2
  shift /1
) else if "%1"=="file-check" (
  set TEST_ALL=0
  set TEST_MANUAL_FILE_CHECK=1
  set MANUAL_FILE_CHECK_PATH=%~2
  shift /1
) else if "%1"=="v" (
  set TEST_ALL=0
  set TEST_CLANG=1
  set TEST_CLANG_FILTER= /name:VerifierTest::*
) else if "%1"=="cmd" (
  set TEST_ALL=0
  set TEST_CMD=1
) else if "%1" == "dxilconv" (
  set TEST_ALL=0
  set TEST_DXILCONV=1
) else if "%1" == "dxilconv-filter" (
  set TEST_ALL=0
  set TEST_DXILCONV=1
  set TEST_DXILCONV_FILTER= /name:%2
  shift /1
) else if "%1"=="noexec" (
  set TEST_ALL=0
  set TEST_CLANG=1
  set TEST_CMD=1
  set TEST_DXILCONV=1
) else if "%1"=="exec" (
  rem If exec is explicitly supplied, hcttest will fail if machine is not configured
  rem to run execution tests, otherwise, execution tests would be skipped.
  set TEST_ALL=0
  set TEST_EXEC=1
  set TEST_EXEC_REQUIRED=1
) else if "%1"=="exec-filter" (
  set TEST_ALL=0
  set TEST_EXEC=1
  set TEST_EXEC_FILTER=ExecutionTest::%2
  set TEST_EXEC_REQUIRED=1
  shift /1
) else if "%1"=="exec-future" (
  set TEST_ALL=0
  set TEST_EXEC=1
  set TEST_EXEC_FUTURE=1
  set TEST_EXEC_REQUIRED=1
) else if "%1"=="exec-future-filter" (
  set TEST_ALL=0
  set TEST_EXEC=1
  set TEST_EXEC_FUTURE=1
  set TEST_EXEC_FILTER=ExecutionTest::%2
  set TEST_EXEC_REQUIRED=1
  shift /1
) else if "%1"=="extras" (
  set TEST_ALL=0
  set TEST_EXTRAS=1
) else if "%1"=="-ninja" (
  set GENERATOR_NINJA=1
) else if "%1"=="-rel" (
  set BUILD_CONFIG=Release
) else if /i "%1"=="-Release" (
  set BUILD_CONFIG=Release
) else if /i "%1"=="-Debug" (
  set BUILD_CONFIG=Debug
) else if "%1"=="-x86" (
  rem Allow BUILD_ARCH override.  This may be used by HCT_EXTRAS scripts.
  set BUILD_ARCH=Win32
) else if "%1"=="-x64" (
  set BUILD_ARCH=x64
) else if "%1"=="-arm" (
  set BUILD_ARCH=ARM
) else if "%1"=="-adapter" (
  set TEST_ADAPTER= /p:"Adapter=%~2"
  shift /1
) else if "%1"=="-verbose" (
  set LOG_FILTER=
  set PARALLEL_OPTION=
) else if "%1"=="-dxilconv-loc" (
  set DXILCONV_LOC=%~2
  shift /1
) else if "%1"=="-custom-bin-set" (
  set CUSTOM_BIN_SET=%~2
  shift /1
) else if "%1"=="-file-check-dump" (
  set ADDITIONAL_OPTS=%ADDITIONAL_OPTS% /p:"FileCheckDumpDir=%~2\HLSL"
  shift /1
) else if "%1"=="-dxil-loc" (
  set DXIL_DLL_LOC=%~2
  shift /1
) else if "%1"=="--" (
  shift /1
  goto :done_opt
) else (
  goto :done_opt
)
shift /1
goto :opt_loop
:done_opt

rem Collect additional arguments for tests
:collect_args
if "%1"=="" goto :done_args
set ADDITIONAL_OPTS=%ADDITIONAL_OPTS% %1
shift /1
goto :collect_args
:done_args

rem By default, run all clang tests and execution tests and dxilconv tests
if "%TEST_ALL%"=="1" (
  set TEST_CLANG=1
  set TEST_CMD=1
  set TEST_EXEC=1
  set TEST_EXTRAS=1
  set TEST_DXILCONV=1
)

where te.exe 1>nul 2>nul
if errorlevel 1 (
  echo Unable to find te.exe on path.
  exit /b 1
)

Rem For the Ninja generator, artifacts are not generated into a directory
Rem matching the current build configuration; instead, they are generated
Rem directly into bin/ under the build root directory.
if "%GENERATOR_NINJA%"=="1" (
  set BIN_DIR=%HLSL_BLD_DIR%\bin
  set TEST_DIR=%HLSL_BLD_DIR%\test
) else (
  set BIN_DIR=%HLSL_BLD_DIR%\%BUILD_CONFIG%\bin
  set TEST_DIR=%HLSL_BLD_DIR%\%BUILD_CONFIG%\test
)

if "%DXILCONV_LOC%"=="" ( 
  set DXILCONV_LOC=%BIN_DIR%
)
if "%TEST_DXILCONV%"=="1" (
  if not exist "%DXILCONV_LOC%\dxilconv.dll" (
    echo Skipping dxilconv tests, dxilconv.dll not found at %DXILCONV_LOC%.
    set TEST_DXILCONV=0
  )
)

if "%TEST_CLEAN%"=="1" (
  echo Cleaning %TEST_DIR% ...
  if exist %TEST_DIR%\. (
    rmdir /q /s %TEST_DIR%
  )
  if "%TEST_CLEAN_ONLY%"=="1" (
    echo exiting after deleting test directory. if clean and test is desired, use -clean option.
    exit /b 0
  )
)

if not exist %TEST_DIR% (mkdir %TEST_DIR%)

echo Copying binaries to test to %TEST_DIR%:
if "%CUSTOM_BIN_SET%"=="" (
  call %HCT_DIR%\hctcopy.cmd %BIN_DIR% %TEST_DIR% dxa.exe dxc.exe dxexp.exe dxopt.exe dxr.exe dxv.exe clang-hlsl-tests.dll dxcompiler.dll d3dcompiler_dxc_bridge.dll dxl.exe dxc_batch.exe dxlib_sample.dll
  if errorlevel 1 exit /b 1
  if "%TEST_DXILCONV%"=="1" (
    call %HCT_DIR%\hctcopy.cmd %BIN_DIR% %TEST_DIR% dxbc2dxil.exe dxilconv-tests.dll opt.exe
    call %HCT_DIR%\hctcopy.cmd %DXILCONV_LOC% %TEST_DIR% dxilconv.dll
  )
) else (
  call %HCT_DIR%\hctcopy.cmd %BIN_DIR% %TEST_DIR% %CUSTOM_BIN_SET%
  if errorlevel 1 exit /b 1
  if "%TEST_DXILCONV%"=="1" (
    call %HCT_DIR%\hctcopy.cmd %DXILCONV_LOC% %TEST_DIR% dxilconv.dll
  )
)
if errorlevel 1 exit /b 1

if not "%DXIL_DLL_LOC%"=="" (
  echo Copying DXIL.dll to %TEST_DIR%:
  call %HCT_DIR%\hctcopy.cmd %DXIL_DLL_LOC% %TEST_DIR% dxil.dll
  if errorlevel 1 exit /b 1
)

rem Begin SPIRV change
if "%TEST_SPIRV%"=="1" (
  if not exist %BIN_DIR%\clang-spirv-tests.exe (
    echo clang-spirv-tests.exe has not been built. Make sure you run "hctbuild -spirvtest" first.
    exit /b 1
  )
  echo Running SPIRV tests ...
  %BIN_DIR%\clang-spirv-tests.exe --spirv-test-root %HLSL_SRC_DIR%\tools\clang\test\CodeGenSPIRV
  if errorlevel 1 (
    echo Failure occured in SPIRV unit tests
    exit /b 1
  )
  if "%TEST_SPIRV_ONLY%"=="1" (
    exit /b 0
  )
)
rem End SPIRV change

echo Running HLSL tests ...

if exist "%HCT_EXTRAS%\hcttest-before.cmd" (
  call "%HCT_EXTRAS%\hcttest-before.cmd" %TEST_DIR%
  if errorlevel 1 (
    echo Fatal error, Failed command: "%HCT_EXTRAS%\hcttest-before.cmd" %TEST_DIR%
    exit /b 1
  )
)

if "%TEST_CLANG%"=="1" (
  echo Running Clang unit tests ...
  call :runte clang-hlsl-tests.dll /p:"HlslDataDir=%HLSL_SRC_DIR%\tools\clang\test\HLSL"%TEST_CLANG_FILTER%%ADDITIONAL_OPTS%
  set RES_CLANG=!ERRORLEVEL!
)

if "%TEST_CMD%"=="1" (
  copy /y %HLSL_SRC_DIR%\utils\hct\cmdtestfiles\smoke.hlsl %TEST_DIR%\smoke.hlsl
  call %HLSL_SRC_DIR%\utils\hct\hcttestcmds.cmd %TEST_DIR% %HLSL_SRC_DIR%\tools\clang\test\HLSL
  set RES_CMD=!ERRORLEVEL!
)

if "%TEST_EXEC%"=="1" (
  echo Sniffing for D3D12 configuration ...
  call :runte clang-hlsl-tests.dll /p:"HlslDataDir=%HLSL_SRC_DIR%\tools\clang\test\HLSL" /name:ExecutionTest::BasicTriangleTest /runIgnoredTests /p:"ExperimentalShaders=*" %TEST_ADAPTER%
  rem  /p:"ExperimentalShaders=*"
  set RES_EXEC=!ERRORLEVEL!
  if errorlevel 1 (
    if not "%TEST_EXEC_REQUIRED%"=="1" (
      echo Basic triangle test failed.
      echo Assuming this is an environmental limitation not a regression
      set TEST_EXEC=0
    ) else (
      echo Basic triangle test succeeded. Proceeding with execution tests.
    )
  )
)

if "%TEST_EXEC%"=="1" (
  if "%TEST_EXEC_FUTURE%"=="1" (
    call :runte clang-hlsl-tests.dll /p:"HlslDataDir=%HLSL_SRC_DIR%\tools\clang\test\HLSL" /select:"@Name='%TEST_EXEC_FILTER%' AND @Priority=2" /runIgnoredTests /p:"ExperimentalShaders=*" %TEST_ADAPTER% %ADDITIONAL_OPTS%
  ) else (
    call :runte clang-hlsl-tests.dll /p:"HlslDataDir=%HLSL_SRC_DIR%\tools\clang\test\HLSL" /select:"@Name='%TEST_EXEC_FILTER%' AND @Priority<2" /runIgnoredTests /p:"ExperimentalShaders=*" %TEST_ADAPTER% %ADDITIONAL_OPTS%
  )
  set RES_EXEC=!ERRORLEVEL!
)

if exist "%HCT_EXTRAS%\hcttest-extras.cmd" (
  if "%TEST_EXTRAS%"=="1" (
    echo Running extra tests ...
    call "%HCT_EXTRAS%\hcttest-extras.cmd" %TEST_DIR%
    set RES_EXTRAS=!ERRORLEVEL!
  )
)

if "%TEST_DXILCONV%"=="1" (
  call :runte dxilconv-tests.dll /p:"HlslDataDir=%HLSL_SRC_DIR%\projects\dxilconv\test" %TEST_DXILCONV_FILTER%
  set RES_DXILCONV=!ERRORLEVEL!
)


if exist "%HCT_EXTRAS%\hcttest-after.cmd" (
  call "%HCT_EXTRAS%\hcttest-after.cmd" %TEST_DIR%
  set RES_HCTTEST_AFTER=!ERRORLEVEL!
)

if "%TEST_MANUAL_FILE_CHECK%"=="1" (
  call :runte clang-hlsl-tests.dll /p:"HlslDataDir=%HLSL_SRC_DIR%\tools\clang\test\HLSL" /name:CompilerTest::ManualFileCheckTest /runIgnoredTests /p:"InputPath=%MANUAL_FILE_CHECK_PATH%"
  set RES_EXEC=!ERRORLEVEL!
)

echo.
echo ==================================
echo Unit test results:
set TESTS_PASSED=0
set TESTS_FAILED=0
call :check_result "clang tests" %RES_CLANG%
call :check_result "command line tests" %RES_CMD%
if "%TEST_EXEC%"=="1" (
  call :check_result "execution tests" %RES_EXEC%
)
call :check_result "hcttest-extras tests" %RES_EXTRAS%
call :check_result "hcttest-after script" %RES_HCTTEST_AFTER%
call :check_result "dxilconv tests" %RES_DXILCONV%

if not "%TESTS_PASSED%"=="0" (
  echo %TESTS_PASSED% succeeded.
) else if "%TESTS_FAILED%"=="0" (
  echo No Unit tests run.
)
if not "%TESTS_FAILED%"=="0" (
  echo %TESTS_FAILED% failed.
)
echo ==================================
exit /b %TESTS_FAILED%

:showhelp

echo Usage:
echo   hcttest [options] [target(s)] [-- additonal test arguments]
echo.
echo target can be empty or a specific subset.
echo.
echo If target if not specified, all tests will be run, but clang tests
echo will be limited by /select: "@Priority<1" by default.
echo You may specify 'clang-filter *' to run all clang tests.
echo Multiple targets may be specified to choose which stages to run.
echo.
echo options:
echo   -clean - deletes test directory before copying binaries and testing
echo   -ninja - artifacts were built using the Ninja generator
echo   -rel   - tests release rather than debug
echo   -adapter "adapter name" - overrides Adapter for execution tests
echo   -verbose - for TAEF: turns off /parallel and removes logging filter
echo   -custom-bin-set "file [file]..." - custom set of binaries to copy into test directory
echo   -dxilconv-loc "dxilconv.dll location" - fetch dxilconv.dll from custom location
echo   -dxil-loc "dxil.dll location" - fetch dxil.dll from provided location
echo   -file-check-dump "dump-path" - dump file-check inputs to files under dump-path
echo.
echo current BUILD_ARCH=%BUILD_ARCH%.  Override with:
echo   -x86 targets an x86 build (aka. Win32)
echo   -x64 targets an x64 build (aka. Win64)
echo   -arm targets an ARM build
echo.
echo target(s):
echo  clang         - run clang tests.
echo  file-check    - run file-check test on single file.
echo                - hcttest file-check "..\CodeGenHLSL\shader-compat-suite\lib_arg_flatten\lib_arg_flatten.hlsl"
echo  compat-suite  - run compat-suite test.
echo                - hcttest compat-suite "..\CodeGenHLSL\shader-compat-suite\lib_arg_flatten"
echo  cmd           - run command line tool tests.
echo  dxilconv      - run dxilconv tests
echo  v             - run the subset of clang tests that are verified-based.
echo  exec          - run execution tests.
echo  exec-future   - run execution tests for future releases.
echo  extras        - run hcttest-extras tests.
echo  noexec        - all except exec and extras tests.
echo.
echo Select clang or exec targets with filter by test name:
echo  clang-filter Name
echo  exec-filter Name
echo  exec-exp-filter Name
echo  dxilconv-filter Name
echo.
echo Use the HCT_EXTRAS environment variable to add hcttest-before and hcttest-after hooks.
echo.
echo Delete test directory and do not copy binaries or run tests:
echo   hcttest clean
echo.
call :showtesample clang-hlsl-tests.dll /p:"HlslDataDir=%HLSL_SRC_DIR%\tools\clang\test\HLSL"

goto :eof

:runte
rem Runs a unit test.
rem %1 - the name of the binary to run
rem %2 - first argument to te
rem %3 - second argument to te
rem %4 - third argument to te

echo te /labMode /miniDumpOnCrash /unicodeOutput:false /outputFolder:%TEST_DIR% %LOG_FILTER% %PARALLEL_OPTION% %TEST_DIR%\%*
call te /labMode /miniDumpOnCrash /unicodeOutput:false /outputFolder:%TEST_DIR% %LOG_FILTER% %PARALLEL_OPTION% %TEST_DIR%\%*

if errorlevel 1 (
  call :showtesample %*
  exit /b 1
)
goto :eof

:showtesample
rem %1 - name of binary to demo
rem %2 - first argument to te

if "%TEST_DIR%"=="" (
  set TEST_DIR=%HLSL_BLD_DIR%\%BUILD_CONFIG%\test
)

echo You can debug the test with the following command line.
echo start devenv /debugexe TE.exe /inproc %TEST_DIR%\%*
echo.
echo Use this te.exe for out-of-proc, or pick the correct one for the target arch, currently %BUILD_ARCH%.
where te.exe
echo.
echo Use /name:TestClass* or /name:TestClass::MethodName to filter and /breakOnError to catch the failure.
goto :eof

:check_result
if not "%2"=="" (
  if "%2"=="0" (
    echo [PASSED] %~1
    set /a TESTS_PASSED=%TESTS_PASSED%+1
  ) else (
    echo [FAILED] %~1
    set /a TESTS_FAILED=%TESTS_FAILED%+1
  )
)
goto :eof
