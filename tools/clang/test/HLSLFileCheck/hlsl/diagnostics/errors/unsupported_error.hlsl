// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// Regression test for a crash when lowering unsupported intrinsics

// CHECK: error: Unsupported intrinsic

sampler TextureSampler;
float4 main(float2 uv	: UV) : SV_Target { return tex2D(TextureSampler, uv); }