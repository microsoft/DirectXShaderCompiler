// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// Verify fixes for:
// Bug 1: extra calls to MulPayload due to LValue emit for function call and skipping arg replacement when deemed unnecessary
// Bug 2: local modification to the payload structure in MullPayload leaks to calling function

// CHECK-DAG: [[p1x:%[^ ]*]] = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0,
// CHECK-DAG: [[p1y:%[^ ]*]] = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1,
// CHECK-DAG: [[p1z:%[^ ]*]] = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2,
// CHECK-DAG: [[inputx:%[^ ]*]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0,
// CHECK-DAG: [[inputy:%[^ ]*]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1,
// CHECK-DAG: [[add_inputxy:%[^ ]*]] = fadd fast float [[inputy]], [[inputx]]
// CHECK-DAG: [[retx:%[^ ]*]] = fmul fast float [[add_inputxy]], [[p1x]]
// CHECK-DAG: [[rety:%[^ ]*]] = fmul fast float [[add_inputxy]], [[p1y]]
// CHECK-DAG: [[retz:%[^ ]*]] = fmul fast float [[add_inputxy]], [[p1z]]
// CHECK-DAG: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float [[retx]])
// CHECK-DAG: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float [[rety]])
// CHECK-DAG: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float [[retz]])

struct PayloadStruct {
  float3 Color;
};

PayloadStruct MulPayload(in PayloadStruct Payload, in float x)
{
  Payload.Color *= x;
  return Payload;
}

PayloadStruct AddPayload(in PayloadStruct Payload0, in PayloadStruct Payload1)
{
  Payload0.Color += Payload1.Color;
  return Payload0;
}

void main(float2 input : INPUT,
          PayloadStruct FirstPayload : FirstPayload,
          out PayloadStruct OutputPayload : SV_Target) {

  OutputPayload = AddPayload(MulPayload(FirstPayload, input.x),
                             MulPayload(FirstPayload, input.y));
}
