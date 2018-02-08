// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Fine.
[shader("anyhit")]
void anyhit_nop() {}

// CHECK: error: return type for ray tracing shaders must be void
// CHECK: error: parameter must have SV_RayPayload or SV_IntersectionAttributes semantic

[shader("anyhit")]
float anyhit_param( in float4 extra ) { return extra.x; }
