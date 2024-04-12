// RUN: %dxc -T lib_6_9 %s | FileCheck %s

// REQUIRES: dxil-1-9

// Test loading of node input and funneling into mesh outputs
// Essentially an end-to-end mesh node test.


RWBuffer<float> buf0;

#define MAX_VERT 32
#define MAX_PRIM 16
#define NUM_THREADS 32

struct MeshPerVertex {
    float4 position : SV_Position;
    float color[4] : COLOR;
};

struct MeshPerPrimitive {
    float normal : NORMAL;
    float malnor : MALNOR;
    float alnorm : ALNORM;
    float ormaln : ORMALN;
    int layer[6] : LAYER;
};

struct MeshPayload {
    float normal;
    float malnor;
    float alnorm;
    float ormaln;
    int layer[6];
};

groupshared float gsMem[MAX_PRIM];

[Shader("node")]
[NodeLaunch("mesh")]
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
[NodeDispatchGrid(64,1,1)]
void node_setmeshoutputcounts(DispatchNodeInputRecord<MeshPayload> mpl,
            out indices uint3 primIndices[MAX_PRIM],
            out vertices MeshPerVertex verts[MAX_VERT],
            out primitives MeshPerPrimitive prims[MAX_PRIM],
            in uint tig : SV_GroupIndex) {
  // SV_GroupIndex
  // CHECK: %[[GID:[0-9]+]] = call i32 @dx.op.flattenedThreadIdInGroup.i32(i32 96)

  // CHECK: call void @dx.op.setMeshOutputCounts(i32 168, i32 32, i32 16)  ; SetMeshOutputCounts(numVertices,numPrimitives)
  SetMeshOutputCounts(32, 16);

  // create mpl
  // CHECK: %[[CHDL:[0-9]+]] = call %dx.types.NodeRecordHandle @dx.op.createNodeInputRecordHandle(i32 250, i32 0)
  // CHECK: %[[AHDL:[0-9]+]] = call %dx.types.NodeRecordHandle @dx.op.annotateNodeRecordHandle(i32 251, %dx.types.NodeRecordHandle %[[CHDL]], %dx.types.NodeRecordInfo { i32 97, i32 40 })

  MeshPerVertex ov;
  ov.position = float4(14.0,15.0,16.0,17.0);
  ov.color[0] = 14.0;
  ov.color[1] = 15.0;
  ov.color[2] = 16.0;
  ov.color[3] = 17.0;

  if (tig % 3) {
    // CHECK: %[[GID1:[0-9]+]] = add i32 %[[GID]], 1
    // CHECK: %[[GID2:[0-9]+]] = add i32 %[[GID]], 2
    // CHECK: %[[GIDIV:[0-9]+]] = udiv i32 %[[GID]], 3
    // CHECK: call void @dx.op.emitIndices(i32 169, i32 %[[GIDIV]], i32 %[[GID]], i32 %[[GID1]], i32 %[[GID2]])
    primIndices[tig / 3] = uint3(tig, tig + 1, tig + 2);

    MeshPerPrimitive op;
    // CHECK: %[[MPL:[0-9]+]] = call %struct.MeshPayload addrspace(6)* @dx.op.getNodeRecordPtr.struct.MeshPayload(i32 239, %dx.types.NodeRecordHandle %[[AHDL]], i32 0)
    // CHECK: %[[NRMGEP:[0-9]+]] = getelementptr %struct.MeshPayload, %struct.MeshPayload addrspace(6)* %[[MPL]], i32 0, i32 0
    // CHECK: %[[NRM:[0-9]+]] = load float, float addrspace(6)* %[[NRMGEP]], align 4
    op.normal = mpl.Get().normal;
    // CHECK: %[[GIDIV1:[0-9]+]] = add nuw i32 %[[GIDIV]], 1
    // CHECK: %[[GSGEP:[0-9]+]] = getelementptr [16 x float], [16 x float] addrspace(3)* @"\01?gsMem@@3PAMA", i32 0, i32 %[[GIDIV1]]
    // CHECK: %[[GS:[0-9]+]] = load float, float addrspace(3)* %[[GSGEP]]
    op.malnor = gsMem[tig / 3 + 1];
    // CHECK: %[[AGEP:[0-9]+]] = getelementptr %struct.MeshPayload, %struct.MeshPayload addrspace(6)* %[[MPL]], i32 0, i32 2
    // CHECK: %[[ANML:[0-9]+]] = load float, float addrspace(6)* %[[AGEP]], align 4
    op.alnorm = mpl.Get().alnorm;
    // CHECK: %[[OGEP:[0-9]+]] = getelementptr %struct.MeshPayload, %struct.MeshPayload addrspace(6)* %[[MPL]], i32 0, i32 3
    // CHECK: %[[ONML:[0-9]+]] = load float, float addrspace(6)* %[[OGEP]], align 4
    op.ormaln = mpl.Get().ormaln;
    // CHECK: %[[L0GEP:[0-9]+]] = getelementptr inbounds %struct.MeshPayload, %struct.MeshPayload addrspace(6)* %[[MPL]], i32 0, i32 4, i32 0
    // CHECK: %[[LAY0:[0-9]+]] = load i32, i32 addrspace(6)* %[[L0GEP]], align 4
    op.layer[0] = mpl.Get().layer[0];
    // CHECK: %[[L1GEP:[0-9]+]] = getelementptr inbounds %struct.MeshPayload, %struct.MeshPayload addrspace(6)* %[[MPL]], i32 0, i32 4, i32 1
    // CHECK: %[[LAY1:[0-9]+]] = load i32, i32 addrspace(6)* %[[L1GEP]], align 4
    op.layer[1] = mpl.Get().layer[1];
    // CHECK: %[[L2GEP:[0-9]+]] = getelementptr inbounds %struct.MeshPayload, %struct.MeshPayload addrspace(6)* %[[MPL]], i32 0, i32 4, i32 2
    // CHECK: %[[LAY2:[0-9]+]] = load i32, i32 addrspace(6)* %[[L2GEP]], align 4
    op.layer[2] = mpl.Get().layer[2];
    // CHECK: %[[L3GEP:[0-9]+]] = getelementptr inbounds %struct.MeshPayload, %struct.MeshPayload addrspace(6)* %[[MPL]], i32 0, i32 4, i32 3
    // CHECK: %[[LAY3:[0-9]+]] = load i32, i32 addrspace(6)* %[[L3GEP]], align 4
    op.layer[3] = mpl.Get().layer[3];
    // CHECK: %[[L4GEP:[0-9]+]] = getelementptr inbounds %struct.MeshPayload, %struct.MeshPayload addrspace(6)* %[[MPL]], i32 0, i32 4, i32 4
    // CHECK: %[[LAY4:[0-9]+]] = load i32, i32 addrspace(6)* %[[L4GEP]], align 4
    op.layer[4] = mpl.Get().layer[4];
    // CHECK: %[[L5GEP:[0-9]+]] = getelementptr inbounds %struct.MeshPayload, %struct.MeshPayload addrspace(6)* %[[MPL]], i32 0, i32 4, i32 5
    // CHECK: %[[LAY5:[0-9]+]] = load i32, i32 addrspace(6)* %[[L5GEP]], align 4
    op.layer[5] = mpl.Get().layer[5];

    // CHECK: %[[GSGEP2:[0-9]+]] = getelementptr [16 x float], [16 x float] addrspace(3)* @"\01?gsMem@@3PAMA", i32 0, i32 %[[GIDIV]]
    // CHECK: store float %[[NRM]], float addrspace(3)* %[[GSGEP2]], align 4
    gsMem[tig / 3] = op.normal;
    // CHECK:  call void @dx.op.storePrimitiveOutput.f32(i32 172, i32 0, i32 0, i8 0, float %[[NRM]], i32 %[[GIDIV]])
    // CHECK:  call void @dx.op.storePrimitiveOutput.f32(i32 172, i32 1, i32 0, i8 0, float %[[GS]], i32 %[[GIDIV]])
    // CHECK:  call void @dx.op.storePrimitiveOutput.f32(i32 172, i32 2, i32 0, i8 0, float %[[ANML]], i32 %[[GIDIV]])
    // CHECK:  call void @dx.op.storePrimitiveOutput.f32(i32 172, i32 3, i32 0, i8 0, float %[[ONML]], i32 %[[GIDIV]])
    // CHECK:  call void @dx.op.storePrimitiveOutput.i32(i32 172, i32 4, i32 0, i8 0, i32 %[[LAY0]], i32 %[[GIDIV]])
    // CHECK:  call void @dx.op.storePrimitiveOutput.i32(i32 172, i32 4, i32 1, i8 0, i32 %[[LAY1]], i32 %[[GIDIV]])
    // CHECK:  call void @dx.op.storePrimitiveOutput.i32(i32 172, i32 4, i32 2, i8 0, i32 %[[LAY2]], i32 %[[GIDIV]])
    // CHECK:  call void @dx.op.storePrimitiveOutput.i32(i32 172, i32 4, i32 3, i8 0, i32 %[[LAY3]], i32 %[[GIDIV]])
    // CHECK:  call void @dx.op.storePrimitiveOutput.i32(i32 172, i32 4, i32 4, i8 0, i32 %[[LAY4]], i32 %[[GIDIV]])
    // CHECK:  call void @dx.op.storePrimitiveOutput.i32(i32 172, i32 4, i32 5, i8 0, i32 %[[LAY5]], i32 %[[GIDIV]])
    prims[tig / 3] = op;
  }
  // CHECK: call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 0, float 1.400000e+01, i32 %[[GID]])
  // CHECK: call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 1, float 1.500000e+01, i32 %[[GID]])
  // CHECK: call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 2, float 1.600000e+01, i32 %[[GID]])
  // CHECK: call void @dx.op.storeVertexOutput.f32(i32 171, i32 0, i32 0, i8 3, float 1.700000e+01, i32 %[[GID]])
  // CHECK: call void @dx.op.storeVertexOutput.f32(i32 171, i32 1, i32 0, i8 0, float 1.400000e+01, i32 %[[GID]])
  // CHECK: call void @dx.op.storeVertexOutput.f32(i32 171, i32 1, i32 1, i8 0, float 1.500000e+01, i32 %[[GID]])
  // CHECK: call void @dx.op.storeVertexOutput.f32(i32 171, i32 1, i32 2, i8 0, float 1.600000e+01, i32 %[[GID]])
  // CHECK: call void @dx.op.storeVertexOutput.f32(i32 171, i32 1, i32 3, i8 0, float 1.700000e+01, i32 %[[GID]])
  verts[tig] = ov;
}
