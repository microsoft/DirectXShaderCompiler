// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// CHECK:    [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:  [[float:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Float
// CHECK: [[float4:%\d+]] = OpExtInst %void [[set]] DebugTypeVector [[float]] 4

//
// Debug function type
// TODO: FlagIsPublic (3u) is shown as FlagIsProtected|FlagIsPrivate.
//
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeFunction FlagIsProtected|FlagIsPrivate [[float4]] [[float4]]

// CHECK: [[bool:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Boolean
// CHECK:  [[int:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Signed

//
// Debug function type
// TODO: FlagIsPublic (3u) is shown as FlagIsProtected|FlagIsPrivate.
//
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeFunction FlagIsProtected|FlagIsPrivate %void [[int]] [[float]]
void foo(int x, float y) {
  x = x + y;
}

float4 main(float4 color : COLOR) : SV_TARGET {
  bool condition = false;
  foo(1, color.x);
  return color;
}

