// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: ; RTAS                              texture     i32         ras      T0             t5     1

// CHECK:@"\01?RTAS@@3URaytracingAccelerationStructure@@A" = external global %struct.RaytracingAccelerationStructure, align 4

// CHECK: define void [[raygen1:@"\\01\?raygen1@[^\"]+"]]() #0 {
// CHECK:   %[[i_0:[0-9]+]] = load %struct.RaytracingAccelerationStructure, %struct.RaytracingAccelerationStructure* @"\01?RTAS@@3URaytracingAccelerationStructure@@A", align 4
// CHECK:   call i32 @dx.op.rayDispatchIndex.i32(i32 145, i8 0)
// CHECK:   call i32 @dx.op.rayDispatchIndex.i32(i32 145, i8 1)
// CHECK:   call i32 @dx.op.rayDispatchDimension.i32(i32 146, i8 0)
// CHECK:   %[[i_8:[0-9]+]] = call %dx.types.Handle @dx.op.createHandleFromResourceStructForLib.struct.RaytracingAccelerationStructure(i32 160, %struct.RaytracingAccelerationStructure %[[i_0]])
// CHECK:   call void @dx.op.traceRay.struct.MyPayload(i32 157, %dx.types.Handle %[[i_8]], i32 0, i32 0, i32 0, i32 1, i32 0, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float 1.250000e-01, float {{.*}}, float {{.*}}, float {{.*}}, float 1.280000e+02, %struct.MyPayload* nonnull {{.*}})
// CHECK:   ret void

////////////////////////////////////////////////////////////////////////////
// Prototype header contents to be removed on implementation of features:
#define HIT_KIND_TRIANGLE_FRONT_FACE              0xFE
#define HIT_KIND_TRIANGLE_BACK_FACE               0xFF

typedef uint RAY_FLAG;
#define RAY_FLAG_NONE                             0x00
#define RAY_FLAG_FORCE_OPAQUE                     0x01
#define RAY_FLAG_FORCE_NON_OPAQUE                 0x02
#define RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH  0x04
#define RAY_FLAG_SKIP_CLOSEST_HIT_SHADER          0x08
#define RAY_FLAG_CULL_BACK_FACING_TRIANGLES       0x10
#define RAY_FLAG_CULL_FRONT_FACING_TRIANGLES      0x20
#define RAY_FLAG_CULL_OPAQUE                      0x40
#define RAY_FLAG_CULL_NON_OPAQUE                  0x80

struct BuiltInTriangleIntersectionAttributes
{
    float2 barycentrics;
};

////////////////////////////////////////////////////////////////////////////

struct MyPayload {
  float4 color;
  uint2 pos;
};

RaytracingAccelerationStructure RTAS : register(t5);

[shader("raygeneration")]
void raygen1()
{
  MyPayload p = (MyPayload)0;
  p.pos = RayDispatchIndex();
  float3 origin = {0, 0, 0};
  float3 dir = normalize(float3(p.pos / (float)RayDispatchDimension(), 1));
  RayDesc ray = { origin, 0.125, dir, 128.0};
  TraceRay(RTAS, RAY_FLAG_NONE, 0, 0, 1, 0, ray, p);
}
