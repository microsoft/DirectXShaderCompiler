// RUN: not %dxc -T cs_6_6 -E main -Od -fspv-target-env=vulkan1.3 -spirv -DNOHEAPFLAG %s 2>&1 | FileCheck %s --check-prefix=NOHEAPFLAG
// RUN: not %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DCOMBINED %s 2>&1 | FileCheck %s --check-prefix=COMBINED
// RUN: not %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DSPECID_USER %s 2>&1 | FileCheck %s --check-prefix=SPECID_USER
// RUN: not %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DSPECID_RS %s 2>&1 | FileCheck %s --check-prefix=SPECID_RS
// RUN: not %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DDUPLICATE %s 2>&1 | FileCheck %s --check-prefix=DUPLICATE
// RUN: not %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DNODEFAULT %s 2>&1 | FileCheck %s --check-prefix=NODEFAULT
// RUN: not %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DNOTPOW2 %s 2>&1 | FileCheck %s --check-prefix=NOTPOW2
// RUN: not %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DNONUINT %s 2>&1 | FileCheck %s --check-prefix=NONUINT
// RUN: not %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DNOTGLOBAL %s 2>&1 | FileCheck %s --check-prefix=NOTGLOBAL

// Verifies: every misuse of the descriptor-heap stride spec-constant attributes emits its specific diagnostic.
//  Diagnostics for the descriptor-heap stride spec-constant attributes. Each RUN
//  activates exactly one case via -D because DXC stops codegen after the first
//  error. The NOHEAPFLAG case intentionally omits -fspv-use-descriptor-heap.

// NOHEAPFLAG:   error: {{.*resource_heap_stride_constant_id.*}} requires -fspv-use-descriptor-heap; without it the attribute has no effect
// COMBINED:     error: {{.*resource_heap_stride_constant_id.*}} and {{.*constant_id.*}} are mutually exclusive; remove one
// SPECID_USER:  error: SpecId {{[0-9]+}} conflict: {{.*constant_id.*}}share the same SpecId
// SPECID_RS:    error: SpecId 3 conflict: {{.*resource_heap_stride_constant_id.*}} and {{.*sampler_heap_stride_constant_id.*}} must use different SpecIds
// DUPLICATE:    error: {{.*resource_heap_stride_constant_id.*}} may only appear once per translation unit; previous declaration here
// NODEFAULT:    error: {{.*resource_heap_stride_constant_id.*}} variable requires an initializer (pipeline specialization constant default)
// NOTPOW2:      error: {{.*resource_heap_stride_constant_id.*}} default value 48 is invalid; must be a power of 2 between 8 and 256 (inclusive)
// NONUINT:      error: {{.*resource_heap_stride_constant_id.*}} variable must be 'uint'; got 'float'
// NOTGLOBAL:    error: {{.*resource_heap_stride_constant_id.*}} attribute only applies to global variables of scalar type

#ifdef NOHEAPFLAG
[[vk::resource_heap_stride_constant_id(0)]] const uint kStride = 64;
#endif

#ifdef COMBINED
[[vk::constant_id(2)]]
[[vk::resource_heap_stride_constant_id(2)]] const uint kStride = 64;
#endif

#ifdef SPECID_USER
[[vk::constant_id(5)]]                      const uint TILE_SIZE = 16;
[[vk::resource_heap_stride_constant_id(5)]] const uint kStride   = 64;
#endif

#ifdef SPECID_RS
[[vk::resource_heap_stride_constant_id(3)]] const uint kResourceStride = 64;
[[vk::sampler_heap_stride_constant_id(3)]]  const uint kSamplerStride  = 32;
#endif

#ifdef DUPLICATE
[[vk::resource_heap_stride_constant_id(0)]] const uint kStride1 = 64;
[[vk::resource_heap_stride_constant_id(1)]] const uint kStride2 = 32;
#endif

#ifdef NODEFAULT
[[vk::resource_heap_stride_constant_id(0)]] const uint kStride;
#endif

#ifdef NOTPOW2
[[vk::resource_heap_stride_constant_id(0)]] const uint kStride = 48;
#endif

#ifdef NONUINT
[[vk::resource_heap_stride_constant_id(0)]] const float kStride = 64.0f;
#endif

RWByteAddressBuffer outputBytes : register(u0);

[numthreads(1, 1, 1)]
void main() {
#ifdef NOTGLOBAL
    [[vk::resource_heap_stride_constant_id(0)]] const uint kLocal = 64;
    outputBytes.Store(0, kLocal);
#else
    outputBytes.Store(0, 0);
#endif
}
