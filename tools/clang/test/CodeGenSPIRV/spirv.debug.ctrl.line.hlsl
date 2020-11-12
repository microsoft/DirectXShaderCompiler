// Run: %dxc -T ps_6_1 -E main -fspv-debug=line

// Have file path
// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.ctrl.line.hlsl
// CHECK:      OpSource HLSL 610 [[file]]
// Have source code
// CHECK:      float4 main(uint val
// Have line
// CHECK:      OpLine

float4 main(uint val : A) : SV_Target {
  uint a = reversebits(val);
  return a;
}
