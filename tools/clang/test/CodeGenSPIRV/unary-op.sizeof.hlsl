// Run: %dxc -T ps_6_0 -E main

struct PSInput
{
  int4 color : COLOR;
};
ByteAddressBuffer g_meshData[] : register(t0, space3);

int3 main(PSInput input) : SV_TARGET
{
// CHECK: OpIMul %uint {{%\d+}} %uint_12
  return g_meshData[input.color.x].Load3(input.color.y * sizeof(float3));
}
