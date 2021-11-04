// RUN: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'reversebits' function can only operate on scalar or vector of uints.

void main() {
  uint a;
  uint4 b;
  
// CHECK:      [[a:%\d+]] = OpLoad %uint %a
// CHECK-NEXT:   {{%\d+}} = OpBitReverse %uint [[a]]
  uint  cb  = reversebits(a);

// CHECK:      [[b:%\d+]] = OpLoad %v4uint %b
// CHECK-NEXT:   {{%\d+}} = OpBitReverse %v4uint [[b]]
  uint4 cb4 = reversebits(b);
}
