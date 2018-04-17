// Run: %dxc -T cs_6_1 -E main -Zi

// CHECK:      [[str:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opsource.hlsl
// CHECK:      OpSource HLSL 610 [[str]]

[numthreads(8, 1, 1)]
void main() {
}
