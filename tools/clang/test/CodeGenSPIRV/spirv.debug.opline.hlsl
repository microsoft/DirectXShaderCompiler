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
  // CHECK:      OpLine [[file]] 24 24
  // CHECK-NEXT: OpLoad %uint %val
  // CHECK-NEXT: OpLine [[file]] 24 12
  // CHECK-NEXT: OpBitReverse
  uint a = reversebits(val);

  // CHECK:      OpLine [[file]] 28 16
  // CHECK-NEXT: OpLoad %uint %a
  uint b = foo(a);

  // CHECK:      OpLine [[file]] 32 14
  // CHECK-NEXT: OpLoad %type_2d_image %MyTexture
  float4 c = MyTexture.Sample(MySampler, float2(0.1, 0.2));

  // CHECK:      OpLine [[file]] 37 7
  // CHECK-NEXT: OpLoad %uint %val
  // CHECK-NEXT: OpUGreaterThan
  if (val > 10) {
    a = 5;
  } else {
    a = 6;
  }

  for (
  // CHECK:      OpLine [[file]] 46 7
  // CHECK-NEXT: OpStore %b %uint_0
      b = 0;
  // CHECK:      OpLine [[file]] 49 7
  // CHECK-NEXT: OpBranch %for_check
      b < 10;
  // CHECK:      OpLine [[file]] 53 7
  // CHECK-NEXT: OpLoad %uint %b
  // CHECK-NEXT: OpIAdd
      ++b) {
    a += 1;
  }

  // CHECK:      OpLine [[file]] 60 10
  // CHECK-NEXT: OpLoad %uint %b
  // CHECK-NEXT: OpISub
  while (--b > 0);

  do {
    c++;
  // CHECK:      OpLine [[file]] 66 12
  // CHECK-NEXT: OpAccessChain %_ptr_Function_float %c %int_0
  } while (c.x < 10);

// CHECK:      OpLine [[file]] 72 7
// CHECK-NEXT: OpAccessChain %_ptr_Function_float %c %int_0
// CHECK:      OpLine [[file]] 72 3
// CHECK-NEXT: pStore %a
  a = c.x;

  return b * c;
}
