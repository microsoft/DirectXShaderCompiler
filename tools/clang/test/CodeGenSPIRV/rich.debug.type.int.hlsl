// Run: %dxc -T ps_6_2 -E main -fspv-debug=rich -enable-16bit-types

// CHECK:        [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK: [[uint64Name:%\d+]] = OpString "uint64_t"
// CHECK:  [[int64Name:%\d+]] = OpString "int64_t"
// CHECK: [[uint16Name:%\d+]] = OpString "uint16_t"
// CHECK:  [[int16Name:%\d+]] = OpString "int16_t"
// CHECK:   [[uintName:%\d+]] = OpString "uint"
// CHECK:    [[intName:%\d+]] = OpString "int"
// CHECK:            %uint_32 = OpConstant %uint 32
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeBasic [[uint64Name]] %uint_64 Unsigned
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeBasic [[int64Name]] %uint_64 Signed
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeBasic [[uint16Name]] %uint_16 Unsigned
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeBasic [[int16Name]] %uint_16 Signed
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeBasic [[uintName]] %uint_32 Unsigned
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeBasic [[intName]] %uint_32 Signed

float4 main(float4 color : COLOR) : SV_TARGET {
  int a = 0;
  uint b = 1;
  int16_t c = 0;
  uint16_t d = 0;
  int64_t e = 0;
  uint64_t f = 0;
  return color;
}
