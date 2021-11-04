// RUN: %dxc -T ps_6_2 -E main -enable-16bit-types

void main() {

  // 32-bit uint to various 64-bit types.
  uint a;
// CHECK:            [[a:%\d+]] = OpLoad %uint %a
// CHECK-NEXT: [[a_ulong:%\d+]] = OpUConvert %ulong [[a]]
// CHECK-NEXT:                    OpStore %b [[a_ulong]]
  uint64_t b = a;
// CHECK:            [[a:%\d+]] = OpLoad %uint %a
// CHECK-NEXT: [[a_ulong:%\d+]] = OpUConvert %ulong [[a]]
// CHECK-NEXT:[[a_double:%\d+]] = OpConvertUToF %double [[a_ulong]]
// CHECK-NEXT:                    OpStore %c [[a_double]]
  double   c = a;
// CHECK:            [[a:%\d+]] = OpLoad %uint %a
// CHECK-NEXT: [[a_ulong:%\d+]] = OpUConvert %ulong [[a]]
// CHECK-NEXT:  [[a_long:%\d+]] = OpBitcast %long [[a_ulong]]
// CHECK-NEXT:                    OpStore %d [[a_long]]
  int64_t  d = a;


  // 32-bit int to various 64-bit types.
  int aa;
// CHECK:            [[aa:%\d+]] = OpLoad %int %aa
// CHECK-NEXT:  [[aa_long:%\d+]] = OpSConvert %long [[aa]]
// CHECK-NEXT: [[aa_ulong:%\d+]] = OpBitcast %ulong [[aa_long]]
// CHECK-NEXT:                     OpStore %bb [[aa_ulong]]
  uint64_t bb = aa;
// CHECK:             [[aa:%\d+]] = OpLoad %int %aa
// CHECK-NEXT:   [[aa_long:%\d+]] = OpSConvert %long [[aa]]
// CHECK-NEXT: [[aa_double:%\d+]] = OpConvertSToF %double [[aa_long]]
// CHECK-NEXT:                      OpStore %cc [[aa_double]]
  double   cc = aa;
// CHECK:           [[aa:%\d+]] = OpLoad %int %aa
// CHECK-NEXT: [[aa_long:%\d+]] = OpSConvert %long [[aa]]
// CHECK-NEXT:                    OpStore %dd [[aa_long]]
  int64_t  dd = aa;


  // 32-bit float to various 64-bit types.
  float aaa;
// CHECK:             [[aaa:%\d+]] = OpLoad %float %aaa
// CHECK-NEXT: [[aaa_double:%\d+]] = OpFConvert %double [[aaa]]
// CHECK-NEXT:  [[aaa_ulong:%\d+]] = OpConvertFToU %ulong [[aaa_double]]
// CHECK-NEXT:                       OpStore %bbb [[aaa_ulong]]
  uint64_t bbb = aaa;
// CHECK:             [[aaa:%\d+]] = OpLoad %float %aaa
// CHECK-NEXT: [[aaa_double:%\d+]] = OpFConvert %double [[aaa]]
// CHECK-NEXT:                       OpStore %ccc [[aaa_double]]
  double   ccc = aaa;
// CHECK:             [[aaa:%\d+]] = OpLoad %float %aaa
// CHECK-NEXT: [[aaa_double:%\d+]] = OpFConvert %double [[aaa]]
// CHECK-NEXT:   [[aaa_long:%\d+]] = OpConvertFToS %long [[aaa_double]]
// CHECK-NEXT:                       OpStore %ddd [[aaa_long]]
  int64_t  ddd = aaa;


  // 64-bit uint to various 32-bit types.
  uint64_t e;
// CHECK:      [[e64:%\d+]] = OpLoad %ulong %e
// CHECK-NEXT: [[e32:%\d+]] = OpUConvert %uint [[e64]]
// CHECK-NEXT:                OpStore %f [[e32]]
  uint  f = e;
// CHECK:          [[e64:%\d+]] = OpLoad %ulong %e
// CHECK-NEXT:     [[e32:%\d+]] = OpUConvert %uint [[e64]]
// CHECK-NEXT: [[e_float:%\d+]] = OpConvertUToF %float [[e32]]
// CHECK-NEXT:                    OpStore %g [[e_float]]
  float g = e;
// CHECK:        [[e64:%\d+]] = OpLoad %ulong %e
// CHECK-NEXT:   [[e32:%\d+]] = OpUConvert %uint [[e64]]
// CHECK-NEXT: [[e_int:%\d+]] = OpBitcast %int [[e32]]
// CHECK-NEXT:                  OpStore %h [[e_int]]
  int   h = e;


  // 64-bit int to various 32-bit types.
  int64_t ee;
// CHECK:           [[e:%\d+]] = OpLoad %long %ee
// CHECK-NEXT:  [[e_int:%\d+]] = OpSConvert %int [[e]]
// CHECK-NEXT: [[e_uint:%\d+]] = OpBitcast %uint [[e_int]]
// CHECK-NEXT:                   OpStore %ff [[e_uint]]
  uint  ff = ee;
// CHECK:            [[e:%\d+]] = OpLoad %long %ee
// CHECK-NEXT:   [[e_int:%\d+]] = OpSConvert %int [[e]]
// CHECK-NEXT: [[e_float:%\d+]] = OpConvertSToF %float [[e_int]]
// CHECK-NEXT:                    OpStore %gg [[e_float]]
  float gg = ee;
// CHECK:          [[e:%\d+]] = OpLoad %long %ee
// CHECK-NEXT: [[e_int:%\d+]] = OpSConvert %int [[e]]
// CHECK-NEXT:                  OpStore %hh [[e_int]]
  int   hh = ee;


  // 64-bit float to various 32-bit types.
  double eee;
// CHECK:         [[e64:%\d+]] = OpLoad %double %eee
// CHECK-NEXT:    [[e32:%\d+]] = OpFConvert %float [[e64]]
// CHECK-NEXT: [[e_uint:%\d+]] = OpConvertFToU %uint [[e32]]
// CHECK-NEXT:                   OpStore %fff [[e_uint]]
  uint  fff = eee;
// CHECK:              [[e:%\d+]] = OpLoad %double %eee
// CHECK-NEXT:   [[e_float:%\d+]] = OpFConvert %float [[e]]
// CHECK-NEXT:                      OpStore %ggg [[e_float]]
  float ggg = eee;
// CHECK:            [[e:%\d+]] = OpLoad %double %eee
// CHECK-NEXT: [[e_float:%\d+]] = OpFConvert %float [[e]]
// CHECK-NEXT:   [[e_int:%\d+]] = OpConvertFToS %int [[e_float]]
// CHECK-NEXT:                    OpStore %hhh [[e_int]]
  int   hhh = eee;


  // Vector case: 64-bit float to various 32-bit types.
  double2 i;
// CHECK:      [[i_double:%\d+]] = OpLoad %v2double %i
// CHECK-NEXT:  [[i_float:%\d+]] = OpFConvert %v2float [[i_double]]
// CHECK-NEXT:   [[i_uint:%\d+]] = OpConvertFToU %v2uint [[i_float]]
// CHECK-NEXT:                     OpStore %j [[i_uint]]
  uint2   j = i;
// CHECK:      [[i_double:%\d+]] = OpLoad %v2double %i
// CHECK-NEXT:  [[i_float:%\d+]] = OpFConvert %v2float [[i_double]]
// CHECK-NEXT:    [[i_int:%\d+]] = OpConvertFToS %v2int [[i_float]]
// CHECK-NEXT:                     OpStore %k [[i_int]]
  int2    k = i;
// CHECK:      [[i_double:%\d+]] = OpLoad %v2double %i
// CHECK-NEXT:  [[i_float:%\d+]] = OpFConvert %v2float [[i_double]]
// CHECK-NEXT:                     OpStore %l [[i_float]]
  float2  l = i;


  // 16-bit uint to various 32-bit types.
  uint16_t m;
// CHECK:      [[m_ushort:%\d+]] = OpLoad %ushort %m
// CHECK-NEXT:   [[m_uint:%\d+]] = OpUConvert %uint [[m_ushort]]
// CHECK-NEXT:                     OpStore %n [[m_uint]]
  uint  n = m;
// CHECK:      [[m_ushort:%\d+]] = OpLoad %ushort %m
// CHECK-NEXT:   [[m_uint:%\d+]] = OpUConvert %uint [[m_ushort]]
// CHECK-NEXT:  [[m_float:%\d+]] = OpConvertUToF %float [[m_uint]]
// CHECK-NEXT:                     OpStore %o [[m_float]]
  float o = m;
// CHECK:      [[m_ushort:%\d+]] = OpLoad %ushort %m
// CHECK-NEXT:   [[m_uint:%\d+]] = OpUConvert %uint [[m_ushort]]
// CHECK-NEXT:    [[m_int:%\d+]] = OpBitcast %int [[m_uint]]
// CHECK-NEXT:                     OpStore %p [[m_int]]
  int   p = m;


  // 16-bit int to various 32-bit types.
  int16_t mm;
// CHECK:      [[mm_short:%\d+]] = OpLoad %short %mm
// CHECK-NEXT:   [[mm_int:%\d+]] = OpSConvert %int [[mm_short]]
// CHECK-NEXT:  [[mm_uint:%\d+]] = OpBitcast %uint [[mm_int]]
// CHECK-NEXT:                     OpStore %nn [[mm_uint]]
  uint  nn = mm;
// CHECK:      [[mm_short:%\d+]] = OpLoad %short %mm
// CHECK-NEXT:   [[mm_int:%\d+]] = OpSConvert %int [[mm_short]]
// CHECK-NEXT: [[mm_float:%\d+]] = OpConvertSToF %float [[mm_int]]
// CHECK-NEXT:                     OpStore %oo [[mm_float]]
  float oo = mm;
// CHECK:      [[mm_short:%\d+]] = OpLoad %short %mm
// CHECK-NEXT:   [[mm_int:%\d+]] = OpSConvert %int [[mm_short]]
// CHECK-NEXT:                     OpStore %pp [[mm_int]]
  int   pp = mm;
}
