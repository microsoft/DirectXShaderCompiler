int g;
#ifndef semantic
#define semantic SV_Target
#endif
#ifdef DX12
#define RS "CBV(b0)"
[RootSignature ( RS )]
#endif

float4 main() : semantic
{
  return g;
}
