// Run: %dxc -T vs_6_0 -E main

// Tests struct variable initialization from different storage class

struct S {
    float4 pos : POSITION;
};

cbuffer Constants {
  S uniform_struct;
}

// uniform_struct is of type %S, while (fn_)private_struct and s is of type %S_0.
// So we need to decompose and assign for uniform_struct -> (fn_)private_struct,
// but not private_struct -> s.

// CHECK-LABEL:           %main = OpFunction

// CHECK:          [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_S %var_Constants %int_0
// CHECK-NEXT: [[uniform:%\d+]] = OpLoad %S [[ptr]]
// CHECK-NEXT:     [[vec:%\d+]] = OpCompositeExtract %v4float [[uniform]] 0
// CHECK-NEXT:     [[ptr:%\d+]] = OpAccessChain %_ptr_Private_v4float %private_struct %uint_0
// CHECK-NEXT:                    OpStore [[ptr]] [[vec]]
static const S private_struct = uniform_struct; // Unifrom -> Private

float4 foo();

S main()
// CHECK-LABEL:       %src_main = OpFunction
{
    S Out;
// CHECK:      [[private:%\d+]] = OpLoad %S_0 %private_struct
// CHECK-NEXT:                    OpStore %s [[private]]
    S s = private_struct; // Private -> Function
    Out.pos = s.pos + foo();
    return Out;
}

float4 foo()
// CHECK-LABEL:            %foo = OpFunction
{
// CHECK:          [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_S %var_Constants %int_0
// CHECK-NEXT: [[uniform:%\d+]] = OpLoad %S [[ptr]]
// CHECK-NEXT:     [[vec:%\d+]] = OpCompositeExtract %v4float [[uniform]] 0
// CHECK-NEXT:     [[ptr:%\d+]] = OpAccessChain %_ptr_Private_v4float %fn_private_struct %uint_0
// CHECK-NEXT:                    OpStore [[ptr]] [[vec]]
    static S fn_private_struct = uniform_struct; // Uniform -> Private
    return fn_private_struct.pos;
}
