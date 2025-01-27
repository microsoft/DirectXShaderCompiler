// RUN: %dxc -T lib_6_9 -E main %s | FileCheck %s

// CHECK:     %[[HIT:[^ ]+]] = call %dx.types.HitObject @dx.op.hitObject_MakeNop(i32 262)  ; HitObject_MakeNop()
// CHECK:     call void @dx.op.reorderThread(i32 264, %dx.types.HitObject %[[HIT]], i32 undef, i32 0)  ; ReorderThread(hitObject,coherenceHint,numCoherenceHintBitsFromLSB)
// CHECK:     call void @dx.op.reorderThread(i32 264, %dx.types.HitObject %[[HIT]], i32 241, i32 3)  ; ReorderThread(hitObject,coherenceHint,numCoherenceHintBitsFromLSB)
// CHECK:     call void @dx.op.reorderThread(i32 264, %dx.types.HitObject undef, i32 242, i32 7)  ; ReorderThread(hitObject,coherenceHint,numCoherenceHintBitsFromLSB)
// CHECK:     ret void

RaytracingAccelerationStructure RTAS;
RWStructuredBuffer<float> UAV : register(u0);

struct [raypayload] Payload {
  float3 dummy : read(closesthit) : write(caller, anyhit);
};

[shader("raygeneration")] void main() {
  HitObject hit;
  Barrier(UAV_MEMORY, REORDER_SCOPE);
  ReorderThread(hit);
  ReorderThread(hit, 0xf1, 3);
  ReorderThread(0xf2, 7);
}
