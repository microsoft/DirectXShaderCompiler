// RUN: %dxc -Flv..\hlsl.bin\Debug\test\lvaout-%t.txt -T lib_6_6 -Zi %s | FileCheck --input-file=..\hlsl.bin\Debug\test\lvaout-%t.txt %s

// CHECK: myStruct.liveVal.z
// CHECK: t (float)
// CHECK-NOT: liveVal.x
// CHECK-NOT: payload.color

struct SceneConstants
{
  float4 eye;
  float4 U;
  float4 V;
  float4 W;
  float sceneScale;
};

RWTexture2D<float4> outputBuffer : register(u0);
RaytracingAccelerationStructure scene : register(t0);
ConstantBuffer<SceneConstants> sceneConstants : register(b0);
StructuredBuffer<float3> vertexBuffer : register(t1);
StructuredBuffer<int> indexBuffer : register(t2);


struct Payload
{
  float3 color;
  float t;
  int depth;
};

struct Attribs
{
  float2 barys;
};

struct LiveStruct
{
  float f32;
  int i32;
  float4 liveVal;
  float pad;
};

float HelperFunc( float3 p, float x )
{
  return p.z * x;
}

[shader("closesthit")]
void ClosestHit( inout Payload payload, in Attribs attribs )
{
  const int pi = PrimitiveIndex();
  float3    v0 = vertexBuffer[indexBuffer[pi*3+0]];
  float3    v1 = vertexBuffer[indexBuffer[pi*3+1]];
  float3    v2 = vertexBuffer[indexBuffer[pi*3+2]];
  float3    n  = normalize( cross( v1-v0, v2-v0 ) );

  float  s = sceneConstants.sceneScale;
  float  t = RayTCurrent();
  float  x = t * s;
  float3 p = WorldRayOrigin() + x * WorldRayDirection(); // Creates a dependency on t.
  float3 d = normalize( WorldRayDirection() );

  Payload refl_payload;
  refl_payload.depth = payload.depth + 1;
  refl_payload.color = float3(1,0,0);
  refl_payload.t     = 0;

  RayDesc refl_ray;
  refl_ray.Origin    = p;
  refl_ray.Direction = reflect( d, n );
  refl_ray.TMin      = 0.001;
  refl_ray.TMax      = 1e10;

  int2 idxA = DispatchRaysIndex().xy;
  idxA.y = DispatchRaysDimensions().y - idxA.y - 1;
  LiveStruct myStruct;
  myStruct.liveVal = outputBuffer[idxA];

  TraceRay(scene, RAY_FLAG_NONE, 0xff, 0, 1, 0, refl_ray, refl_payload);

  payload.color = refl_payload.color;

  idxA.y = DispatchRaysDimensions().y - idxA.y - 1;
  payload.color.z += idxA.y;

  payload.color.z += myStruct.liveVal.z;
  if (myStruct.liveVal.z > 10000)
    payload.color.z += HelperFunc(p, 2.2f); // Never executed but causes spills. Uses the value of t via p.
}
