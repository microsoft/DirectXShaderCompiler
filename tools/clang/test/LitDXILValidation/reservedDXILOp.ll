; REQUIRES: dxil-1-10
; RUN: not %dxv %s 2>&1 | FileCheck %s

; Make sure that using a reserved DXIL opcode produces an appropriate error.

; 302 is currently reserved. If it is ever assigned to a valid DXIL op,
; this test should be updated to use a different reserved opcode.
; See "Reserved values:" comment in main DXIL::OpCode definition in
; DxilConstants.h for reserved opcodes.

; CHECK: error: DXILOpCode must be [0..{{[0-9]+}}] or a supported experimental opcode. 302 (reserved opcode) specified.
; CHECK: note: at 'call void @dx.op.reserved(i32 302)' in block '#0' of function 'main'.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define void @main() {
  call void @dx.op.reserved(i32 302)
  ret void
}

; Function Attrs: nounwind
declare void @dx.op.reserved(i32) #0

attributes #0 = { nounwind}

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!1}
!dx.shaderModel = !{!2}
!dx.resources = !{!3}
!dx.entryPoints = !{!4}

!0 = !{!"hand-crafted"}
!1 = !{i32 1, i32 10}
!2 = !{!"vs", i32 6, i32 10}
!3 = !{null, null, null, null}
!4 = !{void ()* @main, !"main", !5, !3, null}
!5 = !{null, null, null}
