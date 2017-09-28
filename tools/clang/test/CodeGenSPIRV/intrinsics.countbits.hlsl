// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'countbits' function can only operate on scalar or vector of uints.

void main() {
  uint a;
  uint4 b;
  
// CHECK:      [[a:%\d+]] = OpLoad %uint %a
// CHECK-NEXT:   {{%\d+}} = OpBitCount %uint [[a]]
  uint  cb  = countbits(a);

// CHECK:      [[b:%\d+]] = OpLoad %v4uint %b
// CHECK-NEXT:   {{%\d+}} = OpBitCount %v4uint [[b]]
  uint4 cb4 = countbits(b);
}
