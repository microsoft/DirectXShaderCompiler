; RUN: %opt-exe %s -scopenested -scopenestinfo -analyze -S | FileCheck %s

; verify that scope nest info looks correct for fallthrough cases
; we expect to see switch fallthrough treated as "Body", so
; it should not effect the scope indentation level.


; CHECK: @TopLevel_Begin
; CHECK:     entry
; CHECK:     @Switch_Begin
; CHECK:     @Switch_Case
; CHECK:         exit
; CHECK:         @Switch_Break
; CHECK:     @Switch_Case
; CHECK:         case0
; CHECK:         case1
; CHECK:         exit
; CHECK:         @Switch_Break
; CHECK:     @Switch_Case
; CHECK:         case1
; CHECK:         exit
; CHECK:         @Switch_Break
; CHECK:     @Switch_End
; CHECK: @TopLevel_End

declare float @dx.op.waveActiveOp.f32(i32, float, i8, i8)


define void @CSMain(i32 %tid, float %v) convergent {
entry:
  switch i32 %tid, label %exit [
    i32 0, label %case0
    i32 1, label %case1
  ]

case0:                                          ; switch case 0

  %w0 = call float @dx.op.waveActiveOp.f32(i32 119, float %v, i8 0, i8 0)
  br label %case1                               ; FALLTHROUGH


case1:                                          ; switch case 1
  %a = phi float [ %w0, %case0 ],
                   [ 0.0, %entry ]

  %w1 = call float @dx.op.waveActiveOp.f32(i32 119, float %v, i8 0, i8 0)
  %sum = fadd float %a, %w1
  br label %exit

exit:
  %r = phi float [ %sum, %case1 ],
                   [ 0.0, %entry ]
  ret void
}
