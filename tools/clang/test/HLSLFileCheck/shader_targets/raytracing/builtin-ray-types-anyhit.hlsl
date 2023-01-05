// RUN: %dxc -T lib_6_4 %s 2>&1 | FileCheck %s

// CHECK-NOT: error
[shader("anyhit")]
void anyhit_param0( inout RayDesc D1, RayDesc D2 ) { }

[shader("anyhit")]
void anyhit_param1( inout BuiltInTriangleIntersectionAttributes A1, BuiltInTriangleIntersectionAttributes A2 ) { }

// CHECK: builtin-ray-types-anyhit.hlsl:15:37: error: payload and attribute structures must be user defined types with only numeric contents.
// CHECK: builtin-ray-types-anyhit.hlsl:15:48: error: payload and attribute structures must be user defined types with only numeric contents.
// CHECK: builtin-ray-types-anyhit.hlsl:15:6: error: shader must include inout payload structure parameter.
// CHECK: builtin-ray-types-anyhit.hlsl:15:6: error: shader must include attributes structure parameter.
[shader("anyhit")]
void anyhit_param2( inout Texture2D A1, float4 A2 ) { }
// CHECK-NOT: error
