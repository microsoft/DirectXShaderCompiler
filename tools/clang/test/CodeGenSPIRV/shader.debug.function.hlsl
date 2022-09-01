// RUN: %dxc -T ps_6_0 -E main -fspv-debug=vulkan

// TODO: FlagIsPublic is shown as FlagIsProtected|FlagIsPrivate.

// CHECK:             [[set:%\d+]] = OpExtInstImport "NonSemantic.Shader.DebugInfo.100"
// CHECK:         [[fooName:%\d+]] = OpString "foo"
// CHECK:        [[emptyStr:%\d+]] = OpString ""
// CHECK:        [[mainName:%\d+]] = OpString "main"

// CHECK:    [[int:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 %uint_4 %uint_0
// CHECK:  [[float:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 %uint_3 %uint_0

// CHECK: [[fooFnType:%\d+]] = OpExtInst %void [[set]] DebugTypeFunction %uint_3 %void [[int]] [[float]]
// CHECK:          [[source:%\d+]] = OpExtInst %void [[set]] DebugSource
// CHECK: [[compilationUnit:%\d+]] = OpExtInst %void [[set]] DebugCompilationUnit

// Check DebugFunction instructions
//
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugFunction [[fooName]] [[fooFnType]] [[source]] %uint_33 %uint_1 [[compilationUnit]] [[emptyStr]] %uint_3 %uint_34

// CHECK: [[float4:%\d+]] = OpExtInst %void [[set]] DebugTypeVector [[float]] %uint_4
// CHECK: [[mainFnType:%\d+]] = OpExtInst %void [[set]] DebugTypeFunction %uint_3 [[float4]] [[float4]]
// CHECK: [[mainDbgFn:%\d+]] = OpExtInst %void [[set]] DebugFunction [[mainName]] [[mainFnType]] [[source]] %uint_38 %uint_1 [[compilationUnit]] [[emptyStr]] %uint_3 %uint_39 

// Check DebugFunctionDefintion is in main
//
// CHECK: %main = OpFunction %void None {{%\d+}}
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugFunctionDefinition [[mainDbgFn]] %main
// CHECK: OpFunctionEnd
// CHECK: OpFunctionEnd
// CHECK: OpFunctionEnd

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

