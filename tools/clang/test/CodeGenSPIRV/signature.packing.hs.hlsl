// RUN: %dxc -T hs_6_0 -E main -pack-optimized

struct HSPatchConstData {
  float tessFactor[3] : SV_TessFactor;
  float insideTessFactor[1] : SV_InsideTessFactor;

// CHECK-DAG: OpDecorate %out_var_A Location 0
// CHECK-DAG: OpDecorate %out_var_A Patch
  float a : A;

// CHECK-DAG: OpDecorate %out_var_B Location 0
// CHECK-DAG: OpDecorate %out_var_B Component 2
// CHECK-DAG: OpDecorate %out_var_B Patch
  double b : B;

// CHECK-DAG: OpDecorate %out_var_C Location 1
// CHECK-DAG: OpDecorate %out_var_C Patch
  float2 c : C;

// CHECK-DAG: OpDecorate %out_var_D Location 1
// CHECK-DAG: OpDecorate %out_var_D Component 2
// CHECK-DAG: OpDecorate %out_var_D Patch
  float2 d : D;

// CHECK-DAG: OpDecorate %out_var_E Location 2
// CHECK-DAG: OpDecorate %out_var_E Patch
  int e : E;

// CHECK-DAG: OpDecorate %out_var_F Location 2
// CHECK-DAG: OpDecorate %out_var_F Component 1
// CHECK-DAG: OpDecorate %out_var_F Patch
  float2 f : F;

// CHECK-DAG: OpDecorate %out_var_G Location 2
// CHECK-DAG: OpDecorate %out_var_G Component 3
// CHECK-DAG: OpDecorate %out_var_G Patch
  float g : G;
};

struct HSCtrlPt {
// CHECK-DAG: OpDecorate %out_var_H Location 3
  float h : H;

// CHECK-DAG: OpDecorate %out_var_I Location 3
// CHECK-DAG: OpDecorate %out_var_I Component 1
  float2 i : I;

// CHECK-DAG: OpDecorate %out_var_J Location 3
// CHECK-DAG: OpDecorate %out_var_J Component 3
  float j : J;

// CHECK-DAG: OpDecorate %out_var_K Location 4
  float3 k : K;
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
