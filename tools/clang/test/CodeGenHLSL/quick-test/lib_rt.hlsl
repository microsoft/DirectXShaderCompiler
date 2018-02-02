// RUN: %dxc -T lib_6_1 %s | FileCheck %s

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

struct MyAttributes {
  float2 bary;
  uint id;
};

struct MyParam {
  float2 coord;
  float4 output;
};

// Declare_TraceRay(MyPayload);
// Declare_ReportHit(MyAttributes);
// Declare_CallShader(MyParam);

// CHECK: ; S                                 sampler      NA          NA      S0             s1     1
// CHECK: ; RTAS                              texture    byte         r/o      T0             t5     1
// CHECK: ; T                                 texture     f32          2d      T1             t1     1

// CHECK: @RTAS_rangeID = external constant i32
// CHECK: @T_rangeID = external constant i32
// CHECK: @S_rangeID = external constant i32

RaytracingAccelerationStructure RTAS : register(t5);

// CHECK: define void [[raygen1:@"\\01\?raygen1@[^\"]+"]]() {
// CHECK:   [[RAWBUF_ID:[^ ]+]] = load i32, i32* @RTAS_rangeID
// CHECK:   %RTAS_texture_rawbuf = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 [[RAWBUF_ID]], i32 0, i1 false)
// CHECK:   call void {{.*}}RayDispatchIndex{{.*}}
// CHECK:   call void {{.*}}RayDispatchDimension{{.*}}
// CHECK:   call void {{.*}}TraceRay{{.*}}(%dx.types.Handle %RTAS_texture_rawbuf, i32 0, i32 0, i32 0, i32 1, i32 0, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float 1.250000e-01, float {{.*}}, float {{.*}}, float {{.*}}, float 1.280000e+02, float* nonnull {{.*}}, float* nonnull {{.*}}, float* nonnull {{.*}}, float* nonnull {{.*}}, i32* nonnull {{.*}}, i32* nonnull {{.*}})
// CHECK:   ret void

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

// CHECK: define void [[intersection1:@"\\01\?intersection1@[^\"]+"]]() {
// CHECK:   call void {{.*}}CurrentRayT{{.*}}(float* nonnull [[pCurrentRayT:%[^)]+]])
// CHECK:   [[CurrentRayT:%[^ ]+]] = load float, float* [[pCurrentRayT]], align 4
// CHECK:   call void {{.*}}ReportHit{{.*}}(float [[CurrentRayT]], i32 0, float 0.000000e+00, float 0.000000e+00, i32 0, i1* nonnull {{.*}})
// CHECK:   ret void

[shader("intersection")]
void intersection1()
{
  float hitT = CurrentRayT();
  MyAttributes attr = (MyAttributes)0;
  bool bReported = ReportHit(hitT, 0, attr);
}

// CHECK: define void [[anyhit1:@"\\01\?anyhit1@[^\"]+"]](float* noalias nocapture, float* noalias nocapture, float* noalias nocapture, float* noalias nocapture, i32* noalias nocapture, i32* noalias nocapture, float, float, i32)
// CHECK:   call void {{.*}}ObjectRayOrigin{{.*}}(float* nonnull {{.*}}, float* nonnull {{.*}}, float* nonnull {{.*}})
// CHECK:   call void {{.*}}ObjectRayDirection{{.*}}(float* nonnull {{.*}}, float* nonnull {{.*}}, float* nonnull {{.*}})
// CHECK:   call void {{.*}}CurrentRayT{{.*}}(float* nonnull {{.*}})
// CHECK:   call void {{.*}}AcceptHitAndEndSearch{{.*}}()
// CHECK:   call void {{.*}}IgnoreHit{{.*}}()
// CHECK:   store float {{.*}}, float* %0, align 4
// CHECK:   store float {{.*}}, float* %1, align 4
// CHECK:   store float {{.*}}, float* %2, align 4
// CHECK:   store float {{.*}}, float* %3, align 4
// CHECK:   store i32 {{.*}}, i32* %4, align 4
// CHECK:   store i32 {{.*}}, i32* %5, align 4
// CHECK:   ret void

[shader("anyhit")]
void anyhit1( inout MyPayload payload : SV_RayPayload,
              in MyAttributes attr : SV_IntersectionAttributes )
{
  float3 hitLocation = ObjectRayOrigin() + ObjectRayDirection() * CurrentRayT();
  if (hitLocation.z < attr.bary.x)
    AcceptHitAndEndSearch();         // aborts function
  if (hitLocation.z < attr.bary.y)
    IgnoreHit();   // aborts function
  payload.color += float4(0.125, 0.25, 0.5, 1.0);
}

// CHECK: define void [[closesthit1:@"\\01\?closesthit1@[^\"]+"]](float* noalias nocapture, float* noalias nocapture, float* noalias nocapture, float* noalias nocapture, i32* noalias nocapture, i32* noalias nocapture, float, float, i32)
// CHECK:   call void {{.*}}CallShader{{.*}}(i32 {{.*}}, float* nonnull {{.*}}, float* nonnull {{.*}}, float* nonnull {{.*}}, float* nonnull {{.*}}, float* nonnull {{.*}}, float* nonnull {{.*}})
// CHECK:   store float {{.*}}, float* %0, align 4
// CHECK:   store float {{.*}}, float* %1, align 4
// CHECK:   store float {{.*}}, float* %2, align 4
// CHECK:   store float {{.*}}, float* %3, align 4
// CHECK:   ret void

[shader("closesthit")]
void closesthit1( inout MyPayload payload : SV_RayPayload,
                  in MyAttributes attr : SV_IntersectionAttributes )
{
  MyParam param = {attr.bary, {0,0,0,0}};
  CallShader(attr.id, param);
  payload.color += param.output;
}

// CHECK: define void [[miss1:@"\\01\?miss1@[^\"]+"]](float* noalias nocapture, float* noalias nocapture, float* noalias nocapture, float* noalias nocapture, i32* noalias nocapture, i32* noalias nocapture)
// CHECK:   store float 1.000000e+00, float* %0, align 4
// CHECK:   store float 0.000000e+00, float* %1, align 4
// CHECK:   store float 1.000000e+00, float* %2, align 4
// CHECK:   store float 1.000000e+00, float* %3, align 4
// CHECK:   ret void

[shader("miss")]
void miss1(inout MyPayload payload : SV_RayPayload)
{
  payload.color = float4(1.0, 0.0, 1.0, 1.0);
}

Texture2D T : register(t1);
SamplerState S : register(s1);

// CHECK: define void [[callable1:@"\\01\?callable1@[^\"]+"]](float* noalias nocapture, float* noalias nocapture, float* noalias nocapture, float* noalias nocapture, float* noalias nocapture, float* noalias nocapture)
// CHECK:   [[T_ID:[^ ]+]] = load i32, i32* @T_rangeID
// CHECK:   %T_texture_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 [[T_ID]], i32 0, i1 false)
// CHECK:   [[S_ID:[^ ]+]] = load i32, i32* @S_rangeID
// CHECK:   %S_sampler = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 [[S_ID]], i32 0, i1 false)
// CHECK:   {{.*}} = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %T_texture_2d, %dx.types.Handle %S_sampler, float {{.*}}, float {{.*}}, float undef, float undef, i32 undef, i32 undef, i32 undef, float 0.000000e+00)
// CHECK:   store float {{.*}}, float* %2, align 4
// CHECK:   store float {{.*}}, float* %3, align 4
// CHECK:   store float {{.*}}, float* %4, align 4
// CHECK:   store float {{.*}}, float* %5, align 4
// CHECK:   ret void

[shader("callable")]
void callable1(inout MyParam param)
{
  param.output = T.SampleLevel(S, param.coord, 0);
}
