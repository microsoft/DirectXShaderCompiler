; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s

; Work graph opcodes are not valid in shader model 6.10.

; CHECK-DAG: error: Opcode AllocateNodeOutputRecords not valid in shader model lib_6_10(node).
; CHECK-DAG: error: Opcode GetNodeRecordPtr not valid in shader model lib_6_10(node).
; CHECK-DAG: error: Opcode IncrementOutputCount not valid in shader model lib_6_10(node).
; CHECK-DAG: error: Opcode OutputComplete not valid in shader model lib_6_10(node).
; CHECK-DAG: error: Opcode GetInputRecordCount not valid in shader model lib_6_10(node).
; CHECK-DAG: error: Opcode FinishedCrossGroupSharing not valid in shader model lib_6_10(node).
; CHECK-DAG: error: Opcode BarrierByNodeRecordHandle not valid in shader model lib_6_10(node).
; CHECK-DAG: error: Opcode CreateNodeOutputHandle not valid in shader model lib_6_10(node).
; CHECK-DAG: error: Opcode IndexNodeHandle not valid in shader model lib_6_10(node).
; CHECK-DAG: error: Opcode AnnotateNodeHandle not valid in shader model lib_6_10(node).
; CHECK-DAG: error: Opcode CreateNodeInputRecordHandle not valid in shader model lib_6_10(node).
; CHECK-DAG: error: Opcode AnnotateNodeRecordHandle not valid in shader model lib_6_10(node).
; CHECK-DAG: error: Opcode NodeOutputIsValid not valid in shader model lib_6_10(node).
; CHECK-DAG: error: Opcode GetRemainingRecursionLevels not valid in shader model lib_6_10(node).
; CHECK: Validation failed.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.NodeHandle = type { i8* }
%dx.types.NodeInfo = type { i32, i32 }
%dx.types.NodeRecordHandle = type { i8* }
%dx.types.NodeRecordInfo = type { i32, i32 }
%struct.Record = type { i32 }

define void @Node() {
  %output = call %dx.types.NodeHandle @dx.op.createNodeOutputHandle(i32 247, i32 0)
  %indexed = call %dx.types.NodeHandle @dx.op.indexNodeHandle(i32 248, %dx.types.NodeHandle %output, i32 0)
  %annotatedOutput = call %dx.types.NodeHandle @dx.op.annotateNodeHandle(i32 249, %dx.types.NodeHandle %indexed, %dx.types.NodeInfo { i32 6, i32 4 })
  %outputRecord = call %dx.types.NodeRecordHandle @dx.op.allocateNodeOutputRecords(i32 238, %dx.types.NodeHandle %annotatedOutput, i32 1, i1 true)
  %annotatedOutputRecord = call %dx.types.NodeRecordHandle @dx.op.annotateNodeRecordHandle(i32 251, %dx.types.NodeRecordHandle %outputRecord, %dx.types.NodeRecordInfo { i32 38, i32 4 })
  %recordPtr = call %struct.Record addrspace(6)* @dx.op.getNodeRecordPtr.struct.Record(i32 239, %dx.types.NodeRecordHandle %annotatedOutputRecord, i32 0)
  call void @dx.op.incrementOutputCount(i32 240, %dx.types.NodeHandle %annotatedOutput, i32 1, i1 true)
  call void @dx.op.outputComplete(i32 241, %dx.types.NodeRecordHandle %annotatedOutputRecord)
  %inputRecord = call %dx.types.NodeRecordHandle @dx.op.createNodeInputRecordHandle(i32 250, i32 0)
  %inputCount = call i32 @dx.op.getInputRecordCount(i32 242, %dx.types.NodeRecordHandle %inputRecord)
  %finished = call i1 @dx.op.finishedCrossGroupSharing(i32 243, %dx.types.NodeRecordHandle %inputRecord)
  call void @dx.op.barrierByNodeRecordHandle(i32 246, %dx.types.NodeRecordHandle %inputRecord, i32 0)
  %valid = call i1 @dx.op.nodeOutputIsValid(i32 252, %dx.types.NodeHandle %annotatedOutput)
  %levels = call i32 @dx.op.getRemainingRecursionLevels(i32 253)
  ret void
}

declare %dx.types.NodeRecordHandle @dx.op.allocateNodeOutputRecords(i32, %dx.types.NodeHandle, i32, i1)
declare %struct.Record addrspace(6)* @dx.op.getNodeRecordPtr.struct.Record(i32, %dx.types.NodeRecordHandle, i32)
declare void @dx.op.incrementOutputCount(i32, %dx.types.NodeHandle, i32, i1)
declare void @dx.op.outputComplete(i32, %dx.types.NodeRecordHandle)
declare i32 @dx.op.getInputRecordCount(i32, %dx.types.NodeRecordHandle)
declare i1 @dx.op.finishedCrossGroupSharing(i32, %dx.types.NodeRecordHandle)
declare void @dx.op.barrierByNodeRecordHandle(i32, %dx.types.NodeRecordHandle, i32)
declare %dx.types.NodeHandle @dx.op.createNodeOutputHandle(i32, i32)
declare %dx.types.NodeHandle @dx.op.indexNodeHandle(i32, %dx.types.NodeHandle, i32)
declare %dx.types.NodeHandle @dx.op.annotateNodeHandle(i32, %dx.types.NodeHandle, %dx.types.NodeInfo)
declare %dx.types.NodeRecordHandle @dx.op.createNodeInputRecordHandle(i32, i32)
declare %dx.types.NodeRecordHandle @dx.op.annotateNodeRecordHandle(i32, %dx.types.NodeRecordHandle, %dx.types.NodeRecordInfo)
declare i1 @dx.op.nodeOutputIsValid(i32, %dx.types.NodeHandle)
declare i32 @dx.op.getRemainingRecursionLevels(i32)

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.typeAnnotations = !{!3}
!dx.entryPoints = !{!7, !9}

!0 = !{!"custom IR"}
!1 = !{i32 1, i32 10}
!2 = !{!"lib", i32 6, i32 10}
!3 = !{i32 1, void ()* @Node, !4}
!4 = !{!5}
!5 = !{i32 0, !6, !6}
!6 = !{}
!7 = !{null, !"", null, null, !8}
!8 = !{i32 0, i64 524288}
!9 = !{void ()* @Node, !"Node", null, null, !10}
!10 = !{i32 8, i32 15, i32 13, i32 3, i32 15, !11, i32 16, i32 -1, i32 20, !12, i32 21, !15, i32 4, !18, i32 5, !19}
!11 = !{!"Node", i32 0}
!12 = !{!13}
!13 = !{i32 1, i32 37, i32 2, !14}
!14 = !{i32 0, i32 4, i32 2, i32 4}
!15 = !{!16}
!16 = !{i32 1, i32 6, i32 2, !14, i32 3, i32 1, i32 0, !17}
!17 = !{!"output", i32 0}
!18 = !{i32 1, i32 1, i32 1}
!19 = !{i32 0}
