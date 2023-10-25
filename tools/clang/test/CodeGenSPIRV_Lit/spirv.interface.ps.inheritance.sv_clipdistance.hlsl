// RUN: %dxc -T ps_6_0 -E main

struct Parent {
  float clipDistance : SV_ClipDistance;
};

struct PSInput : Parent
{ };

float main(PSInput input) : SV_TARGET
{
// CHECK:  [[ptr0:%\d+]] = OpAccessChain %_ptr_Input_float %gl_ClipDistance %uint_0
// CHECK: [[load0:%\d+]] = OpLoad %float [[ptr0]]

// CHECK: [[parent:%\d+]] = OpCompositeConstruct %Parent [[load0]]
// CHECK:  [[input:%\d+]] = OpCompositeConstruct %PSInput [[parent]]


// CHECK: [[access0:%\d+]] = OpAccessChain %_ptr_Function_Parent %input %uint_0
// CHECK: [[access1:%\d+]] = OpAccessChain %_ptr_Function_float [[access0]] %int_0
// CHECK:   [[load1:%\d+]] = OpLoad %float [[access1]]
    return input.clipDistance;
}

