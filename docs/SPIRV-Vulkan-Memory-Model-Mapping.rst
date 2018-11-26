==================================
SPIR-V Vulkan Memory Model Mapping
==================================

.. contents::
  :local:
  :depth: 3

Introduction
============

**THIS IS A DRAFT PROPOSAL**

This document describes the HLSL mapping for the `Vulkan memory model <https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#memory-model>`_.

Background
==========

HLSL Barrier Intrinsics
-----------------------

- Memory scope:
  - GroupMemoryBarrier* use Workgroup memory scope
  - Otherwise, memory barriers use QueueFamilyKHR memory scope
- Execution scope: Workgroup
- Memory semantics:

  - Memory ordering: AcquireRelease (Vulkan does not support SequentiallyConsistent)
  - Storage classes:

    - GroupMemoryBarrier*: WorkgroupMemory
    - DeviceMemoryBarrier*: mutable memory accessible to *all* invocations on the device: ImageMemory, UniformMemory

      - Maps to QueueFamilyKHR scope

    - AllMemoryBarrier*: any mutable memory which can be shared between invocations on the device: (union of the above two cases): WorkgroupMemory, ImageMemory, UniformMemory

.. table:: Mapping memory barrier intrinsics.
+----------------------------------+------------------------------------------------------------------------------------------------+
| **HLSL**                         | **SPIR-V**                                                                                     |
+----------------------------------+------------------------------------------------------------------------------------------------+
| AllMemoryBarrier                 | OpMemoryBarrier                                                                                |
|                                  |                                                                                                |
|                                  | QueueFamilyKHR memory scope                                                                    |
|                                  |                                                                                                |
|                                  | Uniform|Image|Workgroup|AcquireRelease semantics                                               |
+----------------------------------+------------------------------------------------------------------------------------------------+
| AllMemoryBarrierWithGroupSync    | OpControlBarrier                                                                               |
|                                  |                                                                                                |
|                                  | Workgroup execution scope                                                                      |
|                                  |                                                                                                |
|                                  | QueueFamilyKHR memory scope                                                                    |
|                                  |                                                                                                |
|                                  | Uniform|Image|Workgroup|AcquireRelease semantics                                               |
+----------------------------------+------------------------------------------------------------------------------------------------+
| DeviceMemoryBarrier              | OpMemoryBarrier                                                                                |
|                                  |                                                                                                |
|                                  | QueueFamilyKHR memory scope                                                                    |
|                                  |                                                                                                |
|                                  | Uniform|Image|AcquireRelease semantics                                                         |
+----------------------------------+------------------------------------------------------------------------------------------------+
| DeviceMemoryBarrierWithGroupSync | OpControlBarrier                                                                               |
|                                  |                                                                                                |
|                                  | Workgroup execution scope                                                                      |
|                                  |                                                                                                |
|                                  | QueueFamilyKHR memory scope                                                                    |
|                                  |                                                                                                |
|                                  | Uniform|Image|AcquireRelease semantics                                                         |
+----------------------------------+------------------------------------------------------------------------------------------------+
| GroupMemoryBarrier               | OpMemoryBarrier                                                                                |
|                                  |                                                                                                |
|                                  | Workgroup memory scope                                                                         |
|                                  |                                                                                                |
|                                  | Workgroup|AcquireRelease semantics                                                             |
+----------------------------------+------------------------------------------------------------------------------------------------+
| DeviceMemoryBarrierWithGroupSync | OpControlBarrier                                                                               |
|                                  |                                                                                                |
|                                  | Workgroup execution scope                                                                      |
|                                  |                                                                                                |
|                                  | Workgroup memory scope                                                                         |
|                                  |                                                                                                |
|                                  | Workgroup|AcquireRelease semantics                                                             |
+----------------------------------+------------------------------------------------------------------------------------------------+

HLSL Interlocked Operation Intrinsics
-------------------------------------

* The scope on shared variable interlocked functions should be Workgroup. Otherwise, the scope should be QueueFamilyKHR.
* The semantics on all interlocked functions should be Relaxed.

  * No storage class bits are used since no ordering is being applied to
    locations outside the particular locations being atomically operated on

.. table:: Mapping interlocked intrinsics.
+----------------------------+---------------------------+
| **HLSL**                   | **SPIR-V**                |
+----------------------------+---------------------------+
| InterlockedAdd             | OpAtomicIAdd              |
+----------------------------+---------------------------+
| InterlockedAnd             | OpAtomicAnd               |
+----------------------------+---------------------------+
| InterlockedCompareExchange | OpAtomicCompareExchange   |
+----------------------------+---------------------------+
| InterlockedCompareStore    | OpAtomicCompareExchange   |
+----------------------------+---------------------------+
| InterlockedExchange        | OpAtomicExchange          |
+----------------------------+---------------------------+
| InterlockedMax             | OpAtomicSMax/OpAtomicUMax |
+----------------------------+---------------------------+
| InterlockedMin             | OpAtomicSMin/OpAtomicUMin |
+----------------------------+---------------------------+
| InterlockedOr              | OpAtomicOr                |
+----------------------------+---------------------------+
| InterlockedXor             | OpAtomicXor               |
+----------------------------+---------------------------+

HLSL Variables
--------------

.. table:: Variable modifiers.
+--------------+------------------------------------------------+
| **Modifier** | **Description**                                |
+--------------+------------------------------------------------+
| shared       | Mark a variable for sharing between effects;   |
|              | this is a hint to the compiler                 |
+--------------+------------------------------------------------+
| groupshared  | Mark a variable for thread-group-shared memory |
|              | for compute shaders.                           |
+--------------+------------------------------------------------+

* groupshared variable should be Workgroup storage class and be workgroup coherent

  * This means all loads uses MakePointerVisibleKHR and all stores use MakePointerAvailableKHR with appropriate scope, Workgroup in this case.

Overview
========

The Vulkan memory modle provides precise definitions to produce a
data-race-free program. SPIR-V was extended, via SPV_KHR_vulkan_memory_model,
to provide the functionality required to express the implementation of the
model in SPIR-V. This document describes how DXC's SPIR-V generation maps onto
that model.

New Constants
=============

New constants are needed to parameterize the new intrinsic functions.

Scopes
------

These map directly to SPIR-V scope id values.

.. table:: Scope constants.
+-----------------+------------+
| **Name**        |  **Value** |
+-----------------+------------+
| spv_CrossDevice | 0x0        |
+-----------------+------------+
| spv_Device      | 0x1        |
+-----------------+------------+
| spv_Workgroup   | 0x2        |
+-----------------+------------+
| spv_Subgroup    | 0x3        |
+-----------------+------------+
| spv_Invocation  | 0x4        |
+-----------------+------------+
| spv_QueueFamily | 0x5        |
+-----------------+------------+

* These constants are used for memory and execution scopes.
* The spv_CrossDevice scope is reserved. The compiler should produce an error if it is used.
* The spv_Device scope requires enabling the **VulkanMemoryModelDeviceScopeKHR** capability if it is used.

Semantics
---------

.. table:: Semantics constants.
+--------------------+-----------+
| **Name**           | **Value** |
+--------------------+-----------+
| spv_Relaxed        | 0x0       |
+--------------------+-----------+
| spv_Acquire        | 0x2       |
+--------------------+-----------+
| spv_Release        | 0x4       |
+--------------------+-----------+
| spv_AcquireRelease | 0x8       |
+--------------------+-----------+
| spv_BufferMemory   | 0x40      |
+--------------------+-----------+
| spv_SharedMemory   | 0x100     |
+--------------------+-----------+
| spv_TextureMemory  | 0x800     |
+--------------------+-----------+
| spv_OutputMemory   | 0x1000    |
+--------------------+-----------+
| spv_MakeAvailable  | 0x2000    |
+--------------------+-----------+
| spv_MakeVisible    | 0x4000    |
+--------------------+-----------+

* These constants are used for memory/storage semantics.

New Intrinsics
--------------

These intrinsics provide fully parameterized versions of the base functions.
This provides users with greater flexibility and precision.

.. table:: New intrinsic functions.
+----------------------------+--------------------------------------------------------------------------------+---------------------------+
| **Name**                   | **Parameters**                                                                 | **SPIR-V**                |
+----------------------------+--------------------------------------------------------------------------------+---------------------------+
| MemoryBarrierWithSync      | unsigned int execution_scope - scope constant                                  | OpControlBarrier          |
|                            | unsigned int memory_scope - scope constant                                     |                           |
|                            | unsigned int semantics - semantics constant                                    |                           |
+----------------------------+--------------------------------------------------------------------------------+---------------------------+
| MemoryBarrier              | unsigned int scope - scope constant                                            |                           |
|                            | unsigned int semantics - semantics constant                                    |                           |
+----------------------------+--------------------------------------------------------------------------------+---------------------------+
| InterlockedLoad            | R dest - destination variable                                                  |                           |
|                            | T src - source variable                                                        |                           |
|                            |                                                                                |                           |
|                            | unsigned int scope - scope constant                                            |                           |
|                            | unsigned int semantics - semantics constant                                    |                           |
+----------------------------+--------------------------------------------------------------------------------+---------------------------+
| InterlockedStore           | R dest - destination variable                                                  |                           |
|                            | T src - source variable                                                        |                           |
|                            |                                                                                |                           |
|                            | unsigned int scope - scope constant                                            |                           |
|                            | unsigned int semantics - semantics constant                                    |                           |
+----------------------------+--------------------------------------------------------------------------------+---------------------------+
| InterlockedAdd             | Parameters of current intrinsic of same name plus the following additions:     | OpAtomicAdd               |
+----------------------------+ unsigned int scope - scope constant                                            +---------------------------+
| InterlockedAnd             | unsigned int sematnics - semantics constant                                    | OpAtomicAnd               |
+----------------------------+                                                                                +---------------------------+
| InterlockedExchange        |                                                                                | OpAtomicExchange          |
+----------------------------+                                                                                +---------------------------+
| InterlockedMax             |                                                                                | OpAtomicSMax/OpAtomicUMax |
+----------------------------+                                                                                +---------------------------+
| InterlockedMin             |                                                                                | OpAtomicSMin/OpAtomicUMin |
+----------------------------+                                                                                +---------------------------+
| InterlockedOr              |                                                                                | OpAtomicOr                |
+----------------------------+                                                                                +---------------------------+
| InterlockedXor             |                                                                                | OpAtomicXor               |
+----------------------------+--------------------------------------------------------------------------------+---------------------------+
| InterlockedCompareExchange | Parameters of current intrinsic of same name plus the following additions:     | OpAtomicCompareExchange   |
+----------------------------+                                                                                |                           |
| InterlockedComparseStore   | unsigned int scope - scope constant                                            |                           |
|                            | unsigned int equal_semantics - semantics constant when comparison is equal     |                           |
|                            | unsigned int unequal_semantics - semantics constant when comparison is unequal |                           |
+----------------------------+--------------------------------------------------------------------------------+---------------------------+

- InterlockedLoad and InterlockedStore are newly introduced functions to provide flexibility
- InterlockedLoad and InterlockedStore can only be used in Pixel and Compute shaders
- MemoryBarrier can only be used in Pixel and Compute shaders
- MemoryBarrierWithSync can only be used in Compute shaders

Barriers
========

The generic barrier function, MemoryBarrierWithSync, exposes all the
parameterization of OpControlBarrier in SPIR-V. The memory scope indicates the
extent of synchronization from the current invocation for memory operations.
The execution scope indicates which invocations are involved in the operation.
That is, which invocations are required to wait at the barrier until their
peers also execute the barrier. The semantics argument indicates which storage
classes are affected, the type of barrier (which affects the instructions that
can be reordered with respect to the barrier) and whether bulk availability
and/or visibility operations should be performed.

Availability and Visibility
===========================

MakeAvailable can only be applied if the barrier/interlocked function has
Release or AcquireRelease semantics. If applied, the effects of all writes in
the specified storage classes are made observable in the specified memory
scope.

MakeVisible can only be applied if the barrier/interlocked function has Acquire
or AcquireRelease semantics. If applied, the effects of all writes to the
specified storage classes can be observed by reads in the specified memory
scope.

Note: Availability and visibility are both required operations. It is
insufficient to enable a read to observe the effects of a write that was made
available without a corresponding visibility operation and vice versa.

Note: availability/visibility operations are required, but not sufficient to
guarantee a data-race-free program.

Note: New interlocked operations cannot have AcquireRelease semantics.
Interlocked operations that do not write to memory cannot have Release
semantics. Interlocked operations that do not read from memory cannot have
Acquire semantics.

SPIR-V Deprecated Decorations
=============================

The Vulkan memory model deprecates the Coherent and Volatile decorations.
Instead of reference level decorations, this information will embedded directly
in the appropriate instructions.

New Variable Modifiers
======================

.. table:: New variable modifiers.
+----------------------------------------+-----------------------------------------------+----------------+
| **Name**                               |                                               |                |
+----------------------------------------+-----------------------------------------------+----------------+
| deviceshared _1_                       | NonPrivate{Pointer|Texel}KHR                  | Device         |
+----------------------------------------+ Make{Pointer|Texel}{Available|Visible}KHR _2_ +----------------+
| globallycoherent/queuefamilyshared _1_ |                                               | QueueFamilyKHR |
+----------------------------------------+                                               +----------------+
| groupshared _3_                        |                                               | Workgroup      |
+----------------------------------------+                                               +----------------+
| subgroupshared _1_                     |                                               | Subgroup       |
+----------------------------------------+-----------------------------------------------+----------------+
| spv_NonPrivate _4_                     | NonPrivate{Pointer|Texel}KHR                  |                |
+----------------------------------------+-----------------------------------------------+----------------+
| volatile                               | NonPrivate{Pointer|Texel}KHR                  | QueueFamilyKHR |
|                                        | Make{Pointer|Texel}{Available|Visible}KHR     |                |
|                                        | {Volatile|VolatileTexelKHR} _5_               |                |
+----------------------------------------+-----------------------------------------------+----------------+

1. Only applies to Uniform and SSBO storage images.
2. Pointer variants OpLoad, OpStore and OpCopyMemory*. Texel variants
   OpImageRead, OpImageSparseRead and OpImageWrite. Available variants cannot
   be applied to OpRead, OpImageRead or OpImageSparseRead. Visible variants
   cannot be applied to OpStore or OpImageWrite.
3. groupshared variables should be in Workgroup storage classm unless a storage
   class is specified (in which case the same rules as in 1).
4. Useful for avoiding WAR hazards.
5. Volatile is for instructions with Memory Access operands and
   VolatileTexelKHR is for instructions with Image Operands.

Operations on variables with the shared modifiers have implicit
availability/visibility operations. This allows the variable to act as shared
memory between all invocations on the device that have access to the resource.
Similarly, operations on subgroupshared and groupshared variables have implicit
availability/visibility operations performed that allow invocations in the same
subgroup or workgroup respectively to observe the effect of the memory
operation. Invocations outside the same subgroup/workgroup act as if the
variable is not modified with subgroupshared/groupshared; that is, those
invocations do not observe the effects of another subgroup/workgroup
implicitly.

Unlike the availability/visibility operations on barriers and atomics, these
availability/visibility operations are fine grained. Only the memory locations
accessed are affected.

Note: It should be a compiler error to use multiple of these modifiers on a
single variable.

Note: Read/write resources (e.g. RWBuffer, RWTexture1D, RWStructuredBuffer,
etc.) are implicitly workgroupshared global resources. That is, they should
have both non-private and availability/visibility flags with the Workgroup
scope. Read/write resources can only be made further shared by using the
globallycoherent modifier. They cannot have their coherency “reduced”.

Enabling the Vulkan Memory Model in DXC
=======================================

A new command line option should be added to specify the memory model:
-fspirv-memory-model. The valid values should be glsl and vulkan. This option
should only apply if SPIR-V is being generated. The compiler should also infer
the Vulkan memory model if any of the new intrinsics or variable modifiers are
used in the translation unit. It should be a compiler error if any new
intrinsic or variable modifier is used, but the user specified glsl as the
memory model on the command line.

To enable the model properly in SPIR-V requires specifying the capability
VulkanMemoryModelKHR and specifying the memory model operand of OpMemoryModel
to VulkanKHR. Additionally, the following extension must be specified:
OpExtension "SPV_KHR_vulkan_memory_model".

If the implementation would generate a Device scope operation in SPIR-V, then
the VulkanMemoryModelDeviceScopeKHR capability must also be enabled.

Output must be SPIR-V 1.3 or later.
