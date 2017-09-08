// Run: %dxc -T ps_6_0 -E main

ByteAddressBuffer   b1;
RWByteAddressBuffer b2;

void main() {
  uint dim;

// CHECK:        [[b1:%\d+]] = OpLoad %type_ByteAddressBuffer %b1
// CHECK-NEXT: [[dim1:%\d+]] = OpArrayLength %uint [[b1]] 0
// CHECK-NEXT:                 OpStore %dim [[dim1]]
  b1.GetDimensions(dim);

// CHECK:        [[b2:%\d+]] = OpLoad %type_RWByteAddressBuffer %b2
// CHECK-NEXT: [[dim2:%\d+]] = OpArrayLength %uint [[b2]] 0
// CHECK-NEXT:                 OpStore %dim [[dim2]]
  b2.GetDimensions(dim);
}
