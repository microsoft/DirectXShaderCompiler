// Run: %dxc -T ps_6_2 -E main -enable-16bit-types
 
Texture2D tex;
SamplerState texSampler;

cbuffer CBuf {
    float4 avgLum;
}

half4 main(float2 uv : UV) : SV_TARGET {
  half4 result = tex.Sample(texSampler, uv);

// Testing that compound multiply-assign works correctly if there are type
// mismatches.
//
// Note: Due to the semantics of operators, the operands are promoted, the
// arithmetic performed, an implicit conversion back to the result type done,
// then the assignment takes place.

// CHECK:            [[avgLum:%\d+]] = OpLoad %float {{%\d+}}
// CHECK:        [[multiplier:%\d+]] = OpFMul %float [[avgLum]] %float_10
// CHECK:            [[result:%\d+]] = OpLoad %v4half %result
// CHECK:        [[result_v4f:%\d+]] = OpFConvert %v4float [[result]]
// CHECK:    [[mul_result_v4f:%\d+]] = OpVectorTimesScalar %v4float [[result_v4f]] [[multiplier]]
// CHECK: [[mul_result_v4half:%\d+]] = OpFConvert %v4half [[mul_result_v4f]]
// CHECK:                              OpStore %result [[mul_result_v4half]]
  result *= avgLum.x * 10.0f;

	return result;
}
