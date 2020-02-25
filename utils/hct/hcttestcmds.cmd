@echo off

if "%1"=="" (
  echo First argument to hcttestcmds should be directory with command-line tools.
  exit /b 1
)

echo Testing command line programs at %1 ...

setlocal

set script_dir=%~dp0
set testfiles=%script_dir%cmdtestfiles
set Failed=0
set FailingCmdWritten=0
set OutputLog=%1\testcmd.log
set LogOutput=1

pushd %1

set testname=Basic Rewriter Smoke Test
call :run dxr.exe -remove-unused-globals "%testfiles%\smoke.hlsl" -Emain
call :check_file log find-not g_unused
if %Failed% neq 0 goto :failed


echo Smoke test for dxc command line program ...

set testname=Basic DXC Smoke Test
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" /Fc smoke.hlsl.h
call :check_file smoke.hlsl.h find "define void @main()" del
if %Failed% neq 0 goto :failed

set testname=Test extra DXC outputs together
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" /DDX12 /Dcheck_warning /Fh smoke.hlsl.h /Vn g_myvar /Fc smoke.ll /Fo smoke.cso /Fre smoke.reflection /Frs smoke.rootsig /Fe smoke.err
call :check_file smoke.hlsl.h find g_myvar find "0x44, 0x58" find "define void @main()" del
call :check_file smoke.ll find "define void @main()" del
call :check_file smoke.cso del
call :check_file smoke.reflection del
call :check_file smoke.rootsig del
call :check_file smoke.err find "warning: expression result unused" del
if %Failed% neq 0 goto :failed

set testname=/Fd implies /Qstrip_debug
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" /Zi /Fd smoke.hlsl.d /Fo smoke.hlsl.Fd.dxo
call :check_file smoke.hlsl.d del
call :check_file smoke.hlsl.Fd.dxo
if %Failed% neq 0 goto :failed
call :run dxc.exe -dumpbin smoke.hlsl.Fd.dxo
rem Should have debug name, and debug info should be stripped from module
call :check_file log find "shader debug name: smoke.hlsl.d" find-not "DICompileUnit"
if %Failed% neq 0 goto :failed

rem del .pdb file if exists
del %CD%\*.pdb 1>nul 2>nul

set testname=/Fd implies /Qstrip_debug ; path with \ produces auto hash-named .pdb file
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" /Zi /Fd .\ /Fo smoke.hlsl.strip
rem .pdb file should be produced
call :check_file *.PDB del
if %Failed% neq 0 goto :failed
call :run dxc.exe -dumpbin smoke.hlsl.strip
rem auto debug name is hex digest + .pdb
call :check_file log find-opt -r "shader debug name: [0-9a-f]*.pdb" find-not "DICompileUnit" del
if %Failed% neq 0 goto :failed

rem del .pdb file if exists
del %CD%\*.pdb 1>nul 2>nul

set testname=Embed debug info
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" /Zi /Qembed_debug /Fo smoke.hlsl.embedpdb
call :check_file smoke.hlsl.embedpdb
if %Failed% neq 0 goto :failed
rem .pdb file should NOT be produced
call :check_file_not *.pdb del
call :run dxc.exe -dumpbin smoke.hlsl.embedpdb
rem should have auto debug name, which is hex digest + .pdb
call :check_file log find-opt -r "shader debug name: [0-9a-f]*.pdb" find "DICompileUnit" del
call :check_file smoke.hlsl.embedpdb del
if %Failed% neq 0 goto :failed

set testname=Auto-embed debug info when no debug output, and expect warning signifying that this is the case.
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" /Zi /Fo smoke.hlsl.embedpdb /Fe smoke.err.embedpdb
call :check_file smoke.hlsl.embedpdb
rem Search for warning:
call :check_file smoke.err.embedpdb find "warning: no output provided for debug - embedding PDB in shader container.  Use -Qembed_debug to silence this warning." del
rem .pdb file should NOT be produced
call :check_file_not *.pdb del
if %Failed% neq 0 goto :failed
call :run dxc.exe -dumpbin smoke.hlsl.embedpdb
rem should have auto debug name, which is hex digest + .pdb
call :check_file log find-opt -r "shader debug name: [0-9a-f]*.pdb" find "DICompileUnit"
call :check_file smoke.hlsl.embedpdb del
if %Failed% neq 0 goto :failed

set testname=/Zi with /Qstrip_debug and no output should not embed
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" /Zi /Qstrip_debug /Fo smoke.hlsl.strip
call :check_file smoke.hlsl.strip
if %Failed% neq 0 goto :failed
call :run dxc.exe -dumpbin smoke.hlsl.strip
call :check_file log find-opt -r "shader debug name: [0-9a-f]*.pdb" find-not "DICompileUnit"
call :check_file smoke.hlsl.strip del
if %Failed% neq 0 goto :failed

set testname=/Qstrip_reflect strips reflection
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" -D DX12 /Qstrip_reflect /Fo smoke.hlsl.strip
call :check_file smoke.hlsl.strip
if %Failed% neq 0 goto :failed
call :run dxc.exe -dumpbin smoke.hlsl.strip
call :check_file log find-not "i32 6, !\"g\""
call :check_file smoke.hlsl.strip del
if %Failed% neq 0 goto :failed

set testname=ast-dump
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" /ast-dump
call :check_file log find TranslationUnitDecl
if %Failed% neq 0 goto :failed

set testname=Check Warning
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" /Dcheck_warning
call :check_file log find warning:
if %Failed% neq 0 goto :failed

set testname=/no-warnings
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" /Dcheck_warning /no-warnings
call :check_file log find-not warning:
if %Failed% neq 0 goto :failed

set testname=Preprocess
call :run dxc.exe "%testfiles%\smoke.hlsl" /P preprocessed.hlsl
call :check_file preprocessed.hlsl find "float4 main"
if %Failed% neq 0 goto :failed

set testname=-force_rootsig_ver
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" -force_rootsig_ver rootsig_1_0
if %Failed% neq 0 goto :failed
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" -force_rootsig_ver rootsig_1_1
if %Failed% neq 0 goto :failed
call :run-fail dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" -force_rootsig_ver rootsig_2_0
if %Failed% neq 0 goto :failed

set testname=Root Signature target
echo #define main "CBV(b0)"> test-global-rs.hlsl
call :run dxc -T rootsig_1_1 test-global-rs.hlsl -rootsig-define main -Fo test-global-rs.cso
call :check_file test-global-rs.cso del
if %Failed% neq 0 goto :failed

set testname=Local Root Signature target
echo #define main "CBV(b0), RootFlags(LOCAL_ROOT_SIGNATURE)"> test-local-rs.hlsl
call :run dxc -T rootsig_1_1 test-local-rs.hlsl -rootsig-define main -Fo test-local-rs.cso
call :check_file test-local-rs.cso del
if %Failed% neq 0 goto :failed

set testname=HLSL Version
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" /HV 2016
call :run-fail dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" /HV 2015
if %Failed% neq 0 goto :failed

set testname=Embed Debug, Recompile
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" /Zi /Qembed_debug /Fo smoke.cso 1> nul
call :check_file smoke.cso
if %Failed% neq 0 goto :failed
call :run dxc.exe -dumpbin smoke.cso
call :check_file log find "DICompileUnit"
call :check_file smoke.cso del
if %Failed% neq 0 goto :failed
call :run dxc.exe /T ps_6_0 "%testfiles%\smoke.hlsl" /Zi /Qembed_debug /Fo smoke.cso /Cc /Ni /No /Lx
call :check_file smoke.cso
if %Failed% neq 0 goto :failed
call :run dxc.exe -dumpbin smoke.cso
call :check_file log find "DICompileUnit"
if %Failed% neq 0 goto :failed
call :run dxc.exe smoke.cso /recompile
if %Failed% neq 0 goto :failed
call :run dxc.exe smoke.cso /recompile /T ps_6_0 /E main
if %Failed% neq 0 goto :failed

rem Note: this smoke.cso is used for a lot of other tests, and they rely on options set here
set testname=Command-line Defines, Recompile
call :run dxc.exe "%testfiles%\smoke.hlsl" /D "semantic = SV_Position" /T vs_6_0 /Zi /Qembed_debug /DDX12 /Fo smoke.cso
call :check_file smoke.cso
if %Failed% neq 0 goto :failed
call :run dxc.exe smoke.cso /recompile
if %Failed% neq 0 goto :failed

set testname=Strip Debug from compiled object
call :run dxc.exe smoke.cso /dumpbin /Qstrip_debug /Fo nodebug.cso
call :check_file nodebug.cso
if %Failed% neq 0 goto :failed
set testname=Strip Root Signature from compiled object
call :run dxc.exe smoke.cso /dumpbin /Qstrip_rootsignature /Fo norootsignature.cso
call :check_file norootsignature.cso
if %Failed% neq 0 goto :failed
set testname=Extract rootsignature from compiled object
call :run dxc.exe smoke.cso /dumpbin /extractrootsignature /Fo rootsig.cso
call :check_file rootsig.cso
if %Failed% neq 0 goto :failed
call :check_file smoke.cso del
set testname=Add rootsignature to compiled object
call :run dxc.exe norootsignature.cso /dumpbin /setrootsignature rootsig.cso /Fo smoke.cso
call :check_file rootsig.cso del
rem These are still needed later:
call :check_file smoke.cso
call :check_file norootsignature.cso
if %Failed% neq 0 goto :failed
rem Check that it added by extracting
call :run dxc.exe smoke.cso /dumpbin /extractrootsignature /Fo rootsig.cso
rem Need these later:
call :check_file smoke.cso
call :check_file rootsig.cso
if %Failed% neq 0 goto :failed

set testname=Compile NonUniform.hlsl, extract and strip root signature
call :run dxc.exe "%testfiles%\NonUniform.hlsl" /T ps_6_0 /DDX12 /Fo NonUniform.cso
call :check_file NonUniform.cso
if %Failed% neq 0 goto :failed
call :run dxc.exe NonUniform.cso /dumpbin /Qstrip_rootsignature /Fo NonUniformNoRootSig.cso
call :check_file NonUniformNoRootSig.cso
if %Failed% neq 0 goto :failed
call :run dxc.exe NonUniform.cso /dumpbin /extractrootsignature /Fo NonUniformRootSig.cso
call :check_file NonUniformRootSig.cso
if %Failed% neq 0 goto :failed

set testname=Verify root signature for smoke.cso
call :run dxc.exe smoke.cso /dumpbin /verifyrootsignature rootsig.cso
if %Failed% neq 0 goto :failed
call :run dxc.exe norootsignature.cso /dumpbin /verifyrootsignature rootsig.cso
if %Failed% neq 0 goto :failed

set testname=Verify root signature for NonUniform.cso
call :run dxc.exe NonUniform.cso /dumpbin /verifyrootsignature NonUniformRootSig.cso
if %Failed% neq 0 goto :failed
call :run dxc.exe NonUniformNoRootSig.cso /dumpbin /verifyrootsignature NonUniformRootSig.cso
if %Failed% neq 0 goto :failed

set testname=Verify mismatched root signatures fail verification
call :run-fail dxc.exe NonUniformNoRootSig.cso /dumpbin /verifyrootsignature rootsig.cso
if %Failed% neq 0 goto :failed
call :run-fail dxc.exe norootsignature.cso /dumpbin /verifyrootsignature NonUniformRootSig.cso
if %Failed% neq 0 goto :failed

set testname=Set root signature when already has one should succeed
call :run dxc.exe smoke.cso /dumpbin /setrootsignature rootsig.cso /Fo smoke.rsadded.cso
call :check_file smoke.rsadded.cso del
if %Failed% neq 0 goto :failed

set testname=Set mismatched root signature when already has one should fail
call :run-fail dxc.exe smoke.cso /dumpbin /setrootsignature NonUniformRootSig.cso /Fo smoke.rsadded.cso
call :check_file_not smoke.rsadded.cso del
if %Failed% neq 0 goto :failed

set testname=Compile TextVS.hlsl
call :run dxc.exe "%testfiles%\TextVS.hlsl" /Tvs_6_0 /Zi /Qembed_debug /Fo TextVS.cso
call :check_file TextVS.cso
if %Failed% neq 0 goto :failed
call :run dxc.exe smoke.cso /dumpbin /verifyrootsignature TextVS.cso
call :check_file TextVS.cso del
if %Failed% neq 0 goto :failed

set testname=Set private data
echo private data > private.txt
call :run dxc.exe smoke.cso /dumpbin /setprivate private.txt /Fo private.cso
call :check_file private.cso
if %Failed% neq 0 goto :failed
call :run dxc.exe private.cso /dumpbin /setprivate private.txt /Fo private.cso
call :check_file private.cso
if %Failed% neq 0 goto :failed
call :run dxc.exe private.cso /dumpbin /Qstrip_priv /Fo noprivate.cso
call :check_file noprivate.cso
if %Failed% neq 0 goto :failed
call :run dxc.exe private.cso /dumpbin /getprivate private1.txt
call :check_file private1.txt find "private data" del
if %Failed% neq 0 goto :failed
set testname=Appending and removing private data blob roundtrip
call :run FC smoke.cso noprivate.cso
call :check_file noprivate.cso del
if %Failed% neq 0 goto :failed
set testname=Strip multiple, verify stripping
call :run dxc.exe private.cso /dumpbin /Qstrip_priv /Qstrip_debug /Qstrip_rootsignature /Fo noprivdebugroot.cso
call :check_file noprivdebugroot.cso
if %Failed% neq 0 goto :failed
call :run-fail dxc.exe noprivdebugroot.cso /dumpbin /getprivate private1.txt
call :check_file_not private1.txt del
if %Failed% neq 0 goto :failed
call :run-fail dxc.exe noprivdebugroot.cso /dumpbin /extractrootsignature rootsig1.cso
call :check_file_not rootsig1.cso del
if %Failed% neq 0 goto :failed
call :run dxc.exe noprivdebugroot.cso /dumpbin
call :check_file_not log find-not "DICompileUnit"
if %Failed% neq 0 goto :failed
rem Cleanup
call :check_file private.cso del
call :check_file noprivdebugroot.cso del
if %Failed% neq 0 goto :failed

set testname=dxc.exe shader model upgrade
call :run dxc.exe -dumpbin smoke.cso -Fc smoke.ll
rem smoke.ll is used later to assemble, so don't delete it.
call :check_file smoke.ll find "DICompileUnit"
if %Failed% neq 0 goto :failed

set testname=dxa command line program
call :run dxa.exe smoke.cso -listfiles
if %Failed% neq 0 goto :failed
call :check_file log find "smoke.hlsl"
if %Failed% neq 0 goto :failed

set testname=dxa -listparts
call :run dxa.exe smoke.cso -listparts
if %Failed% neq 0 goto :failed
rem Check for expected parts for this container
call :check_file log find DXIL find ILDB find RTS0 find PSV0 find STAT find ILDN find HASH find ISG1 find OSG1
if %Failed% neq 0 goto :failed

set testname=dxa extract debug module
call :run dxa.exe smoke.cso -extractpart dbgmodule -o smoke.cso.dbgmodule
call :check_file smoke.cso.dbgmodule
if %Failed% neq 0 goto :failed
call :run dxa.exe smoke.cso.dbgmodule -listfiles
call :check_file log find "smoke.hlsl"
if %Failed% neq 0 goto :failed

set testname=dxa extract all files from debug module
call :run dxa.exe smoke.cso.dbgmodule -extractfile *
call :check_file log find "float4 main()"
if %Failed% neq 0 goto :failed

set testname=dxa extract DXIL module and build container from that
call :run dxa.exe smoke.cso -extractpart module -o smoke.cso.plain.bc
call :check_file smoke.cso.plain.bc
if %Failed% neq 0 goto :failed
call :run dxa.exe smoke.cso.plain.bc -o smoke.rebuilt-container.cso
call :check_file smoke.rebuilt-container.cso del
call :check_file smoke.cso.plain.bc del
if %Failed% neq 0 goto :failed

set testname=dxa assemble container from smoke.ll
call :run dxa.exe smoke.ll -o smoke.rebuilt-container2.cso
call :check_file smoke.rebuilt-container2.cso del
if %Failed% neq 0 goto :failed

set testname=Smoke test for dxopt command line
call :run dxc /Odump /T ps_6_0 "%testfiles%\smoke.hlsl" -Fo passes.txt
call :check_file passes.txt find emit
if %Failed% neq 0 goto :failed
echo -print-module >> passes.txt
call :run dxc /T ps_6_0 "%testfiles%\smoke.hlsl" /fcgl -Fc smoke.hl.txt
call :check_file smoke.hl.txt
if %Failed% neq 0 goto :failed
call :run-nolog dxopt -pf passes.txt -o=smoke.opt.ll smoke.hl.txt > smoke.opt.prn.txt
call :check_file smoke.opt.prn.txt find MODULE-PRINT del
if %Failed% neq 0 goto :failed

set testname=Smoke test for dxc_batch command line
call :run dxc_batch.exe -lib-link -multi-thread "%testfiles%\batch_cmds2.txt"
if %Failed% neq 0 goto :failed
call :run dxc_batch.exe -lib-link -multi-thread "%testfiles%\batch_cmds.txt"
if %Failed% neq 0 goto :failed
call :run dxc_batch.exe -multi-thread "%testfiles%\batch_cmds.txt"
if %Failed% neq 0 goto :failed

set testname=Smoke test for dxl command line
call :run dxc.exe -T lib_6_x "%testfiles%\lib_entry4.hlsl" -Fo lib_entry4.dxbc
call :check_file lib_entry4.dxbc
if %Failed% neq 0 goto :failed
call :run dxc.exe -T lib_6_x "%testfiles%\lib_res_match.hlsl" -Fo lib_res_match.dxbc
call :check_file lib_res_match.dxbc
if %Failed% neq 0 goto :failed
call :run dxl.exe -T ps_6_0 lib_res_match.dxbc;lib_entry4.dxbc -Fo res_match_entry.dxbc
call :check_file lib_entry4.dxbc del
call :check_file lib_res_match.dxbc del
call :check_file res_match_entry.dxbc del
if %Failed% neq 0 goto :failed

set testname=Test for denorm options
call :run dxc.exe "%testfiles%\smoke.hlsl" /Tps_6_2 /denorm preserve
if %Failed% neq 0 goto :failed
call :run dxc.exe "%testfiles%\smoke.hlsl" /Tps_6_2 /denorm ftz
if %Failed% neq 0 goto :failed
call :run-fail dxc.exe "%testfiles%\smoke.hlsl" /Tps_6_2 /denorm abc
if %Failed% neq 0 goto :failed
call :run-fail dxc.exe "%testfiles%\smoke.hlsl" /Tps_6_1 /denorm any
if %Failed% neq 0 goto :failed

set testname=Test /enable-16bit-types
call :run dxc.exe "%testfiles%\smoke.hlsl" /Tps_6_2 /enable-16bit-types
if %Failed% neq 0 goto :failed
call :run-fail dxc.exe "%testfiles%\smoke.hlsl" /Tps_6_1 /enable-16bit-types
if %Failed% neq 0 goto :failed
call :run-fail dxc.exe "%testfiles%\smoke.hlsl" /Tps_6_2 /enable-16bit-types /HV 2017
if %Failed% neq 0 goto :failed

set testname=Test file with relative path and include
mkdir subfolder 2>nul
mkdir inc       2>nul
copy "%testfiles%\include-main.hlsl" subfolder >nul
copy "%testfiles%\include-declarations.h" inc  >nul
call :run dxc.exe -Tps_6_0 -I inc subfolder\include-main.hlsl
if %Failed% neq 0 goto :failed
call :run dxc.exe -P include-main.hlsl.pp -I inc subfolder\include-main.hlsl
if %Failed% neq 0 goto :failed

rem SPIR-V Change Starts
echo Smoke test for SPIR-V CodeGen ...
set spirv_smoke_success=0
dxc.exe "%testfiles%\smoke.hlsl" /T ps_6_0 -spirv 1>%CD%\smoke.spirv.log 2>&1
if %errorlevel% equ 0 set spirv_smoke_success=1
findstr /c:"SPIR-V CodeGen not available" %CD%\smoke.spirv.log >nul
if %errorlevel% equ 0 set spirv_smoke_success=1
if %spirv_smoke_success% neq 1 (
  echo dxc failed SPIR-V smoke test
  call :cleanup 2>nul
  exit /b 1
)
rem SPIR-V Change Ends

call :cleanup
exit /b 0

:cleanup
for %%f in (%clanup_files%) do (
  del %%f 1>nul 2>nul
)
popd
exit /b 0

rem ============================================
rem Check that output does not exist
:check_file_not
set check_file_pattern=%CD%\%1
shift /1
if exist %check_file_pattern% (
  call :set_failed
  echo Found unexpected file %check_file_pattern%
  if "%1"=="del" (
    del %check_file_pattern% 1>nul
  )
  exit /b 1
)
exit /b 0

rem ============================================
rem Check that file exists and find text
:check_file
rem echo check_file %*
if "%1"=="log" (
  set check_file_pattern=%OutputLog%
) else (
  set check_file_pattern=%CD%\%1
)
if not exist %check_file_pattern% (
  if !Failed! equ 0 (
    call :set_failed
    echo Failed to find output file %check_file_pattern%
  )
  exit /b 1
)
shift /1

:check_file_loop
if "%1"=="" (
  set clanup_files=!cleanup_files! !check_file_pattern!
  exit /b !Failed!
) else if "%1"=="del" (
  if !Failed! equ 0 (
    del %check_file_pattern% 1>nul
  )
  exit /b !Failed!
) else (
  set mode=find
  set find_not=0
  if "%1"=="find-not" (
    set find_not=1
  ) else if "%1"=="find-opt" (
    set mode=find-opt
    shift /1
  ) else if "%1"=="find-not-opt" (
    set mode=find-opt
    set find_not=1
    shift /1
  ) else if "%1"=="findstr" (
    set mode=findstr
  ) else if "%1"=="findstr-not" (
    set mode=findstr
    set find_not=1
  ) else if not "%1"=="find" (
    echo Error: unrecognized check_file command: %1
    call :set_failed
    exit /b 1
  )

  if "!mode!"=="find" (
    set findcmd=findstr /C:%2 %check_file_pattern%
  ) else if "!mode!"=="find-opt" (
    set findcmd=findstr %2 /C:%3 %check_file_pattern%
  ) else if "!mode!"=="findstr" (
    set findcmd=findstr %* %check_file_pattern%
  )

  !findcmd! 1>nul
  if !find_not! equ 0 (
    if !errorlevel! neq 0 (
      call :set_failed
      echo Failed: !findcmd!
    )
  ) else (
    if !errorlevel! equ 0 (
      call :set_failed
      echo Found: !findcmd!
    )
  )

  if "!mode!"=="findstr" (
    rem findstr must be last, since it uses all the rest of the args
    exit /b !Failed!
  )
  shift /1
)
shift /1
goto :check_file_loop

rem ============================================
rem Run, log, and set Failed flag if failing
:run
rem echo run %*
set testcmd=%*
set FailingCmdWritten=0
del %OutputLog% 1>nul 2>nul
if %LogOutput% neq 0 (
  %testcmd% 1>%OutputLog% 2>&1
) else (
  %testcmd%
)
if %errorlevel% neq 0 (
  echo Command Returned: %errorlevel%
  echo See %OutputLog%
  call :set_failed %errorlevel%
)
exit /b 1

rem ============================================
rem Run but without redirecting to log
:run-nolog
set LogOutput=0
call :run %*
set LogOutput=1
exit /b %errorlevel%

rem ============================================
rem Run, log, and expect failing result, otherwise set Failed
:run-fail
set testcmd=%*
%testcmd% 1>testcmd.log 2>&1
if %errorlevel% equ 0 (
  call :set_failed
  exit /b 1
)
exit /b 0

rem ============================================
rem Set Failed and write testname/testcmd once.
:set_failed
if %FailingCmdWritten% neq 1 (
  if "%1"=="" (
    set Failed=1
  ) else (
    set Failed=%1
  )
  echo Test Failed: %testname%
  echo %testcmd%
  set FailingCmdWritten=1
)
exit /b 0

rem ============================================
rem Cleanup and return failure
:failed
call :cleanup 2>nul
if %Failed%=="0" set Failed=1
exit /b %Failed%
