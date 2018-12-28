; RUN: %opt %s -hlmatrixlower -S | FileCheck %s

; Test lowering matrix to vector casts

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%ConstantBuffer = type opaque
%class.matrix.int.2.2 = type { [2 x <2 x i32>] }
@"$Globals" = external constant %ConstantBuffer

declare %class.matrix.int.2.2 @"dx.hl.init.matrix"(i32, i32, i32, i32, i32)
declare <4 x i32> @"dx.hl.cast"(i32, %class.matrix.int.2.2)

; Function Attrs: nounwind
; CHECK: define void @main()
define void @main() {
entry:
  ; CHECK: %[[vec0:.*]] = insertelement <4 x i32> undef, i32 11, i32 0
  ; CHECK: %[[vec1:.*]] = insertelement <4 x i32> %[[vec0]], i32 12, i32 1
  ; CHECK: %[[vec2:.*]] = insertelement <4 x i32> %[[vec1]], i32 21, i32 2
  ; CHECK: %[[vec3:.*]] = insertelement <4 x i32> %[[vec2]], i32 22, i32 3
  %mat = call %class.matrix.int.2.2 @"dx.hl.init.matrix"(i32 0, i32 11, i32 12, i32 21, i32 22)
  %calMatToVec = call <4 x i32> @"dx.hl.cast"(i32 4, %class.matrix.int.2.2 %mat) ; colMatToVec
  %rowMatToVec = call <4 x i32> @"dx.hl.cast"(i32 5, %class.matrix.int.2.2 %mat) ; rowMatToVec
  ; CHECK: = add <4 x i32> %[[vec3]], %[[vec3]]
  %sum = add <4 x i32> %calMatToVec, %rowMatToVec
  ; CHECK: ret void
  ret void
}

!dx.version = !{!2}
!dx.shaderModel = !{!4}
!dx.entryPoints = !{!9}
!dx.fnprops = !{!13}
!dx.options = !{!14, !15}
!dx.resource.type.annotation = !{!8}

!2 = !{i32 1, i32 0}
!4 = !{!"vs", i32 6, i32 0}
!8 = !{}
!9 = !{void ()* @main, !"main", null, !10, null}
!10 = !{null, null, !11, null}
!11 = !{!12}
!12 = !{i32 0, %ConstantBuffer* @"$Globals", !"$Globals", i32 0, i32 -1, i32 1, i32 0, null}
!13 = !{void ()* @main, i32 1}
!14 = !{i32 144}
!15 = !{i32 -1}