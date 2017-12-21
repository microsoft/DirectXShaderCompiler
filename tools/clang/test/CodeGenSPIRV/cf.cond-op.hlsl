// Run: %dxc -T ps_6_0 -E main

// TODO: write to global variable
bool fn() { return true; }
bool fn1() { return false; }
bool fn2() { return true; }

// CHECK: [[v3i0:%\d+]] = OpConstantComposite %v3int %int_0 %int_0 %int_0

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    // Use in control flow

    bool a, b, c;
    int val = 0;
// CHECK:      [[a0:%\d+]] = OpLoad %bool %a
// CHECK-NEXT: [[b0:%\d+]] = OpLoad %bool %b
// CHECK-NEXT: [[c0:%\d+]] = OpLoad %bool %c
// CHECK-NEXT: [[s0:%\d+]] = OpSelect %bool [[a0]] [[b0]] [[c0]]
// CHECK-NEXT: OpSelectionMerge %if_merge None
// CHECK-NEXT: OpBranchConditional [[s0]] %if_true %if_merge
    if (a ? b : c) val++;

    // Operand with side effects

// CHECK-LABEL: %if_merge = OpLabel
// CHECK-NEXT: [[fn:%\d+]] = OpFunctionCall %bool %fn
// CHECK-NEXT: [[fn1:%\d+]] = OpFunctionCall %bool %fn1
// CHECK-NEXT: [[fn2:%\d+]] = OpFunctionCall %bool %fn2
// CHECK-NEXT: [[s1:%\d+]] = OpSelect %bool [[fn]] [[fn1]] [[fn2]]
// CHECK-NEXT: OpSelectionMerge %if_merge_0 None
// CHECK-NEXT: OpBranchConditional [[s1]] %if_true_0 %if_merge_0
    if (fn() ? fn1() : fn2()) val++;

    // Try condition with various type.
    // Note: the SPIR-V OpSelect selection argument must be the same size as the return type.
    int3 r, s, t;
    bool  cond;
    bool3 cond3;
    float floatCond;
    int3 int3Cond;
 
// CHECK:      [[cond3:%\d+]] = OpLoad %v3bool %cond3
// CHECK-NEXT:     [[r:%\d+]] = OpLoad %v3int %r
// CHECK-NEXT:     [[s:%\d+]] = OpLoad %v3int %s
// CHECK-NEXT:       {{%\d+}} = OpSelect %v3int [[cond3]] [[r]] [[s]]
    t = cond3 ? r : s;

// CHECK:       [[cond:%\d+]] = OpLoad %bool %cond
// CHECK-NEXT:     [[r:%\d+]] = OpLoad %v3int %r
// CHECK-NEXT:     [[s:%\d+]] = OpLoad %v3int %s
// CHECK-NEXT: [[splat:%\d+]] = OpCompositeConstruct %v3bool [[cond]] [[cond]] [[cond]]
// CHECK-NEXT:       {{%\d+}} = OpSelect %v3int [[splat]] [[r]] [[s]]
    t = cond  ? r : s;

// CHECK:      [[floatCond:%\d+]] = OpLoad %float %floatCond
// CHECK-NEXT:  [[boolCond:%\d+]] = OpFOrdNotEqual %bool [[floatCond]] %float_0
// CHECK-NEXT: [[bool3Cond:%\d+]] = OpCompositeConstruct %v3bool [[boolCond]] [[boolCond]] [[boolCond]]
// CHECK-NEXT:         [[r:%\d+]] = OpLoad %v3int %r
// CHECK-NEXT:         [[s:%\d+]] = OpLoad %v3int %s
// CHECK-NEXT:           {{%\d+}} = OpSelect %v3int [[bool3Cond]] [[r]] [[s]]
    t = floatCond ? r : s;

// CHECK:       [[int3Cond:%\d+]] = OpLoad %v3int %int3Cond
// CHECK-NEXT: [[bool3Cond:%\d+]] = OpINotEqual %v3bool [[int3Cond]] [[v3i0]]
// CHECK-NEXT:         [[r:%\d+]] = OpLoad %v3int %r
// CHECK-NEXT:         [[s:%\d+]] = OpLoad %v3int %s
// CHECK-NEXT:           {{%\d+}} = OpSelect %v3int [[bool3Cond]] [[r]] [[s]]
    t = int3Cond ? r : s;
}
