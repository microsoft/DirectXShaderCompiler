// Run: %dxc -T ps_6_0 -E main -Zi

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.hlsl

Texture2D    MyTexture;
SamplerState MySampler;

uint foo(uint val) {
  return val;
}

// Note that we do two passes for debuging info: first preprocessing and then
// compiling the preprocessed source code.
// Because the preprocessor prepends a "#line 1 ..." line to the whole file,
// the compliation sees line numbers incremented by 1.

float4 main(uint val : A) : SV_Target {
  // CHECK:      OpLine [[file]] 23 12
  // CHECK-NEXT: OpLoad %uint %val
  // CHECK-NEXT: OpBitReverse
  uint a = reversebits(val);

  // CHECK:      OpLine [[file]] 27 12
  // CHECK-NEXT: OpLoad %uint %a
  uint b = foo(a);

  // CHECK:      OpLine [[file]] 31 14
  // CHECK-NEXT: OpLoad %type_2d_image %MyTexture
  float4 c = MyTexture.Sample(MySampler, float2(0.1, 0.2));

  return b * c;
}
