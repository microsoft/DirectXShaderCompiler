// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// CHECK:      [[debugSet:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:   [[debugSource:%\d+]] = OpExtInst %void [[debugSet]] DebugSource
// CHECK:               {{%\d+}} = OpExtInst %void [[debugSet]] DebugCompilationUnit 1 4 [[debugSource]] HLSL

float4 main(float4 color : COLOR) : SV_TARGET {
  return color;
}

