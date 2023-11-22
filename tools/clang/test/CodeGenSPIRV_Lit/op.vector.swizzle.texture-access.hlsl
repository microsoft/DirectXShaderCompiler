// RUN: %dxc -T ps_6_0 -E main

Texture2D<float4> myTexture;

float4 main(in float4 pos : SV_Position) : SV_Target0
{
    int2 coord = (int2)pos.xy;

// CHECK:        [[img:%\d+]] = OpLoad %type_2d_image %myTexture
// CHECK-NEXT: [[fetch:%\d+]] = OpImageFetch %v4float [[img]] {{%\d+}} Lod %uint_0
// CHECK-NEXT:   [[val:%\d+]] = OpCompositeExtract %float [[fetch]] 2
// CHECK-NEXT:                  OpStore %a [[val]]
    float  a = myTexture[coord].z;

// CHECK:        [[img:%\d+]] = OpLoad %type_2d_image %myTexture
// CHECK-NEXT: [[fetch:%\d+]] = OpImageFetch %v4float [[img]] {{%\d+}} Lod %uint_0
// CHECK-NEXT:   [[val:%\d+]] = OpVectorShuffle %v2float [[fetch]] [[fetch]] 1 0
// CHECK-NEXT:                  OpStore %b [[val]]
    float2 b = myTexture[coord].yx;

// CHECK:        [[img:%\d+]] = OpLoad %type_2d_image %myTexture
// CHECK-NEXT: [[fetch:%\d+]] = OpImageFetch %v4float [[img]] {{%\d+}} Lod %uint_0
// CHECK-NEXT:   [[val:%\d+]] = OpVectorShuffle %v3float [[fetch]] [[fetch]] 2 3 0
// CHECK-NEXT:       OpStore %c [[val]]
    float3 c = myTexture[coord].zwx;

// CHECK:        [[img:%\d+]] = OpLoad %type_2d_image %myTexture
// CHECK-NEXT: [[fetch:%\d+]] = OpImageFetch %v4float [[img]] {{%\d+}} Lod %uint_0
// CHECK-NEXT:                  OpStore %d [[fetch]]
    float4 d = myTexture[coord].xyzw;

    return float4(c, a) + d + float4(b, b);
}
