; RUN: opt -S -multi-dim-one-dim %s | FileCheck %s
;
; Tests for the pass that changes multi-dimension global variable accesses into
; a flattened one-dimensional access. The tests focus on the case where the geps
; need to be merged but are separated by an addrspacecast operation. This was
; causing the pass to fail because it could not merge the gep through the
; addrspace cast.

; Naming convention: gep0_addrspacecast_gep1

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

@ArrayOfArray = addrspace(3) global [256 x [9 x float]] undef, align 4

; Test that we can merge the geps when all parts are instructions.
; CHECK-LABEL: @merge_gep_instr_instr_instr
; CHECK:  load float, float* addrspacecast (float addrspace(3)* getelementptr inbounds ([2304 x float], [2304 x float] addrspace(3)* @ArrayOfArray.1dim, i32 0, i32 1) to float*)
define void @merge_gep_instr_instr_instr() {
entry:
  %gep0 = getelementptr inbounds [256 x [9 x float]], [256 x [9 x float]] addrspace(3)* @ArrayOfArray, i32 0, i32 0
  %asc  = addrspacecast [9 x float] addrspace(3)* %gep0 to [9 x float]*
  %gep1 = getelementptr inbounds [9 x float], [9 x float]* %asc, i32 0, i32 1
  %load = load float, float* %gep1
  ret void
}

; Test that we can merge the geps when the inner gep are constants.
; CHECK-LABEL: @merge_gep_instr_instr_const
; CHECK:  load float, float* addrspacecast (float addrspace(3)* getelementptr inbounds ([2304 x float], [2304 x float] addrspace(3)* @ArrayOfArray.1dim, i32 0, i32 1) to float*)
define void @merge_gep_instr_instr_const() {
entry:
  %asc  = addrspacecast [9 x float] addrspace(3)* getelementptr inbounds ([256 x [9 x float]], [256 x [9 x float]] addrspace(3)* @ArrayOfArray, i32 0, i32 0) to [9 x float]*
  %gep1 = getelementptr inbounds [9 x float], [9 x float]* %asc, i32 0, i32 1
  %load = load float, float* %gep1
  ret void
}

; Test that we can merge the geps when the addrspace and inner gep are constants.
; CHECK-LABEL: @merge_gep_instr_const_const
; CHECK:  load float, float* addrspacecast (float addrspace(3)* getelementptr inbounds ([2304 x float], [2304 x float] addrspace(3)* @ArrayOfArray.1dim, i32 0, i32 1) to float*)
define void @merge_gep_instr_const_const() {
entry:
  %gep1 = getelementptr inbounds [9 x float], [9 x float]* addrspacecast ([9 x float] addrspace(3)* getelementptr inbounds ([256 x [9 x float]], [256 x [9 x float]] addrspace(3)* @ArrayOfArray, i32 0, i32 0) to [9 x float]*), i32 0, i32 1
  %load = load float, float* %gep1
  ret void
}

; Test that we can merge the geps when all parts are constants.
; CHECK-LABEL: @merge_gep_const_const
; CHECK: load float, float* addrspacecast (float addrspace(3)* getelementptr inbounds ([2304 x float], [2304 x float] addrspace(3)* @ArrayOfArray.1dim, i32 0, i32 1) to float*)
define void @merge_gep_const_const_const() {
entry:
  %load = load float, float* getelementptr inbounds ([9 x float], [9 x float]* addrspacecast ([9 x float] addrspace(3)* getelementptr inbounds ([256 x [9 x float]], [256 x [9 x float]] addrspace(3)* @ArrayOfArray, i32 0, i32 0) to [9 x float]*), i32 0, i32 1)
  ret void
}

; Test that we compute the correct index when the outer array has
; a non-zero constant index.
; CHECK-LABEL: @merge_gep_const_outer_array_index
; CHECK:  load float, float* addrspacecast (float addrspace(3)* getelementptr inbounds ([2304 x float], [2304 x float] addrspace(3)* @ArrayOfArray.1dim, i32 0, i32 66) to float*)
define void @merge_gep_const_outer_array_index() {
entry:
  %gep0 = getelementptr inbounds [256 x [9 x float]], [256 x [9 x float]] addrspace(3)* @ArrayOfArray, i32 0, i32 7
  %asc  = addrspacecast [9 x float] addrspace(3)* %gep0 to [9 x float]*
  %gep1 = getelementptr inbounds [9 x float], [9 x float]* %asc, i32 0, i32 3
  %load = load float, float* %gep1
  ret void
}

; Test that we compute the correct index when the outer array has
; a non-constant index.
; CHECK-LABEL: @merge_gep_dynamic_outer_array_index
; CHECK:   %0 = mul i32 %idx, 9
; CHECK:   %1 = add i32 3, %0
; CHECK:   %2 = getelementptr [2304 x float], [2304 x float] addrspace(3)* @ArrayOfArray.1dim, i32 0, i32 %1
; CHECK:   %3 = addrspacecast float addrspace(3)* %2 to float*
; CHECK:   load float, float* %3
define void @merge_gep_dynamic_outer_array_index(i32 %idx) {
entry:
  %gep0 = getelementptr inbounds [256 x [9 x float]], [256 x [9 x float]] addrspace(3)* @ArrayOfArray, i32 0, i32 %idx
  %asc  = addrspacecast [9 x float] addrspace(3)* %gep0 to [9 x float]*
  %gep1 = getelementptr inbounds [9 x float], [9 x float]* %asc, i32 0, i32 3
  %load = load float, float* %gep1
  ret void
}

; Test that we compute the correct index when the both arrays have
; a non-constant index.
; CHECK-LABEL: @merge_gep_dynamic_array_index
; CHECK:  %0 = mul i32 %idx0, 9
; CHECK:  %1 = add i32 %idx1, %0
; CHECK:  %2 = getelementptr [2304 x float], [2304 x float] addrspace(3)* @ArrayOfArray.1dim, i32 0, i32 %1
; CHECK:  %3 = addrspacecast float addrspace(3)* %2 to float*
; CHECK:  load float, float* %3
define void @merge_gep_dynamic_array_index(i32 %idx0, i32 %idx1) {
entry:
  %gep0 = getelementptr inbounds [256 x [9 x float]], [256 x [9 x float]] addrspace(3)* @ArrayOfArray, i32 0, i32 %idx0
  %asc  = addrspacecast [9 x float] addrspace(3)* %gep0 to [9 x float]*
  %gep1 = getelementptr inbounds [9 x float], [9 x float]* %asc, i32 0, i32 %idx1
  %load = load float, float* %gep1
  ret void
}
