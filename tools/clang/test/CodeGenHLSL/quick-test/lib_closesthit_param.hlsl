// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Fine.
[shader("closesthit")]
void closesthit_nop() {}

// CHECK: error: return type for ray tracing shaders must be void
// CHECK: error: parameter must have SV_RayPayload or SV_IntersectionAttributes semantic

[shader("closesthit")]
float closesthit_param( in float4 extra ) { return extra.x; }
