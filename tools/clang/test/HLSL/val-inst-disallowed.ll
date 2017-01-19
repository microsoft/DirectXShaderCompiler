; RUN: %dxv %s | FileCheck %s

; CHECK: Semantic 'SV_Target' is invalid as vs Output

target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

%dx.types.wave_t = type { i8* }

define void @"\01?main@@YA?AV?$vector@M$03@@XZ.flat"(<4 x float>*) {
entry:
; CHECK: Instructions must not reference reserved opcodes
  %WaveCapture = call %dx.types.wave_t @dx.op.waveCapture(i32 114, i8 0)

; CHECK: Declaration '%dx.types.wave_t = type { i8* }' uses a reserved prefix
  %wave_local = alloca %dx.types.wave_t

  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 0.000000e+00)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 0.000000e+00)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float 0.000000e+00)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float 0.000000e+00)
  ret void
; CHECK: Instructions must be of an allowed type
  unreachable
}

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0
; Function Attrs: nounwind readonly
declare %dx.types.wave_t @dx.op.waveCapture(i32, i8) #1
; Function Attrs: nounwind readonly
declare i1 @dx.op.waveAllIsTrue(i32, %dx.types.wave_t, i1) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.typeAnnotations = !{!3}
!dx.entryPoints = !{!9}

!0 = !{!"clang version 3.7.0 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 4}
!2 = !{!"vs", i32 6, i32 0}
!3 = !{i32 1, void (<4 x float>*)* @"\01?main@@YA?AV?$vector@M$03@@XZ.flat", !4}
!4 = !{!5, !7}
!5 = !{i32 0, !6, !13}
!6 = !{}
!7 = !{i32 1, !8, !13}
!8 = !{i32 4, !"SV_Target", i32 7, i32 9}
!9 = !{void (<4 x float>*)* @"\01?main@@YA?AV?$vector@M$03@@XZ.flat", !"", !10, null, null}
!10 = !{null, !11, null}
!11 = !{!12}
!12 = !{i32 0, !"SV_Target", i8 9, i8 16, !13, i8 0, i32 1, i8 4, i32 0, i8 0, null}
!13 = !{i32 0}
