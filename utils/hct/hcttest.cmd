@echo off

setlocal ENABLEDELAYEDEXPANSION DISABLEDELAYEDEXPANSION

rem By default, run clang tests and execution tests.
rem Verifier tests also run because they are included in the general TEST_CLANG case.
set TEST_CLEAN=0
set TEST_CLANG=1
set TEST_EXEC=1
set TEST_CLANG_VERIF=0
set TEST_EXTRAS=1

if "%BUILD_CONFIG%"=="" (
  set BUILD_CONFIG=Debug
)

if "%1"=="clean" (
  set TEST_CLEAN=1
  set TEST_CLANG=0
  set TEST_EXEC=0
  set TEST_CLANG_VERIF=0
  set TEST_EXTRAS=0
  shift /1
)

if "%1"=="clang" (
  set TEST_EXEC=0
  set TEST_EXTRAS=0
  shift /1
)

if "%1"=="exec" (
  set TEST_CLANG=0
  set TEST_EXTRAS=0
  shift /1
)

if "%1"=="v" (
  set TEST_CLANG=0
  set TEST_EXEC=0
  set TEST_CLANG_VERIF=1
  set TEST_EXTRAS=0
  shift /1
)

if "%1"=="extras" (
  set TEST_CLANG=0
  set TEST_EXEC=0
  set TEST_EXTRAS=1
  shift /1
)

if "%1"=="none" (
  set TEST_CLANG=0
  set TEST_EXEC=0
  set TEST_EXTRAS=0
  shift /1
)

if "%1"=="-rel" (
  set BUILD_CONFIG=Release
  shift /1
)

rem Allow BUILD_ARCH override.  This may be used by HCT_EXTRAS scripts.
if "%1"=="-x86" (
  set BUILD_ARCH=Win32
) else if "%1"=="-x64" (
  set BUILD_ARCH=x64
) else if "%1"=="-arm" (
  set BUILD_ARCH=ARM
) else (
  goto :donearch
)
shift /1
:donearch

set TEST_DIR=%HLSL_BLD_DIR%\%BUILD_CONFIG%\test

if "%1"=="/?" goto :showhelp
if "%1"=="-?" goto :showhelp
if "%1"=="-help" goto :showhelp
if "%1"=="--help" goto :showhelp

where te.exe 1>nul 2>nul
if errorlevel 1 (
  echo Unable to find te.exe on path.
  exit /b 1
)

set TEST_DIR=%HLSL_BLD_DIR%\%BUILD_CONFIG%\test
if exist %TEST_DIR% (
  echo Cleaning %TEST_DIR% ...
  rmdir /q /s %TEST_DIR%
)
if not exist %TEST_DIR%\. (mkdir %TEST_DIR%)

if "%TEST_CLEAN%"=="1" (
  echo Cleaning %TEST_DIR% ...
  rmdir /q /s %TEST_DIR%
  exit /b 0
)

echo Copying binaries to test to %TEST_DIR%:
robocopy %HLSL_BLD_DIR%\%BUILD_CONFIG%\bin %TEST_DIR% *.exe *.dll

echo Running HLSL tests ...

if exist "%HCT_EXTRAS%\hcttest-before.cmd" (
  call "%HCT_EXTRAS%\hcttest-before.cmd" %TEST_DIR%
  if errorlevel 1 (
    echo Failed command: "%HCT_EXTRAS%\hcttest-before.cmd" %TEST_DIR%
    exit /b 1
  )
)

if "%TEST_CLANG%"=="1" (
  echo Running Clang unit tests ...
  call :runte clang-hlsl-tests.dll /p:"HlslDataDir=%HLSL_SRC_DIR%\tools\clang\test\HLSL" /select: "@Priority<1"
  if errorlevel 1 (
    exit /b 1
  )

  copy /y %HLSL_SRC_DIR%\utils\hct\smoke.hlsl %TEST_DIR%\smoke.hlsl
  call %HLSL_SRC_DIR%\utils\hct\hcttestcmds.cmd %TEST_DIR% %HLSL_SRC_DIR%\tools\clang\test\HLSL
  if errorlevel 1 (
    echo Failed - %HLSL_SRC_DIR%\utils\hct\hcttestcmds.cmd %TEST_DIR% %HLSL_SRC_DIR%\tools\clang\test\HLSL
    exit /b 1
  )
)

if "%TEST_EXEC%"=="1" (
  echo Sniffing for D3D12 configuration ...
  call :runte clang-hlsl-tests.dll /p:"HlslDataDir=%HLSL_SRC_DIR%\tools\clang\test\HLSL" /name:ExecutionTest::BasicTriangleTest /runIgnoredTests /p:"ExperimentalShaders=*"
  if errorlevel 1 (
    echo Basic triangle test failed.
    echo Assuming this is an environmental limitation not a regression
  ) else (
    echo Basic triangle test succeeded. Proceeding with execution tests.
    call :runte clang-hlsl-tests.dll /p:"HlslDataDir=%HLSL_SRC_DIR%\tools\clang\test\HLSL" /name:ExecutionTest::* /runIgnoredTests /p:"ExperimentalShaders=*"
    if errorlevel 1 (
      exit /b 1
    )
  )
)

if "%TEST_CLANG_VERIF%"=="1" (
  echo Running verifier-based tests ...
  call :runte clang-hlsl-tests.dll /p:"HlslDataDir=%HLSL_SRC_DIR%\tools\clang\test\HLSL" /name:VerifierTest::*
  if errorlevel 1 (
    exit /b 1
  )
)

if exist "%HCT_EXTRAS%\hcttest-extras.cmd" (
  if "%TEST_EXTRAS%"=="1" (
    echo Running extra tests ...
    call "%HCT_EXTRAS%\hcttest-extras.cmd" %TEST_DIR%
    if errorlevel 1 (
      echo Failed command: "%HCT_EXTRAS%\hcttest-extras.cmd" %TEST_DIR%
      exit /b 1
    )
  )
)

if exist "%HCT_EXTRAS%\hcttest-after.cmd" (
  call "%HCT_EXTRAS%\hcttest-after.cmd" %TEST_DIR%
  if errorlevel 1 (
    echo Failed command: "%HCT_EXTRAS%\hcttest-after.cmd" %TEST_DIR%
    exit /b 1
  )
)

echo Unit tests succeeded.

exit /b 0

:showhelp

echo Usage:
echo   hcttest [-rel] [-arm or -x86 or -x64] [target]
echo.
echo target can be empty or a specific subset.
echo.
echo If target if not specified, all tests will be run.
echo.
echo 'clang' will only run clang tests.
echo 'exec' will only run execution tests.
echo 'v' will run the clang tests that are verified-based.
echo.
echo   -rel builds release rather than debug
echo.
echo current BUILD_ARCH=%BUILD_ARCH%.  Override with:
echo   -x86 targets an x86 build (aka. Win32)
echo   -x64 targets an x64 build (aka. Win64)
echo   -arm targets an ARM build
echo.
echo Use the HCT_EXTRAS environment variable to add hcttest-before and hcttest-after hooks.
echo.
call :showtesample clang-hlsl-tests.dll /p:"HlslDataDir=%HLSL_SRC_DIR%\tools\clang\test\HLSL"

goto :eof

:runte
rem Runs a unit test.
rem %1 - the name of the binary to run
rem %2 - first argument to te
rem %3 - second argument to te
rem %4 - third argument to te

call te %TEST_DIR%\%1 %2 %3 %4 %5 %6 /labMode /miniDumpOnCrash
if errorlevel 1 (
  call :showtesample %1 %2 %3 %4 %5 %6
  exit /b 1
)
goto :eof

:showtesample
rem %1 - name of binary to demo
rem %2 - first argument to te

echo You can debug the test with the following command line.
echo start devenv /debugexe TE.exe %TEST_DIR%\%1 /inproc %2 %3 %4 %5 %6
echo.
echo Use this te.exe for out-of-proc, or pick the correct one for the target arch, currently x86.
where te.exe
echo.
echo Use /name:TestClass* or /name:TestClass::MethodName to filter.
goto :eof

