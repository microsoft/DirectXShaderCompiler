// RUN: %dxc -E main -T ps_6_6 -enable-16bit-types

float4 main(int16_t4 input1 : Inputs1, int16_t4 input2 : Inputs2) : SV_Target {
  // Note: both int8_t4_packed and uint8_t4_packed are represented as
  // 32-bit unsigned integers in SPIR-V.
  int8_t4_packed signedPacked;
  uint8_t4_packed unsignedPacked;

// CHECK:    [[packed:%\d+]] = OpLoad %uint %unsignedPacked
// CHECK: [[bytes_vec:%\d+]] = OpBitcast %v4char [[packed]]
// CHECK:  [[unpacked:%\d+]] = OpSConvert %v4short [[bytes_vec]]
// CHECK:                      OpStore %up1 [[unpacked]]
  int16_t4 up1 = unpack_s8s16(unsignedPacked);

// CHECK:    [[packed:%\d+]] = OpLoad %uint %signedPacked
// CHECK: [[bytes_vec:%\d+]] = OpBitcast %v4char [[packed]]
// CHECK:  [[unpacked:%\d+]] = OpSConvert %v4short [[bytes_vec]]
// CHECK:                      OpStore %up2 [[unpacked]]
  int16_t4 up2 = unpack_s8s16(signedPacked);

// CHECK:    [[packed:%\d+]] = OpLoad %uint %unsignedPacked
// CHECK: [[bytes_vec:%\d+]] = OpBitcast %v4char [[packed]]
// CHECK:  [[unpacked:%\d+]] = OpSConvert %v4int [[bytes_vec]]
// CHECK:                      OpStore %up3 [[unpacked]]
  int32_t4 up3 = unpack_s8s32(unsignedPacked);

// CHECK:    [[packed:%\d+]] = OpLoad %uint %signedPacked
// CHECK: [[bytes_vec:%\d+]] = OpBitcast %v4char [[packed]]
// CHECK:  [[unpacked:%\d+]] = OpSConvert %v4int [[bytes_vec]]
// CHECK:                      OpStore %up4 [[unpacked]]
  int32_t4 up4 = unpack_s8s32(signedPacked);

// CHECK:    [[packed:%\d+]] = OpLoad %uint %unsignedPacked
// CHECK: [[bytes_vec:%\d+]] = OpBitcast %v4uchar [[packed]]
// CHECK:  [[unpacked:%\d+]] = OpUConvert %v4ushort [[bytes_vec]]
// CHECK:                      OpStore %up5 [[unpacked]]
  uint16_t4 up5 = unpack_u8u16(unsignedPacked);

// CHECK:    [[packed:%\d+]] = OpLoad %uint %signedPacked
// CHECK: [[bytes_vec:%\d+]] = OpBitcast %v4uchar [[packed]]
// CHECK:  [[unpacked:%\d+]] = OpUConvert %v4ushort [[bytes_vec]]
// CHECK:                      OpStore %up6 [[unpacked]]
  uint16_t4 up6 = unpack_u8u16(signedPacked);

// CHECK:    [[packed:%\d+]] = OpLoad %uint %unsignedPacked
// CHECK: [[bytes_vec:%\d+]] = OpBitcast %v4uchar [[packed]]
// CHECK:  [[unpacked:%\d+]] = OpUConvert %v4uint [[bytes_vec]]
// CHECK:                      OpStore %up7 [[unpacked]]
  uint32_t4 up7 = unpack_u8u32(unsignedPacked);

// CHECK:    [[packed:%\d+]] = OpLoad %uint %signedPacked
// CHECK: [[bytes_vec:%\d+]] = OpBitcast %v4uchar [[packed]]
// CHECK:  [[unpacked:%\d+]] = OpUConvert %v4uint [[bytes_vec]]
// CHECK:                      OpStore %up8 [[unpacked]]
  uint32_t4 up8 = unpack_u8u32(signedPacked);

  return 0.xxxx;
}
