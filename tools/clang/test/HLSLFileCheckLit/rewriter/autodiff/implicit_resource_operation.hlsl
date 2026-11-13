// RUN: %dxr -generate-differentials %s | FileCheck %s

// CHECK: _Static_assert(false, "auto-diff cannot generate backward-mode for 'sample': resource operation 'SampleLevel' requires an associated custom backward derivative");

Texture2D<float> source : register(t0);
SamplerState sourceSampler : register(s0);

[[dxc::AutoDiff(bwd)]]
float sample(float2 uv) { return source.SampleLevel(sourceSampler, uv, 0.0f); }
