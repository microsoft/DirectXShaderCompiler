; RUN: %dxopt %s -hlsl-passes-resume -dxilgen -S | FileCheck %s

; Verify that DXIL Op Lowering preserves the noduplicate attribute on the
; SetMeshOutputCounts intrinsic when generating the dx.op.setMeshOutputCounts
; DXIL operation. See GitHub issue #8104.

; CHECK: call void @dx.op.setMeshOutputCounts(i32 168,
; CHECK: declare void @dx.op.setMeshOutputCounts(i32, i32, i32) [[ATTR:#[0-9]+]]
; CHECK: attributes [[ATTR]] = { noduplicate nounwind }

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.Payload = type { i32, i32 }

; Function Attrs: nounwind
define void @main(%struct.Payload* %pl) #0 {
  %1 = getelementptr inbounds %struct.Payload, %struct.Payload* %pl, i32 0, i32 1
  %2 = load i32, i32* %1, align 4
  %3 = getelementptr inbounds %struct.Payload, %struct.Payload* %pl, i32 0, i32 0
  %4 = load i32, i32* %3, align 4
  call void @"dx.hl.op.nd.void (i32, i32, i32)"(i32 68, i32 %4, i32 %2)
  ret void
}

; Function Attrs: noduplicate nounwind
declare void @"dx.hl.op.nd.void (i32, i32, i32)"(i32, i32, i32) #1

attributes #0 = { nounwind }
attributes #1 = { noduplicate nounwind }

!pauseresume = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.typeAnnotations = !{!4, !8}
!dx.entryPoints = !{!13}
!dx.fnprops = !{!14}
!dx.options = !{!15, !16}

!0 = !{!"hlsl-hlemit", !"hlsl-hlensure"}
!1 = !{i32 1, i32 5}
!2 = !{i32 1, i32 10}
!3 = !{!"ms", i32 6, i32 5}
!4 = !{i32 0, %struct.Payload undef, !5}
!5 = !{i32 8, !6, !7}
!6 = !{i32 6, !"nv", i32 3, i32 0, i32 7, i32 5}
!7 = !{i32 6, !"np", i32 3, i32 4, i32 7, i32 5}
!8 = !{i32 1, void (%struct.Payload*)* @main, !9}
!9 = !{!10, !12}
!10 = !{i32 1, !11, !11}
!11 = !{}
!12 = !{i32 13, !11, !11}
!13 = !{void (%struct.Payload*)* @main, !"main", null, null, null}
!14 = !{void (%struct.Payload*)* @main, i32 13, i32 1, i32 1, i32 1, i32 0, i32 0, i8 2, i32 8}
!15 = !{i32 64}
!16 = !{i32 -1}
