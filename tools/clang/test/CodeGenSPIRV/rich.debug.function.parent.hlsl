// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// CHECK:      [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"

// CHECK: rich.debug.function.parent.hlsl
// CHECK: spirv.debug.opline.include-file-3.hlsl
// CHECK: [[f3:%\d+]] = OpString "function3"
// CHECK: spirv.debug.opline.include-file-2.hlsl
// CHECK: [[f2:%\d+]] = OpString "function2"
// CHECK: spirv.debug.opline.include-file-1.hlsl
// CHECK: [[f1:%\d+]] = OpString "function1"


#include "spirv.debug.opline.include-file-1.hlsl"

int callFunction1() {
  return function1();
}

#include "spirv.debug.opline.include-file-2.hlsl"

int callFunction2() {
  return function2();
}

#include "spirv.debug.opline.include-file-3.hlsl"

int callFunction3() {
  CALL_FUNCTION_3;
}

// CHECK: [[s3:%\d+]] = OpExtInst %void [[set]] DebugSource
// CHECK: [[c3:%\d+]] = OpExtInst %void [[set]] DebugCompilationUnit 1 4 [[s3]] HLSL
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugFunction [[f3]] {{%\d+}} [[s3]] 3 1 [[c3]]

// CHECK: [[s2:%\d+]] = OpExtInst %void [[set]] DebugSource
// CHECK: [[c2:%\d+]] = OpExtInst %void [[set]] DebugCompilationUnit 1 4 [[s2]] HLSL
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugFunction [[f2]] {{%\d+}} [[s2]] 2 1 [[c2]]

// CHECK: [[s1:%\d+]] = OpExtInst %void [[set]] DebugSource
// CHECK: [[c1:%\d+]] = OpExtInst %void [[set]] DebugCompilationUnit 1 4 [[s1]] HLSL
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugFunction [[f1]] {{%\d+}} [[s1]] 1 1 [[c1]]

// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugSource

void main() {
  callFunction1();
  callFunction2();
  callFunction3();
}
