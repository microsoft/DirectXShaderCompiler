// RUN: %dxc -T ps_6_0 -E main -HV 2021

// CHECK:          %a = OpVariable %_ptr_Function_Number Function
// CHECK: [[call:%\w+]] = OpFunctionCall %int %Number_operator_Call %a
// CHECK:               OpReturnValue [[call]]

// CHECK: %Number_operator_Call = OpFunction %int None
// CHECK: %param_this = OpFunctionParameter %_ptr_Function_Number

struct Number {
    int n;

    int operator()() {
        return n;
    }
};

int main(float4 pos: SV_Position) : SV_Target {
    Number a = {pos.x};
    return a();
}
