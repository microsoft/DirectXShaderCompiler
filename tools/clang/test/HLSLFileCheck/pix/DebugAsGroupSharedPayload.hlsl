// RUN: %dxc -Od -Emain -Tas_6_6 %s | %opt -S -hlsl-dxil-PIX-add-tid-to-as-payload,dispatchArgY=3,dispatchArgZ=7 | %FileCheck %s

// CHECK: mul i32 %ThreadIdX, 3
// CHECK: mul i32
// CHECK: , 7
// CHECK: @dx.op.dispa tchMesh.PIX_AS2MS_Expanded_Type

struct Contained
{
    uint j;
};
struct MyPayload
{
    uint i;
  //Contained a[2];
};

groupshared MyPayload payload;

[numthreads(1, 1, 1)]
void main(uint gid : SV_GroupID)
{
  MyPayload copy;
  copy = payload;
  DispatchMesh(1, 1, 1, copy);
}
