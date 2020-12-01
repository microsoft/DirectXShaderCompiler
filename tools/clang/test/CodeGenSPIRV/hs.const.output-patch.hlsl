// Run: %dxc -T hs_6_0 -E main

struct HSCtrlPt {
  float4 ctrlPt : CONTROLPOINT;
};

struct HSPatchConstData {
  float tessFactor[3] : SV_TessFactor;
  float insideTessFactor[1] : SV_InsideTessFactor;
  float4 constData : CONSTANTDATA;
};

// CHECK: OpDecorate %temp_var_hullMainRetVal Location 2

// CHECK: %temp_var_hullMainRetVal = OpVariable %_ptr_Output__arr_HSCtrlPt_uint_3 Output
// CHECK:        [[invoc_id:%\d+]] = OpLoad %uint %gl_InvocationID
// CHECK:        [[HSResult:%\d+]] = OpFunctionCall %HSCtrlPt %src_main
// CHECK:         [[OutCtrl:%\d+]] = OpAccessChain %_ptr_Output_HSCtrlPt %temp_var_hullMainRetVal [[invoc_id]]
// CHECK:                            OpStore [[OutCtrl]] [[HSResult]]

HSPatchConstData HSPatchConstantFunc(const OutputPatch<HSCtrlPt, 3> input) {
  HSPatchConstData data;

// CHECK: [[OutCtrl0:%\d+]] = OpAccessChain %_ptr_Output_v4float %temp_var_hullMainRetVal %uint_0 %int_0
// CHECK:   [[input0:%\d+]] = OpLoad %v4float [[OutCtrl0]]
// CHECK: [[OutCtrl1:%\d+]] = OpAccessChain %_ptr_Output_v4float %temp_var_hullMainRetVal %uint_1 %int_0
// CHECK:   [[input1:%\d+]] = OpLoad %v4float [[OutCtrl1]]
// CHECK:      [[add:%\d+]] = OpFAdd %v4float [[input0]] [[input1]]
// CHECK: [[OutCtrl2:%\d+]] = OpAccessChain %_ptr_Output_v4float %temp_var_hullMainRetVal %uint_2 %int_0
// CHECK:   [[input2:%\d+]] = OpLoad %v4float [[OutCtrl2]]
// CHECK:                     OpFAdd %v4float [[add]] [[input2]]
  data.constData = input[0].ctrlPt + input[1].ctrlPt + input[2].ctrlPt;

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
  data.ctrlPt = input[CtrlPtID].ctrlPt;
  return data;
}
