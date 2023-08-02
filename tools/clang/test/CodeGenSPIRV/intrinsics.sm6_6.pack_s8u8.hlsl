// RUN: %dxc -E main -T ps_6_6 -enable-16bit-types

// CHECK:    %short = OpTypeInt 16 1
// CHECK:  %v4short = OpTypeVector %short 4
// CHECK:   %ushort = OpTypeInt 16 0
// CHECK: %v4ushort = OpTypeVector %ushort 4
// CHECK:     %char = OpTypeInt 8 1
// CHECK:   %v4char = OpTypeVector %char 4
// CHECK:    %uchar = OpTypeInt 8 0
// CHECK:  %v4uchar = OpTypeVector %uchar 4

float4 main(int16_t4 input1 : Inputs1, int16_t4 input2 : Inputs2) : SV_Target {
  int16_t4 v4int16_var;
  uint16_t4 v4uint16_var;

  int32_t4 v4int32_var;
  uint32_t4 v4uint32_var;

  //////////////////////
  // pack_s8 variants //
  //////////////////////

// CHECK: [[v4int16_var:%\d+]] = OpLoad %v4short %v4int16_var
// CHECK:   [[bytes_vec:%\d+]] = OpSConvert %v4char [[v4int16_var]]
// CHECK:      [[packed:%\d+]] = OpBitcast %uint [[bytes_vec]]
// CHECK:                        OpStore %ps1 [[packed]]
  int8_t4_packed ps1 = pack_s8(v4int16_var);

// CHECK: [[v4uint16_var:%\d+]] = OpLoad %v4ushort %v4uint16_var
// CHECK:    [[bytes_vec:%\d+]] = OpUConvert %v4uchar [[v4uint16_var]]
// CHECK:       [[packed:%\d+]] = OpBitcast %uint [[bytes_vec]]
// CHECK:                         OpStore %ps2 [[packed]]
  int8_t4_packed ps2 = pack_s8(v4uint16_var);

// CHECK: [[v4int32_var:%\d+]] = OpLoad %v4int %v4int32_var
// CHECK:   [[bytes_vec:%\d+]] = OpSConvert %v4char [[v4int32_var]]
// CHECK:      [[packed:%\d+]] = OpBitcast %uint [[bytes_vec]]
// CHECK:                        OpStore %ps3 [[packed]]
  int8_t4_packed ps3 = pack_s8(v4int32_var);
// CHECK: [[v4uint32_var:%\d+]] = OpLoad %v4uint %v4uint32_var
// CHECK:    [[bytes_vec:%\d+]] = OpUConvert %v4uchar [[v4uint32_var]]
// CHECK:       [[packed:%\d+]] = OpBitcast %uint [[bytes_vec]]
// CHECK:                         OpStore %ps4 [[packed]]
  int8_t4_packed ps4 = pack_s8(v4uint32_var);

  //////////////////////
  // pack_u8 variants //
  //////////////////////

// CHECK: [[v4int16_var:%\d+]] = OpLoad %v4short %v4int16_var
// CHECK:   [[bytes_vec:%\d+]] = OpSConvert %v4char [[v4int16_var]]
// CHECK:      [[packed:%\d+]] = OpBitcast %uint [[bytes_vec]]
// CHECK:                        OpStore %pu1 [[packed]]
  uint8_t4_packed pu1 = pack_u8(v4int16_var);

// CHECK: [[v4uint16_var:%\d+]] = OpLoad %v4ushort %v4uint16_var
// CHECK:    [[bytes_vec:%\d+]] = OpUConvert %v4uchar [[v4uint16_var]]
// CHECK:       [[packed:%\d+]] = OpBitcast %uint [[bytes_vec]]
// CHECK:                         OpStore %pu2 [[packed]]
  uint8_t4_packed pu2 = pack_u8(v4uint16_var);

// CHECK: [[v4int32_var:%\d+]] = OpLoad %v4int %v4int32_var
// CHECK:   [[bytes_vec:%\d+]] = OpSConvert %v4char [[v4int32_var]]
// CHECK:      [[packed:%\d+]] = OpBitcast %uint [[bytes_vec]]
// CHECK:                        OpStore %pu3 [[packed]]
  uint8_t4_packed pu3 = pack_u8(v4int32_var);

// CHECK: [[v4uint32_var:%\d+]] = OpLoad %v4uint %v4uint32_var
// CHECK:    [[bytes_vec:%\d+]] = OpUConvert %v4uchar [[v4uint32_var]]
// CHECK:       [[packed:%\d+]] = OpBitcast %uint [[bytes_vec]]
// CHECK:                         OpStore %pu4 [[packed]]
  uint8_t4_packed pu4 = pack_u8(v4uint32_var);

  return 0.xxxx;
}
