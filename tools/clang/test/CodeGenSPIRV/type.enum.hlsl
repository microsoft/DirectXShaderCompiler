// RUN: %dxc -T cs_6_0 -E main

//CHECK:      %First = OpVariable %_ptr_Private_int Private %int_0
//CHECK-NEXT: %Second = OpVariable %_ptr_Private_int Private %int_1
//CHECK-NEXT: %Third = OpVariable %_ptr_Private_int Private %int_3
//CHECK-NEXT: %Fourth = OpVariable %_ptr_Private_int Private %int_n1
enum Number {
  First,
  Second,
  Third = 3,
  Fourth = -1,
};

//CHECK:      %a = OpVariable %_ptr_Private_int Private
//CHECK-NEXT: %b = OpVariable %_ptr_Workgroup_int Workgroup
//CHECK-NEXT: %c = OpVariable %_ptr_Uniform_type_AppendStructuredBuffer_ Uniform

//CHECK:      [[second:%\d+]] = OpLoad %int %Second
//CHECK-NEXT:                   OpStore %a [[second]]
static ::Number a = Second;
groupshared Number b;
AppendStructuredBuffer<Number> c;

void testParam(Number param) {}
void testParamTypeCast(int param) {}

[numthreads(1, 1, 1)]
void main() {
//CHECK:      [[a:%\d+]] = OpLoad %int %a
//CHECK-NEXT:              OpStore %foo [[a]]
  int foo = a;

//CHECK:      [[fourth:%\d+]] = OpLoad %int %Fourth
//CHECK-NEXT:                   OpStore %b [[fourth]]
  b = Fourth;

//CHECK:          [[c:%\d+]] = OpAccessChain %_ptr_Uniform_int %c %uint_0
//CHECK-NEXT: [[third:%\d+]] = OpLoad %int %Third
//CHECK-NEXT:                  OpStore [[c]] [[third]]
  c.Append(Third);

//CHECK:          [[c:%\d+]] = OpAccessChain %_ptr_Uniform_int %c %uint_0 %57
//CHECK-NEXT: [[third:%\d+]] = OpLoad %int %Third
//CHECK-NEXT:                  OpStore [[c]] [[third]]
  c.Append(Number::Third);

  Number d;
//CHECK:      [[d:%\d+]] = OpLoad %int %d
//CHECK-NEXT:              OpSelectionMerge %switch_merge None
//CHECK-NEXT:              OpSwitch [[d]] %switch_default 0 %switch_0 1 %switch_1
  switch (d) {
    case First:
      d = Second;
      break;
    case Second:
      d = First;
      break;
    default:
      d = Third;
      break;
  }

//CHECK:      [[fourth:%\d+]] = OpLoad %int %Fourth
//CHECK-NEXT:                   OpStore %e [[fourth]]
  static ::Number e = Fourth;

//CHECK:          [[d:%\d+]] = OpLoad %int %d
//CHECK-NEXT: [[third:%\d+]] = OpLoad %int %Third
//CHECK-NEXT:                  OpSLessThan %bool [[d]] [[third]]
  if (d < Third) {
//CHECK:       [[first:%\d+]] = OpLoad %int %First
//CHECK-NEXT: [[second:%\d+]] = OpLoad %int %Second
//CHECK-NEXT:    [[add:%\d+]] = OpIAdd %int [[first]] [[second]]
//CHECK-NEXT:                   OpStore %d [[add]]
    d = First + Second;
  }

//CHECK:      [[foo:%\d+]] = OpLoad %int %foo
//CHECK-NEXT: [[foo:%\d+]] = OpBitcast %int [[foo]]
//CHECK-NEXT:                OpStore %d [[foo]]
  if (First < Third)
    d = (Number)foo;

//CHECK:      [[a:%\d+]] = OpLoad %int %a
//CHECK-NEXT:              OpStore %param_var_param [[a]]
//CHECK-NEXT:              OpFunctionCall %void %testParam %param_var_param
  testParam(a);

//CHECK:      [[second:%\d+]] = OpLoad %int %Second
//CHECK-NEXT:                   OpStore %param_var_param_0 [[second]]
//CHECK-NEXT:                   OpFunctionCall %void %testParam %param_var_param_0
  testParam(Second);

//CHECK:      [[a:%\d+]] = OpLoad %int %a
//CHECK-NEXT:              OpStore %param_var_param_1 [[a]]
//CHECK-NEXT:              OpFunctionCall %void %testParamTypeCast %param_var_param_1
  testParamTypeCast(a);

//CHECK:      OpStore %param_var_param_2 %int_1
//CHECK-NEXT: OpFunctionCall %void %testParamTypeCast %param_var_param_2
  testParamTypeCast(Second);

//CHECK:        [[a:%\d+]] = OpLoad %int %a
//CHECK-NEXT:   [[a:%\d+]] = OpBitcast %float [[a]]
//CHECK-NEXT: [[sin:%\d+]] = OpExtInst %float {{%\d+}} Sin [[a]]
//CHECK-NEXT:                OpStore %bar [[sin]]
  float bar = sin(a);
}
