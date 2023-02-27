; RUN: %dxilver 1.0 | %dxv %s | FileCheck %s

; Test based on IR generated from the DXILValidation/SimpleHs1.hlsl test file.
; CHECK: When HS input control point count is 0, no input signature should exist

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define void @PatchFunc() {
  ret void
}

define void @main() {
  ret void
}

!dx.version = !{!0}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.viewIdState = !{!3}
!dx.entryPoints = !{!4}

!0 = !{i32 1, i32 0}
!1 = !{i32 1, i32 0}
!2 = !{!"hs", i32 6, i32 0}
!3 = !{[29 x i32] [i32 13, i32 13, i32 1, i32 2, i32 4, i32 8, i32 16, i32 32, i32 0, i32 0, i32 256, i32 512, i32 1024, i32 0, i32 4096, i32 13, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0]}
!4 = !{void ()* @main, !"main", !5, null, !20}
!5 = !{!6, !6, !16}
!6 = !{!7, !10, !12, !14}
!7 = !{i32 0, !"SV_Position", i8 9, i8 3, !8, i8 4, i32 1, i8 4, i32 0, i8 0, !9}
!8 = !{i32 0}
!9 = !{i32 3, i32 15}
!10 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !8, i8 2, i32 1, i8 2, i32 1, i8 0, !11}
!11 = !{i32 3, i32 3}
!12 = !{i32 2, !"NORMAL", i8 9, i8 0, !8, i8 2, i32 1, i8 3, i32 2, i8 0, !13}
!13 = !{i32 3, i32 7}
!14 = !{i32 3, !"SV_RenderTargetArrayIndex", i8 5, i8 4, !8, i8 1, i32 1, i8 1, i32 3, i8 0, !15}
!15 = !{i32 3, i32 1}
!16 = !{!17, !19}
!17 = !{i32 0, !"SV_TessFactor", i8 9, i8 25, !18, i8 0, i32 3, i8 1, i32 0, i8 3, !15}
!18 = !{i32 0, i32 1, i32 2}
!19 = !{i32 1, !"SV_InsideTessFactor", i8 9, i8 26, !8, i8 0, i32 1, i8 1, i32 3, i8 0, !15}
!20 = !{i32 0, i64 512, i32 3, !21}
!21 = !{void ()* @PatchFunc, i32 0, i32 3, i32 2, i32 3, i32 3, float 6.400000e+01}
