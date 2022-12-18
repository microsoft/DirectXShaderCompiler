// RUN: %dxc -T lib_6_4 %s 2>&1 | FileCheck %s

// CHECK-NOT: error

[shader("callable")]
void callable0( inout RayDesc param ) {}

[shader("callable")]
void callable1( inout BuiltInTriangleIntersectionAttributes param ) {}

// CHECK: builtin-ray-types-callable.hlsl:14:33: error: callable parameter must be a user defined type with only numeric contents.
// CHECK: builtin-ray-types-callable.hlsl:14:6: error: shader must include inout parameter structure.
[shader("callable")]
void callable2( inout Texture2D param ) {}

// CHECK-NOT: error
