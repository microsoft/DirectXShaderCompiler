echo off

REM %1 - $(BuildPlatform)
REM %2 - $(Build.SourcesDirectory)
REM %3 - $(Build.BinariesDirectory)

set BUILD_PLATFORM=%1
set HLSL_SRC_DIR=%~f2
set HLSL_BLD_DIR=%~f3\%BUILD_PLATFORM%

rem Add Windows 10 SDK on PATH
set WIN10_SDK_PATH=%HLSL_SRC_DIR%\Packages\MS.Uwp.RS5RLS.Native.10.0.17763.1\l
set WIN10_SDK_VERSION=10.0.17763
set PATH=%WIN10_SDK_PATH%;%PATH%;

rem Add Python and VS CMake on PATH
set PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\Shared\Python36_64\
set PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\;
