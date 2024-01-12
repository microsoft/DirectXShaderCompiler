// RUN: %dxc -T ps_6_0 -E main

RWByteAddressBuffer foo;
groupshared float bar;

void main() {
// CHECK:      [[foo:%\d+]] = OpAccessChain %_ptr_Uniform_uint %foo %uint_0 {{%\d+}}
// CHECK-NEXT: [[foo:%\d+]] = OpAtomicIAdd %uint [[foo]] %uint_1 %uint_0 %uint_42
// CHECK-NEXT: [[foo:%\d+]] = OpConvertUToF %float [[foo]]
// CHECK-NEXT:                OpStore %bar [[foo]]
  foo.InterlockedAdd(16, 42, bar);
}
