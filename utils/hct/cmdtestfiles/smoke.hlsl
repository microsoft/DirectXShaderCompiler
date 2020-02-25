int g;
static int g_unused;

#ifndef semantic
#define semantic SV_Target
#endif
#ifdef DX12
#define RS "CBV(b0)"
[RootSignature ( RS )]
#endif

float4 main() : semantic
{
  #ifdef check_warning
  int x = 3;
  x;
  #endif
  return g;
}
