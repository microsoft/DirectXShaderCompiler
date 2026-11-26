// RUN: %dxc -T ps_6_0 -E main %s | FileCheck %s -check-prefixes=CHECK,DEFAULT
// RUN: %dxc -T ps_6_0 -E main -opt-enable dxil-trim-cbuffer %s | FileCheck %s -check-prefixes=CHECK,TRIM
// RUN: %dxc -T ps_6_6 -E main -opt-enable dxil-trim-cbuffer %s | FileCheck %s -check-prefixes=CHECK,TRIM

// Tests for the opt-in dxil-trim-cbuffer pass: with -opt-enable, unused
// cbuffer members are dropped and surviving members are re-packed to compact
// offsets (the cbuffer size in dx.resources shrinks accordingly). Without
// the flag, the cbuffer layout is left untouched.

// CHECK-LABEL: ; cbuffer CB
// CHECK: struct CB
// TRIM: gUsed1
// TRIM-NOT: gUnused
// TRIM: gUsed2
// TRIM: Size: {{ *}}32

// DEFAULT: gUsed1
// DEFAULT: gUnused1
// DEFAULT: gUsed2
// DEFAULT: gUnused2
// DEFAULT: Size: {{ *}}52

cbuffer CB : register(b0) {
  float4 gUsed1;   // used
  float3 gUnused1; // never used
  float4 gUsed2;   // used
  float  gUnused2; // never used
};

float4 main() : SV_Target {
  return gUsed1 + gUsed2;
}
