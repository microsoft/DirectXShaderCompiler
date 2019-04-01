echo off

REM %1 - $(BuildPlatform)
REM %2 - $(Build.SourcesDirectory)
REM %3 - $(Build.BinariesDirectory)

echo Running pre_test_setup.cmd

call %~p0\common_setup.cmd %1 %2 %3

echo Build platform: %BUILD_PLATFORM%
echo HLSL build directory: %HLSL_BLD_DIR%

rem Add TAEF package on PATH
set TAEF_PATH=%HLSL_SRC_DIR%\Packages\Taef.Redist.10.33.181113003-develop\build\Binaries\Release\x64
set PATH=%PATH%;%TAEF_PATH%
echo TAEF path: %TAEF_PATH%