// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

struct foo {
};

// CHECK: [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK: [[foo:%\d+]] = OpString "foo"

// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeComposite [[foo]] Structure {{%\d+}} 3 8 {{%\d+}} {{%\d+}} %uint_0 FlagIsProtected|FlagIsPrivate

float4 main(float4 color : COLOR) : SV_TARGET {
  foo a;

  return color;
}
