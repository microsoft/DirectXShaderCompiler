// Run: %dxc -T ds_6_0 -E main -fvk-invert-y

// HS PCF output
struct HsPcfOut {
  float  outTessFactor[4]   : SV_TessFactor;
  float  inTessFactor[2]    : SV_InsideTessFactor;
};

// Per-vertex input structs
struct DsCpIn {
    float4 pos : SV_Position;
};

// Per-vertex output structs
struct DsCpOut {
    float4 pos : SV_Position;
};

[domain("quad")]
DsCpOut main(OutputPatch<DsCpIn, 3> patch,
             HsPcfOut pcfData) {
  DsCpOut dsOut;
  dsOut = (DsCpOut)0;
  return dsOut;
}

// CHECK:      [[call:%\d+]] = OpFunctionCall %DsCpOut %src_main %param_var_patch %param_var_pcfData
// CHECK-NEXT:  [[val:%\d+]] = OpCompositeExtract %v4float [[call]] 0
// CHECK-NEXT: [[oldY:%\d+]] = OpCompositeExtract %float [[val]] 1
// CHECK-NEXT: [[newY:%\d+]] = OpFNegate %float [[oldY]]
// CHECK-NEXT:  [[pos:%\d+]] = OpCompositeInsert %v4float [[newY]] [[val]] 1
// CHECK-NEXT:                 OpStore %gl_Position_0 [[pos]]