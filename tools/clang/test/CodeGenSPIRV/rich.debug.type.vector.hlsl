// Run: %dxc -T ps_6_2 -E main -fspv-debug=rich -enable-16bit-types

// CHECK:        [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:    [[intName:%\d+]] = OpString "int"
// CHECK:  [[floatName:%\d+]] = OpString "float"
// CHECK:            %uint_32 = OpConstant %uint 32
// CHECK: [[intType:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic [[intName]] %uint_32 Signed
// CHECK:    [[int4:%\d+]] = OpExtInst %void [[set]] DebugTypeVector [[intType]] 4
// CHECK: [[floatType:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic [[floatName]] %uint_32 Float
// CHECK:    [[float3:%\d+]] = OpExtInst %void [[set]] DebugTypeVector [[floatType]] 3

float4 main(float4 color : COLOR) : SV_TARGET {
  float3 b = 0.xxx;
  int4 a = 0.xxxx;

  return color;
}
