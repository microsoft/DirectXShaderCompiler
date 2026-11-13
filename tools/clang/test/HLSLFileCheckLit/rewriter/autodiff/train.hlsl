// RUN: %dxr -generate-differentials %S/Inputs/train.hlsl > %t.gen.hlsl
// RUN: FileCheck %s --check-prefix=GEN < %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/train_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T ps_6_0 -E fragmentMain -Fo %t.primal.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.primal.dxil | FileCheck %s --check-prefix=PRIMAL
// RUN: %dxc -T ps_6_0 -E diffFragmentMain -Fo %t.diff.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.diff.dxil | FileCheck %s --check-prefix=DIFF

// The rendering path retains the hardware sample. Differentiation replays the
// software reference implementation and routes texture cotangents to four
// explicit LoadTexel sinks per bilinear sample.

// GEN-NOT: _Static_assert(false
// GEN-NOT: GradientContext<DifferentiableTexture>
// GEN: static float4 sample_reference_impl(inout GradientContext<float2> context, ::DifferentiableTexture value
// GEN: ((::Texture2D)(value.texture)).GetDimensions(0, width, height, levels);
// GEN-COUNT-4: this.bwd_LoadTexel(
// GEN: ::user::ad::bwd::DifferentiableTexture::sample_reference_impl(
// GEN-COUNT-2: ::user::ad::bwd::shadeFragment(
// GEN: float3 loss(inout GradientContext<float3> context, float2 uv, float4 screenPosition, float3 __dxc_ad_seed)

// PRIMAL: call %dx.types.ResRet.f32 @dx.op.sample.f32

// Each loss term differentiates two mip samples, each mip sample touches four
// texels, and every texel updates four fixed-point lanes: 2 * 2 * 4 * 4 = 64.
// DIFF: call %dx.types.ResRet.f32 @dx.op.sample.f32
// DIFF: call %dx.types.ResRet.f32 @dx.op.textureLoad.f32
// DIFF-COUNT-64: call i32 @dx.op.atomicBinOp.i32
