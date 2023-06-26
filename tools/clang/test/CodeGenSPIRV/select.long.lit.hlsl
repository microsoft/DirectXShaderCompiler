// RUN: %dxc -T ps_6_0 -HV 2018 -E main

// the literal type.
void foo(uint x) {
  // CHECK: %foo = OpFunction
  // CHECK: [[cond:%\d+]] = OpULessThan %bool {{%\d+}} %uint_64
  // CHECK: [[result:%\d+]] = OpSelect %long [[cond]] %long_1 %long_0
  // CHECK: {{%\d+}} = OpBitcast %ulong [[result]]
  uint64_t value = x < 64 ? 1 : 0;
}

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
