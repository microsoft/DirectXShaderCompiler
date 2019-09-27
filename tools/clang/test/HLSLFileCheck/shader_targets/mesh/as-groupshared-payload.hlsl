// RUN: %dxc -E amplification -T as_6_5 %s | FileCheck %s

// CHECK: define void @amplification

struct MeshPayload
{
  uint data[4];
};

groupshared MeshPayload pld;

[numthreads(4,1,1)]
void amplification(uint gtid : SV_GroupIndex)
{
  pld.data[gtid] = gtid;
  DispatchMesh(1,1,1,pld);
}
