// RUN: %dxr -generate-differentials %s | FileCheck %s
//
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: %dxc -T ps_6_9 -HV 2021 -Fo %t.dxil %t.gen.hlsl

// Algebraic / piecewise-smooth intrinsics in forward mode are preserved on
// Value<T>: sqrt, rsqrt, rcp, abs, min/max/clamp, lerp, saturate, smoothstep,
// step, fmod.

// CHECK: namespace user { namespace ad { namespace fwd {
// CHECK: Value<float> f(Value<float> x, Value<float> y)
// CHECK: sqrt(x)
// CHECK: rsqrt(x)
// CHECK: rcp(x)
// CHECK: abs(x)
// CHECK: min(x, y)
// CHECK: max(x, y)
// CHECK: clamp(x, x, y)
// CHECK: lerp(x, y, x)
// CHECK: saturate(x)
// CHECK: step(x, y)
// CHECK: smoothstep(x, y, x)
// CHECK: fmod(x, y)
// CHECK: } } } // namespace user::ad::fwd

[[dxc::AutoDiff(fwd)]]
float f(float x, float y) {
  return sqrt(x) + rsqrt(x) + rcp(x) + abs(x)
       + min(x, y) + max(x, y) + clamp(x, x, y)
       + lerp(x, y, x) + saturate(x)
       + step(x, y) + smoothstep(x, y, x)
       + fmod(x, y);
}

float main(float x : A) : SV_Target { return f(x, x); }
