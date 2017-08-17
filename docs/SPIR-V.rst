==============
SPIR-V Codegen
==============

.. contents::
   :local:
   :depth: 3

Introduction
============

This document describes the designs and logistics for supporting SPIR-V codegen
functionality. `SPIR-V <https://www.khronos.org/registry/spir-v/>`_ is a binary
intermediate language for representing graphical-shader stages and compute
kernels for multiple Khronos APIs, such as Vulkan, OpenGL, and OpenCL. At the
moment we only intend to support the Vulkan flavor of SPIR-V.

DirectXShaderCompiler is the reference compiler for HLSL. Adding SPIR-V codegen
in DirectXShaderCompiler will enable the usage of HLSL as a frontend language
for Vulkan shader programming. Sharing the same code base also means we can
track the evolution of HLSL more closely and always deliver the best of HLSL to
developers. Moreover, developers will also have a unified compiler toolchain for
targeting both DirectX and Vulkan. We believe this effort will benefit the
general graphics ecosystem.

Mapping From HLSL to SPIR-V for Vulkan
======================================

Due to the differences of semantics between DirectX and Vulkan, certain HLSL
features do not have corresponding mappings in Vulkan, and certain Vulkan
specific information does not have native ways to express in HLSL source code.
This section will capture the mappings we use to conduct the translation.
Specifically, it lists the mappings from HLSL shader model 6.0 to Vulkan flavor
of SPIR-V.

Note that this section is expected to be an ongoing effort and grow as we
implement more and more HLSL features. We are likely to extract the contents in
this section into a new doc in the future.

Note that the term "semantic" is overloaded. In HLSL, it can mean the string
attached to shader input or output. For such cases, we refer it as "HLSL
semantic" or "semantic string". For other cases, we just use the normal
"semantic" term.

Vulkan semantics
----------------

To provide additional information required by Vulkan in HLSL, we need to extend
the syntax of HLSL.
`C++ attribute specifier sequence <http://en.cppreference.com/w/cpp/language/attributes>`_
is a non-intrusive way of achieving such purpose.

For example, to specify the layout of Vulkan resources:

.. code:: hlsl

  [[vk::set(X), vk::binding(Y)]]
  tbuffer TbufOne {
    [[vk::offset(Z)]]
    float4 field;
  };

  [[vk::push_constant]]
  tbuffer TbufTwo {
    float4 field;
  };

  [[vk::constant_id(M)]]
  const int specConst = N;

The namespace ``vk`` will be used for all Vulkan attributes:

- ``location(X)``: For specifying the location number on stage input/outuput
  variables. Allowed on function parameters, function returns, and struct
  fields.

Only ``vk::`` attributes in the above list are supported. Other attributes will
result in warnings and be ignored by the compiler. All C++11 attributes will
only trigger warnings and be ignored if not compiling towards SPIR-V.

HLSL types
----------

This section lists how various HLSL types are mapped.

Normal scalar types
+++++++++++++++++++

`Normal scalar types <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509646(v=vs.85).aspx>`_
in HLSL are relatively easy to handle and can be mapped directly to SPIR-V
instructions:

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

HLSL also supports various
`minimal precision scalar types <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509646(v=vs.85).aspx>`_,
which graphics drivers can implement by using any precision greater than or
equal to their specified bit precision.

- ``min16float`` - minimum 16-bit floating point value
- ``min10float`` - minimum 10-bit floating point value
- ``min16int`` - minimum 16-bit signed integer
- ``min12int`` - minimum 12-bit signed integer
- ``min16uint`` - minimum 16-bit unsigned integer

There are no direct mapping in SPIR-V for these types. We may need to use
``OpTypeFloat``/``OpTypeInt`` with ``RelaxedPrecision`` for some of them and
issue warnings/errors for the rest.

Vectors and matrices
++++++++++++++++++++

`Vectors <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509707(v=vs.85).aspx>`_
and `matrices <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509623(v=vs.85).aspx>`_
are translated into:

+-------------------------------------+---------------------------------------+
|               HLSL                  |             SPIR-V                    |
+=====================================+=======================================+
| ``|type||count|``                   |                                       |
+-------------------------------------+  ``OpTypeVector |type| |count|``      |
| ``vector<|type|, |count|>``         |                                       |
+-------------------------------------+---------------------------------------+
| ``matrix<|type|, |row|, |column|>`` | ``%v = OpTypeVector |type| |column|`` |
+-------------------------------------+                                       |
| ``|type||row|x|column|``            | ``OpTypeMatrix %v |row|``             |
+-------------------------------------+---------------------------------------+

Note that vectors of size 1 are just translated into scalar values of the
element types since SPIR-V mandates the size of vector to be at least 2.

Also, matrices whose row or column count is 1 are translated into the
corresponding vector types with the same element type. Matrices of size 1x1 are
translated into scalars.

A MxN HLSL matrix is translated into a SPIR-V matrix with M columns, each with
N elements. Conceptually HLSL matrices are row-major while SPIR-V matrices are
column-major, thus all HLSL matrices are represented by their transposes.
Doing so may require special handling of certain matrix operations:

- **Indexing**: no special handling required. ``matrix[m][n]`` will still access
  the correct element since ``m``/``n`` means the ``m``-th/``n``-th row/column
  in HLSL but ``m``-th/``n``-th column/element in SPIR-V.
- **Per-element operation**: no special handling required.
- **Matrix multiplication**: need to swap the operands. ``mat1 x mat2`` should
  be translated as ``transpose(mat2) x transpose(mat1)``. Then the result is
  ``transpose(mat1 x mat2)``.
- **Storage layout**: ``row_major``/``column_major`` will be translated into
  SPIR-V ``ColMajor``/``RowMajor`` decoration. This is because HLSL matrix
  row/column becomes SPIR-V matrix column/row. If elements in a row/column are
  packed together, they should be loaded into a column/row correspondingly.

Structs
+++++++

`Structs <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509668(v=vs.85).aspx>`_
in HLSL are defined in the a format similar to C structs. They are translated
into SPIR-V ``OpTypeStruct``. Semantics attached to struct members are handled
in the `entry function wrapper`_.

Structs can have optional interpolation modifiers for members:

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

`User-defined types <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509702(v=vs.85).aspx>`_
are type aliases introduced by typedef. No new types are introduced and we can
rely on Clang to resolve to the original types.

Samplers
++++++++

All `sampler types <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509644(v=vs.85).aspx>`_
will be translated into SPIR-V ``OpTypeSampler``.

SPIR-V ``OpTypeSampler`` is an opaque type that cannot be parameterized;
therefore state assignments on sampler types is not supported (yet).

Textures
++++++++

`Texture types <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509700(v=vs.85).aspx>`_
are translated into SPIR-V ``OpTypeImage``, with parameters:

====================   ==== ===== ======= == ======= ============
HLSL Texture Type      Dim  Depth Arrayed MS Sampled Image Format
====================   ==== ===== ======= == ======= ============
``Texture1D``          1D    0       0    0    1       Unknown
``Texture2D``          2D    0       0    0    1       Unknown
``Texture3D``          3D    0       0    0    1       Unknown
``TextureCube``        Cube  0       0    0    1       Unknown
``Texture1DArray``     1D    0       1    0    1       Unknown
``Texture2DArray``     2D    0       1    0    1       Unknown
``TextureCubeArray``   3D    0       1    0    1       Unknown
====================   ==== ===== ======= == ======= ============

The meanings of the headers in the above table is explained in ``OpTypeImage``
of the SPIR-V spec.

Buffers
+++++++

[TODO]

HLSL variables and resources
----------------------------

This section lists how various HLSL variables and resources are mapped.

Variable definition
+++++++++++++++++++

Variables are defined in HLSL using the following
`syntax <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509706(v=vs.85).aspx>`_
rules::

  [StorageClass] [TypeModifier] Type Name[Index]
      [: Semantic]
      [: Packoffset]
      [: Register];
      [Annotations]
      [= InitialValue]

Storage class
+++++++++++++

Normal local variables (without any modifier) will be placed in the ``Function``
SPIR-V storage class.

``static``
~~~~~~~~~~

- Global variables with ``static`` modifier will be placed in the ``Private``
  SPIR-V storage class. Initalizers of such global variables will be translated
  into SPIR-V ``OpVariable`` initializers if possible; otherwise, they will be
  initialized at the very beginning of the entry function wrapper using SPIR-V
  ``OpStore``.
- Local variables with ``static`` modifier will also be placed in the
  ``Private`` SPIR-V storage class. initializers of such local variables will
  also be translated into SPIR-V ``OpVariable`` initializers if possible;
  otherwise, they will be initialized at the very beginning of the enclosing
  function. To make sure that such a local variable is only initialized once,
  a second boolean variable of the ``Private`` SPIR-V storage class will be
  generated to mark its initialization status.

Type modifier
+++++++++++++

[TODO]

HLSL semantic
+++++++++++++

Direct3D uses HLSL "`semantics <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509647(v=vs.85).aspx>`_"
to compose and match the interfaces between subsequent stages. These semantic
strings can appear after struct members, function parameters and return
values. E.g.,

.. code:: hlsl

  struct VSInput {
    float4 pos  : POSITION;
    float3 norm : NORMAL;
  };

  float4 VSMain(in  VSInput input,
                in  float4  tex   : TEXCOORD,
                out float4  pos   : SV_Position) : TEXCOORD {
    pos = input.pos;
    return tex;
  }

In contrary, Vulkan stage input and output interface matching is via explicit
``Location`` numbers. Details can be found `here <https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/html/vkspec.html#interfaces-iointerfaces>`_.

To translate HLSL to SPIR-V for Vulkan, semantic strings need to be mapped to
Vulkan ``Location`` numbers properly. This can be done either explicitly via
information provided by the developer or implicitly by the compiler.

Explicit ``Location`` number assignment in source code
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``[[vk::location(X)]]`` can be attached to the entities where semantic are
allowed to attach (struct fields, function parameters, and function returns).
For the above exmaple we can have:

.. code:: hlsl

  struct VSInput {
    [[vk::location(0)]] float4 pos  : POSITION;
    [[vk::location(1)]] float3 norm : NORMAL;
  };

  [[vk::location(1)]]
  float4 VSMain(in  VSInput input,
                [[vk::location(2)]]
                in  float4  tex     : TEXCOORD,
                out float4  pos     : SV_Position) : TEXCOORD {
    pos = input.pos;
    return tex;
  }

In the above, input ``POSITION``, ``NORMAL``, and ``TEXCOORD`` will be mapped to
``Location`` 0, 1, and 2, respectively, and output ``TEXCOORD`` will be mapped
to ``Location`` 1.

[TODO] Another explicit way: using command-line options

Please note that the compiler does prohibits mixing the explicit and implicit
approach for the same SigPoint to avoid complexity and fallibility. However,
for a certain shader stage, one SigPoint using the explicit approach while the
other adopting the implicit approach is permitted.

Implicit ``Location`` number assignment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Without hints from the developer, the compiler will try its best to map
semantics to ``Location`` numbers. However, there is no single rule for this
mapping; semantic strings should be handled case by case.

Firstly, under certain `SigPoints <https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/DXIL.rst#hlsl-signatures-and-semantics>`_,
some system-value (SV) semantic strings will be translated into SPIR-V
``BuiltIn`` decorations:

+----------------------+----------+--------------------+-----------------------+
| HLSL Semantic        | SigPoint | SPIR-V ``BuiltIn`` | SPIR-V Execution Mode |
+======================+==========+====================+=======================+
|                      | VSOut    | ``Position``       | N/A                   |
| SV_Position          +----------+--------------------+-----------------------+
|                      | PSIn     | ``FragCoord``      | N/A                   |
+----------------------+----------+--------------------+-----------------------+
| SV_VertexID          | VSIn     | ``VertexIndex``    | N/A                   |
+----------------------+----------+--------------------+-----------------------+
| SV_InstanceID        | VSIn     | ``InstanceIndex``  | N/A                   |
+----------------------+----------+--------------------+-----------------------+
| SV_Depth             | PSOut    | ``FragDepth``      | N/A                   |
+----------------------+----------+--------------------+-----------------------+
| SV_DepthGreaterEqual | PSOut    | ``FragDepth``      | ``DepthGreater``      |
+----------------------+----------+--------------------+-----------------------+
| SV_DepthLessEqual    | PSOut    | ``FragDepth``      | ``DepthLess``         |
+----------------------+----------+--------------------+-----------------------+

[TODO] add other SV semantic strings in the above

For entities (function parameters, function return values, struct fields) with
the above SV semantic strings attached, SPIR-V variables of the
``Input``/``Output`` storage class will be created. They will have the
corresponding SPIR-V ``Builtin``  decorations according to the above table.

SV semantic strings not translated into SPIR-V BuiltIn decorations will be
handled similarly as non-SV (arbitrary) semantic strings: a SPIR-V variable
of the ``Input``/``Output`` storage class will be created for each entity with
such semantic string. Then sort all semantic strings alphabetically, and assign
``Location`` numbers sequentially to each SPIR-V variable. Note that this means
flattening all structs if structs are used as function parameters or returns.

There is an exception to the above rule for SV_Target[N]. It will always be
mapped to ``Location`` number N.

HLSL expressions
----------------

Unless explicitly noted, matrix per-element operations will be conducted on
each component vector and then collected into the result matrix. The following
sections lists the SPIR-V opcodes for scalars and vectors.

Arithmetic operators
++++++++++++++++++++

`Arithmetic operators <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509631(v=vs.85).aspx#Additive_and_Multiplicative_Operators>`_
(``+``, ``-``, ``*``, ``/``, ``%``) are translated into their corresponding
SPIR-V opcodes according to the following table.

+-------+-----------------------------+-------------------------------+--------------------+
|       | (Vector of) Signed Integers | (Vector of) Unsigned Integers | (Vector of) Floats |
+=======+=============================+===============================+====================+
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

Note that for modulo operation, SPIR-V has two sets of instructions: ``Op*Rem``
and ``Op*Mod``. For ``Op*Rem``, the sign of a non-0 result comes from the first
operand; while for ``Op*Mod``, the sign of a non-0 result comes from the second
operand. HLSL doc does not mandate which set of instructions modulo operations
should be translated into; it only says "the % operator is defined only in cases
where either both sides are positive or both sides are negative." So technically
it's undefined behavior to use the modulo operation with operands of different
signs. But considering HLSL's C heritage and the behavior of Clang frontend, we
translate modulo operators into ``Op*Rem`` (there is no ``OpURem``).

For multiplications of float vectors and float scalars, the dedicated SPIR-V
operation ``OpVectorTimesScalar`` will be used. Similarly, for multiplications
of float matrices and float scalars, ``OpMatrixTimesScalar`` will be generated.

Bitwise operators
+++++++++++++++++

`Bitwise operators <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509631(v=vs.85).aspx#Bitwise_Operators>`_
(``~``, ``&``, ``|``, ``^``, ``<<``, ``>>``) are translated into their
corresponding SPIR-V opcodes according to the following table.

+--------+-----------------------------+-------------------------------+
|        | (Vector of) Signed Integers | (Vector of) Unsigned Integers |
+========+=============================+===============================+
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

`Comparison operators <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509631(v=vs.85).aspx#Comparison_Operators>`_
(``<``, ``<=``, ``>``, ``>=``, ``==``, ``!=``) are translated into their
corresponding SPIR-V opcodes according to the following table.

+--------+-----------------------------+-------------------------------+------------------------------+
|        | (Vector of) Signed Integers | (Vector of) Unsigned Integers |     (Vector of) Floats       |
+========+=============================+===============================+==============================+
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

Note that for comparison of (vectors of) floats, SPIR-V has two sets of
instructions: ``OpFOrd*``, ``OpFUnord*``. We translate into ``OpFOrd*`` ones.

Boolean math operators
++++++++++++++++++++++

`Boolean match operators <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509631(v=vs.85).aspx#Boolean_Math_Operators>`_
(``&&``, ``||``, ``?:``) are translated into their corresponding SPIR-V opcodes
according to the following table.

+--------+----------------------+
|        | (Vector of) Booleans |
+========+======================+
| ``&&`` |  ``OpLogicalAnd``    |
+--------+----------------------+
| ``||`` |  ``OpLogicalOr``     |
+--------+----------------------+
| ``?:`` |  ``OpSelect``        |
+--------+----------------------+

Please note that "unlike short-circuit evaluation of ``&&``, ``||``, and ``?:``
in C, HLSL expressions never short-circuit an evaluation because they are vector
operations. All sides of the expression are always evaluated."

Unary operators
+++++++++++++++

For `unary operators <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509631(v=vs.85).aspx#Unary_Operators>`_:

- ``!`` is translated into ``OpLogicalNot``. Parsing will gurantee the operands
  are of boolean types by inserting necessary casts.
- ``+`` requires no additional SPIR-V instructions.
- ``-`` is translated into ``OpSNegate`` and ``OpFNegate`` for (vectors of)
  integers and floats, respectively.

Casts
+++++

Casting between (vectors) of scalar types is translated according to the following table:

+------------+-------------------+-------------------+-------------------+-------------------+
| From \\ To |        Bool       |       SInt        |      UInt         |       Float       |
+============+===================+===================+===================+===================+
|   Bool     |       no-op       |                 select between one and zero               |
+------------+-------------------+-------------------+-------------------+-------------------+
|   SInt     |                   |     no-op         |  ``OpBitcast``    | ``OpConvertSToF`` |
+------------+                   +-------------------+-------------------+-------------------+
|   UInt     | compare with zero |   ``OpBitcast``   |      no-op        | ``OpConvertUToF`` |
+------------+                   +-------------------+-------------------+-------------------+
|   Float    |                   | ``OpConvertFToS`` | ``OpConvertFToU`` |      no-op        |
+------------+-------------------+-------------------+-------------------+-------------------+

Indexing operator
+++++++++++++++++

The ``[]`` operator can also be used to access elements in a matrix or vector.
A matrix whose row and/or column count is 1 will be translated into a vector or
scalar. If a variable is used as the index for the dimension whose count is 1,
that variable will be ignored in the generated SPIR-V code. This is because
out-of-bound indexing triggers undefined behavior anyway. For example, for a
1xN matrix ``mat``, ``mat[index][0]`` will be translated into
``OpAccessChain ... %mat %uint_0``. Similarly, variable index into a size 1
vector will also be ignored and the only element will be always returned.

HLSL control flows
------------------

This section lists how various HLSL control flows are mapped.

Switch statement
++++++++++++++++

HLSL `switch statements <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509669(v=vs.85).aspx>`_
are translated into SPIR-V using:

- **OpSwitch**: if (all case values are integer literals or constant integer
  variables) and (no attribute or the ``forcecase`` attribute is specified)
- **A series of if statements**: for all other scenarios (e.g., when
  ``flatten``, ``branch``, or ``call`` attribute is specified)

Loops (for, while, do)
++++++++++++++++++++++

HLSL `for statements <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509602(v=vs.85).aspx>`_,
`while statements <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509708(v=vs.85).aspx>`_,
and `do statements <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509593(v=vs.85).aspx>`_
are translated into SPIR-V by constructing all necessary basic blocks and using
``OpLoopMerge`` to organize as structured loops.

The HLSL attributes for these statements are translated into SPIR-V loop control
masks according to the following table:

+-------------------------+--------------------------------------------------+
|   HLSL loop attribute   |            SPIR-V Loop Control Mask              |
+=========================+==================================================+
|        ``unroll(x)``    |                ``Unroll``                        |
+-------------------------+--------------------------------------------------+
|         ``loop``        |              ``DontUnroll``                      |
+-------------------------+--------------------------------------------------+
|        ``fastopt``      |              ``DontUnroll``                      |
+-------------------------+--------------------------------------------------+
| ``allow_uav_condition`` |           Currently Unimplemented                |
+-------------------------+--------------------------------------------------+

HLSL functions
--------------

All functions reachable from the entry-point function will be translated into
SPIR-V code. Functions not reachable from the entry-point function will be
ignored.

Entry function wrapper
++++++++++++++++++++++

HLSL entry functions takes in parameters and returns values. These parameters
and return values can have semantics attached or if they are struct type,
the struct fields can have semantics attached. However, in Vulkan, the entry
function must be of the ``void(void)`` signature. To handle this difference,
for a given entry function ``main``, we will emit a wrapper function for it.

The wrapper function will take the name of the source code entry function,
while the source code entry function will have its name prefixed with "src.".
The wrapper function reads in stage input/builtin variables created according
to semantics and groups them into composites meeting the requirements of the
source code entry point. Then the wrapper calls the source code entry point.
The return value is extracted and components of it will be written to stage
output/builtin variables created according to semantics. For example:


.. code:: hlsl

  // HLSL source code

  struct S {
    bool a : A;
    uint2 b: B;
    float2x3 c: C;
  };

  struct T {
    S x;
    int y: D;
  };

  T main(T input) {
    return input;
  }


.. code:: spirv

  ; SPIR-V code

  %in_var_A = OpVariable %_ptr_Input_bool Input
  %in_var_B = OpVariable %_ptr_Input_v2uint Input
  %in_var_C = OpVariable %_ptr_Input_mat2v3float Input
  %in_var_D = OpVariable %_ptr_Input_int Input

  %out_var_A = OpVariable %_ptr_Output_bool Output
  %out_var_B = OpVariable %_ptr_Output_v2uint Output
  %out_var_C = OpVariable %_ptr_Output_mat2v3float Output
  %out_var_D = OpVariable %_ptr_Output_int Output

  ; Wrapper function starts

  %main    = OpFunction %void None {{%\d+}}
  {{%\d+}} = OpLabel

  %param_var_input = OpVariable %_ptr_Function_T Function

  ; Load stage input variables and group into the expected composite

  [[inA:%\d+]]     = OpLoad %bool %in_var_A
  [[inB:%\d+]]     = OpLoad %v2uint %in_var_B
  [[inC:%\d+]]     = OpLoad %mat2v3float %in_var_C
  [[inS:%\d+]]     = OpCompositeConstruct %S [[inA]] [[inB]] [[inC]]
  [[inD:%\d+]]     = OpLoad %int %in_var_D
  [[inT:%\d+]]     = OpCompositeConstruct %T [[inS]] [[inD]]
                     OpStore %param_var_input [[inT]]

  [[ret:%\d+]]  = OpFunctionCall %T %src_main %param_var_input

  ; Extract component values from the composite and store into stage output variables

  [[outS:%\d+]] = OpCompositeExtract %S [[ret]] 0
  [[outA:%\d+]] = OpCompositeExtract %bool [[outS]] 0
                  OpStore %out_var_A [[outA]]
  [[outB:%\d+]] = OpCompositeExtract %v2uint [[outS]] 1
                  OpStore %out_var_B [[outB]]
  [[outC:%\d+]] = OpCompositeExtract %mat2v3float [[outS]] 2
                  OpStore %out_var_C [[outC]]
  [[outD:%\d+]] = OpCompositeExtract %int [[ret]] 1
                  OpStore %out_var_D [[outD]]

  OpReturn
  OpFunctionEnd

  ; Source code entry point starts

  %src_main = OpFunction %T None ...

In this way, we can concentrate all stage input/output/builtin variable
manipulation in the wrapper function and handle the source code entry function
just like other nomal functions.

Function parameter
++++++++++++++++++

For a function ``f`` which has a parameter of type ``T``, the generated SPIR-V
signature will use type ``T*`` for the parameter. At every call site of ``f``,
additional local variables will be allocated to hold the actual arguments.
The local variables are passed in as direct function arguments. For example:

.. code:: hlsl

  // HLSL source code

  float4 f(float a, int b) { ... }

  void caller(...) {
    ...
    float4 result = f(...);
    ...
  }

.. code:: spirv

  ; SPIR-V code

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

This approach gives us unified handling of function parameters and local
variables: both of them are accessed via load/store instructions.

Intrinsic functions
+++++++++++++++++++

The following intrinsic HLSL functions are currently supported:

- ``dot`` : performs dot product of two vectors, each containing floats or
  integers. If the two parameters are vectors of floats, we use SPIR-V's
  ``OpDot`` instruction to perform the translation. If the two parameters are
  vectors of integers, we multiply corresponding vector elementes using
  ``OpIMul`` and accumulate the results using ``OpIAdd`` to compute the dot
  product.
- ``all``: returns true if all components of the given scalar, vector, or
  matrix are true. Performs conversions to boolean where necessary. Uses SPIR-V
  ``OpAll`` for scalar arguments and vector arguments. For matrix arguments,
  performs ``OpAll`` on each row, and then again on the vector containing the
  results of all rows.
- ``any``: returns true if any component of the given scalar, vector, or matrix
  is true. Performs conversions to boolean where necessary. Uses SPIR-V
  ``OpAny`` for scalar arguments and vector arguments. For matrix arguments,
  performs ``OpAny`` on each row, and then again on the vector containing the
  results of all rows.
- ``asfloat``: converts the component type of a scalar/vector/matrix from float,
  uint, or int into float. Uses ``OpBitcast``. This method currently does not
  support taking non-float matrix arguments.
- ``asint``: converts the component type of a scalar/vector/matrix from float or
  uint into int. Uses ``OpBitcast``. This method currently does not support
  conversion into integer matrices.
- ``asuint``: converts the component type of a scalar/vector/matrix from float
  or int into uint. Uses ``OpBitcast``. This method currently does not support
  conversion into unsigned integer matrices.

- Using SPIR-V Extended Instructions for GLSL: the following intrinsic HLSL
functions are translated using their equivalent instruction in the
`GLSL extended instruction set <https://www.khronos.org/registry/spir-v/specs/1.0/GLSL.std.450.html>`_.

+-----------------------------+-----------------------------------------------------+
|   HLSL intrinsic function   |               GLSL Extended Instruction             |
+=============================+=====================================================+
|        ``abs``              |   ``SAbs`` for ints, and ``FAbs`` for floats        |
+-----------------------------+-----------------------------------------------------+
|        ``acos``             |                       ``Acos``                      |
+-----------------------------+-----------------------------------------------------+
|        ``asin``             |                       ``Asin``                      |
+-----------------------------+-----------------------------------------------------+
|        ``atan``             |                       ``Atan``                      |
+-----------------------------+-----------------------------------------------------+
|        ``ceil``             |                       ``Ceil``                      |
+-----------------------------+-----------------------------------------------------+
|        ``cos``              |                       ``Cos``                       |
+-----------------------------+-----------------------------------------------------+
|        ``cosh``             |                       ``Cosh``                      |
+-----------------------------+-----------------------------------------------------+
|       ``degrees``           |                      ``Degrees``                    |
+-----------------------------+-----------------------------------------------------+
|       ``radians``           |                      ``Radian``                     |
+-----------------------------+-----------------------------------------------------+
|    ``determinant``          |                   ``Determinant``                   |
+-----------------------------+-----------------------------------------------------+
|        ``exp``              |                       ``Exp``                       |
+-----------------------------+-----------------------------------------------------+
|        ``exp2``             |                       ``exp2``                      |
+-----------------------------+-----------------------------------------------------+
|        ``floor``            |                       ``Floor``                     |
+-----------------------------+-----------------------------------------------------+
|      ``length``             |                     ``Length``                      |
+-----------------------------+-----------------------------------------------------+
|        ``log``              |                       ``Log``                       |
+-----------------------------+-----------------------------------------------------+
|        ``log2``             |                       ``Log2``                      |
+-----------------------------+-----------------------------------------------------+
|     ``normalize``           |                   ``Normalize``                     |
+-----------------------------+-----------------------------------------------------+
|        ``round``            |                      ``Round``                      |
+-----------------------------+-----------------------------------------------------+
|       ``rsqrt``             |                  ``InverseSqrt``                    |
+-----------------------------+-----------------------------------------------------+
|       ``sign``              |   ``SSign`` for ints, and ``FSign`` for floats      |
+-----------------------------+-----------------------------------------------------+
|        ``sin``              |                       ``Sin``                       |
+-----------------------------+-----------------------------------------------------+
|        ``sinh``             |                       ``Sinh``                      |
+-----------------------------+-----------------------------------------------------+
|        ``tan``              |                       ``Tan``                       |
+-----------------------------+-----------------------------------------------------+
|        ``tanh``             |                       ``Tanh``                      |
+-----------------------------+-----------------------------------------------------+
|        ``sqrt``             |                       ``Sqrt``                      |
+-----------------------------+-----------------------------------------------------+
|       ``trunc``             |                      ``Trunc``                      |
+-----------------------------+-----------------------------------------------------+

Designs
=======

Various designs are driven by technical considerations together with the
following guidelines for good citizenship within DirectXShaderCompiler:

- Conduct minimal changes to existing interfaces and libraries
- Perfer less intrusive solutions

General approach
----------------

The general approach is to translate frontend AST directly into SPIR-V binary.
We choose this approach considering that

- Frontend AST is much more higher-level than DXIL. For example,
  `DXIL scalarized vectors <https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/DXIL.rst#vectors>`_
  but SPIR-V has native support.
- DXIL has widely different semantics than Vulkan flavor of SPIR-V. For example,
  `structured control flow is not preserved in DXIL <https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/DXIL.rst#control-flow-restrictions>`_
  but SPIR-V for Vulkan requires it.
- Frontend AST perserves the information in the source code better.
- Also, the right place to generate error messages is in Clang's semantic
  analysis step, which is when the compiler is still processing the AST.

Therefore, it is easier to go from frontend AST to SPIR-V than from DXIL since
we do not need to rediscover certain information.

LLVM optimization passes
++++++++++++++++++++++++

Translating frontend AST directly into SPIR-V binary precludes the usage of
existing LLVM optimization passes. This is expected since there are also subtle
semantics differences between SPIR-V and LLVM IR. Certain concepts in SPIR-V
do not have direct corresponding representation in LLVM IR and there are no
existing translation schemes handling the differences. Using vanilla LLVM
optimization passes will likely violate the requirements of SPIR-V and results
in invalid SPIR-V modules.

Instead, optimizations are available in the
`SPIRV-Tools <https://github.com/KhronosGroup/SPIRV-Tools>`_ project.

Library
-------

On the library side, this means introducing a new ``ASTFrontendAction`` and a
SPIR-V module builder.  The new frontend action will traverse the AST and call
the SPIR-V module builder to construct SPIR-V words. These code should be
placed at ``tools/clang/lib/SPIRV`` and packed into one library (or multiple
libraries in the future).

Detailed design will be revised to accommodate more and more HLSL features.
At the moment, we have::

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

- ``SPIRVEmitter``: The derived ``ASTConsumer`` which acts on various frontend
  AST nodes by calling corresponding ``ModuleBuilder`` methods to build SPIR-V
  modules gradually.
- ``ModuleBuilder``: Exposes API for constructing SPIR-V modules. Internally it
  has structured representation of SPIR-V modules, functions, basic blocks as
  well as various SPIR-V specific structs like entry points, debug names, and
  so on.
- ``SPIRVContext``: Responsible for <result-id> allocation and maintaining the
  lifetime of objects allocated to represent types, decorations, and others.
  It is used in conjunction with ``ModuleBuilder``.
- ``InstBuilder``: The low-level interface for generating SPIR-V words for
  various SPIR-V instructions. All SPIR-V instructions are eventually serialized
  via ``InstBuilder``.
- ``WordConsumer``: The consumer of generated SPIR-V words.

Command-line tool
-----------------

On the command-line tool side, this means introducing a new binary,
``hlsl2spirv`` to wrap around the library functionality.

But as the initial scaffolding step, a new option, ``-spirv``, will be added
into ``dxc`` for invoking the new SPIR-V codegen action.

Testing
-------

`GoogleTest <https://github.com/google/googletest>`_ will be used as both the
unit test and the SPIR-V codegen test framework.

Unit tests will be placed under the ``tools/clang/unittests/SPIRV/`` directory,
while SPIR-V codegen tests will be placed under the
``tools/clang/test/CodeGenSPIRV/`` directory.

For SPIR-V codegen tests, there are two test fixtures: one for checking the
whole disassembly of the generated SPIR-V code, the other is
`FileCheck <https://llvm.org/docs/CommandGuide/FileCheck.html>`_-like, for
partial pattern matching.

- **Whole disassembly check**: These tests are in files with suffix
  ``.hlsl2spv``. Each file consists of two parts, HLSL source code input and
  expected SPIR-V disassembly ouput, delimited by ``// CHECK-WHOLE-SPIR-V:``.
  The compiler takes in the whole file as input and compile its into SPIR-V
  binary. The test fixture then disasembles the SPIR-V binary and compares the
  disassembly with the expected disassembly listed after
  ``// CHECK-WHOLE-SPIR-V``.
- **Partial disassembly match**: These tests are in files with suffix ``.hlsl``.
  `Effcee <https://github.com/google/effcee>`_ is used for the stateful pattern
  matching. Effcee itself depends on a regular expression library,
  `RE2 <https://github.com/google/re2>`_. See Effcee for supported ``CHECK``
  syntax. They are largely the same as LLVM FileCheck.

Dependencies
------------

SPIR-V codegen functionality will require two external projects:
`SPIRV-Headers <https://github.com/KhronosGroup/SPIRV-Headers>`_
(for ``spirv.hpp11``) and
`SPIRV-Tools <https://github.com/KhronosGroup/SPIRV-Tools>`_
(for SPIR-V disassembling). These two projects should be checked out under
the ``external/`` directory.

The three projects for testing, GoogleTest, Effcee, and RE2, should also be
checked out under the ``external/`` directory.

Build system
------------

SPIR-V codegen functionality will structured as an optional feature in
DirectXShaderCompiler. Two new CMake options will be introduced to control the
configuring and building SPIR-V codegen:

- ``ENABLE_SPIRV_CODEGEN``: If turned on, enables the SPIR-V codegen
  functionality. (Default: OFF)
- ``SPIRV_BUILD_TESTS``: If turned on, enables building of SPIR-V related tests.
  This option will also implicitly turn on ``ENABLE_SPIRV_CODEGEN``.
  (Default: OFF)

For building, ``hctbuild`` will be extended with two new switches, ``-spirv``
and ``-spirvtest``, to turn on the above two options, respectively.

For testing, ``hcttest spirv`` will run all existing tests together with SPIR-V
tests, while ``htctest spirv_only`` will only trigger SPIR-V tests.

Logistics
=========

Project planning
----------------

We use `GitHub Project feature in the Google fork repo <https://github.com/google/DirectXShaderCompiler/projects/1>`_
to manage tasks and track progress.

Pull requests and code review
-----------------------------

Pull requests are very welcome! However, the Google repo is only used for
project planning. We do not intend to maintain a detached fork; so all pull
requests should be sent against the original `Microsoft repo <https://github.com/Microsoft/DirectXShaderCompiler>`_.
Code reviews will also happen there.

For each pull request, please make sure

- You express your intent in the Google fork to avoid duplicate work.
- Tests are written to cover the modifications.
- This doc is updated for newly supported features.

Testing
-------

We will use `googletest <https://github.com/google/googletest>`_ as the unit
test and codegen test framework. Appveyor will be used to check regression of
all pull requests.
