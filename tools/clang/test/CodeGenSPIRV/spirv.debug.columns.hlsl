// RUN: %dxc -T ps_6_0 -E main -spirv -fspv-debug=vulkan-with-source %s | FileCheck %s

#include "spirv.debug.columns.include.hlsli"
#include "spirv.debug.columns.include.hlsli"

#define DEFINE_CB(n)\
cbuffer CB##n : register(b0, space##n) {\
    float4 dummy_1;\
}

DEFINE_CB(1)

float4 main(float4 c : COLOR) : SV_Target0 {
  /* a comment that preprocessing removes */ float v = SCALE_AND_BIAS_THE_VALUE(c.x); float w = v;
  return OUT_COL * w + dummy_1;
}

// Debug columns refer to the original source, including on lines that contain
// a macro expansion or a comment, a cbuffer declared through a macro is located
// at its expansion, and the define-only header included twice gets one
// DebugSource.

// CHECK:     [[inc:%[0-9]+]] = OpString "{{.*}}spirv.debug.columns.include.hlsli"
// CHECK:     [[w:%[0-9]+]] = OpString "w"
// CHECK:     [[v:%[0-9]+]] = OpString "v"
// CHECK:     [[member:%[0-9]+]] = OpString "dummy_1"
// CHECK:     [[type:%[0-9]+]] = OpString "type.CB1"
// CHECK:     DebugSource [[inc]]
// CHECK-NOT: DebugSource [[inc]]
// CHECK:     DebugLocalVariable [[w]] {{%[0-9]+}} {{%[0-9]+}} %uint_14 %uint_93
// CHECK:     DebugLocalVariable [[v]] {{%[0-9]+}} {{%[0-9]+}} %uint_14 %uint_52
// CHECK:     DebugTypeMember [[member]] {{%[0-9]+}} {{%[0-9]+}} %uint_11 %uint_1
// CHECK:     DebugTypeComposite [[type]] %uint_1 {{%[0-9]+}} %uint_11 %uint_1
