// Run: %dxc -T ps_6_0 -E main -enable-operator-overloading

// CHECK:          %a = OpVariable %_ptr_Function_Number Function
// CHECK:        %foo = OpVariable %_ptr_Function_int Function
// CHECK: [[call:%\w+]] = OpFunctionCall %int %Number_operator_Call %a
// CHECK:               OpStore %foo [[call]]

// CHECK: %Number_operator_Call = OpFunction %int None
// CHECK: %param_this = OpFunctionParameter %_ptr_Function_Number

struct Number {
    int n;

    int operator()() {
        return n;
    }
};

void main() {
    Number a;
    int foo = a();
}
