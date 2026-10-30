; RUN: %dxopt %s -dxil-gvn-eliminate-region -S | FileCheck %s

; DxilSimpleGVNEliminateRegion removes a branch when value numbering shows that
; both successors compute the same values for the PHIs where they join. It
; replaces the branch with a branch to the first successor, so the instructions
; there then also run on the path through the second. The value numbering must
; therefore compare nsw, nuw, exact, inbounds and fast-math flags. Otherwise, a
; flag that holds only in the first successor would also apply on the other
; path, and later passes could fold values on that path to wrong results.
;
; In the first four functions, only the instructions in %then have the flag, so
; the branch must stay. Each comment says what InstCombine folds the function
; to if the branch is removed, and gives inputs that take %else and make the
; original function return something else.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; If the branch is removed, InstCombine folds the function to 'ret i1 true'.
; With %c = false and %x = 2147483647, it returns false.
;
; CHECK-LABEL: define i1 @nsw(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br i1 %c, label %then, label %else
define i1 @nsw(i1 %c, i32 %x) {
entry:
  br i1 %c, label %then, label %else

then:
  %add.then = add nsw i32 %x, 1
  br label %exit

else:
  %add.else = add i32 %x, 1
  br label %exit

exit:
  %add = phi i32 [ %add.then, %then ], [ %add.else, %else ]
  %cmp = icmp sgt i32 %add, %x
  ret i1 %cmp
}

; The values that reach the PHI have the same flags, but the adds that they are
; computed from do not. If the branch is removed, InstCombine folds the
; function to 'ret i1 true'. With %c = false and %x = 2147483647, it returns
; false.
;
; CHECK-LABEL: define i1 @nested(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br i1 %c, label %then, label %else
define i1 @nested(i1 %c, i32 %x) {
entry:
  br i1 %c, label %then, label %else

then:
  %add.then = add nsw i32 %x, 1
  %cmp.then = icmp sgt i32 %add.then, %x
  br label %exit

else:
  %add.else = add i32 %x, 1
  %cmp.else = icmp sgt i32 %add.else, %x
  br label %exit

exit:
  %r = phi i1 [ %cmp.then, %then ], [ %cmp.else, %else ]
  ret i1 %r
}

; Flags on instructions that are not binary operators are compared too. If the
; branch is removed, InstCombine folds the function to 'ret i1 false'. With
; %c = false and %p = inttoptr (i32 -4 to i32*), it returns true.
;
; CHECK-LABEL: define i1 @inbounds(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br i1 %c, label %then, label %else
define i1 @inbounds(i1 %c, i32* %p) {
entry:
  br i1 %c, label %then, label %else

then:
  %gep.then = getelementptr inbounds i32, i32* %p, i32 1
  %cmp.then = icmp ult i32* %gep.then, %p
  br label %exit

else:
  %gep.else = getelementptr i32, i32* %p, i32 1
  %cmp.else = icmp ult i32* %gep.else, %p
  br label %exit

exit:
  %r = phi i1 [ %cmp.then, %then ], [ %cmp.else, %else ]
  ret i1 %r
}

; DXC marks precise floating-point operations by leaving off fast-math flags,
; so %sub.else is what 'precise' produces. If the branch is removed,
; InstCombine folds the function to 'ret float 0.0'. With %c = false and
; %x = +inf, it returns NaN.
;
; CHECK-LABEL: define float @fast(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br i1 %c, label %then, label %else
define float @fast(i1 %c, float %x) {
entry:
  br i1 %c, label %then, label %else

then:
  %sub.then = fsub fast float %x, %x
  br label %exit

else:
  %sub.else = fsub float %x, %x
  br label %exit

exit:
  %r = phi float [ %sub.then, %then ], [ %sub.else, %else ]
  ret float %r
}

; Both successors compute the same value with the same flags, so the branch is
; still removed.
;
; CHECK-LABEL: define i32 @same_flags(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    br label %then
define i32 @same_flags(i1 %c, i32 %x) {
entry:
  br i1 %c, label %then, label %else

then:
  %add.then = add nuw nsw i32 %x, 1
  br label %exit

else:
  %add.else = add nuw nsw i32 %x, 1
  br label %exit

exit:
  %r = phi i32 [ %add.then, %then ], [ %add.else, %else ]
  ret i32 %r
}
