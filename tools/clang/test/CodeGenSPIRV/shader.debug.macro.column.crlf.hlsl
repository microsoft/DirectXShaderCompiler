// RUN: %dxc -T ps_6_0 -E main -Od -spirv -fspv-debug=vulkan-with-source %s | FileCheck %s

// Columns must be clamped against line lengths measured as spirv-val measures them.
// The lines after this comment end in CRLF and the last line has no terminator.
// Keep these line endings.

// DEFINE_CB(1) has 12 characters. The CR of its CRLF is not part of the line.
// CHECK: DebugTypeMember {{%[0-9]+}} {{%[0-9]+}} {{%[0-9]+}} {{%uint_[0-9]+}} %uint_13

// The last line has 91 characters and no terminator.
// CHECK: DebugLine {{%[0-9]+}} {{%uint_[0-9]+}} {{%uint_[0-9]+}} {{%uint_[0-9]+}} %uint_92

#define DEFINE_CB(n)\
cbuffer CB##n : register(b0, space##n) {\
  float4 dummy_1;\
}
#define LIGHT float3(1.0, 2.0, 3.0) + float3(4.0, 5.0, 6.0) + float3(7.0, 8.0, 9.0)
#define FUNC(n) float4 fn_##n(float4 v) { return v + v + v + v + v + v + v + v; }
DEFINE_CB(1)
FUNC(1)
float4 main(float3 p : P) : SV_Target { return float4(normalize(LIGHT - p), 1) + dummy_1; }