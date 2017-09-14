// Run: %dxc -T ps_6_0 -E main

void main() {
  float2x3 m = { {1,2,3} , {4,5,6} };

// CHECK:      [[m:%\d+]] = OpLoad %mat2v3float %m
// CHECK-NEXT:   {{%\d+}} = OpTranspose %mat3v2float [[m]]
  float3x2 n = transpose(m);
}
