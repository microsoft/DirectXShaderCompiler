// Run: %dxc -T cs_6_1 -E main -Zi

// CHECK:      [[str:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opsource.hlsl
// CHECK:      OpSource HLSL 610 [[str]]

// Make sure we have #line directive emitted by the preprocessor
// CHECK-SAME: #line 1

// Make sure we have the original source code
// CHECK:      numthreads(8, 1, 1)
// CHECK-NEXT: void main()

[numthreads(8, 1, 1)]
void main() {
}
