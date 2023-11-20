// RUN: %dxc -T cs_6_0 -E main -fspv-fix-func-call-arguments -O0
RWStructuredBuffer< float4 > output : register(u1);

[noinline]
float4 foo(inout float f0, inout int f1)
{
    return 0;
}

// CHECK: [[s39:%\w+]] = OpVariable %_ptr_Function_int Function
// CHECK: [[s36:%\w+]] = OpVariable %_ptr_Function_float Function
// CHECK: [[s33:%\w+]] = OpAccessChain %_ptr_Uniform_float {{%\w+}} %int_0
// CHECK: [[s34:%\w+]] = OpAccessChain %_ptr_Function_int {{%\w+}} %int_1
// CHECK: [[s37:%\w+]] = OpLoad %float [[s33]]
// CHECK:                OpStore [[s36]] [[s37]]
// CHECK: [[s40:%\w+]] = OpLoad %int [[s34]]
// CHECK:                OpStore [[s39]] [[s40]]
// CHECK: {{%\w+}} = OpFunctionCall %v4float %foo [[s36]] [[s39]]
// CHECK: [[s41:%\w+]] = OpLoad %int [[s39]]
// CHECK:                OpStore [[s34]] [[s41]]
// CHECK: [[s38:%\w+]] = OpLoad %float [[s36]]
// CHECK:                OpStore [[s33]] [[s38]]

struct Stru {
  int x;
  int y;
};

[numthreads(1,1,1)]
void main()
{
    Stru stru;
    foo(output[0].x, stru.y);
}
