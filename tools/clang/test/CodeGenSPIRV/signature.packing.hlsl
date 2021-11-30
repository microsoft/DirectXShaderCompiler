// RUN: %dxc -T vs_6_0 -E main -pack-optimized

struct VS_OUTPUT {
  float4 pos : SV_POSITION;

// CHECK: OpDecorate %out_var_A Location 0
  float a : A;

// CHECK: OpDecorate %out_var_B Location 0
// CHECK: OpDecorate %out_var_B Component 2
  double b : B;

// CHECK: OpDecorate %out_var_C_0 Location 1
// CHECK: OpDecorate %out_var_C_1 Location 1
// CHECK: OpDecorate %out_var_C_1 Component 2
// CHECK: OpDecorate %out_var_C_2 Location 2
  float2 c[3] : C;

// CHECK: OpDecorate %out_var_D_0 Location 2
// CHECK: OpDecorate %out_var_D_0 Component 2
// CHECK: OpDecorate %out_var_D_1 Location 3
  float2x2 d : D;

// CHECK: OpDecorate %out_var_E Location 3
// CHECK: OpDecorate %out_var_E Component 2
  int e : E;

// CHECK: OpDecorate %out_var_F Location 4
  float2 f : F;

// CHECK: OpDecorate %out_var_G Location 3
// CHECK: OpDecorate %out_var_G Component 3
  float g : G;
};

VS_OUTPUT main(float4 pos : POSITION,
               float4 color : COLOR) {
  VS_OUTPUT vout;
  return vout;
}
