// RUN: %dxc -Zi -E main -Od -T ps_6_0 %s | FileCheck %s -check-prefix=CHK_DB
// RUN: %dxc -E main -Od -T ps_6_0 %s | FileCheck %s -check-prefix=CHK_NODB

// CHK_DB: 20:36: error: Offsets for Sample* must be immediated value
// CHK_DB: 20:40: error: Offsets for Sample* must be immediated value
// CHK_NODB: Offsets for Sample* must be immediated value.
// CHK_NODB-SAME Use /Zi for source location.
// CHK_NODB: Offsets for Sample* must be immediated value.
// CHK_NODB-SAME Use /Zi for source location.

SamplerState samp1 : register(s5);
Texture2D<float4> text1 : register(t3);


int x;
int y;

float4 main(float2 a : A) : SV_Target {
  float4 r = 0;
  r = text1.Sample(samp1, a, int2(x+y,x-y));

  return r;
}
