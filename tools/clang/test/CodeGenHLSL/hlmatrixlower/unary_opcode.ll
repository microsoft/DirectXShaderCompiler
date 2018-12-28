; RUN: %opt %s -hlmatrixlower -S | FileCheck %s

; Test lowering of matrix unary operators

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%ConstantBuffer = type opaque
%class.matrix.int.2.2 = type { [2 x <2 x i32>] }
@"$Globals" = external constant %ConstantBuffer

declare %class.matrix.int.2.2 @"dx.hl.matldst.load"(i32, %class.matrix.int.2.2*)
declare %class.matrix.int.2.2 @"dx.hl.matldst.store"(i32, %class.matrix.int.2.2*, %class.matrix.int.2.2)
declare %class.matrix.int.2.2 @"dx.hl.unop.-"(i32, %class.matrix.int.2.2)

; Function Attrs: nounwind
; CHECK: define void @main()
define void @main() {
entry:
  ; CHECK: %[[alloca:.*]] = alloca <4 x i32>
  %alloca = alloca %class.matrix.int.2.2, align 4
  ; CHECK: %[[load:.*]] = load <4 x i32>, <4 x i32>* %[[alloca]]
  %load = call %class.matrix.int.2.2 @"dx.hl.matldst.load"(i32 2, %class.matrix.int.2.2* %alloca)
  ; CHECK: %[[sub:.*]] = sub <4 x i32> zeroinitializer, %[[load]]
  %neg = call %class.matrix.int.2.2 @"dx.hl.unop.-"(i32 6, %class.matrix.int.2.2 %load)
  ; CHECK: store <4 x i32> %[[sub]], <4 x i32>* %[[alloca]]
  call %class.matrix.int.2.2 @"dx.hl.matldst.store"(i32 3, %class.matrix.int.2.2* %alloca, %class.matrix.int.2.2 %neg)
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