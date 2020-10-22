// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// TODO: FlagIsPublic is shown as FlagIsProtected|FlagIsPrivate.

// CHECK:             [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:         [[fooName:%\d+]] = OpString "foo"
// CHECK:        [[emptyStr:%\d+]] = OpString ""
// CHECK:        [[mainName:%\d+]] = OpString "main"

// CHECK:    [[int:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Signed
// CHECK:  [[float:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Float

// CHECK: [[fooFnType:%\d+]] = OpExtInst %void [[set]] DebugTypeFunction FlagIsProtected|FlagIsPrivate %void [[int]] [[float]]
// CHECK:          [[source:%\d+]] = OpExtInst %void [[set]] DebugSource
// CHECK: [[compilationUnit:%\d+]] = OpExtInst %void [[set]] DebugCompilationUnit

// Check DebugFunction instructions
//
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugFunction [[fooName]] [[fooFnType]] [[source]] 25 1 [[compilationUnit]] [[emptyStr]] FlagIsProtected|FlagIsPrivate 26 %foo

// CHECK: [[float4:%\d+]] = OpExtInst %void [[set]] DebugTypeVector [[float]] 4
// CHECK: [[mainFnType:%\d+]] = OpExtInst %void [[set]] DebugTypeFunction FlagIsProtected|FlagIsPrivate [[float4]] [[float4]]
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugFunction [[mainName]] [[mainFnType]] [[source]] 30 1 [[compilationUnit]] [[emptyStr]] FlagIsProtected|FlagIsPrivate 31 %src_main

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

