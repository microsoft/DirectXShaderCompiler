; RUN: %opt-exe %s -scopenested -S | FileCheck %s

; verify that this pass won't identify fallthrough blocks
; as merge points and in turn won't duplicate blocks
; which contain convergent operations

; previously, blocks with wave operations would be cloned,
; violating the principle that wave operations should
; only be called by different threads when control flow
; is distinct between those threads

declare float @dx.op.waveActiveOp.f32(i32, float, i8, i8)

; CHECK-LABEL: define void @CSMain

define void @CSMain(i32 %tid, float %v) convergent {
entry:
  switch i32 %tid, label %exit [
    i32 0, label %case0
    i32 1, label %case1
  ]

; CHECK: case0:
case0:                                          ; switch case 0

; CHECK: call float @dx.op.waveActiveOp.f32(i32 119, float %v, i8 0, i8 0)
; CHECK: br label %case1, !dx.BranchKind ![[BK:.*]]
  %w0 = call float @dx.op.waveActiveOp.f32(i32 119, float %v, i8 0, i8 0)
  br label %case1                               ; FALLTHROUGH


; CHECK: case1:
case1:                                          ; switch case 1
  %a = phi float [ %w0, %case0 ],
                   [ 0.0, %entry ]

; CHECK: call float @dx.op.waveActiveOp.f32(i32 119, float %v, i8 0, i8 0)
; CHECK: br label %exit, !dx.BranchKind ![[BK]]
  %w1 = call float @dx.op.waveActiveOp.f32(i32 119, float %v, i8 0, i8 0)
  %sum = fadd float %a, %w1
  br label %exit

; no cloning, so there should be no more waveops after this point
; CHECK-NOT: call float @dx.waveActiveOp.f32(i32 119

exit:
  %r = phi float [ %sum, %case1 ],
                   [ 0.0, %entry ]
  ret void
}

; The BranchKind::SwitchFallthrough Kind ID is 8
; CHECK: ![[BK]] = !{i32 8}
