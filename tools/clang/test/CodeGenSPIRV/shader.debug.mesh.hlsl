// RUN: %dxc -Zi -O0 -T ms_6_5 %s -spirv | FileCheck %s

// CHECK: OpStore %62 %63
// CHECK: OpLine %6 22 5
// CHECK: OpLine %6 23 5
// CHECK: OpLine %6 25 5
// CHECK: OpLine %6 26 5
// CHECK: OpLine %6 28 5
// CHECK: OpLine %6 29 5

struct MeshShaderOutput {
    float4 position: SV_Position;
    float3 color: COLOR0;
};

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void main(out indices uint3 triangleBox[1], out vertices MeshShaderOutput triangleVertex[3]) {
    SetMeshOutputCounts(3, 1);
    triangleBox[0] = uint3(0, 1, 1);

    triangleVertex[0].position = float4(1, 0.5, 0.0, 1.0);
    triangleVertex[0].color = float3(1, 0.0, 0.0);

    triangleVertex[1].position = float4(1, 0.5, 0.0, 1.0);
    triangleVertex[1].color = float3(1.0, 1.0, 0.0);
    
    triangleVertex[2].position = float4(1.0, -0.5, 0.0, 1.0);
    triangleVertex[2].color = float3(1.0, 0.0, 1.0);
}