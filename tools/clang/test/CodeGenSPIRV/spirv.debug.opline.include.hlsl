// Run: %dxc -T ps_6_0 -E main -Zi

// CHECK:      [[main:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.include.hlsl
// CHECK:      [[file2:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.include-file-2.hlsl
// CHECK:      [[file3:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.include-file-3.hlsl
// CHECK:      [[file1:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.include-file-1.hlsl

// CHECK:              OpLine [[file2]] 1 12
// CHECK-NEXT:    %a = OpVariable %_ptr_Private_int Private
// CHECK:              OpLine [[file3]] 1 17
// CHECK-NEXT:    %b = OpVariable %_ptr_Workgroup_int Workgroup
// CHECK:              OpLine [[main]] 65 1
// CHECK-NEXT: %main = OpFunction %void None

#include "spirv.debug.opline.include-file-1.hlsl"

int callFunction1() {
  return function1();
}

#include "spirv.debug.opline.include-file-2.hlsl"

int callFunction2() {
  // This
  // is
  // an
  // intentional
  // multiple
  // lines.
  // It
  // might
  // be
  // a
  // single
  // line
  // in
  // OpSource.
  return function2();
}

#include "spirv.debug.opline.include-file-3.hlsl"

int callFunction3() {
  // This
  // is
  // an
  // intentional
  // multiple
  // lines.
  // It
  // might
  // be
  // a
  // single
  // line
  // in
  // OpSource.
  CALL_FUNCTION_3;
}

void main() {
// CHECK:      OpLine [[main]] 68 3
// CHECK-NEXT: OpFunctionCall %int %callFunction1
  callFunction1();

  // This
  // is
  // an
  // intentional
  // multiple
  // lines.
  // It
  // might
  // be
  // a
  // single
  // line
  // in
  // OpSource.
// CHECK:      OpLine [[main]] 86 3
// CHECK-NEXT: OpFunctionCall %int %callFunction2
  callFunction2();

// CHECK:      OpLine [[main]] 90 3
// CHECK-NEXT: OpFunctionCall %int %callFunction3
  callFunction3();
}

// CHECK:      OpLine [[main]] 21 1
// CHECK-NEXT: %callFunction1 = OpFunction %int None
// CHECK:      OpLine [[main]] 22 10
// CHECK-NEXT: OpFunctionCall %int %function1
// CHECK:      OpLine [[main]] 22 3
// CHECK-NEXT: OpReturnValue

// CHECK:      OpLine [[main]] 27 1
// CHECK-NEXT: %callFunction2 = OpFunction %int None
// CHECK:      OpLine [[main]] 42 10
// CHECK-NEXT: OpFunctionCall %int %function2
// CHECK:      OpLine [[main]] 42 3
// CHECK-NEXT: OpReturnValue

// CHECK:      OpLine [[main]] 47 1
// CHECK-NEXT: %callFunction3 = OpFunction %int None
// CHECK:      OpLine [[main]] 62 10
// CHECK-NEXT: OpFunctionCall %int %function3
// CHECK:      OpLine [[main]] 62 3
// CHECK-NEXT: OpReturnValue

// CHECK:      OpLine [[file1]] 1 1
// CHECK-NEXT: %function1 = OpFunction %int None

// CHECK:      OpLine [[file2]] 2 1
// CHECK-NEXT: %function2 = OpFunction %int None
// CHECK:      OpLine [[file2]] 3 10
// CHECK-NEXT: OpLoad %int %a

// CHECK:      OpLine [[file3]] 3 1
// CHECK-NEXT: %function3 = OpFunction %int None
// CHECK:      OpLine [[file3]] 4 10
// CHECK-NEXT: OpLoad %int %b
