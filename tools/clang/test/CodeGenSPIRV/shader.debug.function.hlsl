// RUN: %dxc -T ps_6_0 -E main -fspv-debug=vulkan

// TODO: FlagIsPublic is shown as FlagIsProtected|FlagIsPrivate.

// CHECK:             [[set:%\d+]] = OpExtInstImport "NonSemantic.Shader.DebugInfo.100"
// CHECK:         [[fooName:%\d+]] = OpString "foo"
// CHECK:        [[emptyStr:%\d+]] = OpString ""
// CHECK:        [[mainName:%\d+]] = OpString "main"
// CHECK:         [[clOpts:%\d+]] = OpString " -E main -T ps_6_0 -spirv -fcgl -Vd -fspv-debug=vulkan -Qembed_debug" 

// CHECK:    [[int:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 %uint_4 %uint_0
// CHECK:  [[float:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 %uint_3 %uint_0

// CHECK: [[fooFnType:%\d+]] = OpExtInst %void [[set]] DebugTypeFunction %uint_3 %void [[int]] [[float]]
// CHECK:          [[source:%\d+]] = OpExtInst %void [[set]] DebugSource
// CHECK: [[compilationUnit:%\d+]] = OpExtInst %void [[set]] DebugCompilationUnit

// Check DebugFunction instructions
//
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugFunction [[fooName]] [[fooFnType]] [[source]] %uint_35 %uint_1 [[compilationUnit]] [[emptyStr]] %uint_3 %uint_36

// CHECK: [[float4:%\d+]] = OpExtInst %void [[set]] DebugTypeVector [[float]] %uint_4
// CHECK: [[mainFnType:%\d+]] = OpExtInst %void [[set]] DebugTypeFunction %uint_3 [[float4]] [[float4]]
// CHECK: [[mainDbgFn:%\d+]] = OpExtInst %void [[set]] DebugFunction [[mainName]] [[mainFnType]] [[source]] %uint_40 %uint_1 [[compilationUnit]] [[emptyStr]] %uint_3 %uint_41 
// CHECK: [[mainDbgEp:%\d+]] = OpExtInst %void [[set]] DebugEntryPoint [[mainDbgFn]] [[compilationUnit]] {{%\d+}} [[clOpts]]

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

