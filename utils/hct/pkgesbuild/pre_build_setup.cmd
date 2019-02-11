echo off

REM %1 - $(BuildPlatform)
REM %2 - $(Build.SourcesDirectory)
REM %3 - $(Build.BinariesDirectory)

echo Running pre_build_setup.cmd

call %~p0\common_setup.cmd %1 %2 %3

echo Build platform: %BUILD_PLATFORM%
echo HLSL source directory: %HLSL_SRC_DIR%
echo HLSL build directory: %HLSL_BLD_DIR%
echo SDK path: %WIN10_SDK_PATH%