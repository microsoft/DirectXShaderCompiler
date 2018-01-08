// Run: %dxc -T vs_6_0 -E main

struct S {
    float val;

    float getVal() { return val; }
};

static S gSVar = {4.2};

float main() : A {
// CHECK:      %temp_var_S = OpVariable %_ptr_Function_S Function

// CHECK:       [[s:%\d+]] = OpLoad %S %gSVar
// CHECK-NEXT:               OpStore %temp_var_S [[s]]
// CHECK-NEXT:    {{%\d+}} = OpFunctionCall %float %S_getVal %temp_var_S
// CHECK-NEXT:  [[s:%\d+]] = OpLoad %S %temp_var_S
// CHECK-NEXT:               OpStore %gSVar [[s]]
    return gSVar.getVal();
}
