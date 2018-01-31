// Run: %dxc -T vs_6_0 -E main

struct S {
    float4 m : MMM;
};

struct T {
    float3 n : NNN;
};

struct Base {
    float4 a : AAA;
    float4 b : BBB;
    S      s;
    float4 p : SV_Position;
};

struct Derived : Base {
    T      t;
    float4 c : CCC;
    float4 d : DDD;
};

void main(in Derived input, out Derived output) {
// CHECK:         [[a:%\d+]] = OpLoad %v4float %in_var_AAA
// CHECK-NEXT:    [[b:%\d+]] = OpLoad %v4float %in_var_BBB

// CHECK-NEXT:    [[m:%\d+]] = OpLoad %v4float %in_var_MMM
// CHECK-NEXT:    [[s:%\d+]] = OpCompositeConstruct %S [[m]]

// CHECK-NEXT:  [[pos:%\d+]] = OpLoad %v4float %in_var_SV_Position

// CHECK-NEXT: [[base:%\d+]] = OpCompositeConstruct %Base [[a]] [[b]] [[s]] [[pos]]

// CHECK-NEXT:    [[n:%\d+]] = OpLoad %v3float %in_var_NNN
// CHECK-NEXT:    [[t:%\d+]] = OpCompositeConstruct %T [[n]]

// CHECK-NEXT:    [[c:%\d+]] = OpLoad %v4float %in_var_CCC
// CHECK-NEXT:    [[d:%\d+]] = OpLoad %v4float %in_var_DDD

// CHECK-NEXT:  [[drv:%\d+]] = OpCompositeConstruct %Derived [[base]] [[t]] [[c]] [[d]]
// CHECK-NEXT:                 OpStore %param_var_input [[drv]]

// CHECK-NEXT:      {{%\d+}} = OpFunctionCall %void %src_main %param_var_input %param_var_output

// CHECK-NEXT:  [[drv:%\d+]] = OpLoad %Derived %param_var_output

// CHECK-NEXT: [[base:%\d+]] = OpCompositeExtract %Base [[drv]] 0
// CHECK-NEXT:    [[a:%\d+]] = OpCompositeExtract %v4float [[base]] 0
// CHECK-NEXT:                 OpStore %out_var_AAA [[a]]
// CHECK-NEXT:    [[b:%\d+]] = OpCompositeExtract %v4float [[base]] 1
// CHECK-NEXT:                 OpStore %out_var_BBB [[b]]

// CHECK-NEXT:    [[s:%\d+]] = OpCompositeExtract %S [[base]] 2
// CHECK-NEXT:    [[m:%\d+]] = OpCompositeExtract %v4float [[s]] 0
// CHECK-NEXT:                 OpStore %out_var_MMM [[m]]

// CHECK-NEXT:  [[pos:%\d+]] = OpCompositeExtract %v4float [[base]] 3
// CHECK-NEXT:  [[ptr:%\d+]] = OpAccessChain %_ptr_Output_v4float %gl_PerVertexOut %uint_0
// CHECK-NEXT:                 OpStore [[ptr]] [[pos]]

// CHECK-NEXT:    [[t:%\d+]] = OpCompositeExtract %T [[drv]] 1
// CHECK-NEXT:    [[n:%\d+]] = OpCompositeExtract %v3float [[t]] 0
// CHECK-NEXT:                 OpStore %out_var_NNN [[n]]

// CHECK-NEXT:    [[c:%\d+]] = OpCompositeExtract %v4float [[drv]] 2
// CHECK-NEXT:                 OpStore %out_var_CCC [[c]]
// CHECK-NEXT:    [[d:%\d+]] = OpCompositeExtract %v4float [[drv]] 3
// CHECK-NEXT:                 OpStore %out_var_DDD [[d]]
    output = input;
}
