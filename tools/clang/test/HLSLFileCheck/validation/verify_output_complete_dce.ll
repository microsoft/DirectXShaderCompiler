; RUN: %opt %s -hlsl-passes-resume -dce -S | FileCheck %s

; shader hash: 8f77518ca1d616c4259bbb38d029f95c
;
; Buffer Definitions:
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.NodeHandle = type { i8* }
%dx.types.NodeInfo = type { i32, i32 }
%dx.types.NodeRecordHandle = type { i8* }
%dx.types.NodeRecordInfo = type { i32, i32 }
%struct.loadStressRecord.0 = type { [3 x i32], [3 x i32] }

@"\01?loadStressTemp@@3PAIA" = external addrspace(3) global [128 x i32], align 4

define void @loadStress_16() {
  %1 = call i32 @dx.op.flattenedThreadIdInGroup.i32(i32 96)  ; FlattenedThreadIdInGroup()
  %2 = call %dx.types.NodeHandle @dx.op.createNodeOutputHandle(i32 247, i32 0)  ; CreateNodeOutputHandle(MetadataIdx)
  %3 = call %dx.types.NodeHandle @dx.op.annotateNodeHandle(i32 249, %dx.types.NodeHandle %2, %dx.types.NodeInfo { i32 6, i32 24 })  ; AnnotateNodeHandle(node,props)
  %4 = call %dx.types.NodeRecordHandle @dx.op.allocateNodeOutputRecords(i32 238, %dx.types.NodeHandle %3, i32 1, i1 true)  ; AllocateNodeOutputRecords(output,numRecords,perThread)
  %5 = call %dx.types.NodeRecordHandle @dx.op.annotateNodeRecordHandle(i32 251, %dx.types.NodeRecordHandle %4, %dx.types.NodeRecordInfo { i32 38, i32 24 })  ; AnnotateNodeRecordHandle(noderecord,props)
  %6 = urem i32 %1, 3
  %7 = add nuw nsw i32 %6, 1
  %8 = call %struct.loadStressRecord.0 addrspace(6)* @dx.op.getNodeRecordPtr.struct.loadStressRecord.0(i32 239, %dx.types.NodeRecordHandle %5, i32 0)  ; GetNodeRecordPtr(recordhandle,arrayIndex)
  %9 = getelementptr inbounds %struct.loadStressRecord.0, %struct.loadStressRecord.0 addrspace(6)* %8, i32 0, i32 0, i32 0
  store i32 %7, i32 addrspace(6)* %9, align 4
  %10 = getelementptr inbounds %struct.loadStressRecord.0, %struct.loadStressRecord.0 addrspace(6)* %8, i32 0, i32 0, i32 1
  store i32 1, i32 addrspace(6)* %10, align 4
  %11 = getelementptr inbounds %struct.loadStressRecord.0, %struct.loadStressRecord.0 addrspace(6)* %8, i32 0, i32 0, i32 2
  store i32 1, i32 addrspace(6)* %11, align 4
  %12 = load i32, i32 addrspace(3)* getelementptr inbounds ([128 x i32], [128 x i32] addrspace(3)* @"\01?loadStressTemp@@3PAIA", i32 0, i32 0), align 4, !tbaa !22
  %13 = getelementptr inbounds %struct.loadStressRecord.0, %struct.loadStressRecord.0 addrspace(6)* %8, i32 0, i32 1, i32 0
  store i32 %12, i32 addrspace(6)* %13, align 4
  %14 = load i32, i32 addrspace(3)* getelementptr inbounds ([128 x i32], [128 x i32] addrspace(3)* @"\01?loadStressTemp@@3PAIA", i32 0, i32 1), align 4, !tbaa !22
  %15 = getelementptr inbounds %struct.loadStressRecord.0, %struct.loadStressRecord.0 addrspace(6)* %8, i32 0, i32 1, i32 1
  store i32 %14, i32 addrspace(6)* %15, align 4
  %16 = load i32, i32 addrspace(3)* getelementptr inbounds ([128 x i32], [128 x i32] addrspace(3)* @"\01?loadStressTemp@@3PAIA", i32 0, i32 2), align 4, !tbaa !22
  %17 = getelementptr inbounds %struct.loadStressRecord.0, %struct.loadStressRecord.0 addrspace(6)* %8, i32 0, i32 1, i32 2
  store i32 %16, i32 addrspace(6)* %17, align 4
  call void @dx.op.outputComplete(i32 241, %dx.types.NodeRecordHandle %5)  ; OutputComplete(output)
  
  ; test that the 2 subsequent output complete calls are removed by DCE
  ; CHECK: call void @dx.op.outputComplete(i32 241, %dx.types.NodeRecordHandle %5)
  ; CHECK-NEXT: ret void
  call void @dx.op.outputComplete(i32 241, %dx.types.NodeRecordHandle zeroinitializer)  ; OutputComplete(output)
  call void @dx.op.outputComplete(i32 241, %dx.types.NodeRecordHandle zeroinitializer)  ; OutputComplete(output)
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.flattenedThreadIdInGroup.i32(i32) #0

; Function Attrs: nounwind readnone
declare %struct.loadStressRecord.0 addrspace(6)* @dx.op.getNodeRecordPtr.struct.loadStressRecord.0(i32, %dx.types.NodeRecordHandle, i32) #0

; Function Attrs: nounwind
declare %dx.types.NodeRecordHandle @dx.op.allocateNodeOutputRecords(i32, %dx.types.NodeHandle, i32, i1) #1

; Function Attrs: nounwind
declare void @dx.op.outputComplete(i32, %dx.types.NodeRecordHandle) #1

; Function Attrs: nounwind readnone
declare %dx.types.NodeRecordHandle @dx.op.annotateNodeRecordHandle(i32, %dx.types.NodeRecordHandle, %dx.types.NodeRecordInfo) #0

; Function Attrs: nounwind readnone
declare %dx.types.NodeHandle @dx.op.createNodeOutputHandle(i32, i32) #0

; Function Attrs: nounwind readnone
declare %dx.types.NodeHandle @dx.op.annotateNodeHandle(i32, %dx.types.NodeHandle, %dx.types.NodeInfo) #0

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.typeAnnotations = !{!3}
!dx.entryPoints = !{!7, !8}

!0 = !{!"dxc(private) 1.8.0.3958 (fixWaveTests, c110270b3-dirty)"}
!1 = !{i32 1, i32 8}
!2 = !{!"lib", i32 6, i32 8}
!3 = !{i32 1, void ()* @loadStress_16, !4}
!4 = !{!5}
!5 = !{i32 0, !6, !6}
!6 = !{}
!7 = !{null, !"", null, null, null}
!8 = !{void ()* @loadStress_16, !"loadStress_16", null, null, !9}
!9 = !{i32 8, i32 15, i32 13, i32 1, i32 15, !10, i32 16, i32 -1, i32 22, !11, i32 20, !12, i32 21, !16, i32 4, !20, i32 5, !21}
!10 = !{!"loadStress_16", i32 0}
!11 = !{i32 3, i32 1, i32 1}
!12 = !{!13}
!13 = !{i32 1, i32 97, i32 2, !14}
!14 = !{i32 0, i32 12, i32 1, !15}
!15 = !{i32 0, i32 5, i32 3}
!16 = !{!17}
!17 = !{i32 1, i32 6, i32 2, !18, i32 3, i32 0, i32 0, !19}
!18 = !{i32 0, i32 24, i32 1, !15}
!19 = !{!"loadStressChild", i32 0}
!20 = !{i32 1, i32 1, i32 1}
!21 = !{i32 0}
!22 = !{!23, !23, i64 0}
!23 = !{!"int", !24, i64 0}
!24 = !{!"omnipotent char", !25, i64 0}
!25 = !{!"Simple C/C++ TBAA"}
