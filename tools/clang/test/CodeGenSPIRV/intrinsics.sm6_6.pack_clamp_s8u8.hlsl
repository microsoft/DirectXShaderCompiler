// RUN: %dxc -E main -T ps_6_6 -enable-16bit-types

float4 main(int16_t4 input1 : Inputs1, int16_t4 input2 : Inputs2) : SV_Target {
  int16_t4 v4int16_var;
  int32_t4 v4int32_var;

// Note: pack_clamp_s8 and pack_clamp_u8 do NOT accept an unsigned argument.

// CHECK:           [[glsl_set:%\d+]] = OpExtInstImport "GLSL.std.450"

// CHECK:                      %short = OpTypeInt 16 1
// CHECK:                    %v4short = OpTypeVector %short 4

// CHECK: [[const_v4short_n128:%\d+]] = OpConstantComposite %v4short %short_n128 %short_n128 %short_n128 %short_n128
// CHECK:  [[const_v4short_127:%\d+]] = OpConstantComposite %v4short %short_127 %short_127 %short_127 %short_127

// CHECK:   [[const_v4int_n128:%\d+]] = OpConstantComposite %v4int %int_n128 %int_n128 %int_n128 %int_n128
// CHECK:    [[const_v4int_127:%\d+]] = OpConstantComposite %v4int %int_127 %int_127 %int_127 %int_127

// CHECK:    [[const_v4short_0:%\d+]] = OpConstantComposite %v4short %short_0 %short_0 %short_0 %short_0
// CHECK:  [[const_v4short_255:%\d+]] = OpConstantComposite %v4short %short_255 %short_255 %short_255 %short_255

// CHECK:      [[const_v4int_0:%\d+]] = OpConstantComposite %v4int %int_0 %int_0 %int_0 %int_0
// CHECK:    [[const_v4int_255:%\d+]] = OpConstantComposite %v4int %int_255 %int_255 %int_255 %int_255

// CHECK:                       %char = OpTypeInt 8 1
// CHECK:                     %v4char = OpTypeVector %char 4

  ////////////////////////////
  // pack_clamp_s8 variants //
  ////////////////////////////

// CHECK: [[v4int16_var:%\d+]] = OpLoad %v4short %v4int16_var
// CHECK:     [[clamped:%\d+]] = OpExtInst %v4short [[glsl_set]] SClamp [[v4int16_var]] [[const_v4short_n128]] [[const_v4short_127]]
// CHECK:   [[truncated:%\d+]] = OpSConvert %v4char [[clamped]]
// CHECK:      [[packed:%\d+]] = OpBitcast %uint [[truncated]]
// CHECK:                        OpStore %ps1 [[packed]]
  int8_t4_packed ps1 = pack_clamp_s8(v4int16_var);

// CHECK: [[v4int16_var:%\d+]] = OpLoad %v4int %v4int32_var
// CHECK:     [[clamped:%\d+]] = OpExtInst %v4int [[glsl_set]] SClamp [[v4int16_var]] [[const_v4int_n128]] [[const_v4int_127]]
// CHECK:   [[truncated:%\d+]] = OpSConvert %v4char [[clamped]]
// CHECK:      [[packed:%\d+]] = OpBitcast %uint [[truncated]]
// CHECK:                        OpStore %ps3 [[packed]]
  int8_t4_packed ps3 = pack_clamp_s8(v4int32_var);

  ////////////////////////////
  // pack_clamp_u8 variants //
  ////////////////////////////

// CHECK: [[v4int16_var:%\d+]] = OpLoad %v4short %v4int16_var
// CHECK:     [[clamped:%\d+]] = OpExtInst %v4short [[glsl_set]] SClamp [[v4int16_var]] [[const_v4short_0]] [[const_v4short_255]]
// CHECK:   [[truncated:%\d+]] = OpSConvert %v4char [[clamped]]
// CHECK:      [[packed:%\d+]] = OpBitcast %uint [[truncated]]
// CHECK:                        OpStore %pu1 [[packed]]
  uint8_t4_packed pu1 = pack_clamp_u8(v4int16_var);

// CHECK: [[v4int32_var:%\d+]] = OpLoad %v4int %v4int32_var
// CHECK:     [[clamped:%\d+]] = OpExtInst %v4int [[glsl_set]] SClamp [[v4int32_var]] [[const_v4int_0]] [[const_v4int_255]]
// CHECK:   [[truncated:%\d+]] = OpSConvert %v4char [[clamped]]
// CHECK:      [[packed:%\d+]] = OpBitcast %uint [[truncated]]
// CHECK:                        OpStore %pu3 [[packed]]
  uint8_t4_packed pu3 = pack_clamp_u8(v4int32_var);

  return 0.xxxx;
}
