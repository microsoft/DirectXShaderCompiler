// Run: %dxc -T ps_6_2 -E main -fspv-debug=rich -enable-16bit-types

// CHECK:        [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:  [[float64Name:%\d+]] = OpString "float64_t"
// CHECK:    [[floatName:%\d+]] = OpString "float"
// CHECK:  [[float16Name:%\d+]] = OpString "float16_t"
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeBasic [[float64Name]] %uint_64 Float
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeBasic [[floatName]] %uint_32 Float
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeBasic [[float16Name]] %uint_16 Float

float4 main(float4 color : COLOR) : SV_TARGET {
  float a = 0;
  float16_t b = 0;
  float64_t c = 0;
  return color;
}
