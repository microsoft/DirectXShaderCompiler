// RUN: %dxc -T ps_6_0 -E main -Od -spirv -fspv-debug=vulkan-with-source %s | FileCheck %s

// Columns of code that comes from a macro expansion must stay inside the line
// that invokes the macro. Otherwise spirv-val rejects the module. Each check
// below names the invocation, its length and the last valid column, which is
// the length plus one.

// FUNC(1) has 7 characters.
// CHECK: DebugLexicalBlock {{%[0-9]+}} {{%uint_[0-9]+}} %uint_8
// CHECK: DebugLocalVariable {{%[0-9]+}} {{%[0-9]+}} {{%[0-9]+}} {{%uint_[0-9]+}} %uint_8

// "  BLOCK(1)" has 10 characters.
// CHECK: DebugLexicalBlock {{%[0-9]+}} {{%uint_[0-9]+}} %uint_11

// GLOBAL(1) has 9 characters.
// CHECK: DebugGlobalVariable {{%[0-9]+}} {{%[0-9]+}} {{%[0-9]+}} {{%uint_[0-9]+}} %uint_10

// DEFINE_CB(1) has 12 characters.
// CHECK: DebugTypeMember {{%[0-9]+}} {{%[0-9]+}} {{%[0-9]+}} {{%uint_[0-9]+}} %uint_13

// "  float3 d = normalize(LIGHT - p);" has 34 characters.
// CHECK: DebugLine {{%[0-9]+}} {{%uint_[0-9]+}} {{%uint_[0-9]+}} %uint_24 %uint_35

#define DEFINE_CB(n)\
cbuffer CB##n : register(b0, space##n) {\
  float4 dummy_1;\
}

#define LIGHT float3(1.0, 2.0, 3.0) + float3(4.0, 5.0, 6.0) + float3(7.0, 8.0, 9.0)
#define GLOBAL(n) static float4 g_##n = float4(1.0, 2.0, 3.0, 4.0) + float4(5.0, 6.0, 7.0, 8.0);
#define FUNC(n) float4 fn_##n(float4 v) { return v + v + v + v + v + v + v + v; }
#define LOCAL(n) float4 loc_##n = float4(1.0, 2.0, 3.0, 4.0) + float4(5.0, 6.0, 7.0, 8.0);
#define BLOCK(n) if (p.x > 0.0) { p.y += 1.0; p.z += 2.0; p.x += 3.0; }

DEFINE_CB(1)
GLOBAL(1)
FUNC(1)

float4 main(float3 p : P) : SV_Target {
  float3 d = normalize(LIGHT - p);
  LOCAL(1)
  BLOCK(1)
  return float4(d, 1) + dummy_1 + g_1 + fn_1(loc_1);
}
