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

// CHECK: ; S                                 sampler      NA          NA      S0             s1     1
// CHECK: ; RTAS                              texture     i32         ras      T0             t5     1
// CHECK: ; T                                 texture     f32          2d      T1             t1     1

// CHECK:@"\01?RTAS@@3URaytracingAccelerationStructure@@A" = external global %struct.RaytracingAccelerationStructure, align 4
// CHECK:@"\01?T@@3V?$Texture2D@V?$vector@M$03@@@@A" = external global %class.Texture2D, align 4
// CHECK:@"\01?S@@3USamplerState@@A" = external global %struct.SamplerState, align 4

RaytracingAccelerationStructure RTAS : register(t5);

// CHECK: define void [[raygen1:@"\\01\?raygen1@[^\"]+"]]() #0 {
// CHECK:   %[[i_0:[0-9]+]] = load %struct.RaytracingAccelerationStructure, %struct.RaytracingAccelerationStructure* @"\01?RTAS@@3URaytracingAccelerationStructure@@A", align 4
// CHECK:   call i32 @dx.op.rayDispatchIndex.i32(i32 145, i8 0)
// CHECK:   call i32 @dx.op.rayDispatchIndex.i32(i32 145, i8 1)
// CHECK:   call i32 @dx.op.rayDispatchDimension.i32(i32 146, i8 0)
// CHECK:   %[[i_8:[0-9]+]] = call %dx.types.Handle @dx.op.createHandleFromResourceStructForLib.struct.RaytracingAccelerationStructure(i32 160, %struct.RaytracingAccelerationStructure %[[i_0]])
// CHECK:   call void @dx.op.traceRay.struct.MyPayload(i32 157, %dx.types.Handle %[[i_8]], i32 0, i32 0, i32 0, i32 1, i32 0, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float 1.250000e-01, float {{.*}}, float {{.*}}, float {{.*}}, float 1.280000e+02, %struct.MyPayload* nonnull {{.*}})
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

// CHECK: define void [[intersection1:@"\\01\?intersection1@[^\"]+"]]() #0 {
// CHECK:   [[CurrentRayT:%[^ ]+]] = call float @dx.op.currentRayT.f32(i32 154)
// CHECK:   call i1 @dx.op.reportHit.struct.MyAttributes(i32 158, float [[CurrentRayT]], i32 0, %struct.MyAttributes* nonnull {{.*}})
// CHECK:   ret void

[shader("intersection")]
void intersection1()
{
  float hitT = CurrentRayT();
  MyAttributes attr = (MyAttributes)0;
  bool bReported = ReportHit(hitT, 0, attr);
}

// CHECK: define void [[anyhit1:@"\\01\?anyhit1@[^\"]+"]](%struct.MyPayload* noalias nocapture %payload, %struct.MyAttributes* nocapture readonly %attr) #0 {
// CHECK:   call float @dx.op.objectRayOrigin.f32(i32 149, i8 2)
// CHECK:   call float @dx.op.objectRayDirection.f32(i32 150, i8 2)
// CHECK:   call float @dx.op.currentRayT.f32(i32 154)
// CHECK:   call void @dx.op.acceptHitAndEndSearch(i32 156)
// CHECK:   call void @dx.op.ignoreHit(i32 155)
// CHECK:   %color = getelementptr inbounds %struct.MyPayload, %struct.MyPayload* %payload, i32 0, i32 0
// CHECK:   store <4 x float> {{.*}}, <4 x float>* %color, align 4
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

// CHECK: define void [[closesthit1:@"\\01\?closesthit1@[^\"]+"]](%struct.MyPayload* noalias nocapture %payload, %struct.MyAttributes* nocapture readonly %attr) #0 {
// CHECK:   call void @dx.op.callShader.struct.MyParam(i32 159, i32 %2, %struct.MyParam* nonnull %0)
// CHECK:   %color = getelementptr inbounds %struct.MyPayload, %struct.MyPayload* %payload, i32 0, i32 0
// CHECK:   store <4 x float> {{.*}}, <4 x float>* %color, align 4
// CHECK:   ret void

[shader("closesthit")]
void closesthit1( inout MyPayload payload : SV_RayPayload,
                  in MyAttributes attr : SV_IntersectionAttributes )
{
  MyParam param = {attr.bary, {0,0,0,0}};
  CallShader(attr.id, param);
  payload.color += param.output;
}

// CHECK: define void [[miss1:@"\\01\?miss1@[^\"]+"]](%struct.MyPayload* noalias nocapture %payload) #0 {
// CHECK:   %0 = getelementptr inbounds %struct.MyPayload, %struct.MyPayload* %payload, i32 0, i32 0
// CHECK:   store <4 x float> <float 1.000000e+00, float 0.000000e+00, float 1.000000e+00, float 1.000000e+00>, <4 x float>* %0, align 4
// CHECK:   ret void

[shader("miss")]
void miss1(inout MyPayload payload : SV_RayPayload)
{
  payload.color = float4(1.0, 0.0, 1.0, 1.0);
}

Texture2D T : register(t1);
SamplerState S : register(s1);

// CHECK: define void [[callable1:@"\\01\?callable1@[^\"]+"]](%struct.MyParam* noalias nocapture %param) #0 {
// CHECK:   %[[i_0:[0-9]+]] = load %struct.SamplerState, %struct.SamplerState* @"\01?S@@3USamplerState@@A", align 4
// CHECK:   %[[i_1:[0-9]+]] = load %class.Texture2D, %class.Texture2D* @"\01?T@@3V?$Texture2D@V?$vector@M$03@@@@A", align 4
// CHECK:   %[[i_3:[0-9]+]] = call %dx.types.Handle @dx.op.createHandleFromResourceStructForLib.class.Texture2D(i32 160, %class.Texture2D %[[i_1]])
// CHECK:   %[[i_4:[0-9]+]] = call %dx.types.Handle @dx.op.createHandleFromResourceStructForLib.struct.SamplerState(i32 160, %struct.SamplerState %[[i_0]])
// CHECK:   %[[i_7:[0-9]+]] = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %[[i_3]], %dx.types.Handle %[[i_4]], float %[[i_5:[0-9]+]], float %[[i_6:[0-9]+]], float undef, float undef, i32 undef, i32 undef, i32 undef, float 0.000000e+00)
// CHECK:   ret void

[shader("callable")]
void callable1(inout MyParam param)
{
  param.output = T.SampleLevel(S, param.coord, 0);
}
