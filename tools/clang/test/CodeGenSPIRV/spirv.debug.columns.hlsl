// RUN: %dxc -T ps_6_0 -E main -spirv -fspv-debug=vulkan-with-source %s | FileCheck %s

#include "spirv.debug.columns.include.hlsli"
#include "spirv.debug.columns.include.hlsli"

float4 main(float4 c : COLOR) : SV_Target0 {
  /* a comment that preprocessing removes */ float v = SCALE_AND_BIAS_THE_VALUE(c.x); float w = v;
  return OUT_COL * w;
}

// Debug columns refer to the original source, including on lines that contain
// a macro expansion or a comment, and the guarded define-only header gets one
// DebugSource.

// CHECK:     [[inc:%[0-9]+]] = OpString "{{.*}}spirv.debug.columns.include.hlsli"
// CHECK:     [[w:%[0-9]+]] = OpString "w"
// CHECK:     [[v:%[0-9]+]] = OpString "v"
// CHECK:     DebugSource [[inc]]
// CHECK-NOT: DebugSource [[inc]]
// CHECK:     DebugLocalVariable [[w]] {{%[0-9]+}} {{%[0-9]+}} %uint_7 %uint_93
// CHECK:     DebugLocalVariable [[v]] {{%[0-9]+}} {{%[0-9]+}} %uint_7 %uint_52
