// Run: %dxc -T ps_6_0 -E main

// CHECK: %type_MyCBuffer = OpTypeStruct %v4float
// CHECK: %type_MyTBuffer = OpTypeStruct %float
cbuffer MyCBuffer {
    float4 cb_val;

// CHECK:      %get_cb_val = OpFunction %v4float None %17
// CHECK:         {{%\d+}} = OpAccessChain %_ptr_Uniform_v4float %var_MyCBuffer %int_0
    float4 get_cb_val() { return cb_val; }
}

tbuffer MyTBuffer {
    float tb_val;

// CHECK:      %get_tb_val = OpFunction %float None %30
// CHECK:         {{%\d+}} = OpAccessChain %_ptr_Uniform_float %var_MyTBuffer %int_0
    float get_tb_val() { return tb_val; }
}

float4 main() : SV_Target {
    return get_cb_val() * get_tb_val();
}
