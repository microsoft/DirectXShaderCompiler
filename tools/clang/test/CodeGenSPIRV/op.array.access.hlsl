// Run: %dxc -T vs_6_0 -E main

// TODO: collect consecutive OpAccessChains into one

struct S {
    float f[4]; // nested array
    float g[4]; // nested array
};

// CHECK-LABLE: %src_main
float main(float val: A, uint index: B) : C {
    float r;

    S var[8][16];       // struct element
    float4 vecvar[4];   // vector element
    float2x3 matvar[4]; // matrix element

// CHECK:       [[val:%\d+]] = OpLoad %float %val
// CHECK-NEXT:  [[idx:%\d+]] = OpLoad %uint %index
// CHECK-NEXT: [[ptr0:%\d+]] = OpAccessChain %_ptr_Function_float %var [[idx]] %int_1 %int_0 %int_2
// CHECK-NEXT:                 OpStore [[ptr0]] [[val]]

    var[index][1].f[2] = val;
// CHECK-NEXT: [[idx0:%\d+]] = OpLoad %uint %index
// CHECK-NEXT: [[idx1:%\d+]] = OpLoad %uint %index
// CHECK:      [[ptr0:%\d+]] = OpAccessChain %_ptr_Function_float %var %int_0 [[idx0]] %int_1 [[idx1]]
// CHECK-NEXT: [[load:%\d+]] = OpLoad %float [[ptr0]]
// CHECK-NEXT:                 OpStore %r [[load]]
    r = var[0][index].g[index];

// CHECK:       [[val:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[vec2:%\d+]] = OpCompositeConstruct %v2float [[val]] [[val]]
// CHECK-NEXT: [[ptr0:%\d+]] = OpAccessChain %_ptr_Function_v4float %vecvar %int_3
// CHECK-NEXT: [[vec4:%\d+]] = OpLoad %v4float [[ptr0]]
// CHECK-NEXT:  [[res:%\d+]] = OpVectorShuffle %v4float [[vec4]] [[vec2]] 0 1 5 4
// CHECK-NEXT:                 OpStore [[ptr0]] [[res]]
    vecvar[3].ab = val;
// CHECK-NEXT: [[ptr1:%\d+]] = OpAccessChain %_ptr_Function_v4float %vecvar %int_2
// CHECK-NEXT: [[ptr2:%\d+]] = OpAccessChain %_ptr_Function_float [[ptr1]] %uint_1
// CHECK-NEXT: [[load:%\d+]] = OpLoad %float [[ptr2]]
// CHECK-NEXT:                 OpStore %r [[load]]
    r = vecvar[2][1];

// CHECK:       [[val:%\d+]] = OpLoad %float %val
// CHECK-NEXT: [[vec2:%\d+]] = OpCompositeConstruct %v2float [[val]] [[val]]
// CHECK-NEXT: [[ptr0:%\d+]] = OpAccessChain %_ptr_Function_mat2v3float %matvar %int_2
// CHECK-NEXT: [[val0:%\d+]] = OpCompositeExtract %float [[vec2]] 0
// CHECK-NEXT: [[ptr1:%\d+]] = OpAccessChain %_ptr_Function_float [[ptr0]] %int_0 %int_1
// CHECK-NEXT:                 OpStore [[ptr1]] [[val0]]
// CHECK-NEXT: [[val1:%\d+]] = OpCompositeExtract %float [[vec2]] 1
// CHECK-NEXT: [[ptr2:%\d+]] = OpAccessChain %_ptr_Function_float [[ptr0]] %int_1 %int_2
// CHECK-NEXT:                 OpStore [[ptr2]] [[val1]]
    matvar[2]._12_23 = val;
// CHECK-NEXT: [[ptr3:%\d+]] = OpAccessChain %_ptr_Function_mat2v3float %matvar %int_0
// CHECK-NEXT: [[ptr4:%\d+]] = OpAccessChain %_ptr_Function_float [[ptr3]] %uint_1 %uint_2
// CHECK-NEXT: [[load:%\d+]] = OpLoad %float [[ptr4]]
// CHECK-NEXT:                 OpStore %r [[load]]
    r = matvar[0][1][2];

    return r;
}
