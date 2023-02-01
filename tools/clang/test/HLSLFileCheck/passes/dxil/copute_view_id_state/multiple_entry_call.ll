; RUN: %opt %s -viewid-state -S | FileCheck %s

; Just make sure it not crash.
; CHECK: define void @main()
; CHECK: call fastcc float @"\01?foo@@YAMM@Z"(float %{{.+}})
; CHECK: call fastcc float @"\01?foo@@YAMM@Z"(float %{{.+}})

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; Function Attrs: noinline nounwind readnone
define internal fastcc float @"\01?foo@@YAMM@Z"(float %a) #0 {
entry:
  %add = fadd float %a, 2.000000e+00
  ret float %add
}

define void @main() {
entry:
  %0 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %1 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
  %call = call fastcc float @"\01?foo@@YAMM@Z"(float %0)
  %call1 = call fastcc float @"\01?foo@@YAMM@Z"(float %1)
  %add = fadd fast float %call1, %call
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %add)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #2

attributes #0 = { noinline nounwind readnone }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.viewIdState = !{!4}
!dx.entryPoints = !{!5}

!0 = !{!"dxc(private) 1.7.0.3806 (local_taef, 9c3dab689-dirty)"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 7}
!3 = !{!"ps", i32 6, i32 0}
!4 = !{[4 x i32] [i32 2, i32 1, i32 1, i32 1]}
!5 = !{void ()* @main, !"main", !6, null, null}
!6 = !{!7, !11, null}
!7 = !{!8}
!8 = !{i32 0, !"A", i8 9, i8 0, !9, i8 2, i32 1, i8 2, i32 0, i8 0, !10}
!9 = !{i32 0}
!10 = !{i32 3, i32 3}
!11 = !{!12}
!12 = !{i32 0, !"SV_Target", i8 9, i8 16, !9, i8 0, i32 1, i8 1, i32 0, i8 0, !13}
!13 = !{i32 3, i32 1}
