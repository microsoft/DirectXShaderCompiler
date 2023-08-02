// RUN: %dxc -T vs_6_0 -E main

// CHECK: [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

uint4 main(uint reference : REF, uint2 source :SOURCE, uint4 accum : ACCUM) : MSAD_RESULT
{

// CHECK:          [[ref:%\d+]] = OpLoad %uint %reference
// CHECK-NEXT:     [[src:%\d+]] = OpLoad %v2uint %source
// CHECK-NEXT:   [[accum:%\d+]] = OpLoad %v4uint %accum
// CHECK-NEXT:    [[src0:%\d+]] = OpCompositeExtract %uint [[src]] 0
// CHECK-NEXT:  [[src0s8:%\d+]] = OpShiftLeftLogical %uint [[src0]] %uint_8
// CHECK-NEXT: [[src0s16:%\d+]] = OpShiftLeftLogical %uint [[src0]] %uint_16
// CHECK-NEXT: [[src0s24:%\d+]] = OpShiftLeftLogical %uint [[src0]] %uint_24
// CHECK-NEXT:    [[src1:%\d+]] = OpCompositeExtract %uint [[src]] 1
// CHECK-NEXT:    [[bfi0:%\d+]] = OpBitFieldInsert %uint [[src0s8]] [[src1]] %uint_24 %uint_8
// CHECK-NEXT:    [[bfi1:%\d+]] = OpBitFieldInsert %uint [[src0s16]] [[src1]] %uint_16 %uint_16
// CHECK-NEXT:    [[bfi2:%\d+]] = OpBitFieldInsert %uint [[src0s24]] [[src1]] %uint_8 %uint_24
// CHECK-NEXT:  [[accum0:%\d+]] = OpCompositeExtract %uint [[accum]] 0
// CHECK-NEXT:  [[accum1:%\d+]] = OpCompositeExtract %uint [[accum]] 1
// CHECK-NEXT:  [[accum2:%\d+]] = OpCompositeExtract %uint [[accum]] 2
// CHECK-NEXT:  [[accum3:%\d+]] = OpCompositeExtract %uint [[accum]] 3

// Now perforoming MSAD four times

// CHECK-NEXT:           [[refByte0:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_0 %uint_8
// CHECK-NEXT:        [[intRefByte0:%\d+]] = OpBitcast %int [[refByte0]]
// CHECK-NEXT:     [[isRefByte0Zero:%\d+]] = OpIEqual %bool [[refByte0]] %uint_0
// CHECK-NEXT:           [[refByte1:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_8 %uint_8
// CHECK-NEXT:        [[intRefByte1:%\d+]] = OpBitcast %int [[refByte1]]
// CHECK-NEXT:     [[isRefByte1Zero:%\d+]] = OpIEqual %bool [[refByte1]] %uint_0
// CHECK-NEXT:           [[refByte2:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_16 %uint_8
// CHECK-NEXT:        [[intRefByte2:%\d+]] = OpBitcast %int [[refByte2]]
// CHECK-NEXT:     [[isRefByte2Zero:%\d+]] = OpIEqual %bool [[refByte2]] %uint_0
// CHECK-NEXT:           [[refByte3:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_24 %uint_8
// CHECK-NEXT:        [[intRefByte3:%\d+]] = OpBitcast %int [[refByte3]]
// CHECK-NEXT:     [[isRefByte3Zero:%\d+]] = OpIEqual %bool [[refByte3]] %uint_0

// MSAD 0 Byte 0
// CHECK-NEXT:          [[src0Byte0:%\d+]] = OpBitFieldUExtract %uint [[src0]] %uint_0 %uint_8
// CHECK-NEXT:       [[intSrc0Byte0:%\d+]] = OpBitcast %int [[src0Byte0]]
// CHECK-NEXT:               [[sub0:%\d+]] = OpISub %int [[intRefByte0]] [[intSrc0Byte0]]
// CHECK-NEXT:            [[absSub0:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub0]]
// CHECK-NEXT:        [[uintAbsSub0:%\d+]] = OpBitcast %uint [[absSub0]]
// CHECK-NEXT:              [[diff0:%\d+]] = OpSelect %uint [[isRefByte0Zero]] %uint_0 [[uintAbsSub0]]
// CHECK-NEXT:    [[accum0PlusDiff0:%\d+]] = OpIAdd %uint [[accum0]] [[diff0]]

// MSAD 0 Byte 1
// CHECK-NEXT:          [[src0Byte1:%\d+]] = OpBitFieldUExtract %uint [[src0]] %uint_8 %uint_8
// CHECK-NEXT:       [[intSrc0Byte1:%\d+]] = OpBitcast %int [[src0Byte1]]
// CHECK-NEXT:               [[sub1:%\d+]] = OpISub %int [[intRefByte1]] [[intSrc0Byte1]]
// CHECK-NEXT:            [[absSub1:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub1]]
// CHECK-NEXT:        [[uintAbsSub1:%\d+]] = OpBitcast %uint [[absSub1]]
// CHECK-NEXT:              [[diff1:%\d+]] = OpSelect %uint [[isRefByte1Zero]] %uint_0 [[uintAbsSub1]]
// CHECK-NEXT:   [[accum0PlusDiff01:%\d+]] = OpIAdd %uint [[accum0PlusDiff0]] [[diff1]]

// MSAD 0 Byte 2
// CHECK-NEXT:          [[src0Byte2:%\d+]] = OpBitFieldUExtract %uint [[src0]] %uint_16 %uint_8
// CHECK-NEXT:       [[intSrc0Byte2:%\d+]] = OpBitcast %int [[src0Byte2]]
// CHECK-NEXT:               [[sub2:%\d+]] = OpISub %int [[intRefByte2]] [[intSrc0Byte2]]
// CHECK-NEXT:            [[absSub2:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub2]]
// CHECK-NEXT:        [[uintAbsSub2:%\d+]] = OpBitcast %uint [[absSub2]]
// CHECK-NEXT:              [[diff2:%\d+]] = OpSelect %uint [[isRefByte2Zero]] %uint_0 [[uintAbsSub2]]
// CHECK-NEXT:  [[accum0PlusDiff012:%\d+]] = OpIAdd %uint [[accum0PlusDiff01]] [[diff2]]

// MSAD 0 Byte 3
// CHECK-NEXT:          [[src0Byte3:%\d+]] = OpBitFieldUExtract %uint [[src0]] %uint_24 %uint_8
// CHECK-NEXT:       [[intSrc0Byte3:%\d+]] = OpBitcast %int [[src0Byte3]]
// CHECK-NEXT:               [[sub3:%\d+]] = OpISub %int [[intRefByte3]] [[intSrc0Byte3]]
// CHECK-NEXT:            [[absSub3:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub3]]
// CHECK-NEXT:        [[uintAbsSub3:%\d+]] = OpBitcast %uint [[absSub3]]
// CHECK-NEXT:              [[diff3:%\d+]] = OpSelect %uint [[isRefByte3Zero]] %uint_0 [[uintAbsSub3]]
// CHECK-NEXT: [[accum0PlusDiff0123:%\d+]] = OpIAdd %uint [[accum0PlusDiff012]] [[diff3]]


// MSAD 1 Byte 0
// CHECK-NEXT:          [[src1Byte0:%\d+]] = OpBitFieldUExtract %uint [[bfi0]] %uint_0 %uint_8
// CHECK-NEXT:       [[intSrc1Byte0:%\d+]] = OpBitcast %int [[src1Byte0]]
// CHECK-NEXT:               [[sub0:%\d+]] = OpISub %int [[intRefByte0]] [[intSrc1Byte0]]
// CHECK-NEXT:            [[absSub0:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub0]]
// CHECK-NEXT:        [[uintAbsSub0:%\d+]] = OpBitcast %uint [[absSub0]]
// CHECK-NEXT:              [[diff0:%\d+]] = OpSelect %uint [[isRefByte0Zero]] %uint_0 [[uintAbsSub0]]
// CHECK-NEXT:    [[accum1PlusDiff0:%\d+]] = OpIAdd %uint [[accum1]] [[diff0]]

// MSAD 1 Byte 1
// CHECK-NEXT:          [[src1Byte1:%\d+]] = OpBitFieldUExtract %uint [[bfi0]] %uint_8 %uint_8
// CHECK-NEXT:       [[intSrc1Byte1:%\d+]] = OpBitcast %int [[src1Byte1]]
// CHECK-NEXT:               [[sub1:%\d+]] = OpISub %int [[intRefByte1]] [[intSrc1Byte1]]
// CHECK-NEXT:            [[absSub1:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub1]]
// CHECK-NEXT:        [[uintAbsSub1:%\d+]] = OpBitcast %uint [[absSub1]]
// CHECK-NEXT:              [[diff1:%\d+]] = OpSelect %uint [[isRefByte1Zero]] %uint_0 [[uintAbsSub1]]
// CHECK-NEXT:   [[accum1PlusDiff01:%\d+]] = OpIAdd %uint [[accum1PlusDiff0]] [[diff1]]

// MSAD 1 Byte 2
// CHECK-NEXT:          [[src1Byte2:%\d+]] = OpBitFieldUExtract %uint [[bfi0]] %uint_16 %uint_8
// CHECK-NEXT:       [[intSrc1Byte2:%\d+]] = OpBitcast %int [[src1Byte2]]
// CHECK-NEXT:               [[sub2:%\d+]] = OpISub %int [[intRefByte2]] [[intSrc1Byte2]]
// CHECK-NEXT:            [[absSub2:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub2]]
// CHECK-NEXT:        [[uintAbsSub2:%\d+]] = OpBitcast %uint [[absSub2]]
// CHECK-NEXT:              [[diff2:%\d+]] = OpSelect %uint [[isRefByte2Zero]] %uint_0 [[uintAbsSub2]]
// CHECK-NEXT:  [[accum1PlusDiff012:%\d+]] = OpIAdd %uint [[accum1PlusDiff01]] [[diff2]]

// MSAD 1 Byte 3
// CHECK-NEXT:          [[src1Byte3:%\d+]] = OpBitFieldUExtract %uint [[bfi0]] %uint_24 %uint_8
// CHECK-NEXT:       [[intSrc1Byte3:%\d+]] = OpBitcast %int [[src1Byte3]]
// CHECK-NEXT:               [[sub3:%\d+]] = OpISub %int [[intRefByte3]] [[intSrc1Byte3]]
// CHECK-NEXT:            [[absSub3:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub3]]
// CHECK-NEXT:        [[uintAbsSub3:%\d+]] = OpBitcast %uint [[absSub3]]
// CHECK-NEXT:              [[diff3:%\d+]] = OpSelect %uint [[isRefByte3Zero]] %uint_0 [[uintAbsSub3]]
// CHECK-NEXT: [[accum1PlusDiff0123:%\d+]] = OpIAdd %uint [[accum1PlusDiff012]] [[diff3]]


// MSAD 2 Byte 0
// CHECK-NEXT:          [[src2Byte0:%\d+]] = OpBitFieldUExtract %uint [[bfi1]] %uint_0 %uint_8
// CHECK-NEXT:       [[intSrc2Byte0:%\d+]] = OpBitcast %int [[src2Byte0]]
// CHECK-NEXT:               [[sub0:%\d+]] = OpISub %int [[intRefByte0]] [[intSrc2Byte0]]
// CHECK-NEXT:            [[absSub0:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub0]]
// CHECK-NEXT:        [[uintAbsSub0:%\d+]] = OpBitcast %uint [[absSub0]]
// CHECK-NEXT:              [[diff0:%\d+]] = OpSelect %uint [[isRefByte0Zero]] %uint_0 [[uintAbsSub0]]
// CHECK-NEXT:    [[accum2PlusDiff0:%\d+]] = OpIAdd %uint [[accum2]] [[diff0]]

// MSAD 2 Byte 1
// CHECK-NEXT:          [[src2Byte1:%\d+]] = OpBitFieldUExtract %uint [[bfi1]] %uint_8 %uint_8
// CHECK-NEXT:       [[intSrc2Byte1:%\d+]] = OpBitcast %int [[src2Byte1]]
// CHECK-NEXT:               [[sub1:%\d+]] = OpISub %int [[intRefByte1]] [[intSrc2Byte1]]
// CHECK-NEXT:            [[absSub1:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub1]]
// CHECK-NEXT:        [[uintAbsSub1:%\d+]] = OpBitcast %uint [[absSub1]]
// CHECK-NEXT:              [[diff1:%\d+]] = OpSelect %uint [[isRefByte1Zero]] %uint_0 [[uintAbsSub1]]
// CHECK-NEXT:   [[accum2PlusDiff01:%\d+]] = OpIAdd %uint [[accum2PlusDiff0]] [[diff1]]

// MSAD 2 Byte 2
// CHECK-NEXT:          [[src2Byte2:%\d+]] = OpBitFieldUExtract %uint [[bfi1]] %uint_16 %uint_8
// CHECK-NEXT:       [[intSrc2Byte2:%\d+]] = OpBitcast %int [[src2Byte2]]
// CHECK-NEXT:               [[sub2:%\d+]] = OpISub %int [[intRefByte2]] [[intSrc2Byte2]]
// CHECK-NEXT:            [[absSub2:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub2]]
// CHECK-NEXT:        [[uintAbsSub2:%\d+]] = OpBitcast %uint [[absSub2]]
// CHECK-NEXT:              [[diff2:%\d+]] = OpSelect %uint [[isRefByte2Zero]] %uint_0 [[uintAbsSub2]]
// CHECK-NEXT:  [[accum2PlusDiff012:%\d+]] = OpIAdd %uint [[accum2PlusDiff01]] [[diff2]]

// MSAD 2 Byte 3
// CHECK-NEXT:          [[src2Byte3:%\d+]] = OpBitFieldUExtract %uint [[bfi1]] %uint_24 %uint_8
// CHECK-NEXT:       [[intSrc2Byte3:%\d+]] = OpBitcast %int [[src2Byte3]]
// CHECK-NEXT:               [[sub3:%\d+]] = OpISub %int [[intRefByte3]] [[intSrc2Byte3]]
// CHECK-NEXT:            [[absSub3:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub3]]
// CHECK-NEXT:        [[uintAbsSub3:%\d+]] = OpBitcast %uint [[absSub3]]
// CHECK-NEXT:              [[diff3:%\d+]] = OpSelect %uint [[isRefByte3Zero]] %uint_0 [[uintAbsSub3]]
// CHECK-NEXT: [[accum2PlusDiff0123:%\d+]] = OpIAdd %uint [[accum2PlusDiff012]] [[diff3]]


// MSAD 3 Byte 0
// CHECK-NEXT:          [[src3Byte0:%\d+]] = OpBitFieldUExtract %uint [[bfi2]] %uint_0 %uint_8
// CHECK-NEXT:       [[intSrc3Byte0:%\d+]] = OpBitcast %int [[src3Byte0]]
// CHECK-NEXT:               [[sub0:%\d+]] = OpISub %int [[intRefByte0]] [[intSrc3Byte0]]
// CHECK-NEXT:            [[absSub0:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub0]]
// CHECK-NEXT:        [[uintAbsSub0:%\d+]] = OpBitcast %uint [[absSub0]]
// CHECK-NEXT:              [[diff0:%\d+]] = OpSelect %uint [[isRefByte0Zero]] %uint_0 [[uintAbsSub0]]
// CHECK-NEXT:    [[accum3PlusDiff0:%\d+]] = OpIAdd %uint [[accum3]] [[diff0]]

// MSAD 3 Byte 1
// CHECK-NEXT:          [[src3Byte1:%\d+]] = OpBitFieldUExtract %uint [[bfi2]] %uint_8 %uint_8
// CHECK-NEXT:       [[intSrc3Byte1:%\d+]] = OpBitcast %int [[src3Byte1]]
// CHECK-NEXT:               [[sub1:%\d+]] = OpISub %int [[intRefByte1]] [[intSrc3Byte1]]
// CHECK-NEXT:            [[absSub1:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub1]]
// CHECK-NEXT:        [[uintAbsSub1:%\d+]] = OpBitcast %uint [[absSub1]]
// CHECK-NEXT:              [[diff1:%\d+]] = OpSelect %uint [[isRefByte1Zero]] %uint_0 [[uintAbsSub1]]
// CHECK-NEXT:   [[accum3PlusDiff01:%\d+]] = OpIAdd %uint [[accum3PlusDiff0]] [[diff1]]

// MSAD 3 Byte 2
// CHECK-NEXT:          [[src3Byte2:%\d+]] = OpBitFieldUExtract %uint [[bfi2]] %uint_16 %uint_8
// CHECK-NEXT:       [[intSrc3Byte2:%\d+]] = OpBitcast %int [[src3Byte2]]
// CHECK-NEXT:               [[sub2:%\d+]] = OpISub %int [[intRefByte2]] [[intSrc3Byte2]]
// CHECK-NEXT:            [[absSub2:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub2]]
// CHECK-NEXT:        [[uintAbsSub2:%\d+]] = OpBitcast %uint [[absSub2]]
// CHECK-NEXT:              [[diff2:%\d+]] = OpSelect %uint [[isRefByte2Zero]] %uint_0 [[uintAbsSub2]]
// CHECK-NEXT:  [[accum3PlusDiff012:%\d+]] = OpIAdd %uint [[accum3PlusDiff01]] [[diff2]]

// MSAD 3 Byte 3
// CHECK-NEXT:          [[src3Byte3:%\d+]] = OpBitFieldUExtract %uint [[bfi2]] %uint_24 %uint_8
// CHECK-NEXT:       [[intSrc3Byte3:%\d+]] = OpBitcast %int [[src3Byte3]]
// CHECK-NEXT:               [[sub3:%\d+]] = OpISub %int [[intRefByte3]] [[intSrc3Byte3]]
// CHECK-NEXT:            [[absSub3:%\d+]] = OpExtInst %int [[glsl]] SAbs [[sub3]]
// CHECK-NEXT:        [[uintAbsSub3:%\d+]] = OpBitcast %uint [[absSub3]]
// CHECK-NEXT:              [[diff3:%\d+]] = OpSelect %uint [[isRefByte3Zero]] %uint_0 [[uintAbsSub3]]
// CHECK-NEXT: [[accum3PlusDiff0123:%\d+]] = OpIAdd %uint [[accum3PlusDiff012]] [[diff3]]

// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %v4uint [[accum0PlusDiff0123]] [[accum1PlusDiff0123]] [[accum2PlusDiff0123]] [[accum3PlusDiff0123]]

  uint4 result = msad4(reference, source, accum);
  return result;
}
