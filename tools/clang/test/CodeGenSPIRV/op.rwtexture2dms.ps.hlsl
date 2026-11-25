// RUN: %dxc -T ps_6_7 -E PS %s -spirv | FileCheck %s --implicit-check-not="OpCapability ImageMSArray"

// The shader from issue #5244 plus a one-component write, compiled without
// -fcgl so the module is optimized and validated. StorageImageMultisample must
// survive trimming.

// CHECK-DAG: OpCapability StorageImageMultisample
// CHECK-DAG: OpTypeImage %uint 2D 2 0 1 2 Rgba32ui

RWTexture2DMS<uint4, 2> gUav : register(u1);

struct VSOUTPUT
{
    float4 Pos : SV_POSITION;
};

// The plain subscript accesses sample 0, and .sample[1][coord] sample 1. The
// component write replaces .y of the texel written before it.
// CHECK:      [[col:%[0-9]+]] = OpImageRead %v4uint {{%[0-9]+}} [[coord:%[0-9]+]] Sample %uint_0
// CHECK:      OpImageWrite {{%[0-9]+}} [[coord]] [[col]] Sample %uint_1
// CHECK:      [[old:%[0-9]+]] = OpImageRead %v4uint {{%[0-9]+}} [[coord]] Sample %uint_1
// CHECK:      [[new:%[0-9]+]] = OpCompositeInsert %v4uint %uint_7 [[old]] 1
// CHECK:      OpImageWrite {{%[0-9]+}} [[coord]] [[new]] Sample %uint_1
// CHECK:      OpImageRead %v4uint {{%[0-9]+}} [[coord]] Sample %uint_1
float4 PS(VSOUTPUT pin) : SV_Target
{
    uint2 coord = uint2(pin.Pos.xy);
    uint4 col = gUav[coord];

    gUav.sample[1][coord] = col;
    gUav.sample[1][coord].y = 7;
    uint4 outColor = gUav.sample[1][coord];
    return outColor;
}
