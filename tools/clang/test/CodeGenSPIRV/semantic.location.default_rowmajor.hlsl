// Run: %dxc -T ps_6_0 -Zpr -E main

// CHECK: OpDecorate %in_var_TEXCOORD0 Location 0
// CHECK: OpDecorate %in_var_POSITION Location 3
// CHECK: OpDecorate %in_var_TANGENT Location 7
// CHECK: OpDecorate %in_var_NORMAL Location 11
// CHECK: OpDecorate %in_var_COLOR Location 23

struct PSInput {
  column_major float4x3 a : TEXCOORD0;
  row_major float4x3 b : POSITION;
  float4x3 c : TANGENT;
  float4x3 d[3] : NORMAL;
  float4 color : COLOR;
};

float4 main(PSInput input) : SV_TARGET {
  return input.color;
}
