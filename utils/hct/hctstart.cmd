@echo off

if "%1"=="/?" goto :showhelp
if "%1"=="-?" goto :showhelp
if "%1"=="-help" goto :showhelp
if "%1"=="--help" goto :showhelp

rem Default build arch is x64
if "%BUILD_ARCH%"=="" (
  set BUILD_ARCH=x64
)

if "%1"=="-x86" (
  set BUILD_ARCH=Win32
) else if "%1"=="-Win32" (
  set BUILD_ARCH=Win32
) else if "%1"=="-x64" (
  set BUILD_ARCH=x64
) else if "%1"=="-amd64" (
  set BUILD_ARCH=x64
) else if "%1"=="-arm" (
  set BUILD_ARCH=ARM
) else if "%1"=="-arm64" (
  set BUILD_ARCH=ARM64
) else (
  goto :donearch
)
shift /1

:donearch 
echo Default architecture - set BUILD_ARCH=%BUILD_ARCH%
rem Set the following environment variable globally, or start Visual Studio
rem from this command line in order to use 64-bit tools.
set PreferredToolArchitecture=x64

if "%1"=="" (
  echo Source directory missing.
  goto :showhelp
)
if "%2"=="" (
  echo Build directory missing.
  goto :showhelp
)

if not exist "%~f1\utils\hct\hctstart.cmd" (
  echo %1 does not look like a directory with sources - cannot find %~f1\utils\hct\hctstart.cmd
  exit /b 1
)

set HLSL_SRC_DIR=%~f1
set HLSL_BLD_DIR=%~f2
echo HLSL source directory set to HLSL_SRC_DIR=%HLSL_SRC_DIR%
echo HLSL source directory set to HLSL_BLD_DIR=%HLSL_BLD_DIR%
echo.
echo You can recreate the environment with this command.
echo %0 %*
echo.

echo Setting up macros for this console - run hcthelp for a reference.
echo.
doskey hctbld=pushd %HLSL_BLD_DIR%
doskey hctbuild=%HLSL_SRC_DIR%\utils\hct\hctbuild.cmd $*
doskey hctcheckin=%HLSL_SRC_DIR%\utils\hct\hctcheckin.cmd $*
doskey hctclean=%HLSL_SRC_DIR%\utils\hct\hctclean.cmd $*
doskey hcthelp=%HLSL_SRC_DIR%\utils\hct\hcthelp.cmd $*
doskey hctshortcut=cscript.exe //Nologo %HLSL_SRC_DIR%\utils\hct\hctshortcut.js $*
doskey hctspeak=cscript.exe //Nologo %HLSL_SRC_DIR%\utils\hct\hctspeak.js $*
doskey hctsrc=pushd %HLSL_SRC_DIR%
doskey hcttest=%HLSL_SRC_DIR%\utils\hct\hcttest.cmd $*
doskey hcttools=pushd %HLSL_SRC_DIR%\utils\hct
doskey hcttodo=cscript.exe //Nologo %HLSL_SRC_DIR%\utils\hct\hcttodo.js $*
doskey hctvs=%HLSL_SRC_DIR%\utils\hct\hctvs.cmd $*

call :checksdk
if errorlevel 1 (
  echo Windows SDK not properly installed. Build enviornment could not be setup correctly.
  echo Please see the README.md instructions in the project root.
  exit /b 1
)

where cmake.exe 1>nul 2>nul
if errorlevel 1 (
  call :findcmake
)

call :checkcmake
if errorlevel 1 (
  echo WARNING: cmake version is not supported. Your build may fail.
)

where python.exe 1>nul 2>nul
if errorlevel 1 (
  call :findpython
)

where te.exe 1>nul 2>nul
if errorlevel 1 (
  call :findte
)

where git.exe 1>nul 2>nul
if errorlevel 1 (
  call :findgit
)

pushd %HLSL_SRC_DIR%

goto :eof

:showhelp 
echo hctstart - Start the HLSL console tools environment.
echo.
echo This script sets up the sources and binary environment variables
echo and installs convenience console aliases. See hcthelp for a reference.
echo.
echo Usage:
echo  hctstart [-x86 or -x64] [path-to-sources] [path-to-build]
echo.
goto :eof

:findcmake 
call :ifexistaddpath "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
if "%ERRORLEVEL%"=="0" (
  echo Path adjusted to include cmake from Visual Studio 2017 Community
  exit /b 0
)
call :ifexistaddpath "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
if "%ERRORLEVEL%"=="0" (
  echo Path adjusted to include cmake from Visual Studio 2017 Professional
  exit /b 0
)
call :ifexistaddpath "C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
if "%ERRORLEVEL%"=="0" (
  echo Path adjusted to include cmake from Visual Studio 2017 Enterprise
  exit /b 0
)
if errorlevel 1 if exist "%programfiles%\CMake\bin" set path=%path%;%programfiles%\CMake\bin
if errorlevel 1 if exist "%programfiles(x86)%\CMake\bin" set path=%path%;%programfiles(x86)%\CMake\bin
where cmake.exe 1>nul 2>nul
if errorlevel 1 (
  echo Unable to find cmake on path - you will have to add this before building.
  exit /b 1
)
echo Path adjusted to include cmake.
goto :eof

:findte 
if exist "%programfiles%\windows kits\10\Testing\Runtimes\TAEF\Te.exe" set path=%path%;%programfiles%\windows kits\10\Testing\Runtimes\TAEF
if exist "%programfiles(x86)%\windows kits\10\Testing\Runtimes\TAEF\Te.exe" set path=%path%;%programfiles(x86)%\windows kits\10\Testing\Runtimes\TAEF
if exist "%programfiles%\windows kits\8.1\Testing\Runtimes\TAEF\Te.exe" set path=%path%;%programfiles%\windows kits\8.1\Testing\Runtimes\TAEF
if exist "%programfiles(x86)%\windows kits\8.1\Testing\Runtimes\TAEF\Te.exe" set path=%path%;%programfiles(x86)%\windows kits\8.1\Testing\Runtimes\TAEF
if exist "%HLSL_SRC_DIR%\external\taef\build\Binaries\amd64\TE.exe" set path=%path%;%HLSL_SRC_DIR%\external\taef\build\Binaries\amd64
where te.exe 1>nul 2>nul
if errorlevel 1 (
  echo Unable to find TAEF te.exe on path - you will have to add this before running tests.
  echo WDK includes TAEF and is available from https://msdn.microsoft.com/en-us/windows/hardware/dn913721.aspx
  echo Alternatively, consider a project-local install by running %HLSL_SRC_DIR%\utils\hct\hctgettaef.py
  echo Please see the README.md instructions in the project root.
  exit /b 1
)
echo Path adjusted to include TAEF te.exe.

:ifexistaddpath 
rem If the argument exists, add to PATH and return 0, else 1. Useful to avoid parens in values without setlocal changes.
if exist %1 set PATH=%PATH%;%~1
if exist %1 exit /b 0
exit /b 1

:findgit 
if exist "C:\Program Files (x86)\Git\cmd\git.exe" set path=%path%;C:\Program Files (x86)\Git\cmd
if exist "C:\Program Files\Git\cmd\git.exe" set path=%path%;C:\Program Files\Git\cmd
if exist "%LOCALAPPDATA%\Programs\Git\cmd\git.exe" set path=%path%;%LOCALAPPDATA%\Programs\Git\cmd
where git 1>nul 2>nul
if errorlevel 1 (
  echo Unable to find git. Having git is convenient but not necessary to build and test.
)
echo Path adjusted to include git.
goto :eof

:findpython 
if exist C:\Python37\python.exe set path=%path%;C:\Python37
where python.exe 1>nul 2>nul
if errorlevel 1 (
  echo Unable to find python.
  exit /b 1
)
echo Path adjusted to include python.
goto :eof

:checksdk 
setlocal
set min_sdk_ver=14393

set REG_QUERY=REG QUERY "HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\Microsoft SDKs\Windows\v10.0"
set kit_root=
for /F "tokens=1,2*" %%A in ('%REG_QUERY% /v InstallationFolder') do (
  if "%%A"=="InstallationFolder" (
    rem echo Found Windows 10 SDK
    rem echo   InstallationFolder: "%%C"
    set kit_root=%%C
  )
)
if ""=="%kit_root%" (
    set kit_root=%WIN10_SDK_PATH%
)
if ""=="%kit_root%" (
  echo Did not find a Windows 10 SDK installation.
  exit /b 1
)
if not exist "%kit_root%" (
  echo Windows 10 SDK was installed but is not accessible.
  exit /b 1
)

set sdk_ver=
set d3d12_sdk_ver=
for /F "tokens=1-3" %%A in ('%REG_QUERY% /v ProductVersion') do (
  if "%%A"=="ProductVersion" (
    rem echo       ProductVersion: %%C
    for /F "tokens=1-3 delims=." %%X in ("%%C") do (
      set sdk_ver=%%Z
      if exist "%kit_root%\include\10.0.%%Z.0\um\d3d12.h" (
        set d3d12_sdk_ver=%%Z
      )
    )
  )
)
if ""=="%sdk_ver%" (
  set sdk_ver=%WIN10_SDK_VERSION%
)
if ""=="%sdk_ver%" (
  echo Could not detect Windows 10 SDK version.
  exit /b 1
)
if NOT %min_sdk_ver% LEQ %sdk_ver% (
  echo Found unsupported Windows 10 SDK version 10.0.%sdk_ver%.0 installed.
  echo Windows 10 SDK version 10.0.%min_sdk_ver%.0 or newer is required.
  exit /b 1
)

if ""=="%d3d12_sdk_ver%" (
  echo Windows 10 SDK version 10.0.%sdk_ver%.0 installed, but did not find d3d12.h.
  exit /b 1
) else (
  echo Found Windows 10 SDK 10.0.%d3d12_sdk_ver%.0
)
endlocal
goto :eof

:checkcmake 
cmake --version | findstr 3.4.3 1>nul 2>nul
if "0"=="%ERRORLEVEL%" exit /b 0
cmake --version | findstr 3.7.2 1>nul 2>nul
if "0"=="%ERRORLEVEL%" exit /b 0
cmake --version | findstr 3.9.0-MSVC 1>nul 2>nul
if "0"=="%ERRORLEVEL%" exit /b 0
cmake --version | findstr 3.11.2 1>nul 2>nul
if "0"=="%ERRORLEVEL%" exit /b 0
cmake --version | findstr /R 3.*MSVC 1>nul 2>nul
if errorlevel 1 (
  echo CMake 3.4.3, 3.7.2, 3.9.0 or 3.11.2 are the currently supported versions for VS 2015 and VS 2017 - your installed cmake is not supported.
  echo See README.md at the root for an explanation of dependencies.
  exit /b 1
)
goto :eof
