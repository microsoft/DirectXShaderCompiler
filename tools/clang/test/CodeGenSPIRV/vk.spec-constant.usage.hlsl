// Run: %dxc -T vs_6_0 -E main

// CHECK: OpDecorate [[sc:%\d+]] SpecId 10
[[vk::constant_id(10)]]
// CHECK: [[sc]] = OpSpecConstant %int 12
const int specConst = 12;

// TODO: The frontend parsing hits assertion failures saying cannot evaluating
// as constant int for the following usages.
/*
cbuffer Data {
    float4 pos[specConst];
    float4 tex[specConst + 5];
};
*/

// CHECK: [[add:%\d+]] = OpSpecConstantOp %int IAdd [[sc]] %int_3
static const int val = specConst + 3;

// CHECK-LABEL:  %main = OpFunction
// CHECK:                OpStore %val [[add]]
void main() {

}
