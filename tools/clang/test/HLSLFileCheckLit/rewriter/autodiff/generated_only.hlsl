// RUN: %dxr -generate-differentials %s | FileCheck %s --check-prefix=AUTHORED
// RUN: %dxr -generate-differentials -generated-only -keep-user-macro %s > %t.gen.hlsl
// RUN: FileCheck %s --input-file=%t.gen.hlsl --implicit-check-not="struct AuthoredPayload" --implicit-check-not="authored_constant" --implicit-check-not="float square(float" --implicit-check-not="float composed(float" --implicit-check-not="AutoDiff" --implicit-check-not="#define" --implicit-check-not="Rewrite unchanged"
// RUN: cat %s %t.gen.hlsl %S/Inputs/generated_only_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Generated code still depends on authored types and primal functions, but
// must not duplicate them when it is combined with the original source.
// AUTHORED: struct AuthoredPayload
// AUTHORED: authored_constant
// AUTHORED: float square(float x)
// AUTHORED: float composed(float x)
// AUTHORED: struct Box

// CHECK: #include <ad/fwd>
// CHECK-NEXT: #include <ad/bwd>
// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: struct Box : ::Box {
// CHECK: Value<float> square(Value<float> x)
// CHECK: namespace user { namespace ad { namespace bwd {
// CHECK: struct Box : ::Box {
// CHECK: float square(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: namespace user { namespace ad { namespace bwd { namespace math {
// CHECK: float square(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: float composed(inout GradientContext<float> context, Variable<float> x, float __dxc_ad_seed)
// CHECK: float __dxc_ad_primal = (::math::square(x.value) + x.value);

// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 4.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 6.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 1.500000e+01
// EXEC: rawBufferStore.f32{{.*}}i32 4, i32 0, float 9.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 5, i32 0, float 1.200000e+01

#define AUTHORED_SCALE 2.0f
struct AuthoredPayload { float value; };
static const float authored_constant = AUTHORED_SCALE;

namespace math {
[[dxc::AutoDiff(bwd)]]
float square(float x) { return x * x; }

[[dxc::AutoDiff(bwd)]]
float composed(float x) { return square(x) + x; }
}

struct Box {
  [[dxc::AutoDiff(fwd, bwd)]]
  float square(float x) { return x * x; }
};
