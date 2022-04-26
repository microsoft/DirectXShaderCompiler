// RUN: %dxc -HV 2021 -T cs_6_0 -DOP=InterlockedOr %s | FileCheck %s
// RUN: %dxc -HV 2021 -T cs_6_0 -DOP=InterlockedCompareStore %s | FileCheck %s

// Ensure that atomic operations fail when used with struct members of a typed resource
// The only typed resource that can use structs is RWBuffer
// Use a binary op and an exchange op because they use different code paths

struct TexCoords {
  uint s, t, r, q;
};

RWBuffer<TexCoords> bufA;

[numthreads(8,8,1)]
void main( uint3 gtid : SV_GroupThreadID , uint gid : SV_GroupIndex)
{
  uint orig = 0;
  TexCoords tc = {0, 0, 0, 0};

  // CHECK: error: Typed resources used in atomic operations must have a scalar element type.
  OP(bufA[gid].q, 2, orig);
}
