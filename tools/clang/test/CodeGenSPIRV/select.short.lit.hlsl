// RUN: %dxc -T ps_6_2 -HV 2018 -E main -enable-16bit-types

// Check that the literals get a 16-bit type, and the result of the select is
// then cast to an unsigned 16-bit value.
void foo(uint x) {
  // CHECK: %foo = OpFunction
  // CHECK: [[cond:%\d+]] = OpULessThan %bool {{%\d+}} %uint_64
  // CHECK: [[result:%\d+]] = OpSelect %short [[cond]] %short_1 %short_0
  // CHECK: {{%\d+}} = OpBitcast %ushort [[result]]
  uint16_t value = x < 64 ? 1 : 0;
}

// Check that the literals get a 16-bit type, and the result of the select is
// then cast to an signed 16-bit value. Note that the bitcast is redundant in
// this case, but we add the bitcast before the type of the literal has been
// determined, so we add it anyway.
void bar(uint x) {
  // CHECK: %bar = OpFunction
  // CHECK: [[cond:%\d+]] = OpULessThan %bool {{%\d+}} %uint_64
  // CHECK: [[result:%\d+]] = OpSelect %short [[cond]] %short_1 %short_0
  // CHECK: {{%\d+}} = OpBitcast %short [[result]]
  int16_t value = x < 64 ? 1 : 0;
}

void main() {
  uint value;
  foo(2);
  bar(2);
}
