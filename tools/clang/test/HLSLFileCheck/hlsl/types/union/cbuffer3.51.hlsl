// RUN: %dxc -E main -T ps_6_0 -HV 202x %s | FileCheck %s

// CHECK: %dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
// CHECK: %dx.types.CBufRet.f32 = type { float, float, float, float }
// CHECK: %union.Foo = type { [16 x <4 x float>] }
// CHECK: %union.Bar = type { [16 x <3 x i32>] }
union Foo
{
  float4 g1[16];
};

union Bar
{
  uint3 idx[16];
};

ConstantBuffer<Foo> buf1[32] : register(b77, space3);
ConstantBuffer<Bar> buf2[64] : register(b17);

float4 main(int3 a : A) : SV_TARGET
{
  return buf1[ buf2[a.x].idx[a.y].z ].g1[a.z + 12].wyyy;
}
