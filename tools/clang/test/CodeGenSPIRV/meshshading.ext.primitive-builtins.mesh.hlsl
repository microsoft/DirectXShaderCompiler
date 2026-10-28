// RUN: %dxc -T ms_6_5 -E main -fspv-target-env=vulkan1.3 %s -spirv | FileCheck %s
// RUN: %dxc -T ms_6_5 -E main -fspv-target-env=vulkan1.3 -fcgl %s -spirv | FileCheck %s

// MeshShadingEXT enables the PrimitiveId, Layer and ViewportIndex builtins in a
// mesh stage, so writing them must not add Geometry or MultiViewport.

// CHECK-NOT: OpCapability Geometry
// CHECK-NOT: OpCapability MultiViewport
// CHECK:     OpCapability MeshShadingEXT
// CHECK-NOT: OpCapability Geometry
// CHECK-NOT: OpCapability MultiViewport
// CHECK:     OpExtension "SPV_EXT_mesh_shader"

// CHECK-DAG: OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId
// CHECK-DAG: OpDecorate %gl_Layer BuiltIn Layer
// CHECK-DAG: OpDecorate %gl_ViewportIndex BuiltIn ViewportIndex

struct MeshPerVertex {
    float4 position : SV_Position;
};

struct MeshPerPrimitive {
    uint primId : SV_PrimitiveID;
    uint layer  : SV_RenderTargetArrayIndex;
    uint vpIdx  : SV_ViewportArrayIndex;
};

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void main(out vertices MeshPerVertex verts[3],
          out indices uint3 primitiveInd[1],
          out primitives MeshPerPrimitive prims[1]) {
    SetMeshOutputCounts(3, 1);
    verts[0].position = float4(0, 0, 0, 1);
    verts[1].position = float4(1, 0, 0, 1);
    verts[2].position = float4(0, 1, 0, 1);
    primitiveInd[0] = uint3(0, 1, 2);
    prims[0].primId = 7;
    prims[0].layer = 1;
    prims[0].vpIdx = 2;
}
