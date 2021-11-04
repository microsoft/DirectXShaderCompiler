// RUN: %dxc -T gs_6_0 -E main

struct Empty { };

struct Base : Empty {
    float4 a  : AAA;
    float4 pos: SV_Position;
};

struct Derived : Base {
    float4 b  : BBB;
};

// CHECK-LABEL: %main = OpFunction

// CHECK:         [[empty0:%\d+]] = OpCompositeConstruct %Empty
// CHECK-NEXT:    [[empty1:%\d+]] = OpCompositeConstruct %Empty
// CHECK-NEXT: [[empty_arr:%\d+]] = OpCompositeConstruct %_arr_Empty_uint_2 [[empty0]] [[empty1]]

// CHECK-NEXT:     [[a_arr:%\d+]] = OpLoad %_arr_v4float_uint_2 %in_var_AAA

// CHECK-NEXT:   [[pos_arr:%\d+]] = OpLoad %_arr_v4float_uint_2 %gl_Position

// CHECK-NEXT:    [[empty0:%\d+]] = OpCompositeExtract %Empty [[empty_arr]] 0
// CHECK-NEXT:        [[a0:%\d+]] = OpCompositeExtract %v4float [[a_arr]] 0
// CHECK-NEXT:      [[pos0:%\d+]] = OpCompositeExtract %v4float [[pos_arr]] 0
// CHECK-NEXT:     [[base0:%\d+]] = OpCompositeConstruct %Base [[empty0]] [[a0]] [[pos0]]

// CHECK-NEXT:    [[empty1:%\d+]] = OpCompositeExtract %Empty [[empty_arr]] 1
// CHECK-NEXT:        [[a1:%\d+]] = OpCompositeExtract %v4float [[a_arr]] 1
// CHECK-NEXT:      [[pos1:%\d+]] = OpCompositeExtract %v4float [[pos_arr]] 1
// CHECK-NEXT:     [[base1:%\d+]] = OpCompositeConstruct %Base [[empty1]] [[a1]] [[pos1]]

// CHECK-NEXT:  [[base_arr:%\d+]] = OpCompositeConstruct %_arr_Base_uint_2 [[base0]] [[base1]]

// CHECK-NEXT:     [[b_arr:%\d+]] = OpLoad %_arr_v4float_uint_2 %in_var_BBB

// CHECK-NEXT:     [[base0:%\d+]] = OpCompositeExtract %Base [[base_arr]] 0
// CHECK-NEXT:        [[b0:%\d+]] = OpCompositeExtract %v4float [[b_arr]] 0
// CHECK-NEXT:  [[derived0:%\d+]] = OpCompositeConstruct %Derived [[base0]] [[b0]]

// CHECK-NEXT:     [[base1:%\d+]] = OpCompositeExtract %Base [[base_arr]] 1
// CHECK-NEXT:        [[b1:%\d+]] = OpCompositeExtract %v4float [[b_arr]] 1
// CHECK-NEXT:  [[derived1:%\d+]] = OpCompositeConstruct %Derived [[base1]] [[b1]]

// CHECK-NEXT:    [[inData:%\d+]] = OpCompositeConstruct %_arr_Derived_uint_2 [[derived0]] [[derived1]]
// CHECK-NEXT:                      OpStore %param_var_inData [[inData]]

// CHECK-LABEL: %src_main = OpFunction

[maxvertexcount(2)]
void main(in    line Derived             inData[2],
          inout      LineStream<Derived> outData)
{
// CHECK:            [[ptr:%\d+]] = OpAccessChain %_ptr_Function_Derived %inData %int_0
// CHECK-NEXT:   [[inData0:%\d+]] = OpLoad %Derived [[ptr]]
// CHECK-NEXT:      [[base:%\d+]] = OpCompositeExtract %Base [[inData0]] 0

// CHECK-NEXT:           {{%\d+}} = OpCompositeExtract %Empty [[base]] 0

// CHECK-NEXT:         [[a:%\d+]] = OpCompositeExtract %v4float [[base]] 1
// CHECK-NEXT:                      OpStore %out_var_AAA [[a]]

// CHECK-NEXT:       [[pos:%\d+]] = OpCompositeExtract %v4float [[base]] 2
// CHECK-NEXT:                      OpStore %gl_Position_0 [[pos]]

// CHECK-NEXT:         [[b:%\d+]] = OpCompositeExtract %v4float [[inData0]] 1
// CHECK-NEXT:                      OpStore %out_var_BBB [[b]]

// CHECK-NEXT:       OpEmitVertex
    outData.Append(inData[0]);

    outData.RestartStrip();
}
