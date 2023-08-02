// RUN: %dxc -T ps_6_0 -E main -fspv-debug=rich-with-source

// CHECK:      [[debugSet:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"

// CHECK: rich.debug.debugsource.multiple.hlsl
// CHECK: spirv.debug.opline.include-file-3.hlsli
// CHECK: [[file3_code:%\d+]] = OpString "int b;
// CHECK: spirv.debug.opline.include-file-2.hlsli
// CHECK: [[file2_code:%\d+]] = OpString "static int a;
// CHECK: spirv.debug.opline.include-file-1.hlsli
// CHECK: [[file1_code:%\d+]] = OpString "int function1() {
// CHECK: [[main_code:%\d+]] = OpString "// RUN: %dxc -T ps_6_0 -E main -fspv-debug=rich-with-source

// CHECK: {{%\d+}} = OpExtInst %void [[debugSet]] DebugSource {{%\d+}} [[file3_code]]
// CHECK: {{%\d+}} = OpExtInst %void [[debugSet]] DebugSource {{%\d+}} [[file2_code]]
// CHECK: {{%\d+}} = OpExtInst %void [[debugSet]] DebugSource {{%\d+}} [[file1_code]]
// CHECK: {{%\d+}} = OpExtInst %void [[debugSet]] DebugSource {{%\d+}} [[main_code]]

#include "spirv.debug.opline.include-file-1.hlsli"

int callFunction1() {
  return function1();
}

#include "spirv.debug.opline.include-file-2.hlsli"

int callFunction2() {
  return function2();
}

#include "spirv.debug.opline.include-file-3.hlsli"

int callFunction3() {
  CALL_FUNCTION_3;
}

void main() {
  callFunction1();
  callFunction2();
  callFunction3();
}
