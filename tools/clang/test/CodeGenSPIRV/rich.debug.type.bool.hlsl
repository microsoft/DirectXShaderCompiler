// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// CHECK:      [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK: [[typeName:%\d+]] = OpString "bool"

float4 main(float4 color : COLOR) : SV_TARGET {
  // CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeBasic [[typeName]] %uint_32 Boolean
  bool condition = false;
  return color;
}

