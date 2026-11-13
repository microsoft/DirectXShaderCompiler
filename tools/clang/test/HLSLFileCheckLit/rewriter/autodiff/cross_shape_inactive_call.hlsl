// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/cross_shape_inactive_call_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// Inactive calls in direct reverse mode must rebuild their arguments from the
// typed graph; copying source text would leave a reference to erased `scaled`.

// CHECK: float4 __dxc_ad_primal = float4((uv.value.x - ::floor((uv.value * 2.F).x)), uv.value.y, 0.F, 1.F);
// CHECK: context.gradients[uv.id] += float2(__dxc_ad_seed.x, 0.0f);
// CHECK: context.gradients[uv.id] += float2(0.0f, __dxc_ad_seed.y);

// At uv=(2.25,3), floor(2*uv.x)=4 is primal-only. The result is
// (-1.75,3,0,1), and seed (2,3,0,0) produces gradient (2,3).
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float -1.750000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 3.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 2.000000e+00
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 3.000000e+00

[[dxc::AutoDiff(bwd)]]
float4 f(float2 uv) {
  float2 scaled = uv * 2.0f;
  float x0 = [[dxc::NoDiff]] floor(scaled.x);
  return float4(uv.x - x0, uv.y, 0.0f, 1.0f);
}
