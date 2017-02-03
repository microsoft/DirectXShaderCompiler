@echo off

if "%1"=="" (
  echo First argument to hcttestcmds should be directory with command-line tools.
  exit /b 1
)

echo Testing command line programs at %1 ...

setlocal

pushd %1

echo Smoke test for dxr command line program ...
dxr.exe -remove-unused-globals smoke.hlsl -Emain 1> nul
if %errorlevel% neq  0 (
  echo Failed - %CD%\dxr.exe -remove-unused-globals %CD%\smoke.hlsl -Emain
  exit /b 1
)


dxc.exe /T ps_6_0 smoke.hlsl /Fc smoke.hlsl.c 1>nul
if %errorlevel% neq 0 (
  echo Failed - %CD%\dxc.exe /T ps_6_0 smoke.hlsl /Fc %CD%\smoke.hlsl.c
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /Zi /Fd smoke.hlsl.d 2>nul
if %errorlevel% neq 0 (
  echo Failed - %CD%\dxc.exe /T ps_6_0 smoke.hlsl /Zi /Fd %CD%\smoke.hlsl.d
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /Fe smoke.hlsl.e 1>nul
if %errorlevel% neq 0 (
  echo Failed - %CD%\dxc.exe /T ps_6_0 smoke.hlsl /Fe %CD%\smoke.hlsl.e
  exit /b 1
)

echo Smoke test for dxc command line program ...
dxc.exe /T ps_6_0 smoke.hlsl /Fh smoke.hlsl.h /Vn g_myvar 1> nul
if %errorlevel% neq 0 (
  echo Failed - %CD%\dxc.exe /T ps_6_0 %CD%\smoke.hlsl /Fh %CD%\smoke.hlsl.h /Vn g_myvar
  exit /b 1
)
findstr g_myvar %CD%\smoke.hlsl.h 1>nul
if %errorlevel% neq 0 (
  echo Failed to find the variable g_myvar in %CD%\smoke.hlsl.h
  echo Debug with start devenv /debugexe %CD%\dxc.exe /T ps_6_0 %CD%\smoke.hlsl /Fh %CD%\smoke.hlsl.h /Vn g_myvar
  exit /b 1
)
findstr "0x44, 0x58" %CD%\smoke.hlsl.h 1>nul
if %errorlevel% neq 0 (
  echo Failed to find the bytecode for DXBC container in %CD%\smoke.hlsl.h
  exit /b 1
)

dxc.exe smoke.hlsl /P preprocessed.hlsl 1>nul
if %errorlevel% neq 0 (
  echo Failed to preprocess smoke.hlsl
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl -force_rootsig_ver rootsig_1_0 1>nul
if %errorlevel% neq 0 (
  echo Failed to compile with forcing rootsignature rootsig_1_0
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl -force_rootsig_ver rootsig_1_1 1>nul
if %errorlevel% neq 0 (
  echo Failed to compile with forcing rootsignature rootsig_1_1
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl -force_rootsig_ver rootsig_2_0 2>nul
if %errorlevel% equ 0 (
  echo rootsig_2_0 is not supported but compilation passed
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /HV 2016 1>nul
if %errorlevel% neq 0 (
  echo Failed to compile with HLSL version 2016
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /HV 2015 2>nul
if %errorlevel% equ 0 (
  echo Unsupported HLSL version 2015 should fail but did not fail
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /Zi /Fo smoke.cso 1> nul
if %errorlevel% neq 0 (
  echo Failed to compile to binary object from %CD%\smoke.hlsl
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /Zi /Fo smoke.cso /Cc /Ni /No /Lx 1> nul
if %errorlevel% neq 0 (
  echo Failed to compile to binary object from %CD%\smoke.hlsl with disassembly options
  exit /b 1
)

dxc.exe -dumpbin smoke.cso 1> nul
if %errorlevel% neq 0 (
  echo Failed to disassemble binary object from %CD%\smoke.hlsl
  exit /b 1
)

dxc.exe smoke.cso /recompile 1>nul
if %errorlevel% neq 0 (
  echo Failed to recompile binary object compiled from %CD%\smoke.hlsl
  exit /b 1
)

dxc.exe smoke.cso /recompile /T ps_6_0 /E main 1>nul
if %errorlevel% neq 0 (
  echo Failed to recompile binary object with target ps_6_0 from %CD%\smoke.hlsl
  exit /b 1
)

dxc.exe smoke.hlsl /D "semantic = SV_Position" /T vs_6_0 /Zi /DDX12 /Fo smoke.cso 1> nul
if %errorlevel% neq 0 (
  echo Failed to compile smoke.hlsl with command line defines
  exit /b 1
)

dxc.exe smoke.cso /recompile 1> nul
if %errorlevel% neq 0 (
  echo Failed to recompile smoke.cso with command line defines
  exit /b 1
)


dxc.exe smoke.cso /dumpbin /Qstrip_debug /Fo nodebug.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to strip debug part from DXIL container blob
  exit /b 1
)

dxc.exe smoke.cso /dumpbin /Qstrip_rootsignature /Fo norootsignature.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to strip rootsignature from DXIL container blob
  exit /b 1
)

dxc.exe smoke.cso /dumpbin /extractrootsignature /Fo rootsig.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to extract rootsignature from DXIL container blob
  exit /b 1
)

dxc.exe norootsignature.cso /dumpbin /setrootsignature rootsig.cso /Fo smoke.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to setrootsignature to DXIL conatiner with no root signature
  exit /b 1
)

dxc.exe smoke.cso /dumpbin /setrootsignature rootsig.cso /Fo smoke.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to setrootsignature to DXIL container that already contains root signature
)

echo private data > private.txt
dxc.exe smoke.cso /dumpbin /setprivate private.txt /Fo private.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to set private data to DXIL container with no private data
  exit /b 1
)

dxc.exe private.cso /dumpbin /setprivate private.txt /Fo private.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to set private data to DXIL container that already contains private data
)

dxc.exe private.cso /dumpbin /Qstrip_priv /Fo noprivate.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to strip private data from DXIL container blob
  exit /b 1
)

dxc.exe private.cso /dumpbin /getprivate private1.txt 1>nul
if %errorlevel% neq 0 (
  echo Failed to get private data from DXIL container blob
  exit /b 1
)

findstr "private data" %CD%\private1.txt 1>nul
if %errorlevel% neq 0 (
  echo Failed to get private data content from DXIL container blob
  exit /b 1
)

FC smoke.cso noprivate.cso 1>nul
if %errorlevel% neq 0 (
  echo Appending and removing blob roundtrip failed.
  exit /b 1
)

dxc.exe private.cso /dumpbin /Qstrip_priv /Qstrip_debug /Qstrip_rootsignature /Fo noprivdebugroot.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to extract multiple parts from DXIL container blob
  exit /b 1
)

echo Smoke test for dxc.exe shader model upgrade...
dxc.exe /T ps_5_0 smoke.hlsl 1> nul
if %errorlevel% neq 0 (
  echo Failed shader model upgrade test - %CD%\dxc.exe /T ps_5_0 %CD%\smoke.hlsl
  exit /b 1
)

echo Smoke test for dxa command line program ...
dxa.exe smoke.cso -listfiles 1> nul
if %errorlevel% neq 0 (
  echo Failed to list files from blob 
  exit /b 1
)

dxa.exe smoke.cso -listparts 1> nul
if %errorlevel% neq 0 (
  echo Failed to list parts from blob
  exit /b 1
)

dxa.exe smoke.cso -extractpart dbgmodule -o smoke.cso.ll 1>nul
if %errorlevel% neq 0 (
  echo Failed to extract DXIL part from the blob generated by %CD%\smoke.hlsl
  exit /b 1
)

dxa.exe smoke.cso.ll -listfiles 1> nul
if %errorlevel% neq 0 (
  echo Failed to list files from Dxil part with Dxil with Debug Info
  exit /b 1
)

dxa.exe smoke.cso.ll -extractfile * 1> nul
if %errorlevel% neq 0 (
  echo Failed to extract files from Dxil part with Dxil with Debug Info
  exit /b 1
)

echo Smoke test for dxopt ...
dxopt.exe -passes | findstr inline 1>nul
if %errorlevel% neq 0 (
  echo Failed to find the inline pass via dxopt -passes
  exit /b 1
)
dxa.exe smoke.cso -extractpart module -o smoke.cso.plain.ll 1>nul
if %errorlevel% neq 0 (
  echo Failed to extract plain module via dxa.exe smoke.cso -extractpart module -o smoke.cso.plain.ll
  exit /b 1
)
dxopt.exe smoke.cso.plain.ll -o=smoke.cso.inlined.ll -inline
if %errorlevel% neq 0 (
  echo Failed to run an inline pass via dxopt.exe smoke.cso.ll -o=smoke.cso.inlined.ll -inline
  exit /b 1
)
dxc.exe -dumpbin smoke.cso.inlined.ll 1>nul
if %errorlevel% neq 0 (
  echo Failed to dump the inlined file.
  exit /b 1
)

rem Clean up.
del %CD%\preprocessed.hlsl
del %CD%\smoke.hlsl.c
del %CD%\smoke.hlsl.d
del %CD%\smoke.hlsl.e
del %CD%\smoke.hlsl.h
del %CD%\smoke.cso
del %CD%\private.cso
del %CD%\private.txt
del %CD%\private1.txt
del %CD%\noprivate.cso
del %CD%\nodebug.cso
del %CD%\noprivdebugroot.cso
del %CD%\norootsignature.cso
del %CD%\smoke.cso.inlined.ll
del %CD%\smoke.cso.plain.ll

exit /b 0
