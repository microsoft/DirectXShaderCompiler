// RUN: %dxc -T ps_6_0 -HV 2018 -E main

// The 'out' argument in the function should be handled correctly when deducing
// the literal type.
void foo(out uint value, uint x) {
  // CHECK: [[cond:%\d+]] = OpULessThan %bool {{%\d+}} %uint_64
  // CHECK: [[result:%\d+]] = OpSelect %int [[cond]] %int_1 %int_0
  // CHECK: {{%\d+}} = OpBitcast %uint [[result]]
  value = x < 64 ? 1 : 0;
}

void main() {
  uint value;
  foo(value, 2);
}
