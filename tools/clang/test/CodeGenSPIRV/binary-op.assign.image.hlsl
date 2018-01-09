// Run: %dxc -T vs_6_0 -E main

RWBuffer<uint>     MyRWBuffer;
RWTexture2D<int>   MyRWTexture;
RWBuffer<uint4>    MyRWBuffer4;
RWTexture3D<uint3> MyRWTexture3;

// CHECK:      [[c25:%\d+]] = OpConstantComposite %v3uint %uint_25 %uint_25 %uint_25
// CHECK:      [[c26:%\d+]] = OpConstantComposite %v4uint %uint_26 %uint_26 %uint_26 %uint_26
// CHECK:      [[c28:%\d+]] = OpConstantComposite %v2uint %uint_28 %uint_28

void main() {
    // <scalar-value>.x
// CHECK:      [[buf:%\d+]] = OpLoad %type_buffer_image %MyRWBuffer
// CHECK-NEXT:                OpImageWrite [[buf]] %uint_1 %uint_15
    MyRWBuffer[1].x = 15;

    // <scalar-value>.x
// CHECK:      [[tex:%\d+]] = OpLoad %type_2d_image %MyRWTexture
// CHECK-NEXT:                OpImageWrite [[tex]] {{%\d+}} %int_16
    MyRWTexture[uint2(2, 3)].x = 16;

    // Out-of-order swizzling
// CHECK:      [[buf:%\d+]] = OpLoad %type_buffer_image_0 %MyRWBuffer4
// CHECK-NEXT: [[old:%\d+]] = OpImageRead %v4uint [[buf]] %uint_4 None
// CHECK-NEXT: [[new:%\d+]] = OpVectorShuffle %v4uint [[old]] [[c25]] 6 1 4 5
// CHECK-NEXT: [[buf:%\d+]] = OpLoad %type_buffer_image_0 %MyRWBuffer4
// CHECK-NEXT:                OpImageWrite [[buf]] %uint_4 [[new]]
    MyRWBuffer4[4].zwx = 25;

    // Swizzling resulting in the original vector
// CHECK:      [[buf:%\d+]] = OpLoad %type_buffer_image_0 %MyRWBuffer4
// CHECK-NEXT:                OpImageWrite [[buf]] %uint_4 [[c26]]
    MyRWBuffer4[4].xyzw = 26;

    // Selecting one element
// CHECK:      [[tex:%\d+]] = OpLoad %type_3d_image %MyRWTexture3
// CHECK-NEXT: [[old:%\d+]] = OpImageRead %v4uint [[tex]] {{%\d+}} None
// CHECK-NEXT:  [[v3:%\d+]] = OpVectorShuffle %v3uint [[old]] [[old]] 0 1 2
// CHECK-NEXT: [[new:%\d+]] = OpCompositeInsert %v3uint %uint_27 [[v3]] 1
// CHECK-NEXT: [[tex:%\d+]] = OpLoad %type_3d_image %MyRWTexture3
// CHECK-NEXT:                OpImageWrite [[tex]] {{%\d+}} [[new]]
    MyRWTexture3[uint3(5, 6, 7)].y = 27;

    // In-order swizzling
// CHECK:      [[tex:%\d+]] = OpLoad %type_3d_image %MyRWTexture3
// CHECK-NEXT: [[old:%\d+]] = OpImageRead %v4uint [[tex]] {{%\d+}} None
// CHECK-NEXT:  [[v3:%\d+]] = OpVectorShuffle %v3uint [[old]] [[old]] 0 1 2
// CHECK-NEXT: [[new:%\d+]] = OpVectorShuffle %v3uint [[v3]] [[c28]] 3 4 2
// CHECK-NEXT: [[tex:%\d+]] = OpLoad %type_3d_image %MyRWTexture3
// CHECK-NEXT:                OpImageWrite [[tex]] {{%\d+}} [[new]]
    MyRWTexture3[uint3(8, 9, 10)].xy = 28;

// CHECK:      [[buf:%\d+]] = OpLoad %type_buffer_image %MyRWBuffer
// CHECK-NEXT: [[old:%\d+]] = OpImageRead %v4uint [[buf]] %uint_11 None
// CHECK-NEXT: [[val:%\d+]] = OpCompositeExtract %uint [[old]] 0
// CHECK-NEXT: [[add:%\d+]] = OpIAdd %uint [[val]] %uint_30
// CHECK-NEXT: [[buf:%\d+]] = OpLoad %type_buffer_image %MyRWBuffer
// CHECK-NEXT:                OpImageWrite [[buf]] %uint_11 [[add]]
    MyRWBuffer[11] += 30;

// CHECK:      [[tex:%\d+]] = OpLoad %type_2d_image %MyRWTexture
// CHECK-NEXT: [[old:%\d+]] = OpImageRead %v4int [[tex]] {{%\d+}} None
// CHECK-NEXT: [[val:%\d+]] = OpCompositeExtract %int [[old]] 0
// CHECK-NEXT: [[mul:%\d+]] = OpIMul %int [[val]] %int_31
// CHECK-NEXT: [[tex:%\d+]] = OpLoad %type_2d_image %MyRWTexture
// CHECK-NEXT:                OpImageWrite [[tex]] {{%\d+}} [[mul]]
    MyRWTexture[uint2(12, 13)] *= 31;
}
