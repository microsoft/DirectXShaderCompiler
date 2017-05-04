@echo off

if "%1"=="" (
  echo First argument to hcttestcmds should be directory with command-line tools.
  exit /b 1
)

if "%2"=="" (
  echo Second argument to hcttestcmds should be the absolute path to tools\clang\test\HLSL
  exit /b 1
)

echo Testing command line programs at %1 ...

setlocal

pushd %1

echo Smoke test for dxr command line program ...
dxr.exe -remove-unused-globals smoke.hlsl -Emain 1> nul
if %errorlevel% neq  0 (
  echo Failed - %CD%\dxr.exe -remove-unused-globals %CD%\smoke.hlsl -Emain
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /Fc smoke.hlsl.c 1>nul
if %errorlevel% neq 0 (
  echo Failed - %CD%\dxc.exe /T ps_6_0 smoke.hlsl /Fc %CD%\smoke.hlsl.c
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /Zi /Fd smoke.hlsl.d 1>nul
if %errorlevel% neq 0 (
  echo Failed - %CD%\dxc.exe /T ps_6_0 smoke.hlsl /Zi /Fd %CD%\smoke.hlsl.d
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /Zi /Fd %CD%\ /Fo smoke.hlsl.strip 1>nul
if %errorlevel% neq 0 (
  echo Failed - %CD%\dxc.exe /T ps_6_0 smoke.hlsl /Zi /Fd %CD%\
  call :cleanup 2>nul
  exit /b 1
)
rem .lld file should be produced
dir %CD%\*.lld 1>nul
if %errorlevel% neq 0 (
  echo Failed to find some .lld file at %CD%
  call :cleanup 2>nul
  exit /b 1
)
rem /Fd with trailing backslash implies /Qstrip_debug
dxc.exe -dumpbin smoke.hlsl.strip | findstr "shader debug name" 1>nul
if %errorlevel% neq 0 (
  echo Failed to find shader debug name.
  call :cleanup 2>nul
  exit /b 1
)
dxc.exe -dumpbin smoke.hlsl.strip | findstr "DICompileUnit" 1>nul
if %errorlevel% equ 0 (
  echo Found DICompileUnit after implied strip.
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /Fe smoke.hlsl.e 1>nul
if %errorlevel% neq 0 (
  echo Failed - %CD%\dxc.exe /T ps_6_0 smoke.hlsl /Fe %CD%\smoke.hlsl.e
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /ast-dump 1>nul
if %errorlevel% neq 0 (
  echo Failed - %CD%\dxc.exe /T ps_6_0 smoke.hlsl /ast-dump
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /Dcheck_warning 1>nul 2>smoke.warning.txt
if %errorlevel% neq 0 (
  echo Failed - %CD%\dxc.exe /T ps_6_0 smoke.hlsl /Dcheck_warning
  call :cleanup 2>nul
  exit /b 1
)

findstr warning: %CD%\smoke.warning.txt 1>nul
if %errorlevel% neq 0 (
  echo Failed to get warning message from command %CD%\dxc.exe /T ps_6_0 smoke.hlsl /Dcheck_warning
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /Dcheck_warning /no-warnings 1>nul 2>smoke.no.warning.txt
if %errorlevel% neq 0 (
  echo Failed - %CD%\dxc.exe /T ps_6_0 smoke.hlsl /Dcheck_warning /no-warnings
  call :cleanup 2>nul
  exit /b 1
)

findstr warning: %CD%\smoke.no.warning.txt 1>nul
if %errorlevel% equ 0 (
  echo no-warning option failed : %CD%\dxc.exe /T ps_6_0 smoke.hlsl /Dcheck_warning /no-warnings
  call :cleanup 2>nul
  exit /b 1
)


echo Smoke test for dxc command line program ...
dxc.exe /T ps_6_0 smoke.hlsl /Fh smoke.hlsl.h /Vn g_myvar 1> nul
if %errorlevel% neq 0 (
  echo Failed - %CD%\dxc.exe /T ps_6_0 %CD%\smoke.hlsl /Fh %CD%\smoke.hlsl.h /Vn g_myvar
  call :cleanup 2>nul
  exit /b 1
)
findstr g_myvar %CD%\smoke.hlsl.h 1>nul
if %errorlevel% neq 0 (
  echo Failed to find the variable g_myvar in %CD%\smoke.hlsl.h
  echo Debug with start devenv /debugexe %CD%\dxc.exe /T ps_6_0 %CD%\smoke.hlsl /Fh %CD%\smoke.hlsl.h /Vn g_myvar
  call :cleanup 2>nul
  exit /b 1
)
findstr "0x44, 0x58" %CD%\smoke.hlsl.h 1>nul
if %errorlevel% neq 0 (
  echo Failed to find the bytecode for DXBC container in %CD%\smoke.hlsl.h
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe smoke.hlsl /P preprocessed.hlsl 1>nul
if %errorlevel% neq 0 (
  echo Failed to preprocess smoke.hlsl
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl -force_rootsig_ver rootsig_1_0 1>nul
if %errorlevel% neq 0 (
  echo Failed to compile with forcing rootsignature rootsig_1_0
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl -force_rootsig_ver rootsig_1_1 1>nul
if %errorlevel% neq 0 (
  echo Failed to compile with forcing rootsignature rootsig_1_1
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl -force_rootsig_ver rootsig_2_0 2>nul
if %errorlevel% equ 0 (
  echo rootsig_2_0 is not supported but compilation passed
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /HV 2016 1>nul
if %errorlevel% neq 0 (
  echo Failed to compile with HLSL version 2016
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /HV 2015 2>nul
if %errorlevel% equ 0 (
  echo Unsupported HLSL version 2015 should fail but did not fail
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /Zi /Fo smoke.cso 1> nul
if %errorlevel% neq 0 (
  echo Failed to compile to binary object from %CD%\smoke.hlsl
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe /T ps_6_0 smoke.hlsl /Zi /Fo smoke.cso /Cc /Ni /No /Lx 1> nul
if %errorlevel% neq 0 (
  echo Failed to compile to binary object from %CD%\smoke.hlsl with disassembly options
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe -dumpbin smoke.cso 1> nul
if %errorlevel% neq 0 (
  echo Failed to disassemble binary object from %CD%\smoke.hlsl
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe smoke.cso /recompile 1>nul
if %errorlevel% neq 0 (
  echo Failed to recompile binary object compiled from %CD%\smoke.hlsl
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe smoke.cso /recompile /T ps_6_0 /E main 1>nul
if %errorlevel% neq 0 (
  echo Failed to recompile binary object with target ps_6_0 from %CD%\smoke.hlsl
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe smoke.hlsl /D "semantic = SV_Position" /T vs_6_0 /Zi /DDX12 /Fo smoke.cso 1> nul
if %errorlevel% neq 0 (
  echo Failed to compile smoke.hlsl with command line defines
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe smoke.cso /recompile 1> nul
if %errorlevel% neq 0 (
  echo Failed to recompile smoke.cso with command line defines
  call :cleanup 2>nul
  exit /b 1
)


dxc.exe smoke.cso /dumpbin /Qstrip_debug /Fo nodebug.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to strip debug part from DXIL container blob
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe smoke.cso /dumpbin /Qstrip_rootsignature /Fo norootsignature.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to strip rootsignature from DXIL container blob
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe smoke.cso /dumpbin /extractrootsignature /Fo rootsig.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to extract rootsignature from DXIL container blob
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe norootsignature.cso /dumpbin /setrootsignature rootsig.cso /Fo smoke.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to setrootsignature to DXIL conatiner with no root signature
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe "%2"\..\CodeGenHLSL\NonUniform.hlsl /T ps_6_0 /DDX12 /Fo NonUniform.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to compile NonUniform.hlsl
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe NonUniform.cso /dumpbin /Qstrip_rootsignature /Fo NonUniformNoRootSig.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to strip rootsignature from DXIL container blob for NonUniform.cso
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe NonUniform.cso /dumpbin /extractrootsignature /Fo NonUniformRootSig.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to extract rootsignature from DXIL container blob for NonUniform.cso
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe smoke.cso /dumpbin /verifyrootsignature rootsig.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to verify root signature for somke.cso
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe norootsignature.cso /dumpbin /verifyrootsignature rootsig.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to verify root signature for smoke.cso without root signature
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe NonUniform.cso /dumpbin /verifyrootsignature NonUniformRootSig.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to verify root signature for NonUniform.cso
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe NonUniformNoRootSig.cso /dumpbin /verifyrootsignature NonUniformRootSig.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to verify root signature for somke1.cso without root signature
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe NonUniformNoRootSig.cso /dumpbin /verifyrootsignature rootsig.cso 2>nul
if %errorlevel% equ 0 (
  echo Verifying invalid root signature for NonUniformNoRootSig.cso should fail but passed
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe norootsignature.cso /dumpbin /verifyrootsignature NonUniformRootSig.cso 2>nul
if %errorlevel% equ 0 (
  echo Verifying invalid root signature for norootsignature.cso should fail but passed
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe smoke.cso /dumpbin /setrootsignature rootsig.cso /Fo smoke.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to setrootsignature to DXIL container that already contains root signature
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe smoke.cso /dumpbin /setrootsignature NonUniformRootSig.cso /Fo smoke.cso 2>nul
if %errorlevel% equ 0 (
  echo setrootsignature of invalid root signature should fail but passed
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe %2\..\CodeGenHLSL\Samples\MiniEngine\TextVS.hlsl /Tvs_6_0 /Zi /Fo TextVS.cso 1>nul
if %errorlevel% neq 0 (
  echo failed to compile %2\..\CodeGenHLSL\Samples\MiniEngine\TextVS.hlsl
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe smoke.cso /dumpbin /verifyrootsignature TextVS.cso 1>nul
if %errorlevel% neq 0 ( 
  echo Verifying valid replacement of root signature failed
  call :cleanup 2>nul
  exit /b 1
)

echo private data > private.txt
dxc.exe smoke.cso /dumpbin /setprivate private.txt /Fo private.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to set private data to DXIL container with no private data
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe private.cso /dumpbin /setprivate private.txt /Fo private.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to set private data to DXIL container that already contains private data
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe private.cso /dumpbin /Qstrip_priv /Fo noprivate.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to strip private data from DXIL container blob
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe private.cso /dumpbin /getprivate private1.txt 1>nul
if %errorlevel% neq 0 (
  echo Failed to get private data from DXIL container blob
  call :cleanup 2>nul
  exit /b 1
)

findstr "private data" %CD%\private1.txt 1>nul
if %errorlevel% neq 0 (
  echo Failed to get private data content from DXIL container blob
  call :cleanup 2>nul
  exit /b 1
)

FC smoke.cso noprivate.cso 1>nul
if %errorlevel% neq 0 (
  echo Appending and removing blob roundtrip failed.
  call :cleanup 2>nul
  exit /b 1
)

dxc.exe private.cso /dumpbin /Qstrip_priv /Qstrip_debug /Qstrip_rootsignature /Fo noprivdebugroot.cso 1>nul
if %errorlevel% neq 0 (
  echo Failed to extract multiple parts from DXIL container blob
  call :cleanup 2>nul
  exit /b 1
)

echo Smoke test for dxc.exe shader model upgrade...
dxc.exe /T ps_5_0 smoke.hlsl 1> nul
if %errorlevel% neq 0 (
  echo Failed shader model upgrade test - %CD%\dxc.exe /T ps_5_0 %CD%\smoke.hlsl
  call :cleanup 2>nul
  exit /b 1
)

echo Smoke test for dxa command line program ...
dxa.exe smoke.cso -listfiles 1> nul
if %errorlevel% neq 0 (
  echo Failed to list files from blob 
  call :cleanup 2>nul
  exit /b 1
)

dxa.exe smoke.cso -listparts 1> nul
if %errorlevel% neq 0 (
  echo Failed to list parts from blob
  call :cleanup 2>nul
  exit /b 1
)

dxa.exe smoke.cso -extractpart dbgmodule -o smoke.cso.ll 1>nul
if %errorlevel% neq 0 (
  echo Failed to extract DXIL part from the blob generated by %CD%\smoke.hlsl
  call :cleanup 2>nul
  exit /b 1
)

dxa.exe smoke.cso.ll -listfiles 1> nul
if %errorlevel% neq 0 (
  echo Failed to list files from Dxil part with Dxil with Debug Info
  call :cleanup 2>nul
  exit /b 1
)

dxa.exe smoke.cso.ll -extractfile * 1> nul
if %errorlevel% neq 0 (
  echo Failed to extract files from Dxil part with Dxil with Debug Info
  call :cleanup 2>nul
  exit /b 1
)

dxa.exe smoke.cso -extractpart module -o smoke.cso.plain.ll 1>nul
if %errorlevel% neq 0 (
  echo Failed to extract plain module via dxa.exe smoke.cso -extractpart module -o smoke.cso.plain.ll
  call :cleanup 2>nul
  exit /b 1
)

echo Smoke test for debug info extraction.
dxc.exe smoke.hlsl

call :cleanup
exit /b 0

:cleanup
del %CD%\preprocessed.hlsl
del %CD%\smoke.hlsl.c
del %CD%\smoke.hlsl.d
del %CD%\smoke.hlsl.e
del %CD%\smoke.hlsl.h
del %CD%\smoke.hlsl.strip
del %CD%\smoke.cso
del %CD%\NonUniform.cso
del %CD%\private.cso
del %CD%\private.txt
del %CD%\private1.txt
del %CD%\noprivate.cso
del %CD%\nodebug.cso
del %CD%\rootsig.cso
del %CD%\NonUniformRootSig.cso
del %CD%\noprivdebugroot.cso
del %CD%\norootsignature.cso
del %CD%\NonUniformNoRootSig.cso
del %CD%\TextVS.cso
del %CD%\smoke.cso.plain.ll
del %CD%\smoke.cso.ll
del %CD%\*.lld

exit /b 0

