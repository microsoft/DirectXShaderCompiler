@echo off

if "%1"=="/?" goto :showhelp
if "%1"=="-?" goto :showhelp
if "%1"=="-help" goto :showhelp
if "%1"=="--help" goto :showhelp

if "%HLSL_SRC_DIR%"=="" (
  echo Missing source directory - consider running hctstart.
  exit /b 1
)

setlocal ENABLEDELAYEDEXPANSION

echo Running hctcheckin for the current architecture.

call %HLSL_SRC_DIR%\utils\hct\hctclean.cmd
if errorlevel 1 (
  echo Failed to clean binaries, stopping hctcheckin.
  exit /b 1
)
call %HLSL_SRC_DIR%\utils\hct\hctbuild.cmd -parallel
if errorlevel 1 (
  echo Failed to build binaries, stopping hctcheckin.
  exit /b 1
)

call %HLSL_SRC_DIR%\utils\hct\hcttest.cmd
if errorlevel 1 (
  echo Failed to test binaries, stopping hctcheckin.
  exit /b 1
)

echo Ready to check in.
cscript //Nologo %HLSL_SRC_DIR%\utils\hct\hctspeak.js /say:"checkin tasks successfully completed"

endlocal

goto :eof

:showhelp
echo Runs the clean, build and test steps.
echo.
echo hctcheckin
echo.
goto :eof
