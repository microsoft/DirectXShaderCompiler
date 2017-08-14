// Run: %dxc -T ps_6_0 -E main

// CHECK-NOT: OpName %fnNoCaller "fnNoCaller"

// CHECK: [[voidf:%\d+]] = OpTypeFunction %void
// CHECK: [[intfint:%\d+]] = OpTypeFunction %int %_ptr_Function_int
// CHECK: [[intfintint:%\d+]] = OpTypeFunction %int %_ptr_Function_int %_ptr_Function_int

// CHECK-NOT: %fnNoCaller = OpFunction
void fnNoCaller() {}

int fnOneParm(int v) { return v; }

int fnTwoParm(int m, int n) { return m + n; }

int fnCallOthers(int v) { return fnOneParm(v); }

// Recursive functions are disallowed by the front end.

// CHECK: %main = OpFunction %void None [[voidf]]
void main() {
// CHECK-LABEL: %bb_entry = OpLabel
// CHECK-NEXT: %v = OpVariable %_ptr_Function_int Function
    int v;
// CHECK-NEXT: [[oneParam:%\w+]] = OpVariable %_ptr_Function_int Function
// CHECK-NEXT: [[twoParam1:%\w+]] = OpVariable %_ptr_Function_int Function
// CHECK-NEXT: [[twoParam2:%\w+]] = OpVariable %_ptr_Function_int Function
// CHECK-NEXT: [[nestedParam1:%\w+]] = OpVariable %_ptr_Function_int Function
// CHECK-NEXT: [[nestedParam2:%\w+]] = OpVariable %_ptr_Function_int Function

// CHECK-NEXT: OpStore [[oneParam]] %int_1
// CHECK-NEXT: [[call0:%\d+]] = OpFunctionCall %int %fnOneParm [[oneParam]]
// CHECK-NEXT: OpStore %v [[call0]]
    v = fnOneParm(1); // Pass in constant; use return value

// CHECK-NEXT: [[v0:%\d+]] = OpLoad %int %v
// CHECK-NEXT: OpStore [[twoParam1]] [[v0]]
// CHECK-NEXT: [[v1:%\d+]] = OpLoad %int %v
// CHECK-NEXT: OpStore [[twoParam2]] [[v1]]
// CHECK-NEXT: [[call2:%\d+]] = OpFunctionCall %int %fnTwoParm [[twoParam1]] [[twoParam2]]
    fnTwoParm(v, v);  // Pass in variable; ignore return value

// CHECK-NEXT: OpStore [[nestedParam2]] %int_1
// CHECK-NEXT: [[call3:%\d+]] = OpFunctionCall %int %fnOneParm [[nestedParam2]]
// CHECK-NEXT: OpStore [[nestedParam1]] [[call3]]
// CHECK-NEXT: [[call4:%\d+]] = OpFunctionCall %int %fnCallOthers [[nestedParam1]]
// CHECK-NEXT: OpReturn
// CHECK-NEXT: OpFunctionEnd
    fnCallOthers(fnOneParm(1)); // Nested function calls
}

// CHECK-NOT: %fnNoCaller = OpFunction

/* For int fnOneParm(int v) { return v; } */

// %fnOneParm = OpFunction %int None [[intfint]]
// %v_0 = OpFunctionParameter %_ptr_Function_int
// %bb_entry_0 = OpLabel
// [[v2:%\d+]] = OpLoad %int %v_0
// OpReturnValue [[v2]]
// OpFunctionEnd


// CHECK-NOT: %fnNoCaller = OpFunction

/* For int fnTwoParm(int m, int n) { return m + n; } */

// %fnTwoParm = OpFunction %int None %27
// %m = OpFunctionParameter %_ptr_Function_int
// %n = OpFunctionParameter %_ptr_Function_int
// %bb_entry_1 = OpLabel
// [[m0:%\d+]] = OpLoad %int %m
// [[n0:%\d+]] = OpLoad %int %n
// [[add0:%\d+]] = OpIAdd %int [[m0]] [[n0]]
// OpReturnValue [[add0]]
// OpFunctionEnd

// CHECK-NOT: %fnNoCaller = OpFunction

/* For int fnCallOthers(int v) { return fnOneParm(v); } */

// %fnCallOthers = OpFunction %int None [[intfintint]]
// %v_1 = OpFunctionParameter %_ptr_Function_int
// %bb_entry_2 = OpLabel
// [[param0:%\d+]] = OpVariable %_ptr_Function_int Function
// [[v3:%\d+]] = OpLoad %int %v_1
// OpStore [[param0]] [[v3]]
// [[call5:%\d+]] = OpFunctionCall %int %fnOneParm [[param0]]
// OpReturnValue [[call5]]
// OpFunctionEnd

// CHECK-NOT: %fnNoCaller = OpFunction
