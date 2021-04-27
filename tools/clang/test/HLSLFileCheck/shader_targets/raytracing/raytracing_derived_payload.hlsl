// RUN: %dxc -T lib_6_3 -Od %s | FileCheck %s

// Make sure bitcast to base gets folded into GEP on -Od
// CHECK: define void [[raygen1:@"\\01\?raygen1@[^\"]+"]]() #0 {
// CHECK-NOT: bitcast
// CHECK:   ret void

struct SuperBasePayload {
  float g;
  bool DoInner() { return g < 0; }
};

struct BasePayload : SuperBasePayload {
  float f;
  bool DoSomething() { return f < 0 && DoInner(); }
};

struct MyPayload : BasePayload {
  float4 color;
  uint2 pos;
};

RaytracingAccelerationStructure RTAS : register(t5);
RWByteAddressBuffer BAB :  register(u0) ;


[shader("raygeneration")]
void raygen1()
{
  MyPayload p = (MyPayload)0;
  p.pos = DispatchRaysIndex();
  float3 origin = {0, 0, 0};
  float3 dir = normalize(float3(p.pos / (float)DispatchRaysDimensions(), 1));
  RayDesc ray = { origin, 0.125, dir, 128.0};
  TraceRay(RTAS, RAY_FLAG_NONE, 0, 0, 1, 0, ray, p);
  BAB.Store(p.pos.x + p.pos.y * DispatchRaysDimensions().x, p.DoSomething());  // Causes bad bitcast
}
