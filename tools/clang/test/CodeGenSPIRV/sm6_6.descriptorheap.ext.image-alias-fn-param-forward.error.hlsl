// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck %s

// Verifies a heap-sourced image forwarded through TWO levels of parameters
// is still diagnosed:
//   main() calls forward(heapImage), and forward's own parameter `t` (which
//   has no initializer or local assignment of its own -- it only ever
//   receives its value as a call argument) is passed on to atomicHelper,
//   which uses its parameter in an atomic.
//
// isExprStaticallyHeapSourcedImage's VarDecl handling must recurse into
// paramReceivesHeapSourcedArg when the var is itself a ParmVarDecl, not just
// check its (nonexistent) initializer/assignment, or this chain silently
// falls through to an invalid OpImageTexelPointer.

// CHECK: interlocked operation on a heap-backed RWTexture must read the descriptor directly at this call site

RWByteAddressBuffer outputBytes : register(u0);

void atomicHelper(RWTexture2D<uint> u, uint2 coord, out uint orig) {
  InterlockedAdd(u[coord], 1, orig);
}

void forward(RWTexture2D<uint> t, uint2 coord, out uint orig) {
  atomicHelper(t, coord, orig);
}

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  RWTexture2D<uint> tex = ResourceDescriptorHeap[40];
  uint r;
  forward(tex, tid.xy, r);
  outputBytes.Store(0, r);
}
