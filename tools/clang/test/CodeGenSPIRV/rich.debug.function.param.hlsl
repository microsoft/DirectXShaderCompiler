// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// TODO: FlagIsPublic is shown as FlagIsProtected|FlagIsPrivate.

// CHECK:             [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:         [[fooName:%\d+]] = OpString "foo"
// CHECK:               [[y:%\d+]] = OpString "y"
// CHECK:               [[x:%\d+]] = OpString "x"
// CHECK:        [[mainName:%\d+]] = OpString "src.main"
// CHECK:           [[color:%\d+]] = OpString "color"

// CHECK:    [[int:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Signed
// CHECK:  [[float:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Float
// CHECK:          [[source:%\d+]] = OpExtInst %void [[set]] DebugSource

// CHECK: [[foo:%\d+]] = OpExtInst %void [[set]] DebugFunction [[fooName]] {{%\d+}} [[source]] 23 1 {{%\d+}} [[fooName]] FlagIsProtected|FlagIsPrivate 24 %foo
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[y]] [[float]] [[source]] 23 23 [[foo]] FlagIsLocal 2
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[x]] [[int]] [[source]] 23 14 [[foo]] FlagIsLocal 1
// CHECK: [[float4:%\d+]] = OpExtInst %void [[set]] DebugTypeVector [[float]] 4
// CHECK: [[main:%\d+]] = OpExtInst %void [[set]] DebugFunction [[mainName]] {{%\d+}} [[source]] 28 1 {{%\d+}} [[mainName]] FlagIsProtected|FlagIsPrivate 29 %src_main
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[color]] [[float4]] [[source]] 28 20 [[main]] FlagIsLocal 1

void foo(int x, float y)
{
  x = x + y;
}

float4 main(float4 color : COLOR) : SV_TARGET
{
  bool condition = false;
  foo(1, color.x);
  return color;
}

