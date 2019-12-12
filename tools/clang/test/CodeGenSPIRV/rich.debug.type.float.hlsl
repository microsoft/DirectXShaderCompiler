// Run: %dxc -T ps_6_2 -E main -fspv-debug=rich -enable-16bit-types

// CHECK:        [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:    [[floatName:%\d+]] = OpString "float"
// CHECK:  [[float16Name:%\d+]] = OpString "float16_t"
// CHECK:  [[float64Name:%\d+]] = OpString "float64_t"
float4 main(float4 color : COLOR) : SV_TARGET {
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeBasic [[floatName]] %uint_32 Float
  float a = 0;
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeBasic [[float16Name]] %uint_16 Float
  float16_t b = 0;
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeBasic [[float64Name]] %uint_64 Float
  float64_t c = 0;

  return color;
}
