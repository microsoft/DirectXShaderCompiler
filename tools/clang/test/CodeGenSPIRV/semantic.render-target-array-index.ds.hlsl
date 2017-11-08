// Run: %dxc -T ds_6_0 -E main

// HS PCF output
struct HsPcfOut {
  float  outTessFactor[4]   : SV_TessFactor;
  float  inTessFactor[2]    : SV_InsideTessFactor;
  uint   index              : SV_RenderTargetArrayIndex;
};

// Per-vertex input structs
struct DsCpIn {
  uint   index              : SV_RenderTargetArrayIndex;
};

// Per-vertex output structs
struct DsCpOut {
  uint   index              : SV_RenderTargetArrayIndex;
};

// CHECK:      OpEntryPoint TessellationEvaluation %main "main"
// CHECK-SAME: %in_var_SV_RenderTargetArrayIndex
// CHECK-SAME: %in_var_SV_RenderTargetArrayIndex_0
// CHECK-SAME: %out_var_SV_RenderTargetArrayIndex


// CHECK:      OpDecorate %in_var_SV_RenderTargetArrayIndex Location 0
// CHECK:      OpDecorate %in_var_SV_RenderTargetArrayIndex_0 Location 1
// CHECK:      OpDecorate %out_var_SV_RenderTargetArrayIndex Location 0

// CHECK:      %in_var_SV_RenderTargetArrayIndex = OpVariable %_ptr_Input__arr_uint_uint_3 Input
// CHECK:      %in_var_SV_RenderTargetArrayIndex_0 = OpVariable %_ptr_Input_uint Input
// CHECK:      %out_var_SV_RenderTargetArrayIndex = OpVariable %_ptr_Output_uint Output

[domain("quad")]
DsCpOut main(OutputPatch<DsCpIn, 3> patch, HsPcfOut pcfData) {
  DsCpOut dsOut;
  dsOut = (DsCpOut)0;
  return dsOut;
}
