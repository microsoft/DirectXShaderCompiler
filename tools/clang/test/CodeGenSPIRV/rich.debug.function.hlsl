// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// TODO: FlagIsPublic is shown as FlagIsProtected|FlagIsPrivate.

// CHECK:             [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:        [[mainName:%\d+]] = OpString "src.main"
// CHECK: [[mainLinkageName:%\d+]] = OpString "src.main"
// CHECK:         [[fooName:%\d+]] = OpString "foo"
// CHECK:  [[fooLinkageName:%\d+]] = OpString "foo"
// CHECK:          [[source:%\d+]] = OpExtInst %void [[set]] DebugSource
// CHECK: [[compilationUnit:%\d+]] = OpExtInst %void [[set]] DebugCompilationUnit


//
// Check DebugFunction instructions
//
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugFunction [[mainName]] [[mainFnType:%\d+]] [[source]] 31 1 [[compilationUnit]] [[mainLinkageName]] FlagIsProtected|FlagIsPrivate 32 %src_main
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugFunction [[fooName]] [[fooFnType:%\d+]] [[source]] 26 1 [[compilationUnit]] [[fooLinkageName]] FlagIsProtected|FlagIsPrivate 27 %foo

// CHECK:  [[float:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Float
// CHECK: [[float4:%\d+]] = OpExtInst %void [[set]] DebugTypeVector [[float]] 4
// CHECK:  [[mainFnType]] = OpExtInst %void [[set]] DebugTypeFunction FlagIsProtected|FlagIsPrivate [[float4]] [[float4]]
// CHECK:   [[bool:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Boolean
// CHECK:    [[int:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Signed
// CHECK:   [[fooFnType]] = OpExtInst %void [[set]] DebugTypeFunction FlagIsProtected|FlagIsPrivate %void [[int]] [[float]]
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

