// RUN: %dxc -E main -T vs_5_0 %s

struct VSOut
{
  float4 f[7] : F;
  float4 pos : SV_POSITION;
};

uint c;

VSOut main(float a : A, uint b : B)
{
  VSOut Out;
  Out.pos = a;
  Out.f[2] = a;
  return Out;
}
