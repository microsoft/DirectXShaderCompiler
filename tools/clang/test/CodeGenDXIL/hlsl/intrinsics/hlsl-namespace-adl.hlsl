// RUN: %dxc -T lib_6_3 -HV 202x -default-linkage external %s | FileCheck %s

RaytracingAccelerationStructure scene;

namespace imported {
struct Payload {
  float value;
};
using hlsl::TraceRay;

float Ordinary(Payload value) {
  return value.value;
}

float cos(Payload a, Payload b) {
  return a.value + b.value;
}
}

namespace hlsl {
struct Payload {
  float value;
};
}

void TraceImported(RayDesc ray, inout imported::Payload payload) {
  TraceRay(scene, 0, 255, 0, 1, 0, ray, payload);
}

void TraceNative(RayDesc ray, inout hlsl::Payload payload) {
  TraceRay(scene, 0, 255, 0, 1, 0, ray, payload);
}

template <typename T>
void TraceTemplate(RayDesc ray, inout T payload) {
  TraceRay(scene, 0, 255, 0, 1, 0, ray, payload);
}

void InstantiateTrace(RayDesc ray, inout imported::Payload payload) {
  TraceTemplate(ray, payload);
}

template <typename T>
float OrdinaryTemplate(T value) {
  return Ordinary(value);
}

float CallOrdinary(imported::Payload value) {
  return Ordinary(value) + OrdinaryTemplate(value);
}

float CallIntrinsicNameOverload(imported::Payload value) {
  return cos(value, value);
}

// CHECK-LABEL: define void {{.*}}TraceImported
// CHECK: call void @"dx.op.traceRay.struct.imported::Payload"(i32 157,
// CHECK-LABEL: define void {{.*}}TraceNative
// CHECK: call void @"dx.op.traceRay.struct.hlsl::Payload"(i32 157,
// CHECK-LABEL: define void {{.*}}InstantiateTrace
// CHECK: call void @"dx.op.traceRay.struct.imported::Payload"(i32 157,
// CHECK-LABEL: define float {{.*}}CallOrdinary
// CHECK: ret float
// CHECK-LABEL: define float {{.*}}CallIntrinsicNameOverload
// CHECK: ret float
