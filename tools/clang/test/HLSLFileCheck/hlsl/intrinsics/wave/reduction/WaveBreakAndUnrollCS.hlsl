// RUN: %dxc -T cs_6_0 %s | FileCheck %s
// A test of explicit loop unrolling on a loop that uses a wave op in a break block

StructuredBuffer<float> t0;
RWStructuredBuffer<float> u0;

[RootSignature("DescriptorTable(SRV(t0), UAV(u0))")]
[numthreads(64,1,1)]
void main(uint GI : SV_GroupIndex)
{
  float r = 0;
  [unroll]
  for (int i = 0; i < 8; ++i) {
    r += t0[i];
    if (i > 4) {
      r += WaveActiveSum(t0[i+1]);
      break;
    }
  }
  u0[GI] = r;
}
