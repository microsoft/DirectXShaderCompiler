// RUN: %dxc -E main -T hs_6_0 %s | FileCheck %s

// Expect error when the patch constant signature is too large
// CHECK: error: Failed to allocate all patch constant signature elements in available space.

#define NumOutPoints 2

struct HsCpIn {
    float4 pos : SV_Position;
};

struct HsCpOut {
    float4 pos : SV_Position;
};

struct HsPcfOut
{
  float tessOuter[4] : SV_TessFactor;
  float tessInner[2] : SV_InsideTessFactor;
  float4 array1[25] : BIGARRAY;
  float4 array2[2] : SMALLARRAY;
};

HsPcfOut pcf(InputPatch<HsCpIn, NumOutPoints> patch, uint patchId : SV_PrimitiveID) {
  HsPcfOut output = (HsPcfOut)0;
  return output;
}

[patchconstantfunc("pcf")]
[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(NumOutPoints)]
HsCpOut main(InputPatch<HsCpIn, NumOutPoints> patch,
             uint cpId : SV_OutputControlPointID,
             uint patchId : SV_PrimitiveID) {
    HsCpOut output = (HsCpOut)0;
    return output;
}
