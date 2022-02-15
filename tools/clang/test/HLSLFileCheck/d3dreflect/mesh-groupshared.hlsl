// RUN: %dxc -T lib_6_7 %s | %D3DReflect %s | FileCheck %s

// CHECK:DxilRuntimeData (size = 92 bytes):
// CHECK:  StringBuffer (size = 8 bytes)
// CHECK:  IndexTable (size = 0 bytes)
// CHECK:  RawBytes (size = 0 bytes)
// CHECK:  RecordTable (stride = 44 bytes) FunctionTable[1] = {
// CHECK:    <0:RuntimeDataFunctionInfo> = {
// CHECK:      Name: "main"
// CHECK:      UnmangledName: "main"
// CHECK:      Resources: <RecordArrayRef<RuntimeDataResourceInfo>[0]> = {}
// CHECK:      FunctionDependencies: <string[0]> = {}
// CHECK:      ShaderKind: Mesh
// CHECK:      PayloadSizeInBytes: 0
// CHECK:      AttributeSizeInBytes: 0
// CHECK:      FeatureInfo1: 65536
// CHECK:      FeatureInfo2: 0
// CHECK:      ShaderStageFlag: 8192
// CHECK:      MinShaderTarget: 852069
// CHECK:    }
// CHECK:  }
// CHECK:ID3D12LibraryReflection:
// CHECK:  D3D12_LIBRARY_DESC:
// CHECK:    Creator: <nullptr>
// CHECK:    Flags: 0
// CHECK:    FunctionCount: 1
// CHECK:  ID3D12FunctionReflection:
// CHECK:    D3D12_FUNCTION_DESC: Name: main
// CHECK:      Shader Version: Mesh 6.7
// CHECK:      Creator: <nullptr>
// CHECK:      Flags: 0
// CHECK:      ConstantBuffers: 0
// CHECK:      BoundResources: 0
// CHECK:      FunctionParameterCount: 0
// CHECK:      HasReturn: FALSE

#define MAX_VERT 32
#define MAX_PRIM 16
#define NUM_THREADS 32
struct MeshPerVertex {
    float4 position : SV_Position;
    float color[4] : COLOR;
};

struct MeshPerPrimitive {
    float normal : NORMAL;
};

struct MeshPayload {
    float normal;
    int4 data;
    bool2x2 mat;
};

groupshared float gsMem[MAX_PRIM];

[numthreads(NUM_THREADS, 1, 1)]
[shader("mesh")]
[outputtopology("triangle")]
void main(
            out indices uint3 primIndices[MAX_PRIM],
            out vertices MeshPerVertex verts[MAX_VERT],
            out primitives MeshPerPrimitive prims[MAX_PRIM],
            in payload MeshPayload mpl,
            in uint tig : SV_GroupIndex,
            in uint vid : SV_ViewID
         )
{
    SetMeshOutputCounts(MAX_VERT, MAX_PRIM);
    MeshPerVertex ov;
    if (vid % 2) {
        ov.position = float4(4.0,5.0,6.0,7.0);
        ov.color[0] = 4.0;
        ov.color[1] = 5.0;
        ov.color[2] = 6.0;
        ov.color[3] = 7.0;
    } else {
        ov.position = float4(14.0,15.0,16.0,17.0);
        ov.color[0] = 14.0;
        ov.color[1] = 15.0;
        ov.color[2] = 16.0;
        ov.color[3] = 17.0;
    }
    if (tig % 3) {
        primIndices[tig / 3] = uint3(tig, tig + 1, tig + 2);
        MeshPerPrimitive op;
        op.normal = dot(mpl.normal.xx, mul(mpl.data.xy, mpl.mat));
        gsMem[tig / 3] = op.normal;
        prims[tig / 3] = op;
    }
    verts[tig] = ov;
}
