// RUN: %dxc -T lib_6_9 %s | FileCheck %s

// REQUIRES: dxil-1-9

// Test all valid mesh node input parameters work

// CHECK: define void @node01()
// CHECK:  %[[tid_x:.+]] = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
// CHECK:  %[[tid_y:.+]] = call i32 @dx.op.threadId.i32(i32 93, i32 1)  ; ThreadId(component)
// CHECK:  %[[tid_z:.+]] = call i32 @dx.op.threadId.i32(i32 93, i32 2)  ; ThreadId(component)

// CHECK:  %[[ftid:.+]] = call i32 @dx.op.flattenedThreadIdInGroup.i32(i32 96)  ; FlattenedThreadIdInGroup()

// CHECK:  %[[tid_group_x:.+]] = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 0)  ; ThreadIdInGroup(component)
// CHECK:  %[[tid_group_y:.+]] = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 1)  ; ThreadIdInGroup(component)
// CHECK:  %[[tid_group_z:.+]] = call i32 @dx.op.threadIdInGroup.i32(i32 95, i32 2)  ; ThreadIdInGroup(component)

// CHECK:  %[[Hdl:.+]] = call %dx.types.NodeRecordHandle @dx.op.createNodeInputRecordHandle(i32 250, i32 0)  ; CreateNodeInputRecordHandle(MetadataIdx)
// CHECK:  %[[annotHdl:.+]] = call %dx.types.NodeRecordHandle @dx.op.annotateNodeRecordHandle(i32 251, %dx.types.NodeRecordHandle %[[Hdl]], %dx.types.NodeRecordInfo { i32 97, i32 52 })  ; AnnotateNodeRecordHandle(noderecord,props)

// CHECK:  %[[node_ptr:.+]] = call %struct.RECORD.0 addrspace(6)* @dx.op.getNodeRecordPtr.struct.RECORD.0(i32 239, %dx.types.NodeRecordHandle %[[annotHdl]], i32 0)  ; GetNodeRecordPtr(recordhandle,arrayIndex)
// CHECK:  %[[ptr:.+]] = getelementptr inbounds %struct.RECORD.0, %struct.RECORD.0 addrspace(6)* %[[node_ptr]], i32 0, i32 0, i32 0
// CHECK:  %[[ld1:.+]] = load i32, i32 addrspace(6)* %[[ptr]], align 4
// CHECK:  %[[ptr:.+]] = getelementptr inbounds %struct.RECORD.0, %struct.RECORD.0 addrspace(6)* %[[node_ptr]], i32 0, i32 0, i32 1
// CHECK:  %[[ld2:.+]] = load i32, i32 addrspace(6)* %[[ptr]], align 4
// CHECK:  %[[ptr:.+]] = getelementptr inbounds %struct.RECORD.0, %struct.RECORD.0 addrspace(6)* %[[node_ptr]], i32 0, i32 0, i32 2
// CHECK:  %[[ld3:.+]] = load i32, i32 addrspace(6)* %[[ptr]], align 4
// CHECK:  call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %{{[0-9]+}}, i32 1, i32 0, i32 %[[ld1]], i32 %[[ld2]], i32 %[[ld3]], i32 undef, i8 7, i32 4)  ; RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)

// CHECK:  %[[ptr:.+]] = getelementptr inbounds %struct.RECORD.0, %struct.RECORD.0 addrspace(6)* %[[node_ptr]], i32 0, i32 1, i32 0
// CHECK:  %[[ld1:.+]] = load i32, i32 addrspace(6)* %[[ptr]], align 4
// CHECK:  %[[ptr:.+]] = getelementptr inbounds %struct.RECORD.0, %struct.RECORD.0 addrspace(6)* %[[node_ptr]], i32 0, i32 1, i32 1
// CHECK:  %[[ld2:.+]] = load i32, i32 addrspace(6)* %[[ptr]], align 4
// CHECK:  %[[ptr:.+]] = getelementptr inbounds %struct.RECORD.0, %struct.RECORD.0 addrspace(6)* %[[node_ptr]], i32 0, i32 1, i32 2
// CHECK:  %[[ld3:.+]] = load i32, i32 addrspace(6)* %[[ptr]], align 4
// CHECK:  call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %{{[0-9]+}}, i32 2, i32 0, i32 %[[ld1]], i32 %[[ld2]], i32 %[[ld3]], i32 undef, i8 7, i32 4)  ; RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)

// CHECK:  %[[ptr:.+]] = getelementptr %struct.RECORD.0, %struct.RECORD.0 addrspace(6)* %[[node_ptr]], i32 0, i32 2
// CHECK:  %[[ld1:.+]] = load i32, i32 addrspace(6)* %[[ptr]], align 4
// CHECK:  call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %{{[0-9]+}}, i32 3, i32 0, i32 %[[ld1]], i32 %[[ld1]], i32 %[[ld1]], i32 undef, i8 7, i32 4)  ; RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)

// CHECK:  %[[ptr:.+]] = getelementptr inbounds %struct.RECORD.0, %struct.RECORD.0 addrspace(6)* %[[node_ptr]], i32 0, i32 3, i32 0
// CHECK:  %[[ld1:.+]] = load i32, i32 addrspace(6)* %[[ptr]], align 4
// CHECK:  %[[ptr:.+]] = getelementptr inbounds %struct.RECORD.0, %struct.RECORD.0 addrspace(6)* %[[node_ptr]], i32 0, i32 3, i32 1
// CHECK:  %[[ld2:.+]] = load i32, i32 addrspace(6)* %[[ptr]], align 4
// CHECK:  %[[ptr:.+]] = getelementptr inbounds %struct.RECORD.0, %struct.RECORD.0 addrspace(6)* %[[node_ptr]], i32 0, i32 3, i32 2
// CHECK:  %[[ld3:.+]] = load i32, i32 addrspace(6)* %[[ptr]], align 4
// CHECK:  call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %{{[0-9]+}}, i32 4, i32 0, i32 %[[ld1]], i32 %[[ld2]], i32 %[[ld3]], i32 undef, i8 7, i32 4)  ; RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
// CHECK:  ret void

struct RECORD
{
  uint3 dtid;
  uint3 gid;
  uint gidx;
  uint3 gtid;
  uint3 dg : SV_DispatchGrid;
};

RWStructuredBuffer<uint3> outbuf;

[Shader("node")]
[numthreads(4,4,4)]
[NodeMaxDispatchGrid(4,4,4)]
[NodeLaunch("mesh")]
[OutputTopology("line")]
void node01(DispatchNodeInputRecord<RECORD> input,
 uint3 DTID : SV_DispatchThreadID,
 uint3 GID : SV_GroupID,
 uint GIdx : SV_GroupIndex,
 uint3 GTID : SV_GroupThreadID )
{
  outbuf[0] = input.Get().dg;
  if (any(DTID) != 0)
    return;
  outbuf[1] = input.Get().dtid;
  if (any(GID) != 0)
    return;
  outbuf[2] = input.Get().gid;
  if (GIdx != 0)
    return;
  outbuf[3] = input.Get().gidx;
  if (any(GTID) != 0)
    return;
  outbuf[4] = input.Get().gtid;
}
