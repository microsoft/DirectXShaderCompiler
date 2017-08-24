// Run: %dxc -T ps_6_0 -E main

// Required by the sample interpolation mode
// CHECK: OpCapability SampleRateShading

struct PSInput {
                       float   fp_a: FPA;
  linear               float1  fp_b: FPB;
// CHECK: OpDecorate %in_var_FPC Centroid
  centroid             float2  fp_c: FPC;
// CHECK: OpDecorate %in_var_FPD Flat
  nointerpolation      float3  fp_d: FPD;
// CHECK: OpDecorate %in_var_FPE NoPerspective
  noperspective        float4  fp_e: FPE;
// CHECK: OpDecorate %in_var_FPF Sample
  sample               float   fp_f: FPF;
// CHECK: OpDecorate %in_var_FPG NoPerspective
// CHECK: OpDecorate %in_var_FPG Sample
  noperspective sample float2  fp_g: FPG;

// CHECK: OpDecorate %in_var_INTA Flat
                       int    int_a: INTA;
// CHECK: OpDecorate %in_var_INTD Flat
  nointerpolation      int3   int_d: INTD;

// CHECK: OpDecorate %in_var_UINTA Flat
                       uint  uint_a: UINTA;
// CHECK: OpDecorate %in_var_UINTD Flat
  nointerpolation      uint3 uint_d: UINTD;

// CHECK: OpDecorate %in_var_BOOLA Flat
                       bool  bool_a: BOOLA;
// CHECK: OpDecorate %in_var_BOOLD Flat
  nointerpolation      bool3 bool_d: BOOLD;
};

float4 main(PSInput input) : SV_Target {
  return 1.0;
}
