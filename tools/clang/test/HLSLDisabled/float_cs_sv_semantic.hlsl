// RUN: %dxc -E main -T cs_6_2 %s | FileCheck %s
// TODO: No check lines found, we should update this

// Repro of GitHub #2299

[numthreads(1, 1, 1)]
void main(float3 tid : SV_DispatchThreadID) {}