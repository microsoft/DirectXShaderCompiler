// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// CHECK:      [[debugSet:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"

// CHECK: rich.debug.debugsource.multiple.hlsl
// CHECK: spirv.debug.opline.include-file-2.hlsl
// CHECK: spirv.debug.opline.include-file-3.hlsl
// CHECK: spirv.debug.opline.include-file-1.hlsl

// CHECK: {{%\d+}} = OpExtInst %void [[debugSet]] DebugSource {{%\d+}}
// CHECK: {{%\d+}} = OpExtInst %void [[debugSet]] DebugSource {{%\d+}}
// CHECK: {{%\d+}} = OpExtInst %void [[debugSet]] DebugSource {{%\d+}}

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
