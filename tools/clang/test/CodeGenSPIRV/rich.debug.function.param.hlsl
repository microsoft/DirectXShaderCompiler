// RUN: %dxc -T ps_6_0 -E main -fspv-debug=rich -fcgl  %s -spirv | FileCheck %s

// TODO: FlagIsPublic is shown as FlagIsProtected|FlagIsPrivate.

// CHECK:             [[set:%[0-9]+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:         [[fooName:%[0-9]+]] = OpString "foo"
// CHECK:        [[emptyStr:%[0-9]+]] = OpString ""
// CHECK:               [[y:%[0-9]+]] = OpString "y"
// CHECK:               [[x:%[0-9]+]] = OpString "x"
// CHECK:        [[mainName:%[0-9]+]] = OpString "wrapper"
// CHECK:           [[color:%[0-9]+]] = OpString "color"
// CHECK:     [[srcMainName:%[0-9]+]] = OpString "main"

// CHECK: [[int:%[0-9]+]] = OpExtInst %void [[set]] DebugTypeBasic {{%[0-9]+}} %uint_32 Signed
// CHECK: [[float:%[0-9]+]] = OpExtInst %void [[set]] DebugTypeBasic {{%[0-9]+}} %uint_32 Float
// CHECK: [[source:%[0-9]+]] = OpExtInst %void [[set]] DebugSource
// CHECK: [[foo:%[0-9]+]] = OpExtInst %void [[set]] DebugFunction [[fooName]] {{%[0-9]+}} [[source]] 25 1 {{%[0-9]+}} [[emptyStr]] FlagIsProtected|FlagIsPrivate 26 %foo
// CHECK: {{%[0-9]+}} = OpExtInst %void [[set]] DebugLocalVariable [[y]] [[float]] [[source]] 25 23 [[foo]] FlagIsLocal 2
// CHECK: {{%[0-9]+}} = OpExtInst %void [[set]] DebugLocalVariable [[x]] [[int]] [[source]] 25 14 [[foo]] FlagIsLocal 1
// CHECK: [[srcMain:%[0-9]+]] = OpExtInst %void [[set]] DebugFunction [[mainName]] {{%[0-9]+}} [[source]] 30 1 {{%[0-9]+}} [[emptyStr]] FlagIsProtected|FlagIsPrivate 31 %main
// CHECK: [[float4:%[0-9]+]] = OpExtInst %void [[set]] DebugTypeVector [[float]] 4
// CHECK: {{%[0-9]+}} = OpExtInst %void [[set]] DebugLocalVariable [[color]] [[float4]] [[source]] 30 20 [[srcMain]] FlagIsLocal 1
// CHECK: [[main:%[0-9]+]] = OpExtInst %void [[set]] DebugFunction [[srcMainName]] {{%[0-9]+}} [[source]] 30 1 {{%[0-9]+}} [[emptyStr]] FlagIsProtected|FlagIsPrivate 31 %src_main

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

