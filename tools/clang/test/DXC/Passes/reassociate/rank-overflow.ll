; RUN: opt < %s -always-inline -reassociate -S | FileCheck %s

; Check that Reassociate skips functions that exceed the 16-bit per-block
; instruction rank limit or the 16-bit basic-block rank limit, preserving
; l1 - l2 across an intervening store, while still optimizing subsequent
; functions in the module.

declare void @barrier()

define internal void @b0() alwaysinline { call void @barrier() ret void }
define internal void @b1() alwaysinline { call void @b0() call void @b0() ret void }
define internal void @b2() alwaysinline { call void @b1() call void @b1() ret void }
define internal void @b3() alwaysinline { call void @b2() call void @b2() ret void }
define internal void @b4() alwaysinline { call void @b3() call void @b3() ret void }
define internal void @b5() alwaysinline { call void @b4() call void @b4() ret void }
define internal void @b6() alwaysinline { call void @b5() call void @b5() ret void }
define internal void @b7() alwaysinline { call void @b6() call void @b6() ret void }
define internal void @b8() alwaysinline { call void @b7() call void @b7() ret void }
define internal void @b9() alwaysinline { call void @b8() call void @b8() ret void }
define internal void @b10() alwaysinline { call void @b9() call void @b9() ret void }
define internal void @b11() alwaysinline { call void @b10() call void @b10() ret void }
define internal void @b12() alwaysinline { call void @b11() call void @b11() ret void }
define internal void @b13() alwaysinline { call void @b12() call void @b12() ret void }
define internal void @b14() alwaysinline { call void @b13() call void @b13() ret void }
define internal void @b15() alwaysinline { call void @b14() call void @b14() ret void }
define internal void @b16() alwaysinline { call void @b15() call void @b15() ret void }

define internal void @c0(i1 %c) alwaysinline {
  br i1 %c, label %t, label %m
t:
  call void @barrier()
  br label %m
m:
  ret void
}
define internal void @c1(i1 %c) alwaysinline { call void @c0(i1 %c) call void @c0(i1 %c) ret void }
define internal void @c2(i1 %c) alwaysinline { call void @c1(i1 %c) call void @c1(i1 %c) ret void }
define internal void @c3(i1 %c) alwaysinline { call void @c2(i1 %c) call void @c2(i1 %c) ret void }
define internal void @c4(i1 %c) alwaysinline { call void @c3(i1 %c) call void @c3(i1 %c) ret void }
define internal void @c5(i1 %c) alwaysinline { call void @c4(i1 %c) call void @c4(i1 %c) ret void }
define internal void @c6(i1 %c) alwaysinline { call void @c5(i1 %c) call void @c5(i1 %c) ret void }
define internal void @c7(i1 %c) alwaysinline { call void @c6(i1 %c) call void @c6(i1 %c) ret void }
define internal void @c8(i1 %c) alwaysinline { call void @c7(i1 %c) call void @c7(i1 %c) ret void }
define internal void @c9(i1 %c) alwaysinline { call void @c8(i1 %c) call void @c8(i1 %c) ret void }
define internal void @c10(i1 %c) alwaysinline { call void @c9(i1 %c) call void @c9(i1 %c) ret void }
define internal void @c11(i1 %c) alwaysinline { call void @c10(i1 %c) call void @c10(i1 %c) ret void }
define internal void @c12(i1 %c) alwaysinline { call void @c11(i1 %c) call void @c11(i1 %c) ret void }
define internal void @c13(i1 %c) alwaysinline { call void @c12(i1 %c) call void @c12(i1 %c) ret void }
define internal void @c14(i1 %c) alwaysinline { call void @c13(i1 %c) call void @c13(i1 %c) ret void }
define internal void @c15(i1 %c) alwaysinline { call void @c14(i1 %c) call void @c14(i1 %c) ret void }

; CHECK-LABEL: define i32 @test_inst_overflow(
; CHECK:         %l1 = load i32, i32* %p
; CHECK-NEXT:    br i1 %c, label %then, label %exit
; CHECK:       then:
; CHECK-NEXT:    store i32 %v, i32* %q
; CHECK-NEXT:    %l2 = load i32, i32* %p
; CHECK-NEXT:    %add = add i32 %base, %l1
; CHECK-NEXT:    %sub = sub i32 %add, %l2
; CHECK-NEXT:    br label %exit
; CHECK:       exit:
; CHECK-NEXT:    %res = phi i32 [ %l1, %entry ], [ %sub, %then ]
; CHECK-NEXT:    ret i32 %res
define i32 @test_inst_overflow(i32* %p, i32* %q, i32 %v, i32 %base, i1 %c) {
entry:
  call void @b16()
  %l1 = load i32, i32* %p
  br i1 %c, label %then, label %exit

then:
  store i32 %v, i32* %q
  %l2 = load i32, i32* %p
  %add = add i32 %base, %l1
  %sub = sub i32 %add, %l2
  br label %exit

exit:
  %res = phi i32 [ %l1, %entry ], [ %sub, %then ]
  ret i32 %res
}

; CHECK-LABEL: define i32 @test_block_overflow(
; CHECK:         %l1 = load i32, i32* %p
; CHECK:         store i32 %v, i32* %q
; CHECK-NEXT:    %l2 = load i32, i32* %p
; CHECK-NEXT:    %add = add i32 %base, %l1
; CHECK-NEXT:    %sub = sub i32 %add, %l2
; CHECK-NEXT:    ret i32 %sub
define i32 @test_block_overflow(i32* %p, i32* %q, i32 %v, i32 %base, i1 %c) {
entry:
  %l1 = load i32, i32* %p
  call void @c15(i1 %c)
  store i32 %v, i32* %q
  %l2 = load i32, i32* %p
  %add = add i32 %base, %l1
  %sub = sub i32 %add, %l2
  ret i32 %sub
}

; CHECK-LABEL: define i32 @small(
; CHECK-NEXT:    ret i32 %b
define i32 @small(i32 %a, i32 %b) {
  %sum = add i32 %b, %a
  %diff = sub i32 %sum, %a
  ret i32 %diff
}
