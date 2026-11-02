// RUN: %dxc -T gs_6_0 -E main -fspv-target-env=vulkan1.3 %s -spirv | FileCheck %s

// A geometry stage needs Geometry for its own execution model, so declaring
// MeshShadingEXT beside a PrimitiveId must not drop it.

// CHECK-DAG: OpCapability Geometry
// CHECK-DAG: OpCapability MeshShadingEXT

// CHECK:      OpEntryPoint Geometry %main "main"
// CHECK-SAME: %gl_PrimitiveID

// CHECK:      OpDecorate %gl_PrimitiveID BuiltIn PrimitiveId

struct GsVIn {
    float4 pos : SV_Position;
};

struct GsVOut {
    float4 pos : SV_Position;
    nointerpolation uint id : ID;
};

[[vk::ext_capability(/* MeshShadingEXT */ 5283)]]
[[vk::ext_extension("SPV_EXT_mesh_shader")]]
[maxvertexcount(1)]
void main(in point GsVIn inData[1],
          uint inId : SV_PrimitiveID,
          inout PointStream<GsVOut> outData) {
    GsVOut vertex;
    vertex.pos = inData[0].pos;
    vertex.id = inId;
    outData.Append(vertex);
}
