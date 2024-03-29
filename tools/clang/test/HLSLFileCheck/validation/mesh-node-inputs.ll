; RUN: %dxilver 1.9 | %dxv %s | FileCheck %s

; Test that appropriate errors are produced for invalid mesh node input types

; Generated from mesh-node-inputs.hlsl and altered by converting all the nodes to mesh

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.NodeRecordHandle = type { i8* }
%dx.types.NodeRecordInfo = type { i32, i32 }
%struct.RECORD = type { i32 }
%dx.types.ResourceProperties = type { i32, i32 }
%"class.RWBuffer<unsigned int>" = type { i32 }

@"\01?buf0@@3V?$RWBuffer@I@@A" = external constant %dx.types.Handle, align 4


; CHECK: mesh node shader 'node_RWDispatchNodeInputRecord' has incompatible input record type (should be DispatchNodeInputRecord)
define void @node_RWDispatchNodeInputRecord() {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?buf0@@3V?$RWBuffer@I@@A", align 4
  %2 = call %dx.types.NodeRecordHandle @dx.op.createNodeInputRecordHandle(i32 250, i32 0)  ; CreateNodeInputRecordHandle(MetadataIdx)
  %3 = call %dx.types.NodeRecordHandle @dx.op.annotateNodeRecordHandle(i32 251, %dx.types.NodeRecordHandle %2, %dx.types.NodeRecordInfo { i32 101, i32 4 })  ; AnnotateNodeRecordHandle(noderecord,props)
  %4 = call %struct.RECORD addrspace(6)* @dx.op.getNodeRecordPtr.struct.RECORD(i32 239, %dx.types.NodeRecordHandle %3, i32 0)  ; GetNodeRecordPtr(recordhandle,arrayIndex)
  %5 = getelementptr %struct.RECORD, %struct.RECORD addrspace(6)* %4, i32 0, i32 0
  %6 = load i32, i32 addrspace(6)* %5, align 4
  %7 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %8 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %7, %dx.types.ResourceProperties { i32 4106, i32 261 })  ; AnnotateHandle(res,props)  resource: RWTypedBuffer<U32>
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %8, i32 0, i32 undef, i32 %6, i32 %6, i32 %6, i32 %6, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  ret void
}

; CHECK: mesh node shader 'node_GroupNodeInputRecords' has incompatible input record type (should be DispatchNodeInputRecord)
define void @node_GroupNodeInputRecords() {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?buf0@@3V?$RWBuffer@I@@A", align 4
  %2 = call %dx.types.NodeRecordHandle @dx.op.createNodeInputRecordHandle(i32 250, i32 0)  ; CreateNodeInputRecordHandle(MetadataIdx)
  %3 = call %dx.types.NodeRecordHandle @dx.op.annotateNodeRecordHandle(i32 251, %dx.types.NodeRecordHandle %2, %dx.types.NodeRecordInfo { i32 65, i32 4 })  ; AnnotateNodeRecordHandle(noderecord,props)
  %4 = call %struct.RECORD addrspace(6)* @dx.op.getNodeRecordPtr.struct.RECORD(i32 239, %dx.types.NodeRecordHandle %3, i32 0)  ; GetNodeRecordPtr(recordhandle,arrayIndex)
  %5 = getelementptr %struct.RECORD, %struct.RECORD addrspace(6)* %4, i32 0, i32 0
  %6 = load i32, i32 addrspace(6)* %5, align 4
  %7 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %8 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %7, %dx.types.ResourceProperties { i32 4106, i32 261 })  ; AnnotateHandle(res,props)  resource: RWTypedBuffer<U32>
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %8, i32 0, i32 undef, i32 %6, i32 %6, i32 %6, i32 %6, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  ret void
}

; CHECK: mesh node shader 'node_RWGroupNodeInputRecords' has incompatible input record type (should be DispatchNodeInputRecord)
define void @node_RWGroupNodeInputRecords() {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?buf0@@3V?$RWBuffer@I@@A", align 4
  %2 = call %dx.types.NodeRecordHandle @dx.op.createNodeInputRecordHandle(i32 250, i32 0)  ; CreateNodeInputRecordHandle(MetadataIdx)
  %3 = call %dx.types.NodeRecordHandle @dx.op.annotateNodeRecordHandle(i32 251, %dx.types.NodeRecordHandle %2, %dx.types.NodeRecordInfo { i32 69, i32 4 })  ; AnnotateNodeRecordHandle(noderecord,props)
  %4 = call %struct.RECORD addrspace(6)* @dx.op.getNodeRecordPtr.struct.RECORD(i32 239, %dx.types.NodeRecordHandle %3, i32 0)  ; GetNodeRecordPtr(recordhandle,arrayIndex)
  %5 = getelementptr %struct.RECORD, %struct.RECORD addrspace(6)* %4, i32 0, i32 0
  %6 = load i32, i32 addrspace(6)* %5, align 4
  %7 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %8 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %7, %dx.types.ResourceProperties { i32 4106, i32 261 })  ; AnnotateHandle(res,props)  resource: RWTypedBuffer<U32>
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %8, i32 0, i32 undef, i32 %6, i32 %6, i32 %6, i32 %6, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  ret void
}

; CHECK: mesh node shader 'node_ThreadNodeInputRecord' has incompatible input record type (should be DispatchNodeInputRecord)
define void @node_ThreadNodeInputRecord() {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?buf0@@3V?$RWBuffer@I@@A", align 4
  %2 = call %dx.types.NodeRecordHandle @dx.op.createNodeInputRecordHandle(i32 250, i32 0)  ; CreateNodeInputRecordHandle(MetadataIdx)
  %3 = call %dx.types.NodeRecordHandle @dx.op.annotateNodeRecordHandle(i32 251, %dx.types.NodeRecordHandle %2, %dx.types.NodeRecordInfo { i32 33, i32 4 })  ; AnnotateNodeRecordHandle(noderecord,props)
  %4 = call %struct.RECORD addrspace(6)* @dx.op.getNodeRecordPtr.struct.RECORD(i32 239, %dx.types.NodeRecordHandle %3, i32 0)  ; GetNodeRecordPtr(recordhandle,arrayIndex)
  %5 = getelementptr %struct.RECORD, %struct.RECORD addrspace(6)* %4, i32 0, i32 0
  %6 = load i32, i32 addrspace(6)* %5, align 4
  %7 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %8 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %7, %dx.types.ResourceProperties { i32 4106, i32 261 })  ; AnnotateHandle(res,props)  resource: RWTypedBuffer<U32>
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %8, i32 0, i32 undef, i32 %6, i32 %6, i32 %6, i32 %6, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  ret void
}

; CHECK: mesh node shader 'node_RWThreadNodeInputRecord' has incompatible input record type (should be DispatchNodeInputRecord)
define void @node_RWThreadNodeInputRecord() {
  %1 = load %dx.types.Handle, %dx.types.Handle* @"\01?buf0@@3V?$RWBuffer@I@@A", align 4
  %2 = call %dx.types.NodeRecordHandle @dx.op.createNodeInputRecordHandle(i32 250, i32 0)  ; CreateNodeInputRecordHandle(MetadataIdx)
  %3 = call %dx.types.NodeRecordHandle @dx.op.annotateNodeRecordHandle(i32 251, %dx.types.NodeRecordHandle %2, %dx.types.NodeRecordInfo { i32 37, i32 4 })  ; AnnotateNodeRecordHandle(noderecord,props)
  %4 = call %struct.RECORD addrspace(6)* @dx.op.getNodeRecordPtr.struct.RECORD(i32 239, %dx.types.NodeRecordHandle %3, i32 0)  ; GetNodeRecordPtr(recordhandle,arrayIndex)
  %5 = getelementptr %struct.RECORD, %struct.RECORD addrspace(6)* %4, i32 0, i32 0
  %6 = load i32, i32 addrspace(6)* %5, align 4
  %7 = call %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32 160, %dx.types.Handle %1)  ; CreateHandleForLib(Resource)
  %8 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %7, %dx.types.ResourceProperties { i32 4106, i32 261 })  ; AnnotateHandle(res,props)  resource: RWTypedBuffer<U32>
  call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %8, i32 0, i32 undef, i32 %6, i32 %6, i32 %6, i32 %6, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
  ret void
}

; Function Attrs: nounwind readnone
declare %struct.RECORD addrspace(6)* @dx.op.getNodeRecordPtr.struct.RECORD(i32, %dx.types.NodeRecordHandle, i32) #0

; Function Attrs: nounwind
declare void @dx.op.bufferStore.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i8) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #0

; Function Attrs: nounwind readnone
declare %dx.types.NodeRecordHandle @dx.op.createNodeInputRecordHandle(i32, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.NodeRecordHandle @dx.op.annotateNodeRecordHandle(i32, %dx.types.NodeRecordHandle, %dx.types.NodeRecordInfo) #0

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandleForLib.dx.types.Handle(i32, %dx.types.Handle) #2

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }
attributes #2 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.typeAnnotations = !{!7}
!dx.entryPoints = !{!11, !13, !21, !27, !32, !38}

!0 = !{!"dxc(private) 1.8.0.4537 (ready-for-6.9, baa5d308a-dirty)"}
!1 = !{i32 1, i32 9}
!2 = !{!"lib", i32 6, i32 9}
!3 = !{null, !4, null, null}
!4 = !{!5}
!5 = !{i32 0, %"class.RWBuffer<unsigned int>"* bitcast (%dx.types.Handle* @"\01?buf0@@3V?$RWBuffer@I@@A" to %"class.RWBuffer<unsigned int>"*), !"buf0", i32 -1, i32 -1, i32 1, i32 10, i1 false, i1 false, i1 false, !6}
!6 = !{i32 0, i32 5}
!7 = !{i32 1, void ()* @node_RWDispatchNodeInputRecord, !8, void ()* @node_GroupNodeInputRecords, !8, void ()* @node_RWGroupNodeInputRecords, !8, void ()* @node_ThreadNodeInputRecord, !8, void ()* @node_RWThreadNodeInputRecord, !8}
!8 = !{!9}
!9 = !{i32 0, !10, !10}
!10 = !{}
!11 = !{null, !"", null, !3, !12}
!12 = !{i32 0, i64 8589934592}
!13 = !{void ()* @node_GroupNodeInputRecords, !"node_GroupNodeInputRecords", null, null, !14}
!14 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !15, i32 16, i32 -1, i32 20, !16, i32 4, !19, i32 5, !20}
!15 = !{!"node_GroupNodeInputRecords", i32 0}
!16 = !{!17}
!17 = !{i32 1, i32 65, i32 2, !18}
!18 = !{i32 0, i32 4, i32 2, i32 4}
!19 = !{i32 1024, i32 1, i32 1}
!20 = !{i32 0}
!21 = !{void ()* @node_RWDispatchNodeInputRecord, !"node_RWDispatchNodeInputRecord", null, null, !22}
!22 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !23, i32 16, i32 -1, i32 18, !24, i32 20, !25, i32 4, !19, i32 5, !20}
!23 = !{!"node_RWDispatchNodeInputRecord", i32 0}
!24 = !{i32 64, i32 1, i32 1}
!25 = !{!26}
!26 = !{i32 1, i32 101, i32 2, !18}
!27 = !{void ()* @node_RWGroupNodeInputRecords, !"node_RWGroupNodeInputRecords", null, null, !28}
!28 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !29, i32 16, i32 -1, i32 20, !30, i32 4, !19, i32 5, !20}
!29 = !{!"node_RWGroupNodeInputRecords", i32 0}
!30 = !{!31}
!31 = !{i32 1, i32 69, i32 2, !18}
!32 = !{void ()* @node_RWThreadNodeInputRecord, !"node_RWThreadNodeInputRecord", null, null, !33}
!33 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !34, i32 16, i32 -1, i32 20, !35, i32 4, !37, i32 5, !20}
!34 = !{!"node_RWThreadNodeInputRecord", i32 0}
!35 = !{!36}
!36 = !{i32 1, i32 37, i32 2, !18}
!37 = !{i32 1, i32 1, i32 1}
!38 = !{void ()* @node_ThreadNodeInputRecord, !"node_ThreadNodeInputRecord", null, null, !39}
!39 = !{i32 8, i32 15, i32 13, i32 4, i32 15, !40, i32 16, i32 -1, i32 20, !41, i32 4, !37, i32 5, !20}
!40 = !{!"node_ThreadNodeInputRecord", i32 0}
!41 = !{!42}
!42 = !{i32 1, i32 33, i32 2, !18}

