// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck --check-prefix=ORDER %s
// RUN: not %dxc -T cs_6_6 -E main -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv -DTEST_FORWARDED_LOCAL %s 2>&1 | FileCheck --check-prefix=FORWARDED %s

// Verifies two cases where a heap-sourced argument to a parameter later used
// in an atomic must still be diagnosed, neither of which the old
// processCall-time flag (descriptorHeapImageBoundaryLossVars, set only as a
// side effect of lowering one call) could catch:
//
// Case ORDER - helper() is called with a BOUND image first, which queues and
//   fully emits helper() (including its atomic) before the second call site,
//   which passes a HEAP image, is ever processed. A flag written during
//   that second call arrives too late: helper's body is already emitted.
//   Detection must be a static, whole-program scan independent of work-queue
//   order (paramReceivesHeapSourcedArg), not a record of what's been lowered
//   so far.
// Case FORWARDED_LOCAL - the argument is a local initialized from a call to
//   another function that returns a heap-sourced image, not a direct heap
//   subscript or an existing alias variable. isHeapSourcedValue (the old
//   predicate used at the call site) only consults the runtime alias maps
//   and misses this; isExprStaticallyHeapSourcedImage (already used for the
//   return-crossing case) recognizes it.

// ORDER: interlocked operation on a heap-backed RWTexture passed to or returned from a helper function is not supported
// FORWARDED: interlocked operation on a heap-backed RWTexture passed to or returned from a helper function is not supported

RWByteAddressBuffer outputBytes : register(u0);
RWTexture2D<uint> bound : register(u1);

void helper(RWTexture2D<uint> t, uint2 coord, out uint orig) {
  InterlockedAdd(t[coord], 1, orig);
}

#ifdef TEST_FORWARDED_LOCAL

RWTexture2D<uint> makeHeapImage(uint slot) {
  RWTexture2D<uint> tex = ResourceDescriptorHeap[slot];
  return tex;
}

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  RWTexture2D<uint> local = makeHeapImage(40);
  uint r;
  helper(local, tid.xy, r);
  outputBytes.Store(0, r);
}

#else

// Second call site: reached from main only through callWithHeap, which is
// queued after helper (main calls helper(bound) first, so helper is queued
// and then fully emitted at the next work-queue slot, before main's second
// statement -- the call to callWithHeap -- is even processed).
void callWithHeap(uint2 coord, out uint orig) {
  RWTexture2D<uint> tex = ResourceDescriptorHeap[41];
  helper(tex, coord, orig);
}

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  uint r0, r1;
  helper(bound, tid.xy, r0);
  callWithHeap(tid.xy, r1);
  outputBytes.Store(0, r0);
  outputBytes.Store(4, r1);
}

#endif
