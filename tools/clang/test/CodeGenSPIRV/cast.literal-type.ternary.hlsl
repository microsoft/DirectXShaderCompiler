// Run: %dxc -T ps_6_0 -E main

// The 'out' argument in the function should be handled correctly when deducing
// the literal type.
void foo(out uint value, uint x) {
  // CHECK: [[cond:%\d+]] = OpULessThan %bool {{%\d+}} %uint_64
  // CHECK:      {{%\d+}} = OpSelect %uint [[cond]] %uint_1 %uint_0
  value = x < 64 ? 1 : 0;
}

void main() {
  uint value;
  foo(value, 2);
}
