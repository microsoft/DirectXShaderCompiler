==============
SPIR-V Codegen
==============

.. contents::
   :local:
   :depth: 2

Introduction
============

This document describes the designs and logistics for supporting SPIR-V codegen functionality. `SPIR-V <https://www.khronos.org/registry/spir-v/>`_ is a binary intermediate language for representing graphical-shader stages and compute kernels for multiple Khronos APIs, such as Vulkan, OpenGL, and OpenCL. At the moment we only intend to support the Vulkan flavor of SPIR-V.

DirectXShaderCompiler is the reference compiler for HLSL. Adding SPIR-V codegen in DirectXShaderCompiler will enable the usage of HLSL as a frontend language for Vulkan shader programming. Sharing the same code base also means we can track the evolution of HLSL more closely and always deliver the best of HLSL to developers. Moreover, developers will also have a unified compiler toolchain for targeting both DirectX and Vulkan. We believe this effort will benefit the general graphics ecosystem.

Designs
=======

Various designs are driven by technical considerations together with the following guidelines for good citizenship within DirectXShaderCompiler:

- Conduct minimal changes to existing interfaces and libraries
- Perfer less intrusive solutions

General approach
----------------

The general approach is to translate frontend AST directly into SPIR-V binary. We choose this approach considering that

- Frontend AST is much more higher-level than DXIL. For example, `DXIL scalarized vectors <https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/DXIL.rst#vectors>`_ but SPIR-V has native support.
- DXIL has widely different semantics than Vulkan flavor of SPIR-V. For example, `structured control flow is not preserved in DXIL <https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/DXIL.rst#control-flow-restrictions>`_ but SPIR-V for Vulkan requires it.
- Frontend AST perserves the information in the source code better.
- Also, the right place to generate error messages is in Clang's semantic analysis step, which is when the compiler is still processing the AST.

Therefore, it is easier to go from frontend AST to SPIR-V than from DXIL since we do not need to rediscover certain information.

LLVM optimization passes
++++++++++++++++++++++++

Translating frontend AST directly into SPIR-V binary precludes the usage of existing LLVM optimization passes. This is expected since there are also subtle semantics differences between SPIR-V and LLVM IR. Certain concepts in SPIR-V do not have direct corresponding representation in LLVM IR and there are no existing translation schemes handling the differences. Using vanilla LLVM optimization passes will likely violate the requirements of SPIR-V and results in invalid SPIR-V modules.

Library
-------

On the library side, this means introducing a new ``ASTFrontendAction`` and a SPIR-V module builder.  The new frontend action will traverse the AST and call the SPIR-V module builder to construct SPIR-V words. These code should be placed at ``tools/clang/lib/SPIRV`` and packed into one library (or multiple libraries in the future).

Detailed design will be revised to accommodate more and more HLSL features. At the moment, we have::

                EmitSPIRVAction
                     |
                     | creates
                     V
                SPIRVEmitter
                     |
                     | contains
                     |
       +-------------+------------+
       |                          |
       |                          |
       V         references       V
  SPIRVContext <------------ ModuleBuilder
                                  |
                                  | contains
                                  V
                              InstBuilder
                                  |
                                  | depends on
                                  V
                             WordConsumer

``SPIRVEmitter``
  The derived ``ASTConsumer`` which acts on various frontend AST nodes by calling corresponding ``ModuleBuilder`` methods to build SPIR-V modules gradually.
``ModuleBuilder``
  Exposes API for constructing SPIR-V modules. Internally it has structured representation of SPIR-V modules, functions, basic blocks as well as various SPIR-V specific structs like entry points, debug names, and so on.
``SPIRVContext``
  Responsible for <result-id> allocation and maintaining the lifetime of objects allocated to represent types, decorations, and others. It is used in conjunction with ``ModuleBuilder``.
``InstBuilder``
  The low-level interface for generating SPIR-V words for various SPIR-V instructions. All SPIR-V instructions are eventually serialized via ``InstBuilder``.
``WordConsumer``
  The consumer of generated SPIR-V words.

Command-line tool
-----------------

On the command-line tool side, this means introducing a new binary, ``hlsl2spirv`` to wrap around the library functionality.

But as the initial scaffolding step, a new option, ``-spirv``, will be added into ``dxc`` for invoking the new SPIR-V codegen action.

Build system
------------

SPIR-V codegen functionality will require two external projects: `SPIRV-Headers <https://github.com/KhronosGroup/SPIRV-Headers>`_ (for ``spirv.hpp11``) and `SPIRV-Tools <https://github.com/KhronosGroup/SPIRV-Tools>`_ (for SPIR-V disassembling). These two projects should be checked out under the ``external/`` directory.

SPIR-V codegen functionality will structured as an optional feature in DirectXShaderCompiler. Two new CMake options will be introduced to control the configuring and building SPIR-V codegen:

- ``ENABLE_SPIRV_CODEGEN``: If turned on, enables the SPIR-V codegen functionality. (Default: OFF)
- ``SPIRV_BUILD_TESTS``: If turned on, enables building of SPIR-V related tests. This option will also implicitly turn on ``ENABLE_SPIRV_CODEGEN``. (Default: OFF)

For building, ``hctbuild`` will be extended with two new switches, ``-spirv`` and ``-spirvtest``, to turn on the above two options, respectively.

For testing, ``hcttest spirv`` will run all existing tests together with SPIR-V tests, while ``htctest spirv_only`` will only trigger SPIR-V tests.

Mapping From HLSL to SPIR-V for Vulkan
======================================

Due to the differences of semantics between DirectX and Vulkan, certain HLSL features do not have corresponding mappings in Vulkan, and certain Vulkan specific information does not have native ways to express in HLSL source code. This section will capture the mappings we use to conduct the translation. Specifically, it lists the mappings from HLSL shader model 6.0 to Vulkan flavor of SPIR-V.

Note that this section is expected to be an ongoing effort and grow as we implement more and more HLSL features. We are likely to extract the contents in this section into a new doc in the future.

Vulkan semantics
----------------

To provide additional information required by Vulkan in HLSL, we need to extend the syntax of HLSL. `C++ attribute specifier sequence <http://en.cppreference.com/w/cpp/language/attributes>`_ is a non-intrusive way of achieving such purpose.

An example is specifying the layout of Vulkan resources::

  [[using Vulkan: set(X), binding(Y)]]
  tbuffer TbufOne {
    [[using Vulkan: offset(Z)]]
    float4 field;
  };

  [[using Vulkan: push_constant]]
  tbuffer TbufTwo {
    float4 field;
  };

  [[using Vulkan: constant_id(M)]]
  const int specConst = N;

Types
-----

Normal scalar types
+++++++++++++++++++

`Normal scalar types <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509646(v=vs.85).aspx>`_ in HLSL are relatively easy to handle and can be mapped directly to SPIR-V instructions:

================== ==================
      HLSL               SPIR-V
================== ==================
``bool``           ``OpTypeBool``
``int``            ``OpTypeInt 32 1``
``uint``/``dword`` ``OpTypeInt 32 0``
``half``           ``OpTypeFloat 16``
``float``          ``OpTypeFloat 32``
``double``         ``OpTypeFloat 64``
================== ==================

Minimal precision scalar types
++++++++++++++++++++++++++++++

HLSL also supports various `minimal precision scalar types <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509646(v=vs.85).aspx>`_, which graphics drivers can implement by using any precision greater than or equal to their specified bit precision.

- ``min16float`` - minimum 16-bit floating point value
- ``min10float`` - minimum 10-bit floating point value
- ``min16int`` - minimum 16-bit signed integer
- ``min12int`` - minimum 12-bit signed integer
- ``min16uint`` - minimum 16-bit unsigned integer

There are no direct mapping in SPIR-V for these types. We may need to use ``OpTypeFloat``/``OpTypeInt`` with ``RelaxedPrecision`` for some of them and issue warnings/errors for the rest.

Vectors and matrixes
++++++++++++++++++++

`Vectors <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509707(v=vs.85).aspx>`_ and `matrixes <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509623(v=vs.85).aspx>`_ are also relatively straightforward to handle:

+------------------------------------+------------------------------------+
|               HLSL                 |             SPIR-V                 |
+------------------------------------+------------------------------------+
|``vector<|type|, |count|>``         | ``OpTypeVector |type| |count|``    |
+------------------------------------+------------------------------------+
|``matrix<|type|, |row|, |column|>`` | ``%v = OpTypeVector |type| |row|`` |
+------------------------------------+                                    |
|``|type||row|x|column|``            | ``OpTypeMatrix %v |column|``       |
+------------------------------------+------------------------------------+

Structs
+++++++

`Structs <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509668(v=vs.85).aspx>`_ in HLSL are defined in the a format similar to C structs, with optional interpolation modifiers for members:

=========================== =================
HLSL Interpolation Modifier SPIR-V Decoration
=========================== =================
``linear``                  <none>
``centroid``                ``Centroid``
``nointerpolation``         ``Flat``
``noperspective``           ``NoPerspective``
``sample``                  ``Sample``
=========================== =================

User-defined types
++++++++++++++++++

`User-defined types <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509702(v=vs.85).aspx>`_ are type aliases introduced by typedef. No new types are introduced and we can rely on Clang to resolve to the original types.

Samplers and textures
+++++++++++++++++++++

[TODO]

Buffers
+++++++

[TODO]

Variables and resources
-----------------------

Definition
++++++++++

Variables are defined in HLSL using the following `syntax <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509706(v=vs.85).aspx>`_ rules::

  [StorageClass] [TypeModifier] Type Name[Index]
      [: Semantic]
      [: Packoffset]
      [: Register];
      [Annotations]
      [= InitialValue]

Storage class
+++++++++++++

[TODO]

Type modifier
+++++++++++++

[TODO]

Interface variables
+++++++++++++++++++

Direct3D uses "`semantics <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509647(v=vs.85).aspx>`_" to compose and match the interfaces between subsequent stages. These semantics modifiers can appear after struct members, global variables, and also function parameters and return values. E.g.,::

  struct VSInput {
    float4 pos  : POSITION;
    float3 norm : NORMAL;
    float4 tex  : TEXCOORD0;
  };

  float4 pos: SV_POSITION;

  float4 VSFunction(float4 pos : POSITION) : POSITION {
    return pos;
  }

In Clang AST, these semantics are represented as ``SemanticDecl``, which is attached to the corresponding struct members (``FieldDecl``), global variables (``VarDecl``), and function parameters (``ParmVarDecl``) and return values (``FunctionDecl``).

[TODO] How to map semantics to SPIR-V interface variables

Expressions
-----------

Arithmetic operators
++++++++++++++++++++

`Arithmetic operators <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509631(v=vs.85).aspx#Additive_and_Multiplicative_Operators>`_ (``+``, ``-``, ``*``, ``/``, ``%``) are translated into their corresponding SPIR-V opcodes according to the following table.

+-------+-----------------------------+-------------------------------+--------------------+
|       | (Vector of) Signed Integers | (Vector of) Unsigned Integers | (Vector of) Floats |
+-------+-----------------------------+-------------------------------+--------------------+
| ``+`` |                         ``OpIAdd``                          |     ``OpFAdd``     |
+-------+-------------------------------------------------------------+--------------------+
| ``-`` |                         ``OpISub``                          |     ``OpFSub``     |
+-------+-------------------------------------------------------------+--------------------+
| ``*`` |                         ``OpIMul``                          |     ``OpFMul``     |
+-------+-----------------------------+-------------------------------+--------------------+
| ``/`` |    ``OpSDiv``               |       ``OpUDiv``              |     ``OpFDiv``     |
+-------+-----------------------------+-------------------------------+--------------------+
| ``%`` |    ``OpSRem``               |       ``OpUMod``              |     ``OpFRem``     |
+-------+-----------------------------+-------------------------------+--------------------+

Note that for modulo operation, SPIR-V has two sets of instructions: ``Op*Rem`` and ``Op*Mod``. For ``Op*Rem``, the sign of a non-0 result comes from the first operand; while for ``Op*Mod``, the sign of a non-0 result comes from the second operand. HLSL doc does not mandate which set of instructions modulo operations should be translated into; it only says "the % operator is defined only in cases where either both sides are positive or both sides are negative." So technically it's undefined behavior to use the modulo operation with operands of different signs. But considering HLSL's C heritage and the behavior of Clang frontend, we translate modulo operators into ``Op*Rem`` (there is no ``OpURem``).

For multiplications of float vectors and float scalars, the dedicated SPIR-V operation ``OpVectorTimesScalar`` will be used.

Bitwise operators
+++++++++++++++++

`Bitwise operators <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509631(v=vs.85).aspx#Bitwise_Operators>`_ (``~``, ``&``, ``|``, ``^``, ``<<``, ``>>``) are translated into their corresponding SPIR-V opcodes according to the following table.

+--------+-----------------------------+-------------------------------+
|        | (Vector of) Signed Integers | (Vector of) Unsigned Integers |
+--------+-----------------------------+-------------------------------+
| ``~``  |                         ``OpNot``                           |
+--------+-------------------------------------------------------------+
| ``&``  |                      ``OpBitwiseAnd``                       |
+--------+-------------------------------------------------------------+
| ``|``  |                      ``OpBitwiseOr``                        |
+--------+-----------------------------+-------------------------------+
| ``^``  |                      ``OpBitwiseXor``                       |
+--------+-----------------------------+-------------------------------+
| ``<<`` |                   ``OpShiftLeftLogical``                    |
+--------+-----------------------------+-------------------------------+
| ``>>`` | ``OpShiftRightArithmetic``  | ``OpShiftRightLogical``       |
+--------+-----------------------------+-------------------------------+

Comparison operators
++++++++++++++++++++

`Comparison operators <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509631(v=vs.85).aspx#Comparison_Operators>`_ (``<``, ``<=``, ``>``, ``>=``, ``==``, ``!=``) are translated into their corresponding SPIR-V opcodes according to the following table.

+--------+-----------------------------+-------------------------------+------------------------------+
|        | (Vector of) Signed Integers | (Vector of) Unsigned Integers |     (Vector of) Floats       |
+--------+-----------------------------+-------------------------------+------------------------------+
| ``<``  |  ``OpSLessThan``            |  ``OpULessThan``              |  ``OpFOrdLessThan``          |
+--------+-----------------------------+-------------------------------+------------------------------+
| ``<=`` |  ``OpSLessThanEqual``       |  ``OpULessThanEqual``         |  ``OpFOrdLessThanEqual``     |
+--------+-----------------------------+-------------------------------+------------------------------+
| ``>``  |  ``OpSGreaterThan``         |  ``OpUGreaterThan``           |  ``OpFOrdGreaterThan``       |
+--------+-----------------------------+-------------------------------+------------------------------+
| ``>=`` |  ``OpSGreaterThanEqual``    |  ``OpUGreaterThanEqual``      |  ``OpFOrdGreaterThanEqual``  |
+--------+-----------------------------+-------------------------------+------------------------------+
| ``==`` |                     ``OpIEqual``                            |  ``OpFOrdEqual``             |
+--------+-------------------------------------------------------------+------------------------------+
| ``!=`` |                     ``OpINotEqual``                         |  ``OpFOrdNotEqual``          |
+--------+-------------------------------------------------------------+------------------------------+

Note that for comparison of (vectors of) floats, SPIR-V has two sets of instructions: ``OpFOrd*``, ``OpFUnord*``. We translate into ``OpFOrd*`` ones.

Control flows
-------------

[TODO]

Functions
---------

All functions reachable from the entry-point function will be translated into SPIR-V code. Functions not reachable from the entry-point function will be ignored.

Function parameter
++++++++++++++++++

For a function ``f`` which has a parameter of type ``T``, the generated SPIR-V signature will use type ``T*`` for the parameter. At every call site of ``f``, additional local variables will be allocated to hold the actual arguments. The local variables are passed in as direct function arguments. For example::

  // HLSL source code

  float4 f(float a, int b) { ... }

  void caller(...) {
    ...
    float4 result = f(...);
    ...
  }

  // SPIR-V code

                ...
  %i32PtrType = OpTypePointer Function %int
  %f32PtrType = OpTypePointer Function %float
      %fnType = OpTypeFunction %v4float %f32PtrType %i32PtrType
                ...

           %f = OpFunction %v4float None %fnType
           %a = OpFunctionParameter %f32PtrType
           %b = OpFunctionParameter %i32PtrType
                ...

      %caller = OpFunction ...
                ...
     %aAlloca = OpVariable %_ptr_Function_float Function
     %bAlloca = OpVariable %_ptr_Function_int Function
                ...
                OpStore %aAlloca ...
                OpStore %bAlloca ...
      %result = OpFunctioncall %v4float %f %aAlloca %bAlloca
                ...

This approach gives us unified handling of function parameters and local variables: both of them are accessed via load/store instructions.

Builtin functions
-----------------

[TODO]

Logistics
=========

Project planning
----------------

We use `GitHub Project feature in the Google fork repo <https://github.com/google/DirectXShaderCompiler/projects/1>`_ to manage tasks and track progress.

Pull requests and code review
-----------------------------

Pull requests are very welcome! However, the Google repo is only used for project planning. We do not intend to maintain a detached fork; so all pull requests should be sent against the original `Microsoft repo <https://github.com/Microsoft/DirectXShaderCompiler>`_. Code reviews will also happen there.

For each pull request, please make sure

- You express your intent in the Google fork to avoid duplicate work.
- Tests are written to cover the modifications.
- This doc is updated for newly supported features.

Testing
-------

We will use `googletest <https://github.com/google/googletest>`_ as the unit test and codegen test framework. Appveyor will be used to check regression of all pull requests.
