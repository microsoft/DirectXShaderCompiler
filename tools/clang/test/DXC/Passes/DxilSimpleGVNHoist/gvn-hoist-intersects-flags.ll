; RUN: %dxopt %s -dxil-gvn-hoist -S | FileCheck %s

; DxilSimpleGVNHoist gives the same value number to instructions that differ
; only in nsw, nuw, exact, inbounds or fast-math flags. It hoists the
; instruction from the first successor and replaces the matching instruction in
; the second successor with it. The hoisted instruction must keep only the
; flags that all of the merged instructions have. Otherwise, a flag that holds
; only in the first successor would also apply in the second, and later passes
; could fold values on that path to wrong results.
;
; In the first six functions, only the instruction in %then has the flag. Each
; comment says what InstCombine folds the function to if the flag is kept, and
; gives inputs that take %else and make the original function return something
; else.

target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

; If nsw is kept, InstCombine folds the function to 'ret i1 true'. With
; %c = false and %x = 2147483647, it returns false.
;
; CHECK-LABEL: define i1 @nsw(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[ADD:%.*]] = add i32 %x, 1
; CHECK-NEXT:    [[CMP:%.*]] = icmp sgt i32 [[ADD]], %x
; CHECK-NEXT:    br i1 %c, label %then, label %else
; CHECK:         phi i1 [ [[CMP]], %then ], [ [[CMP]], %else ]
define i1 @nsw(i1 %c, i32 %x) {
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

; If nuw is kept, InstCombine folds this carry check to 'ret i1 false'. With
; %c = false, %x = -1 and %y = 1, it returns true.
;
; CHECK-LABEL: define i1 @nuw(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[ADD:%.*]] = add i32 %x, %y
; CHECK-NEXT:    [[CARRY:%.*]] = icmp ult i32 [[ADD]], %x
; CHECK-NEXT:    br i1 %c, label %then, label %else
; CHECK:         phi i1 [ [[CARRY]], %then ], [ [[CARRY]], %else ]
define i1 @nuw(i1 %c, i32 %x, i32 %y) {
entry:
  br i1 %c, label %then, label %else

then:
  %add.then = add nuw i32 %x, %y
  %carry.then = icmp ult i32 %add.then, %x
  br label %exit

else:
  %add.else = add i32 %x, %y
  %carry.else = icmp ult i32 %add.else, %x
  br label %exit

exit:
  %r = phi i1 [ %carry.then, %then ], [ %carry.else, %else ]
  ret i1 %r
}

; If exact is kept, InstCombine folds the function to 'ret i32 %x'. With
; %c = false and %x = 3, it returns 2.
;
; CHECK-LABEL: define i32 @exact(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[SHR:%.*]] = lshr i32 %x, 1
; CHECK-NEXT:    [[SHL:%.*]] = shl i32 [[SHR]], 1
; CHECK-NEXT:    br i1 %c, label %then, label %else
; CHECK:         phi i32 [ [[SHL]], %then ], [ [[SHL]], %else ]
define i32 @exact(i1 %c, i32 %x) {
entry:
  br i1 %c, label %then, label %else

then:
  %shr.then = lshr exact i32 %x, 1
  %shl.then = shl i32 %shr.then, 1
  br label %exit

else:
  %shr.else = lshr i32 %x, 1
  %shl.else = shl i32 %shr.else, 1
  br label %exit

exit:
  %r = phi i32 [ %shl.then, %then ], [ %shl.else, %else ]
  ret i32 %r
}

; If inbounds is kept, InstCombine folds the function to 'ret i1 false'. With
; %c = false and %p = inttoptr (i32 -4 to i32*), it returns true.
;
; CHECK-LABEL: define i1 @inbounds(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr i32, i32* %p, i32 1
; CHECK-NEXT:    [[CMP:%.*]] = icmp ult i32* [[GEP]], %p
; CHECK-NEXT:    br i1 %c, label %then, label %else
; CHECK:         phi i1 [ [[CMP]], %then ], [ [[CMP]], %else ]
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
; so %sub.else is what 'precise' produces. If fast is kept, InstCombine folds
; the function to 'ret float 0.0'. With %c = false and %x = +inf, it returns
; NaN.
;
; CHECK-LABEL: define float @fast(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[SUB:%.*]] = fsub float %x, %x
; CHECK-NEXT:    br i1 %c, label %then, label %else
; CHECK:         phi float [ [[SUB]], %then ], [ [[SUB]], %else ]
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

; fcmp can also have fast-math flags, even though it is not a BinaryOperator.
; If fast is kept, InstCombine folds this NaN check to 'ret i1 false'. With
; %c = false and %x = NaN, it returns true.
;
; CHECK-LABEL: define i1 @fcmp_fast(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[ISNAN:%.*]] = fcmp uno float %x, 0.000000e+00
; CHECK-NEXT:    br i1 %c, label %then, label %else
; CHECK:         phi i1 [ [[ISNAN]], %then ], [ [[ISNAN]], %else ]
define i1 @fcmp_fast(i1 %c, float %x) {
entry:
  br i1 %c, label %then, label %else

then:
  %isnan.then = fcmp fast uno float %x, 0.000000e+00
  br label %exit

else:
  %isnan.else = fcmp uno float %x, 0.000000e+00
  br label %exit

exit:
  %r = phi i1 [ %isnan.then, %then ], [ %isnan.else, %else ]
  ret i1 %r
}

; Fast-math flags are intersected individually. Only nnan is on both
; instructions, so ninf, which only %mul.then has, must be dropped.
;
; CHECK-LABEL: define float @fast_math_intersect(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[MUL:%.*]] = fmul nnan float %x, %y
; CHECK-NEXT:    br i1 %c, label %then, label %else
; CHECK:         phi float [ [[MUL]], %then ], [ [[MUL]], %else ]
define float @fast_math_intersect(i1 %c, float %x, float %y) {
entry:
  br i1 %c, label %then, label %else

then:
  %mul.then = fmul nnan ninf float %x, %y
  br label %exit

else:
  %mul.else = fmul nnan nsz float %x, %y
  br label %exit

exit:
  %r = phi float [ %mul.then, %then ], [ %mul.else, %else ]
  ret float %r
}

; The flag is only on the instruction in %else. The hoisted instruction must
; not pick it up, because it does not hold in %then.
;
; CHECK-LABEL: define i32 @flag_only_in_else(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[ADD:%.*]] = add i32 %x, 1
; CHECK-NEXT:    br i1 %c, label %then, label %else
; CHECK:         phi i32 [ [[ADD]], %then ], [ [[ADD]], %else ]
define i32 @flag_only_in_else(i1 %c, i32 %x) {
entry:
  br i1 %c, label %then, label %else

then:
  %add.then = add i32 %x, 1
  br label %exit

else:
  %add.else = add nsw i32 %x, 1
  br label %exit

exit:
  %r = phi i32 [ %add.then, %then ], [ %add.else, %else ]
  ret i32 %r
}

; Flags that both instructions have are kept.
;
; CHECK-LABEL: define i32 @same_flags(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[ADD:%.*]] = add nuw nsw i32 %x, 1
; CHECK-NEXT:    br i1 %c, label %then, label %else
; CHECK:         phi i32 [ [[ADD]], %then ], [ [[ADD]], %else ]
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
