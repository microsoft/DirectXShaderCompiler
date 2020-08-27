// RUN: %dxc -T ps_6_5 -enable-16bit-types  %s | FileCheck %s

// CHECK: Opcode Pack4x8 not valid in shader model ps_6_5
// CHECK: Opcode Pack4x8 not valid in shader model ps_6_5
// CHECK: Opcode Unpack4x8 not valid in shader model ps_6_5
// CHECK: Opcode Unpack4x8 not valid in shader model ps_6_5
// CHECK: Opcode Unpack4x8 not valid in shader model ps_6_5
// CHECK: Opcode Unpack4x8 not valid in shader model ps_6_5
// CHECK: Opcode Pack4x8 not valid in shader model ps_6_5
// CHECK: Opcode Pack4x8 not valid in shader model ps_6_5

int16_t4 main(int4 input1 : Inputs1, int16_t4 input2 : Inputs2) : SV_Target {
  int8_t4_packed ps1 = pack_s8(input1);
  int8_t4_packed ps2 = pack_clamp_s8(input1);
  int16_t4 up1_out = unpack_s8s16(ps1) + unpack_s8s16(ps2);

  uint8_t4_packed pu1 = pack_u8(input2);
  uint8_t4_packed pu2 = pack_clamp_u8(input2);
  uint16_t4 up2_out = unpack_u8u16(pu1) + unpack_u8u16(pu2);

  return up1_out + up2_out;
}
