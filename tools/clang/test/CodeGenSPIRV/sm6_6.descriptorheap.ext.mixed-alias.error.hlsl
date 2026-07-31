// RUN: %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DCASE=0 %s | FileCheck %s --check-prefix=OK
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DCASE=1 %s 2>&1 | FileCheck %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DCASE=2 %s 2>&1 | FileCheck %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DCASE=3 %s 2>&1 | FileCheck %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DCASE=4 %s 2>&1 | FileCheck %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DCASE=5 %s 2>&1 | FileCheck %s

// Verifies that a local resource variable assigned from both a bound resource
// and ResourceDescriptorHeap is rejected.
//
// The alias table maps each aliased VarDecl to heap descriptor info at compile
// time. Once a variable is recorded as an alias, every later use is re-lowered
// as a heap access chain, ignoring control flow. That is only correct when the
// variable holds a heap descriptor on every path reaching the use, so assigning
// it from both kinds of source must be diagnosed rather than silently
// miscompiled.
//
// Before this was diagnosed, CASE=1 emitted
//   %index = OpSelect %uint %cond %uint_1 %undef
//   OpUntypedAccessChainKHR ... %resource_heap %index
// i.e. the else path indexed the resource heap with an undefined value, and
// CASE=2 atomically updated heap descriptor 2 after the variable had been
// reassigned to `boundTex`.
//
// Both assignment orderings are detected: bound-then-heap assignments (CASE=1,
// CASE=3, CASE=5) are tracked in descriptorHeapVarState; heap-then-bound
// assignments (CASE=2, CASE=4) are detected by observing that the alias map
// already contains an entry for the variable.

// CHECK: error: {{.*}}mixing bound and descriptor heap resources in the same variable is not supported with SPV_EXT_descriptor_heap
// OK:    OpUntypedImageTexelPointerEXT

RWByteAddressBuffer outputBytes : register(u0);
RWTexture2D<uint> boundTex : register(u1);
RWByteAddressBuffer boundBuf : register(u2);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint original;

#if CASE == 0
  // Control: heap-only reassignment is legal. Both sources are heap descriptors,
  // so the alias index is simply updated on each assignment and every path holds
  // a heap descriptor.
  RWTexture2D<uint> heapOnly = ResourceDescriptorHeap[1];
  if (tid.x == 0)
    heapOnly = ResourceDescriptorHeap[2];
  InterlockedAdd(heapOnly[tid.xy], 4, original);

#elif CASE == 1
  // Bound-then-heap (conditional): the heap descriptor is assigned on one path
  // only. The else path still holds the bound resource, but the atomic would be
  // lowered as a heap access with an undefined index.
  RWTexture2D<uint> mixed = boundTex;
  if (tid.x == 0)
    mixed = ResourceDescriptorHeap[1];
  InterlockedAdd(mixed[tid.xy], 1, original);

#elif CASE == 2
  // Heap-then-bound: the alias would never be cleared, so the atomic would keep
  // targeting heap descriptor 2 even after the variable was reassigned to the
  // bound texture.
  RWTexture2D<uint> mixed = ResourceDescriptorHeap[2];
  mixed = boundTex;
  InterlockedAdd(mixed[tid.xy], 2, original);

#elif CASE == 3
  // Bound-then-heap (loop): same as CASE=1 but the heap index is undefined when
  // the loop body never executes.
  RWTexture2D<uint> mixed = boundTex;
  for (uint i = 0; i < tid.y; ++i)
    mixed = ResourceDescriptorHeap[i];
  InterlockedAdd(mixed[tid.xy], 3, original);

#elif CASE == 4
  // Buffer heap-then-bound: the alias records heap index 3; the reassignment to
  // a bound buffer must be rejected before any load sees the stale alias.
  RWByteAddressBuffer mixedBuf = ResourceDescriptorHeap[3];
  mixedBuf = boundBuf;
  original = mixedBuf.Load(0);

#elif CASE == 5
  // Buffer bound-then-heap: the bound assignment is recorded in
  // descriptorHeapVarState; the later heap assignment must be rejected.
  RWByteAddressBuffer mixedBuf = boundBuf;
  mixedBuf = ResourceDescriptorHeap[3];
  original = mixedBuf.Load(0);

#endif

  outputBytes.Store(0, original);
}
