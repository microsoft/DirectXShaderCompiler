# DirectX Shader Compiler

[![Build status](https://ci.appveyor.com/api/projects/status/2wsw8t8clpgt1kfm?svg=true)](https://ci.appveyor.com/project/dmpots/directxshadercompiler)

The DirectX Shader Compiler project includes a compiler and related tools used to compile High-Level Shader Language (HLSL) programs into DirectX Intermediate Language (DXIL) representation. Applications that make use of DirectX for graphics, games, and computation can use it to generate shader programs.

For more information, see the [Wiki](https://github.com/Microsoft/DirectXShaderCompiler/wiki).

## Features and Goals

The starting point of the project is a fork of the [LLVM](http://llvm.org/) and [Clang](http://clang.llvm.org/) projects, modified to accept HLSL and emit a validated container that can be consumed by GPU drivers.

At the moment, the DirectX HLSL Compiler provides the following components:

- dxc.exe, a command-line tool that can compile shader model 6 HLSL programs

- dxcompiler.dll, a DLL providing a componentized compiler, assembler, disassembler, and validator

- various other tools based on the above components

The DirectX Shader Compiler is currently in preview stage but is expected to be finalized in the next few months. The Microsoft Windows SDK releases will include a supported version of the compiler and validator.

The goal of the project is to allow the broader community of shader developers to contribute to the language and representation of shader programs, maintaining the principles of compatibility and supportability for the platform. It's currently in active development across two axes: language evolution (with no impact to DXIL representation), and surfacing hardware capabilities (with impact to DXIL, and thus requiring coordination with GPU implementations).

## Building Sources

Before you build, you will need to have some additional software installed.

* [Git](http://git-scm.com/downloads).
* [Visual Studio](https://www.visualstudio.com/downloads)
  * Visual Studio 2015, Update 3. This will install the Windows Development Kit. In the install options, make sure the following options are checked:
      * Windows 10 SDK (version 14393)
      * Common Tools for Visual C++ 2015
  * Visual Studio 2017. Select the following workloads:
      * Universal Windows Platform Development
      * Desktop Development with C++
* [Windows 10 SDK](https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk). This is needed to build tests that reference the D3D12 runtime. You may get this as part of installing/updating Visual Studio.
* TAEF. You can run the script at `utils\hct\hctgettaef.py` from your build environment before you start building to download and unzip them as an external dependency. Alternatively, install the [Windows Driver Kit](https://developer.microsoft.com/en-us/windows/hardware/windows-driver-kit). No need to download and install tests. This is used to build and run tests.
* [CMake](https://cmake.org/files/v3.4/cmake-3.4.3-win32-x86.exe). Version 3.4.3 and 3.7.2 are the supported versions. You need not change your PATH variable during installation.
* [Python](https://www.python.org/downloads/). Version 2.7.x is required, 3.x might work but it's not officially supported. You need not change your PATH variable during installation.

To setup the build environment run the `utils\hct\hctstart.cmd` script passing the path to the source and build directories from a regular command prompt window. For example:

```
git clone https://github.com/Microsoft/DirectXShaderCompiler.git C:\DirectXShaderCompiler
cd C:\DirectXShaderCompiler
utils\hct\hctstart.cmd C:\DirectXShaderCompiler C:\DirectXShaderCompiler.bin
```

To create a shortcut to the build environment with the default build directory, double-click on the `utils\hct\hctshortcut.js` file.

To build, open the HLSL Console and run this command.
 
    hctbuild

You can also clean, build and run tests with this command.

    hctcheckin 

To see a list of additional commands available, run `hcthelp`

### Building with Visual Studio 2017

You can build with vs2017 either on the command line or using the new integrated [CMake support](https://blogs.msdn.microsoft.com/vcblog/2016/11/16/cmake-support-in-visual-studio-the-visual-studio-2017-rc-update/).

To build from the command line follow the normal build steps, but pass `-vs2017` as a parameter
to `hctbuild`.
 
To build using the integrated cmake support, simply start Visual Studio
and open the folder where you have the source. From the CMake menu 
select "Build CMakeLists.txt"

By default the binaries will be built in %LOCALAPPDATA%\CMakeBuild\DirectXShaderCompiler\build\{build-flavor}.
The build location can be changed by editing the `CMakeSettings.json` file.

You can then use the build directory in the `hctstart` script to test the build. For example,

    hctstart C:\source\DirectXShaderCompiler %LOCALAPPDATA%\CMakeBuild\DirectXShaderCompiler\build\x64-Debug

## Running Tests

To run tests, open the HLSL Console and run this command after a successful build.

    hcttest

Some tests will run shaders and verify their behavior. These tests also involve a driver that can run these execute these shaders. See the next section on how this should be currently set up.

## Running Shaders

To run shaders compiled as DXIL, you will need support from the operating system as well as from the driver for your graphics adapter.

At the moment, the [Windows 10 Insider Preview Build 15007](https://blogs.windows.com/windowsexperience/2017/01/12/announcing-windows-10-insider-preview-build-15007-pc-mobile/#XqlQ5FZfXw5WVhpS.97) is able to run DXIL shaders.

Drivers indicate they can run DXIL by reporting support for Shader Model 6, possibly in experimental mode. To enable support in these cases, the [Developer mode](https://msdn.microsoft.com/windows/uwp/get-started/enable-your-device-for-development) setting must be enabled.

### Hardware Support

Hardware GPU support for DXIL is provided by the following vendors:

NVIDIA r378 drivers (r378.49 and later) provide experimental mode support for DXIL and shader model 6. This is an early beta version to enable developers to try out DXIL and the new shader model 6 features – Wave Math and int64. Only DXIL version 0.7 (beta) is accepted by the r378 driver.  Experimental mode support for DXIL v1.0 will be provided in a future driver release. 

### Software Rendering

In the absence of hardware support, tests will run using the Windows Advanced Rasterization Platform (WARP) adapter. To get the correct version of WARP working, in addition to setting Developer mode, you should install the 'Graphics Tools' optional feature via the Settings app (click the 'Apps' icon, then the 'Manage optional features' link, then 'Add a feature', and select 'Graphics Tools' from the list).

For more information, see this [Wiki page](https://github.com/Microsoft/DirectXShaderCompiler/wiki/Running-Shaders).

## Making Changes

To make contributions, see the CONTRIBUTING.md file in this project.

## Documentation

You can find documentation for this project in the `docs` directory. These contain the original LLVM documentation files, as well as two new files worth nothing:

* HLSLChanges.rst: this is the starting point for how this fork diverges from the original llvm/clang sources
* DXIL.rst: this file contains the specification for the DXIL format
* tools/clang/docs/UsingDxc.rst: this file contains a user guide for dxc.exe

## License

DirectX Shader Compiler is distributed under the terms of the University of Illinois Open Source License.

See LICENSE.txt and ThirdPartyNotices.txt for details.

## Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

