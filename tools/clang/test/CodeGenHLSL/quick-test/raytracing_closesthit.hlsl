// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// CHECK: define void [[closesthit1:@"\\01\?closesthit1@[^\"]+"]](%struct.MyPayload* noalias nocapture %payload, %struct.MyAttributes* nocapture readonly %attr) #0 {
// CHECK:   call void @dx.op.callShader.struct.MyParam(i32 159, i32 {{.*}}, %struct.MyParam* nonnull {{.*}})
// CHECK:   %color = getelementptr inbounds %struct.MyPayload, %struct.MyPayload* %payload, i32 0, i32 0
// CHECK:   store <4 x float> {{.*}}, <4 x float>* %color, align 4
// CHECK:   ret void

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

[shader("closesthit")]
void closesthit1( inout MyPayload payload : SV_RayPayload,
                  in MyAttributes attr : SV_IntersectionAttributes )
{
  MyParam param = {attr.bary, {0,0,0,0}};
  CallShader(attr.id, param);
  payload.color += param.output;
}
