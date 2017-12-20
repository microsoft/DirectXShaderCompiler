# DirectX Shader Compiler

[![Build status](https://ci.appveyor.com/api/projects/status/5cwy3b8y1oi71lvl?svg=true)](https://ci.appveyor.com/project/marcelolr/directxshadercompiler)

The DirectX Shader Compiler project includes a compiler and related tools used to compile High-Level Shader Language (HLSL) programs into DirectX Intermediate Language (DXIL) representation. Applications that make use of DirectX for graphics, games, and computation can use it to generate shader programs.

For more information, see the [Wiki](https://github.com/Microsoft/DirectXShaderCompiler/wiki).

## Features and Goals

The starting point of the project is a fork of the [LLVM](http://llvm.org/) and [Clang](http://clang.llvm.org/) projects, modified to accept HLSL and emit a validated container that can be consumed by GPU drivers.

At the moment, the DirectX HLSL Compiler provides the following components:

- dxc.exe, a command-line tool that can compile HLSL programs for shader model 6.0 or higher

- dxcompiler.dll, a DLL providing a componentized compiler, assembler, disassembler, and validator

- various other tools based on the above components

The Microsoft Windows SDK releases include a supported version of the compiler and validator.

The goal of the project is to allow the broader community of shader developers to contribute to the language and representation of shader programs, maintaining the principles of compatibility and supportability for the platform. It's currently in active development across two axes: language evolution (with no impact to DXIL representation), and surfacing hardware capabilities (with impact to DXIL, and thus requiring coordination with GPU implementations).

### SPIR-V CodeGen

As an example of community contribution, this project can also target the [SPIR-V](https://www.khronos.org/registry/spir-v/) intermediate representation. Please see the [doc](docs/SPIR-V.rst) for how HLSL features are mapped to SPIR-V, and the [wiki](https://github.com/Microsoft/DirectXShaderCompiler/wiki/SPIR%E2%80%90V-CodeGen) page for how to build, use, and contribute to the SPIR-V CodeGen.

## Building Sources

Before you build, you will need to have some additional software installed. This is the most straightforward path - see [Building Sources](https://github.com/Microsoft/DirectXShaderCompiler/wiki/Building-Sources) on the Wiki for more options, including Visual Studio 2015 and Ninja support.

* [Git](http://git-scm.com/downloads).
* [Visual Studio 2017](https://www.visualstudio.com/downloads). Select the following workloads: Universal Windows Platform Development and Desktop Development with C++.
* [Python](https://www.python.org/downloads/). Version 2.7.x is required, 3.x might work but it's not officially supported. You need not change your PATH variable during installation.

After cloning the project, you can set up a build environment shortcut by double-clicking the `utils\hct\hctshortcut.js` file. This will create a shortcut on your desktop with a default configuration.

Tests are built using the TAEF framework. Unless you have the Windows Driver Kit installed, you should run the script at `utils\hct\hctgettaef.py` from your build environment before you start building to download and unzip it as an external dependency. You should only need to do this once.

To build, run this command on the HLSL Console.

    hctbuild

You can also clean, build and run tests with this command.

    hctcheckin

To see a list of additional commands available, run `hcthelp`

## Running Tests

To run tests, open the HLSL Console and run this command after a successful build.

    hcttest

Some tests will run shaders and verify their behavior. These tests also involve a driver that can run these execute these shaders. See the next section on how this should be currently set up.

## Running Shaders

To run shaders compiled as DXIL, you will need support from the operating system as well as from the driver for your graphics adapter. Windows 10 Creators Update is the first version to support DXIL shaders.

Drivers indicate they can run DXIL by reporting support for Shader Model 6, possibly in experimental mode. To enable support in these cases, the [Developer mode](https://msdn.microsoft.com/windows/uwp/get-started/enable-your-device-for-development) setting must be enabled.

### Hardware Support

Hardware GPU support for DXIL is provided by the following vendors:

NVIDIA's r387 drivers (r387.92 and later) provide release mode support for DXIL
1.0 and Shader Model 6.0 on Win10 FCU and later, and experimental mode support
for DXIL 1.1 and Shader Model 6.1. This driver can be downloaded from
[geforce.com](https://www.geforce.com/drivers). Direct links for r388.59 (most
current as of this update) are provided below:

[Win10 Installer](http://uk.download.nvidia.com/Windows/388.59/388.59-desktop-win10-64bit-international-whql.exe)

[Release Notes](http://us.download.nvidia.com/Windows/388.59/388.59-win10-win8-win7-desktop-release-notes.pdf)

AMD's latest driver with support for DXIL 1.0 and Shader Model 6 in experimental mode is [Radeon Software Crimson ReLive Edition 17.4.2](http://support.amd.com/en-us/kb-articles/Pages/Radeon-Software-Crimson-ReLive-Edition-17.4.2-Release-Notes.aspx).


### Software Rendering

In the absence of hardware support, tests will run using the Windows Advanced Rasterization Platform (WARP) adapter. To get the correct version of WARP working, in addition to setting Developer mode, you should install the 'Graphics Tools' optional feature via the Settings app (click the 'Apps' icon, then the 'Manage optional features' link, then 'Add a feature', and select 'Graphics Tools' from the list).

For more information, see this [Wiki page](https://github.com/Microsoft/DirectXShaderCompiler/wiki/Running-Shaders).

## Making Changes

To make contributions, see the [CONTRIBUTING.md](CONTRIBUTING.md) file in this project.

## Documentation

You can find documentation for this project in the `docs` directory. These contain the original LLVM documentation files, as well as two new files worth nothing:

* [HLSLChanges.rst](docs/HLSLChanges.rst): this is the starting point for how this fork diverges from the original llvm/clang sources
* [DXIL.rst](docs/DXIL.rst): this file contains the specification for the DXIL format
* [tools/clang/docs/UsingDxc.rst](tools/clang/docs/UsingDxc.rst): this file contains a user guide for dxc.exe

## License

DirectX Shader Compiler is distributed under the terms of the University of Illinois Open Source License.

See [LICENSE.txt](LICENSE.TXT) and [ThirdPartyNotices.txt](ThirdPartyNotices.txt) for details.

## Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
