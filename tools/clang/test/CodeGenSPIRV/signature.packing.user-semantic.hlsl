// RUN: %dxc -T hs_6_0 -E main -pack-optimized -fspv-reflect

struct HSPatchConstData {
  float tessFactor[3] : SV_TessFactor;
  float insideTessFactor[1] : SV_InsideTessFactor;

  float a : A;
  double b : B;

//CHECK: OpDecorateString {{%\w+}} UserSemantic "C0"
//CHECK: OpDecorateString {{%\w+}} UserSemantic "C1"
//CHECK: OpDecorateString {{%\w+}} UserSemantic "C2"
  float2 c[3] : C0;

//CHECK: OpDecorateString {{%\w+}} UserSemantic "C3"
//CHECK: OpDecorateString {{%\w+}} UserSemantic "C4"
  float2x2 d : C3;
};

struct HSCtrlPt {
  float x : X;
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
  data.x = input[CtrlPtID].x;
  return data;
}
