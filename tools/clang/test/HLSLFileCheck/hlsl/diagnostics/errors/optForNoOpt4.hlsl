// RUN: %dxc -Zi -E main -Od -T ps_6_0 %s | FileCheck %s -check-prefix=CHK_DB
// RUN: %dxc -E main -Od -T ps_6_0 %s | FileCheck %s -check-prefix=CHK_NODB

// CHK_DB: 27:39: error: Offsets for Sample* must be immediated value
// CHK_DB: 27:43: error: Offsets for Sample* must be immediated value
// CHK_DB: 27:10: error: Offsets for Sample* must be immediated value
// CHK_DB: 27:10: error: Offsets for Sample* must be immediated value

// CHK_NODB: error: Offsets for Sample* must be immediated value.
// CHK_NODB-SAME Use /Zi for source location.
// CHK_NODB: error: Offsets for Sample* must be immediated value.
// CHK_NODB-SAME Use /Zi for source location.
// CHK_NODB: error: Offsets for Sample* must be immediated value.
// CHK_NODB-SAME Use /Zi for source location.
// CHK_NODB: error: Offsets for Sample* must be immediated value.
// CHK_NODB-SAME Use /Zi for source location.

SamplerState samp1 : register(s5);
Texture2D<float4> text1 : register(t3);

int i;

float4 main(float2 a : A) : SV_Target {
  float4 r = 0;
  for (uint x=0; x<i;x++)
  for (uint y=0; y<2;y++) {
    r += text1.Sample(samp1, a, int2(x+y,x-y));
  }
  return r;
}
