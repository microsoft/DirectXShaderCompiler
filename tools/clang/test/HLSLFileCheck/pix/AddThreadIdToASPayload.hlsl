// RUN: %dxc -Emain -Tas_6_6 %s | %opt -S -hlsl-dxil-PIX-add-tid-to-as-payload | %FileCheck %s

// CHECK: @dx.op.dispatch Mesh.struct.Payload

struct MyPayload
{
    uint i;
};

[numthreads(1, 1, 1)]
void main(uint gid : SV_GroupID)
{
    MyPayload payload;
    payload.i = gid;
    DispatchMesh(1, 1, 1, payload);
}
