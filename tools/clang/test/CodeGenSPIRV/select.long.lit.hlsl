// RUN: %dxc -T ps_6_0 -HV 2018 -E main

// Check that the literals get a 64-bit type, and the result of the select is
// then cast to an unsigned 64-bit value.
void foo(uint x) {
  // CHECK: %foo = OpFunction
  // CHECK: [[cond:%\d+]] = OpULessThan %bool {{%\d+}} %uint_64
  // CHECK: [[result:%\d+]] = OpSelect %long [[cond]] %long_1 %long_0
  // CHECK: {{%\d+}} = OpBitcast %ulong [[result]]
  uint64_t value = x < 64 ? 1 : 0;
}

// Check that the literals get a 64-bit type, and the result of the select is
// then cast to an signed 64-bit value. Note that the bitcast is redundant in
// this case, but we add the bitcast before the type of the literal has been
// determined, so we add it anyway.
void bar(uint x) {
  // CHECK: %bar = OpFunction
  // CHECK: [[cond:%\d+]] = OpULessThan %bool {{%\d+}} %uint_64
  // CHECK: [[result:%\d+]] = OpSelect %long [[cond]] %long_1 %long_0
  // CHECK: {{%\d+}} = OpBitcast %long [[result]]
  int64_t value = x < 64 ? 1 : 0;
}

void main() {
  uint value;
  foo(2);
  bar(2);
}
