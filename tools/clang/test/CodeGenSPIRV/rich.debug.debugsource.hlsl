// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich-with-source

// CHECK:      [[debugSet:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:               {{%\d+}} = OpExtInst %void [[debugSet]] DebugSource {{%\d+}} {{%\d+}}

float4 main(float4 color : COLOR) : SV_TARGET {
  return color;
}

