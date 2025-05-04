; RUN: not opt -S -multi-dim-one-dim %s

; We do not handle addrspacecast that changes the shape of the underlying type
; when merging geps (e.g. array to non-array). Instead of trying to handle some
; cases we just avoid trying to merge geps in all cases where the underlying
; type changes shape. This causes the multi-dim-one-dim pass to crash because
; it expects the geps to be merged. I do not think we can hit this case from
; hlsl, but it may be possible.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

@ArrayOfArray = addrspace(3) global [256 x [9 x float]] undef, align 4

; This case could be successfully handled by the current code by removing
; the check for different pointer element types.
define void @addrspace_cast_new_type_same_shape(i32 %idx0, i32 %idx1) {
entry:
  %gep0 = getelementptr inbounds [256 x [9 x float]], [256 x [9 x float]] addrspace(3)* @ArrayOfArray, i32 0, i32 %idx0
  %asc  = addrspacecast [9 x float] addrspace(3)* %gep0 to [3 x i32]*
  %gep1 = getelementptr inbounds [3 x i32], [3 x i32]* %asc, i32 0, i32 %idx1
  %load = load i32, i32* %gep1
  ret void
}

; This case could not be successfully handled by the current code because
; the change in type also changes the shape.
define void @addrspace_cast_new_type_new_shape(i32 %idx0, i32 %idx1) {
entry:
  %gep0 = getelementptr inbounds [256 x [9 x float]], [256 x [9 x float]] addrspace(3)* @ArrayOfArray, i32 0, i32 %idx0
  %asc  = addrspacecast [9 x float] addrspace(3)* %gep0 to i32*
  %gep1 = getelementptr inbounds i32, i32* %asc, i32 0
  %load = load i32, i32* %gep1
  ret void
}