// RUN: %dxc -T ps_6_0 -HV 2021 -E main

// The 'out' argument in the function should be handled correctly when deducing
// the literal type, even in HLSL 2021 (with shortcicuiting).
void foo(out uint value, uint x) {
  // CHECK:   [[cond:%\d+]] = OpULessThan %bool {{%\d+}} %uint_64
  // CHECK:                   OpBranchConditional [[cond]] [[ternary_lhs:%\w+]] [[ternary_rhs:%\w+]]
  // CHECK: [[ternary_lhs]] = OpLabel
  // CHECK:                   OpStore [[tmp:%\w+]] %int_1
  // CHECK:                   OpBranch [[merge:%\w+]]
  // CHECK: [[ternary_rhs]] = OpLabel
  // CHECK:                   OpStore [[tmp:%\w+]] %int_0
  // CHECK:                   OpBranch [[merge]]
  // CHECK:       [[merge]] = OpLabel
  // CHECK:    [[res:%\d+]] = OpLoad %int [[tmp]]
  // CHECK:        {{%\d+}} = OpBitcast %uint [[res]]
  value = x < 64 ? 1 : 0;
}

void main() {
  uint value;
  foo(value, 2);
}
