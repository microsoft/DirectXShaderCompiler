// Run: %dxc -T ps_6_0 -E main

ByteAddressBuffer   b1;
RWByteAddressBuffer b2;

void main() {
  uint dim;

// CHECK:           [[dim1:%\d+]] = OpArrayLength %uint %b1 0
// CHECK-NEXT: [[numBytes1:%\d+]] = OpIMul %uint [[dim1]] %uint_4
// CHECK-NEXT:                      OpStore %dim [[numBytes1]]
  b1.GetDimensions(dim);

// CHECK:           [[dim2:%\d+]] = OpArrayLength %uint %b2 0
// CHECK-NEXT: [[numBytes2:%\d+]] = OpIMul %uint [[dim2]] %uint_4
// CHECK-NEXT:                      OpStore %dim [[numBytes2]]
  b2.GetDimensions(dim);
}
