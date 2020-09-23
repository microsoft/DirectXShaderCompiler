@echo off

if "%1"=="/?" goto :showhelp
if "%1"=="-?" goto :showhelp
if "%1"=="-h" goto :showhelp
if "%1"=="-help" goto :showhelp
if "%1"=="--help" goto :showhelp

setlocal

if "%HLSL_SRC_DIR%"=="" (
  echo Missing source directory.
  if exist %~dp0..\..\LLVMBuild.txt (
    set HLSL_SRC_DIR=%~dp0..\..
    echo Source directory deduced to be %~dp0..\..
  ) else (
    exit /b 1
  )
)

if "%1"=="-buildoutdir" (
  echo Build output directory set to %2
  set HLSL_BLD_DIR=%2
  shift /1
  shift /1
)

if "%HLSL_BLD_DIR%"=="" (
  echo Missing build directory.
  exit /b 1
)

where cmake.exe 1>nul 2>nul
if errorlevel 1 (
  echo Unable to find cmake.exe on the path.
  echo cmake 3.4 is available from https://cmake.org/files/v3.4/cmake-3.4.0-win32-x86.exe
  exit /b 1
)

if "%BUILD_ARCH%"=="" (
  set BUILD_ARCH=Win32
)

set BUILD_GENERATOR=Visual Studio 16 2019
set BUILD_VS_VER=2019
set BUILD_CONFIG=Debug
set DO_SETUP=1
set DO_BUILD=1
set CMAKE_OPTS=
set SPEAK=1
set PARALLEL_OPT=/m
set ALL_DEFS=OFF
set ANALYZE=OFF
set OFFICIAL=OFF
set FIXED_VER=OFF
set FIXED_LOC=
set VENDOR=
set SPIRV=OFF
set SPV_TEST=OFF
set DXILCONV=ON

if "%1"=="-s" (
  set DO_BUILD=0
  shift /1
)
if "%1"=="-b" (
  set DO_SETUP=0
  shift /1
)

if "%1"=="-alldef" (
  set ALL_DEFS=ON
  shift /1
)
if "%1"=="-analyze" (
  set ANALYZE=ON
  shift /1
)
if "%1"=="-official" (
  echo Will generate official version for build
  set OFFICIAL=ON
  shift /1
)
if "%1"=="-fv" (
  echo Fixed version flag set for build.
  set FIXED_VER=ON
  shift /1
)
if "%1"=="-fvloc" (
  echo Fixed version flag set for build, version file location: %2
  set FIXED_VER=ON
  set FIXED_LOC=%2
  shift /1
  shift /1
)
if "%1"=="-cv" (
  echo Set the CLANG_VENDOR value.
  set VENDOR=%2
  shift /1
  shift /1
)
if "%1"=="-rel" (
  set BUILD_CONFIG=Release
  shift /1
)
if "%1"=="-x86" (
  set BUILD_ARCH=Win32
  shift /1
)
if "%1"=="-x64" (
  set BUILD_ARCH=x64
  shift /1
)
if /i "%1"=="-arm" (
  set BUILD_ARCH=ARM
  shift /1
)
if /i "%1"=="-arm64" (
  set BUILD_ARCH=ARM64
  shift /1
)
if "%1"=="-Debug" (
  set BUILD_CONFIG=Debug
  shift /1
)
if "%1"=="-Release" (
  set BUILD_CONFIG=Release
  shift /1
)
if "%1"=="-vs2017" (
  set BUILD_GENERATOR=Visual Studio 15 2017
  set BUILD_VS_VER=2017
  shift /1
)
if "%1"=="-vs2019" (
  shift /1
)

if "%1"=="-tblgen" (
  if "%2" == "" (
    echo Missing path argument after -tblgen.
    exit /b
  ) 
  set BUILD_TBLGEN_PATH=%2
  shift /1
  shift /1
)

if "%1"=="-dont-speak" (
  set SPEAK=0
  shift /1
)

if "%1"=="-no-parallel" (
  set PARALLEL_OPT=
  shift /1
)

if "%1"=="-no-dxilconv" (
  set DXILCONV=OFF
  shift /1
)

if "%1"=="-dxc-cmake-extra-args" (
  set CMAKE_OPTS=%CMAKE_OPTS% %~2
  shift /1
  shift /1
)

if "%1"=="-dxc-cmake-begins-include" (
  set CMAKE_OPTS=%CMAKE_OPTS% -DDXC_CMAKE_BEGINS_INCLUDE=%2
  shift /1
  shift /1
)

if "%1"=="-dxc-cmake-ends-include" (
  set CMAKE_OPTS=%CMAKE_OPTS% -DDXC_CMAKE_ENDS_INCLUDE=%2
  shift /1
  shift /1
)

rem Begin SPIRV change
if "%1"=="-spirv" (
  echo SPIR-V codegen is enabled.
  set SPIRV=ON
  shift /1
)
if "%1"=="-spirvtest" (
  echo Building SPIR-V tests is enabled.
  set SPV_TEST=ON
  shift /1
)
rem End SPIRV change

set BUILD_ARM_CROSSCOMPILING=0

if /i "%BUILD_ARCH%"=="Win32" (
  if "%BUILD_VS_VER%"=="2019" (
    set VS2019ARCH=-AWin32
  )
)

if /i "%BUILD_ARCH%"=="x64" (
  set BUILD_GENERATOR=%BUILD_GENERATOR% %BUILD_ARCH:x64=Win64%
  if "%BUILD_VS_VER%"=="2019" (
    set BUILD_GENERATOR=%BUILD_GENERATOR%
    set VS2019ARCH=-Ax64
  )
)

if /i "%BUILD_ARCH%"=="arm" (
  set BUILD_GENERATOR_PLATFORM=ARM
  set BUILD_ARM_CROSSCOMPILING=1
  if "%BUILD_VS_VER%"=="2019" (
    set VS2019ARCH=-AARM
  )
)

if /i "%BUILD_ARCH%"=="arm64" (
  set BUILD_GENERATOR_PLATFORM=ARM64
  set BUILD_ARM_CROSSCOMPILING=1
  if "%BUILD_VS_VER%"=="2019" (
    set VS2019ARCH=-AARM64
  )
)

if "%1"=="-ninja" (
  set BUILD_GENERATOR=Ninja
  shift /1
)

set CMAKE_OPTS=%CMAKE_OPTS% -DHLSL_OPTIONAL_PROJS_IN_DEFAULT:BOOL=%ALL_DEFS%
set CMAKE_OPTS=%CMAKE_OPTS% -DHLSL_ENABLE_ANALYZE:BOOL=%ANALYZE%
set CMAKE_OPTS=%CMAKE_OPTS% -DHLSL_OFFICIAL_BUILD:BOOL=%OFFICIAL%
set CMAKE_OPTS=%CMAKE_OPTS% -DHLSL_ENABLE_FIXED_VER:BOOL=%FIXED_VER%
set CMAKE_OPTS=%CMAKE_OPTS% -DHLSL_ENABLE_FIXED_VER:BOOL=%FIXED_VER% -DHLSL_FIXED_VERSION_LOCATION:STRING=%FIXED_LOC%
set CMAKE_OPTS=%CMAKE_OPTS% -DHLSL_BUILD_DXILCONV:BOOL=%DXILCONV%
set CMAKE_OPTS=%CMAKE_OPTS% -DCLANG_VENDOR:STRING=%VENDOR%
set CMAKE_OPTS=%CMAKE_OPTS% -DENABLE_SPIRV_CODEGEN:BOOL=%SPIRV%
set CMAKE_OPTS=%CMAKE_OPTS% -DSPIRV_BUILD_TESTS:BOOL=%SPV_TEST%
set CMAKE_OPTS=%CMAKE_OPTS% -DCLANG_ENABLE_ARCMT:BOOL=OFF
set CMAKE_OPTS=%CMAKE_OPTS% -DCLANG_ENABLE_STATIC_ANALYZER:BOOL=OFF
set CMAKE_OPTS=%CMAKE_OPTS% -DCLANG_INCLUDE_TESTS:BOOL=OFF -DLLVM_INCLUDE_TESTS:BOOL=OFF
set CMAKE_OPTS=%CMAKE_OPTS% -DHLSL_INCLUDE_TESTS:BOOL=ON
set CMAKE_OPTS=%CMAKE_OPTS% -DLLVM_TARGETS_TO_BUILD:STRING=None
set CMAKE_OPTS=%CMAKE_OPTS% -DLLVM_INCLUDE_DOCS:BOOL=OFF -DLLVM_INCLUDE_EXAMPLES:BOOL=OFF
set CMAKE_OPTS=%CMAKE_OPTS% -DLIBCLANG_BUILD_STATIC:BOOL=ON
rem set CMAKE_OPTS=%CMAKE_OPTS% -DLLVM_OPTIMIZED_TABLEGEN:BOOL=ON
set CMAKE_OPTS=%CMAKE_OPTS% -DLLVM_OPTIMIZED_TABLEGEN:BOOL=OFF
set CMAKE_OPTS=%CMAKE_OPTS% -DLLVM_REQUIRES_EH:BOOL=ON
set CMAKE_OPTS=%CMAKE_OPTS% -DLLVM_APPEND_VC_REV:BOOL=ON

rem Enable exception handling (which requires RTTI).
set CMAKE_OPTS=%CMAKE_OPTS% -DLLVM_ENABLE_RTTI:BOOL=ON
set CMAKE_OPTS=%CMAKE_OPTS% -DLLVM_ENABLE_EH:BOOL=ON

rem Setup a specific, stable triple for HLSL.
set CMAKE_OPTS=%CMAKE_OPTS% -DLLVM_DEFAULT_TARGET_TRIPLE:STRING=dxil-ms-dx

set CMAKE_OPTS=%CMAKE_OPTS% -DCLANG_BUILD_EXAMPLES:BOOL=OFF
set CMAKE_OPTS=%CMAKE_OPTS% -DLLVM_REQUIRES_RTTI:BOOL=ON
set CMAKE_OPTS=%CMAKE_OPTS% -DCLANG_CL:BOOL=OFF
set CMAKE_OPTS=%CMAKE_OPTS% -DCMAKE_SYSTEM_VERSION=10.0.14393.0
set CMAKE_OPTS=%CMAKE_OPTS% -DDXC_BUILD_ARCH=%BUILD_ARCH%

rem ARM cross-compile setup
if %BUILD_ARM_CROSSCOMPILING% == 0 goto :after-cross-compile

rem The ARM build needs to have access to x86 or x64 build of clang-tblgen and llvm-tblgen tools.
call :verify-tblgen %BUILD_TBLGEN_PATH%
if errorlevel 1 (
  echo Cannot find x86/x64 version clang-tblgen and llvm-tblgen tools.
  echo Please set BUILD_TBLGEN_PATH or use hctbuild -tblgen option to specify location of x86/x64 build of DXC.
  call :handlefail
  exit /b 1
)

echo TableGen path: %BUILD_TBLGEN_PATH%
set CMAKE_OPTS=%CMAKE_OPTS% -DCMAKE_CROSSCOMPILING=True
set CMAKE_OPTS=%CMAKE_OPTS% -DCMAKE_GENERATOR_PLATFORM=%BUILD_GENERATOR_PLATFORM%
set CMAKE_OPTS=%CMAKE_OPTS% -DLLVM_TABLEGEN=%BUILD_TBLGEN_PATH%\llvm-tblgen.exe
set CMAKE_OPTS=%CMAKE_OPTS% -DCLANG_TABLEGEN=%BUILD_TBLGEN_PATH%\clang-tblgen.exe

echo Cross-compiling enabled.

:after-cross-compile

rem This parameter is used with vcvarsall to force use of 64-bit build tools
rem instead of 32-bit tools that run out of memory.
if /i "%BUILD_ARCH%"=="Win32" (
  set BUILD_TOOLS=amd64_x86
) else if /i "%BUILD_ARCH%"=="x64" (
  set BUILD_TOOLS=amd64
) else if /i "%BUILD_ARCH%"=="ARM" (
  set BUILD_TOOLS=amd64_arm
) else if /i "%BUILD_ARCH%"=="ARM64" (
  set BUILD_TOOLS=amd64_arm64
)

call :configandbuild %BUILD_CONFIG% %BUILD_ARCH% %HLSL_BLD_DIR% "%BUILD_GENERATOR%" "%VS2019ARCH%"
if errorlevel 1 exit /b 1

if "%BUILD_GENERATOR%"=="Ninja" (
  echo Success - files are available at %HLSL_BLD_DIR%\bin
) else (
  echo Success - files are available at %HLSL_BLD_DIR%\%BUILD_CONFIG%\bin
)
call :handlesuccess
exit /b 0

:showhelp
echo Builds HLSL solutions and the product and test binaries for the current
echo flavor and architecture.
echo.
echo hctbuild [-s or -b] [-alldef] [-analyze] [-official] [-fv] [-fvloc <path>] [-rel] [-arm or -arm64 or -x86 or -x64] [-Release] [-Debug] [-vs2017] [-vs2019] [-ninja] [-tblgen path] [-dont-speak] [-no-parallel] [-no-dxilconv]
echo.
echo   -s   creates the projects only, without building
echo   -b   builds the existing project
echo.
echo   -alldef        adds optional projects to the default build
echo   -analyze       adds /analyze option
echo   -official      will generate official version for build
echo   -fv            fixes the resource version for release (utils\version\version.inc)
echo   -fvloc <path>  directory with the version.inc file
echo   -rel           builds release rather than debug
echo   -dont-speak    disables audible build confirmation
echo   -no-parallel   disables parallel build
echo   -no-dxilconv   disables build of DXBC to DXIL converter and tools
echo   -vs2017        uses Visual Studio 2017 to build
echo   -vs2019        uses Visual Studio 2019 to build
echo.
echo current BUILD_ARCH=%BUILD_ARCH%.  Override with:
echo   -x86 targets an x86 build (aka. Win32)
echo   -x64 targets an x64 build (aka. Win64)
echo   -arm targets an ARM build
echo   -arm64 targets an ARM64 build
echo.
echo Generator:
echo   -ninja   use Ninja as the generator
echo.
echo AppVeyor Support
echo   -Release builds release
echo   -Debug builds debug
echo.
echo ARM build support
echo   -tblgen sets path to x86 or x64 versions of clang-tblgen and llvm-tblgen tools
echo.
if not "%HLSL_BLD_DIR%"=="" (
  echo The solution file is at %HLSL_BLD_DIR%\LLVM.sln
  echo.
)
goto :eof


:configandbuild
rem Configure and build a specific configuration, typically Debug or Release.
rem %1 - the conf name
rem %2 - the platform name
rem %3 - the build directory
rem %4 - the generator name
rem %5 - the vs2019 architecture name
if not exist %3 (
  mkdir %3
  if errorlevel 1 (
    echo Unable to create %3
    call :handlefail
    exit /b 1
  )
)
cd /d %3
if "%DO_SETUP%"=="1" (
  echo Creating solution files for %2, logging to %3\cmake-log.txt
  if "%BUILD_GENERATOR%"=="Ninja" (
    echo Running cmake -DCMAKE_BUILD_TYPE:STRING=%1 %CMAKE_OPTS% -G %4 %HLSL_SRC_DIR% > %3\cmake-log.txt
    cmake -DCMAKE_BUILD_TYPE:STRING=%1 %CMAKE_OPTS% -G %4 %HLSL_SRC_DIR% >> %3\cmake-log.txt 2>&1
  ) else (
    rem -DCMAKE_BUILD_TYPE:STRING=%1 is not necessary for multi-config generators like VS
    echo Running cmake %CMAKE_OPTS% -G %4 %5 %HLSL_SRC_DIR% > %3\cmake-log.txt
    cmake %CMAKE_OPTS% -G %4 %5 %HLSL_SRC_DIR% >> %3\cmake-log.txt 2>&1
  )
  if errorlevel 1 (
    echo Failed to configure cmake projects.
    echo ===== begin cmake-log.txt =====
    type %3\cmake-log.txt
    echo ===== end cmake-log.txt =====
    echo Run 'cmake %HLSL_SRC_DIR%' in %3 will continue project generation after fixing the issue.
    cmake --version | findstr 3.4
    if errorlevel 1 (
      echo CMake 3.4 is the currently supported version - your installed cmake may be out of date.
      echo See README.md at the root for an explanation of dependencies.
    )
    findstr -c:"Could NOT find D3D12" %3\cmake-log.txt >NUL
    if errorlevel 1 (
      rem D3D12 has been found, nothing to diagnose here.
    ) else (
      echo D3D12 has not been found. Confirm that you have installed the Windows 10 SDK.
      echo See README.md at the root for an explanation of dependencies.
      echo Run hctclean after installing the SDK to completely rebuild the projects.
    )
    call :handlefail
    exit /b 1
  )
)

if "%DO_BUILD%" neq "1" (
  exit /b 0
)

rem Just defer to cmake for now.
cmake --build . --config %1 -- %PARALLEL_OPT%
goto :donebuild

:donebuild
if errorlevel 1 (
  echo Failed to build projects.
  echo After fixing, run 'cmake --build --config %1 .' in %2
  call :handlefail
  exit /b 1
)
endlocal
exit /b 0

:verify-tblgen
if exist %1\clang-tblgen.exe (
  if exist %1\llvm-tblgen.exe exit /b 0
)
exit /b 1

:handlefail
if %SPEAK%==1 (
  cscript.exe //Nologo %HLSL_SRC_DIR%\utils\hct\hctspeak.js /say:"build failed"
)
exit /b 0

:handlesuccess
if %SPEAK%==1 (
  cscript.exe //Nologo %HLSL_SRC_DIR%\utils\hct\hctspeak.js /say:"build succeeded"
)
exit /b 0
