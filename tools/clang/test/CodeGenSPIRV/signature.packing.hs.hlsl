// RUN: %dxc -T hs_6_0 -E main -pack-optimized

struct HSPatchConstData {
  float tessFactor[3] : SV_TessFactor;
  float insideTessFactor[1] : SV_InsideTessFactor;

// CHECK: OpDecorate %out_var_A Location 0
  float a : A;

// CHECK: OpDecorate %out_var_B Location 0
// CHECK: OpDecorate %out_var_B Component 2
  double b : B;

// CHECK: OpDecorate %out_var_C Location 1
  float2 c[3] : C;

// CHECK: OpDecorate %out_var_D Location 4
  float2x2 d : D;

// CHECK: OpDecorate %out_var_E Location 1
// CHECK: OpDecorate %out_var_E Component 2
  int e : E;

// CHECK: OpDecorate %out_var_F Location 2
// CHECK: OpDecorate %out_var_F Component 2
  float2 f : F;

// CHECK: OpDecorate %out_var_G Location 1
// CHECK: OpDecorate %out_var_G Component 3
  float g : G;
};

struct HSCtrlPt {
// CHECK: OpDecorate %out_var_H Location 6
  float h : H;

// CHECK: OpDecorate %out_var_I Location 7
  float2 i : I;

// CHECK: OpDecorate %out_var_J Location 8
  float j : J;

// CHECK: OpDecorate %out_var_K Location 9
  float4 k : K;
};

HSPatchConstData HSPatchConstantFunc(const OutputPatch<HSCtrlPt, 3> input) {
  HSPatchConstData data;
  data.tessFactor[0] = 3.0;
  data.tessFactor[1] = 3.0;
  data.tessFactor[2] = 3.0;
  data.insideTessFactor[0] = 3.0;
  return data;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("HSPatchConstantFunc")]
[maxtessfactor(15)]
HSCtrlPt main(InputPatch<HSCtrlPt, 3> input, uint CtrlPtID : SV_OutputControlPointID) {
  HSCtrlPt data;
  data.k = input[CtrlPtID].k;
  return data;
}
