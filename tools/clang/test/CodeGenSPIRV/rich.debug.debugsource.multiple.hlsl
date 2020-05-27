// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich-with-source

// CHECK:      [[debugSet:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"

// CHECK-NOT: OpSource
// CHECK: [[main_file:%\d+]] = OpString
// CHECK-SAME: rich.debug.debugsource.multiple.hlsl
// CHECK: [[main_code:%\d+]] = OpString "// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich-with-source
// CHECK: [[file2:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.include-file-2.hlsl
// CHECK: [[file2_code:%\d+]] = OpString "static int a;
// CHECK: [[file3:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.include-file-3.hlsl
// CHECK: [[file3_code:%\d+]] = OpString "groupshared int b;
// CHECK: [[file1:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.include-file-1.hlsl
// CHECK: [[file1_code:%\d+]] = OpString "int function1() {

// CHECK: {{%\d+}} = OpExtInst %void [[debugSet]] DebugSource [[main_file]] [[main_code]]
// CHECK: {{%\d+}} = OpExtInst %void [[debugSet]] DebugSource [[file2]] [[file2_code]]
// CHECK: {{%\d+}} = OpExtInst %void [[debugSet]] DebugSource [[file3]] [[file3_code]]
// CHECK: {{%\d+}} = OpExtInst %void [[debugSet]] DebugSource [[file1]] [[file1_code]]

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

void main() {
  callFunction1();
  callFunction2();
  callFunction3();
}
