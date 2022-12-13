// RUN: %dxc -T lib_6_4 %s 2>&1 | FileCheck %s

// CHECK-NOT: error

[shader("miss")]
void miss0(inout RayDesc PL) { }

[shader("miss")]
void miss1(inout BuiltInTriangleIntersectionAttributes PL) { }

// CHECK: builtin-ray-types-miss.hlsl:15:28: error: ray payload parameter must be a user defined type with only numeric contents.
// CHECK: builtin-ray-types-miss.hlsl:15:6: error: shader must include inout payload structure parameter.

[shader("miss")]
void miss2(inout Texture2D PL) { }

// CHECK-NOT: error
