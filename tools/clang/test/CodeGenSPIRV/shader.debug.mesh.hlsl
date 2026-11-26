// RUN: %dxc -Zi -O0 -T ms_6_5 %s -spirv | FileCheck %s

// CHECK: [[file:%[0-9]+]] = OpString

struct MeshShaderOutput {
    float4 position: SV_Position;
    float3 color: COLOR0;
};

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void main(out indices uint3 triangleBox[1], out vertices MeshShaderOutput triangleVertex[3]) {
    SetMeshOutputCounts(3, 1);

    triangleBox[0] = uint3(0, 1, 2);
// CHECK:      OpLine [[file]] [[@LINE-1]] 5
// CHECK-NOT:  OpLine
// CHECK:      OpStore
// CHECK-NOT:  OpLine
// CHECK:      OpStore
// CHECK-NOT:  OpLine
// CHECK:      OpStore

    triangleVertex[0].position = float4(1, 0.5, 0.0, 1.0);
// CHECK:      OpLine [[file]] [[@LINE-1]] 5
// CHECK-NOT:  OpLine
// CHECK:      OpStore
    triangleVertex[0].color = float3(1, 0.0, 0.0);
// CHECK:      OpLine [[file]] [[@LINE-1]] 5
// CHECK-NOT:  OpLine
// CHECK:      OpStore

    triangleVertex[1].position = float4(1, 0.5, 0.0, 1.0);
// CHECK:      OpLine [[file]] [[@LINE-1]] 5
// CHECK-NOT:  OpLine
// CHECK:      OpStore
    triangleVertex[1].color = float3(1.0, 1.0, 0.0);
// CHECK:      OpLine [[file]] [[@LINE-1]] 5
// CHECK-NOT:  OpLine
// CHECK:      OpStore

    MeshShaderOutput v;
    v.position = float4(1.0, -0.5, 0.0, 1.0);
    v.color = float3(1.0, 0.0, 1.0);
    triangleVertex[2] = v;
// CHECK:      OpLine [[file]] [[@LINE-1]] 5
// CHECK-NOT:  OpLine
// CHECK:      OpStore
// CHECK-NOT:  OpLine
// CHECK:      OpStore
}
