#ifndef semantic
#define semantic SV_Target
#endif
#ifdef DX12
#define RS ""
[RootSignature ( RS )]
#endif

float4 main() : semantic
{
  return 0;
}
