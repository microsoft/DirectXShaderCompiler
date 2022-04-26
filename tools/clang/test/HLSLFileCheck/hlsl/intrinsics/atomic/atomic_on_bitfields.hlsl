// RUN: %dxc -HV 2021 -T cs_6_0 %s | FileCheck %s

// Ensure that atomic operations fail when used with bitfields
// Use structured and typed buffers as the resources that can use structs
// and also both binary op and exchange atomic ops as either difference
// can cause the compiler to take different paths

struct TexCoords {
  uint s : 8;
  uint t : 8;
  uint r : 8;
  uint q : 8;
};

RWBuffer<TexCoords> buf;
RWStructuredBuffer<TexCoords> str;
groupshared TexCoords gs;

[numthreads(8,8,1)]
void main( uint2 tid : SV_DispatchThreadID) {

  // CHECK: error: no matching function for call to 'InterlockedOr'
  // CHECK: error: no matching function for call to 'InterlockedCompareStore'
  InterlockedOr(buf[tid.y].q, 2);
  InterlockedCompareStore(buf[tid.y].q, 3, 1);

  // CHECK: error: no matching function for call to 'InterlockedOr'
  // CHECK: error: no matching function for call to 'InterlockedCompareStore'
  InterlockedOr(str[tid.y].q, 2);
  InterlockedCompareStore(str[tid.y].q, 3, 1);

  // CHECK: error: no matching function for call to 'InterlockedOr'
  // CHECK: error: no matching function for call to 'InterlockedCompareStore'
  InterlockedOr(gs.q, 2);
  InterlockedCompareStore(gs.q, 3, 1);
}
