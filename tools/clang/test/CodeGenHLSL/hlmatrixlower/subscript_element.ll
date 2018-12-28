; RUN: %opt %s -hlmatrixlower -S | FileCheck %s

; Test lowering of matrix element subscripts (m3x3._12_21)

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%ConstantBuffer = type opaque
%class.matrix.int.3.3 = type { [3 x <3 x i32>] }
@"$Globals" = external constant %ConstantBuffer

declare <2 x i32>* @"dx.hl.subscript.elem"(i32, %class.matrix.int.3.3*, <2 x i32>)

; Function Attrs: nounwind
; CHECK: define void @main()
define void @main() {
entry:
  ; CHECK: %[[alloca:.*]] = alloca <9 x i32>
  %alloca = alloca %class.matrix.int.3.3, align 4

  ; CHECK: %[[load1:.*]] = load <9 x i32>, <9 x i32>* %[[alloca]]
  ; CHECK: %[[elems13:.*]] = shufflevector <9 x i32> %[[load1]], <9 x i32> %[[load1]], <2 x i32> <i32 1, i32 3>
  %rowElems13Ptr = call <2 x i32>* @"dx.hl.subscript.elem"(i32 4, %class.matrix.int.3.3* %alloca, <2 x i32> <i32 1, i32 3>) ; RowMatElement
  %elems = load <2 x i32>, <2 x i32>* %rowElems13Ptr

  ; CHECK: %[[load2:.*]] = load <9 x i32>, <9 x i32>* %[[alloca]]
  ; CHECK: %[[elem1:.*]] = extractelement <2 x i32> %[[elems13]], i64 0
  ; CHECK: %[[vecWithElem1:.*]] = insertelement <9 x i32> %[[load2]], i32 %[[elem1]], i64 1
  ; CHECK: %[[elem3:.*]] = extractelement <2 x i32> %[[elems13]], i64 1
  ; CHECK: %[[vecWithElems13:.*]] = insertelement <9 x i32> %[[vecWithElem1]], i32 %[[elem3]], i64 3
  ; CHECK: store <9 x i32> %[[vecWithElems13]], <9 x i32>* %[[alloca]]
  %colElems13Ptr = call <2 x i32>* @"dx.hl.subscript.elem"(i32 3, %class.matrix.int.3.3* %alloca, <2 x i32> <i32 1, i32 3>) ; ColMatElement
  store <2 x i32> %elems, <2 x i32>* %colElems13Ptr

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