// RUN: %dxc -E main -T cs_6_0 %s | FileCheck %s
// RUN: %dxc -E main -T cs_6_0 -Zpr %s | FileCheck %s --check-prefix=ROWMAJOR


// CHECK: %[[COL3:.+]] = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %{{.+}}, i32 3)
// CHECK: extractvalue %dx.types.CBufRet.f32 %[[COL3]], 2
// CHECK: extractvalue %dx.types.CBufRet.f32 %[[COL3]], 3

// ROWMAJOR: %[[ROW2:.+]] = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %2, i32 2)
// ROWMAJOR: extractvalue %dx.types.CBufRet.f32 %[[ROW2]], 3
// ROWMAJOR: %[[ROW3:.+]] = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %2, i32 3)
// ROWMAJOR: extractvalue %dx.types.CBufRet.f32 %[[ROW3]], 3

RWStructuredBuffer<float> o;

float4x4 m;

[numthreads(8,8,1)]
void main( uint2 tid : SV_DispatchThreadID, uint2 gid : SV_GroupID, uint2 gtid : SV_GroupThreadID, uint gidx : SV_GroupIndex )
{
o[gid.x] = m._34 + m._44;
}
