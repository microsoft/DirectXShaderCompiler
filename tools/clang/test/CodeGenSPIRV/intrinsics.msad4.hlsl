// Run: %dxc -T vs_6_0 -E main

// CHECK: [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

uint4 main(uint reference : REF, uint2 source :SOURCE, uint4 accum : ACCUM) : MSAD_RESULT
{

// CHECK:      [[ref:%\d+]] = OpLoad %uint %reference
// CHECK-NEXT: [[src:%\d+]] = OpLoad %v2uint %source
// CHECK-NEXT: [[accum:%\d+]] = OpLoad %v4uint %accum
// CHECK-NEXT: [[src0:%\d+]] = OpCompositeExtract %uint [[src]] 0
// CHECK-NEXT: [[src0s8:%\d+]] = OpShiftLeftLogical %uint [[src0]] %uint_8
// CHECK-NEXT: [[src0s16:%\d+]] = OpShiftLeftLogical %uint [[src0]] %uint_16
// CHECK-NEXT: [[src0s24:%\d+]] = OpShiftLeftLogical %uint [[src0]] %uint_24
// CHECK-NEXT: [[src1:%\d+]] = OpCompositeExtract %uint [[src]] 1
// CHECK-NEXT: [[bfi0:%\d+]] = OpBitFieldInsert %uint [[src0s8]] [[src1]] %uint_24 %uint_8
// CHECK-NEXT: [[bfi1:%\d+]] = OpBitFieldInsert %uint [[src0s16]] [[src1]] %uint_16 %uint_16
// CHECK-NEXT: [[bfi2:%\d+]] = OpBitFieldInsert %uint [[src0s24]] [[src1]] %uint_8 %uint_24
// CHECK-NEXT: [[accum0:%\d+]] = OpCompositeExtract %uint [[accum]] 0
// CHECK-NEXT: [[accum1:%\d+]] = OpCompositeExtract %uint [[accum]] 1
// CHECK-NEXT: [[accum2:%\d+]] = OpCompositeExtract %uint [[accum]] 2
// CHECK-NEXT: [[accum3:%\d+]] = OpCompositeExtract %uint [[accum]] 3

// Now perforoming MSAD four times

// MSAD 0

// MSAD0 BYTE 0
//
// CHECK-NEXT:           [[refByte0:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_0 %uint_8
// CHECK-NEXT:          [[src0Byte0:%\d+]] = OpBitFieldUExtract %uint [[src0]] %uint_0 %uint_8
// CHECK-NEXT:          [[iRefByte0:%\d+]] = OpBitcast %int [[refByte0]]
// CHECK-NEXT:         [[iSrc0Byte0:%\d+]] = OpBitcast %int [[src0Byte0]]
// CHECK-NEXT:              [[isub0:%\d+]] = OpISub %int [[iRefByte0]] [[iSrc0Byte0]]
// CHECK-NEXT:           [[iSub0Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub0]]
// CHECK-NEXT:     [[isRefByte0Zero:%\d+]] = OpIEqual %bool [[refByte0]] %uint_0
// CHECK-NEXT:           [[uSub0Abs:%\d+]] = OpBitcast %uint [[iSub0Abs]]
// CHECK-NEXT:              [[diff0:%\d+]] = OpSelect %uint [[isRefByte0Zero]] %uint_0 [[uSub0Abs]]
// CHECK-NEXT:    [[accum0PlusDiff0:%\d+]] = OpIAdd %uint [[accum0]] [[diff0]]

// MSAD0 BYTE 1
//
// CHECK-NEXT:           [[refByte1:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_8 %uint_8
// CHECK-NEXT:          [[src0Byte1:%\d+]] = OpBitFieldUExtract %uint [[src0]] %uint_8 %uint_8
// CHECK-NEXT:          [[iRefByte1:%\d+]] = OpBitcast %int [[refByte1]]
// CHECK-NEXT:         [[iSrc0Byte1:%\d+]] = OpBitcast %int [[src0Byte1]]
// CHECK-NEXT:              [[isub1:%\d+]] = OpISub %int [[iRefByte1]] [[iSrc0Byte1]]
// CHECK-NEXT:           [[iSub1Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub1]]
// CHECK-NEXT:     [[isRefByte1Zero:%\d+]] = OpIEqual %bool [[refByte1]] %uint_0
// CHECK-NEXT:           [[uSub1Abs:%\d+]] = OpBitcast %uint [[iSub1Abs]]
// CHECK-NEXT:              [[diff1:%\d+]] = OpSelect %uint [[isRefByte1Zero]] %uint_0 [[uSub1Abs]]
// CHECK-NEXT:   [[accum0PlusDiff01:%\d+]] = OpIAdd %uint [[accum0PlusDiff0]] [[diff1]]

// MSAD0 BYTE 2
//
// CHECK-NEXT:           [[refByte2:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_16 %uint_8
// CHECK-NEXT:          [[src0Byte2:%\d+]] = OpBitFieldUExtract %uint [[src0]] %uint_16 %uint_8
// CHECK-NEXT:          [[iRefByte2:%\d+]] = OpBitcast %int [[refByte2]]
// CHECK-NEXT:         [[iSrc0Byte2:%\d+]] = OpBitcast %int [[src0Byte2]]
// CHECK-NEXT:              [[isub2:%\d+]] = OpISub %int [[iRefByte2]] [[iSrc0Byte2]]
// CHECK-NEXT:           [[iSub2Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub2]]
// CHECK-NEXT:     [[isRefByte2Zero:%\d+]] = OpIEqual %bool [[refByte2]] %uint_0
// CHECK-NEXT:           [[uSub2Abs:%\d+]] = OpBitcast %uint [[iSub2Abs]]
// CHECK-NEXT:              [[diff2:%\d+]] = OpSelect %uint [[isRefByte2Zero]] %uint_0 [[uSub2Abs]]
// CHECK-NEXT:  [[accum0PlusDiff012:%\d+]] = OpIAdd %uint [[accum0PlusDiff01]] [[diff2]]

// MSAD0 BYTE 3
//
// CHECK-NEXT:           [[refByte3:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_24 %uint_8
// CHECK-NEXT:          [[src0Byte3:%\d+]] = OpBitFieldUExtract %uint [[src0]] %uint_24 %uint_8
// CHECK-NEXT:          [[iRefByte3:%\d+]] = OpBitcast %int [[refByte3]]
// CHECK-NEXT:         [[iSrc0Byte3:%\d+]] = OpBitcast %int [[src0Byte3]]
// CHECK-NEXT:              [[isub3:%\d+]] = OpISub %int [[iRefByte3]] [[iSrc0Byte3]]
// CHECK-NEXT:           [[iSub3Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub3]]
// CHECK-NEXT:     [[isRefByte3Zero:%\d+]] = OpIEqual %bool [[refByte3]] %uint_0
// CHECK-NEXT:           [[uSub3Abs:%\d+]] = OpBitcast %uint [[iSub3Abs]]
// CHECK-NEXT:              [[diff3:%\d+]] = OpSelect %uint [[isRefByte3Zero]] %uint_0 [[uSub3Abs]]
// CHECK-NEXT: [[accum0PlusDiff0123:%\d+]] = OpIAdd %uint [[accum0PlusDiff012]] [[diff3]]

// MSAD 1

// MSAD1 BYTE 0
//
// CHECK-NEXT:           [[refByte0:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_0 %uint_8
// CHECK-NEXT:          [[src1Byte0:%\d+]] = OpBitFieldUExtract %uint [[bfi0]] %uint_0 %uint_8
// CHECK-NEXT:          [[iRefByte0:%\d+]] = OpBitcast %int [[refByte0]]
// CHECK-NEXT:         [[iSrc1Byte0:%\d+]] = OpBitcast %int [[src1Byte0]]
// CHECK-NEXT:              [[isub0:%\d+]] = OpISub %int [[iRefByte0]] [[iSrc1Byte0]]
// CHECK-NEXT:           [[iSub0Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub0]]
// CHECK-NEXT:     [[isRefByte0Zero:%\d+]] = OpIEqual %bool [[refByte0]] %uint_0
// CHECK-NEXT:           [[uSub0Abs:%\d+]] = OpBitcast %uint [[iSub0Abs]]
// CHECK-NEXT:              [[diff0:%\d+]] = OpSelect %uint [[isRefByte0Zero]] %uint_0 [[uSub0Abs]]
// CHECK-NEXT:    [[accum1PlusDiff0:%\d+]] = OpIAdd %uint [[accum1]] [[diff0]]

// MSAD1 BYTE 1
//
// CHECK-NEXT:           [[refByte1:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_8 %uint_8
// CHECK-NEXT:          [[src1Byte1:%\d+]] = OpBitFieldUExtract %uint [[bfi0]] %uint_8 %uint_8
// CHECK-NEXT:          [[iRefByte1:%\d+]] = OpBitcast %int [[refByte1]]
// CHECK-NEXT:         [[iSrc1Byte1:%\d+]] = OpBitcast %int [[src1Byte1]]
// CHECK-NEXT:              [[isub1:%\d+]] = OpISub %int [[iRefByte1]] [[iSrc1Byte1]]
// CHECK-NEXT:           [[iSub1Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub1]]
// CHECK-NEXT:     [[isRefByte1Zero:%\d+]] = OpIEqual %bool [[refByte1]] %uint_0
// CHECK-NEXT:           [[uSub1Abs:%\d+]] = OpBitcast %uint [[iSub1Abs]]
// CHECK-NEXT:              [[diff1:%\d+]] = OpSelect %uint [[isRefByte1Zero]] %uint_0 [[uSub1Abs]]
// CHECK-NEXT:   [[accum1PlusDiff01:%\d+]] = OpIAdd %uint [[accum1PlusDiff0]] [[diff1]]

// MSAD1 BYTE 2
//
// CHECK-NEXT:           [[refByte2:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_16 %uint_8
// CHECK-NEXT:          [[src1Byte2:%\d+]] = OpBitFieldUExtract %uint [[bfi0]] %uint_16 %uint_8
// CHECK-NEXT:          [[iRefByte2:%\d+]] = OpBitcast %int [[refByte2]]
// CHECK-NEXT:         [[iSrc1Byte2:%\d+]] = OpBitcast %int [[src1Byte2]]
// CHECK-NEXT:              [[isub2:%\d+]] = OpISub %int [[iRefByte2]] [[iSrc1Byte2]]
// CHECK-NEXT:           [[iSub2Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub2]]
// CHECK-NEXT:     [[isRefByte2Zero:%\d+]] = OpIEqual %bool [[refByte2]] %uint_0
// CHECK-NEXT:           [[uSub2Abs:%\d+]] = OpBitcast %uint [[iSub2Abs]]
// CHECK-NEXT:              [[diff2:%\d+]] = OpSelect %uint [[isRefByte2Zero]] %uint_0 [[uSub2Abs]]
// CHECK-NEXT:  [[accum1PlusDiff012:%\d+]] = OpIAdd %uint [[accum1PlusDiff01]] [[diff2]]

// MSAD1 BYTE 3
//
// CHECK-NEXT:           [[refByte3:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_24 %uint_8
// CHECK-NEXT:          [[src1Byte3:%\d+]] = OpBitFieldUExtract %uint [[bfi0]] %uint_24 %uint_8
// CHECK-NEXT:          [[iRefByte3:%\d+]] = OpBitcast %int [[refByte3]]
// CHECK-NEXT:         [[iSrc1Byte3:%\d+]] = OpBitcast %int [[src1Byte3]]
// CHECK-NEXT:              [[isub3:%\d+]] = OpISub %int [[iRefByte3]] [[iSrc1Byte3]]
// CHECK-NEXT:           [[iSub3Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub3]]
// CHECK-NEXT:     [[isRefByte3Zero:%\d+]] = OpIEqual %bool [[refByte3]] %uint_0
// CHECK-NEXT:           [[uSub3Abs:%\d+]] = OpBitcast %uint [[iSub3Abs]]
// CHECK-NEXT:              [[diff3:%\d+]] = OpSelect %uint [[isRefByte3Zero]] %uint_0 [[uSub3Abs]]
// CHECK-NEXT: [[accum1PlusDiff0123:%\d+]] = OpIAdd %uint [[accum1PlusDiff012]] [[diff3]]

// MSAD 2

// MSAD2 BYTE 0
//
// CHECK-NEXT:           [[refByte0:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_0 %uint_8
// CHECK-NEXT:          [[src2Byte0:%\d+]] = OpBitFieldUExtract %uint [[bfi1]] %uint_0 %uint_8
// CHECK-NEXT:          [[iRefByte0:%\d+]] = OpBitcast %int [[refByte0]]
// CHECK-NEXT:         [[iSrc2Byte0:%\d+]] = OpBitcast %int [[src2Byte0]]
// CHECK-NEXT:              [[isub0:%\d+]] = OpISub %int [[iRefByte0]] [[iSrc2Byte0]]
// CHECK-NEXT:           [[iSub0Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub0]]
// CHECK-NEXT:     [[isRefByte0Zero:%\d+]] = OpIEqual %bool [[refByte0]] %uint_0
// CHECK-NEXT:           [[uSub0Abs:%\d+]] = OpBitcast %uint [[iSub0Abs]]
// CHECK-NEXT:              [[diff0:%\d+]] = OpSelect %uint [[isRefByte0Zero]] %uint_0 [[uSub0Abs]]
// CHECK-NEXT:    [[accum2PlusDiff0:%\d+]] = OpIAdd %uint [[accum2]] [[diff0]]

// MSAD2 BYTE 1
//
// CHECK-NEXT:           [[refByte1:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_8 %uint_8
// CHECK-NEXT:          [[src2Byte1:%\d+]] = OpBitFieldUExtract %uint [[bfi1]] %uint_8 %uint_8
// CHECK-NEXT:          [[iRefByte1:%\d+]] = OpBitcast %int [[refByte1]]
// CHECK-NEXT:         [[iSrc2Byte1:%\d+]] = OpBitcast %int [[src2Byte1]]
// CHECK-NEXT:              [[isub1:%\d+]] = OpISub %int [[iRefByte1]] [[iSrc2Byte1]]
// CHECK-NEXT:           [[iSub1Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub1]]
// CHECK-NEXT:     [[isRefByte1Zero:%\d+]] = OpIEqual %bool [[refByte1]] %uint_0
// CHECK-NEXT:           [[uSub1Abs:%\d+]] = OpBitcast %uint [[iSub1Abs]]
// CHECK-NEXT:              [[diff1:%\d+]] = OpSelect %uint [[isRefByte1Zero]] %uint_0 [[uSub1Abs]]
// CHECK-NEXT:   [[accum2PlusDiff01:%\d+]] = OpIAdd %uint [[accum2PlusDiff0]] [[diff1]]

// MSAD2 BYTE 2
//
// CHECK-NEXT:           [[refByte2:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_16 %uint_8
// CHECK-NEXT:          [[src2Byte2:%\d+]] = OpBitFieldUExtract %uint [[bfi1]] %uint_16 %uint_8
// CHECK-NEXT:          [[iRefByte2:%\d+]] = OpBitcast %int [[refByte2]]
// CHECK-NEXT:         [[iSrc2Byte2:%\d+]] = OpBitcast %int [[src2Byte2]]
// CHECK-NEXT:              [[isub2:%\d+]] = OpISub %int [[iRefByte2]] [[iSrc2Byte2]]
// CHECK-NEXT:           [[iSub2Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub2]]
// CHECK-NEXT:     [[isRefByte2Zero:%\d+]] = OpIEqual %bool [[refByte2]] %uint_0
// CHECK-NEXT:           [[uSub2Abs:%\d+]] = OpBitcast %uint [[iSub2Abs]]
// CHECK-NEXT:              [[diff2:%\d+]] = OpSelect %uint [[isRefByte2Zero]] %uint_0 [[uSub2Abs]]
// CHECK-NEXT:  [[accum2PlusDiff012:%\d+]] = OpIAdd %uint [[accum2PlusDiff01]] [[diff2]]

// MSAD2 BYTE 3
//
// CHECK-NEXT:           [[refByte3:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_24 %uint_8
// CHECK-NEXT:          [[src2Byte3:%\d+]] = OpBitFieldUExtract %uint [[bfi1]] %uint_24 %uint_8
// CHECK-NEXT:          [[iRefByte3:%\d+]] = OpBitcast %int [[refByte3]]
// CHECK-NEXT:         [[iSrc2Byte3:%\d+]] = OpBitcast %int [[src2Byte3]]
// CHECK-NEXT:              [[isub3:%\d+]] = OpISub %int [[iRefByte3]] [[iSrc2Byte3]]
// CHECK-NEXT:           [[iSub3Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub3]]
// CHECK-NEXT:     [[isRefByte3Zero:%\d+]] = OpIEqual %bool [[refByte3]] %uint_0
// CHECK-NEXT:           [[uSub3Abs:%\d+]] = OpBitcast %uint [[iSub3Abs]]
// CHECK-NEXT:              [[diff3:%\d+]] = OpSelect %uint [[isRefByte3Zero]] %uint_0 [[uSub3Abs]]
// CHECK-NEXT: [[accum2PlusDiff0123:%\d+]] = OpIAdd %uint [[accum2PlusDiff012]] [[diff3]]

// MSAD 3

// MSAD3 BYTE 0
//
// CHECK-NEXT:           [[refByte0:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_0 %uint_8
// CHECK-NEXT:          [[src3Byte0:%\d+]] = OpBitFieldUExtract %uint [[bfi2]] %uint_0 %uint_8
// CHECK-NEXT:          [[iRefByte0:%\d+]] = OpBitcast %int [[refByte0]]
// CHECK-NEXT:         [[iSrc3Byte0:%\d+]] = OpBitcast %int [[src3Byte0]]
// CHECK-NEXT:              [[isub0:%\d+]] = OpISub %int [[iRefByte0]] [[iSrc3Byte0]]
// CHECK-NEXT:           [[iSub0Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub0]]
// CHECK-NEXT:     [[isRefByte0Zero:%\d+]] = OpIEqual %bool [[refByte0]] %uint_0
// CHECK-NEXT:           [[uSub0Abs:%\d+]] = OpBitcast %uint [[iSub0Abs]]
// CHECK-NEXT:              [[diff0:%\d+]] = OpSelect %uint [[isRefByte0Zero]] %uint_0 [[uSub0Abs]]
// CHECK-NEXT:    [[accum3PlusDiff0:%\d+]] = OpIAdd %uint [[accum3]] [[diff0]]

// MSAD3 BYTE 1
//
// CHECK-NEXT:           [[refByte1:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_8 %uint_8
// CHECK-NEXT:          [[src3Byte1:%\d+]] = OpBitFieldUExtract %uint [[bfi2]] %uint_8 %uint_8
// CHECK-NEXT:          [[iRefByte1:%\d+]] = OpBitcast %int [[refByte1]]
// CHECK-NEXT:         [[iSrc3Byte1:%\d+]] = OpBitcast %int [[src3Byte1]]
// CHECK-NEXT:              [[isub1:%\d+]] = OpISub %int [[iRefByte1]] [[iSrc3Byte1]]
// CHECK-NEXT:           [[iSub1Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub1]]
// CHECK-NEXT:     [[isRefByte1Zero:%\d+]] = OpIEqual %bool [[refByte1]] %uint_0
// CHECK-NEXT:           [[uSub1Abs:%\d+]] = OpBitcast %uint [[iSub1Abs]]
// CHECK-NEXT:              [[diff1:%\d+]] = OpSelect %uint [[isRefByte1Zero]] %uint_0 [[uSub1Abs]]
// CHECK-NEXT:   [[accum3PlusDiff01:%\d+]] = OpIAdd %uint [[accum3PlusDiff0]] [[diff1]]

// MSAD3 BYTE 2
//
// CHECK-NEXT:           [[refByte2:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_16 %uint_8
// CHECK-NEXT:          [[src3Byte2:%\d+]] = OpBitFieldUExtract %uint [[bfi2]] %uint_16 %uint_8
// CHECK-NEXT:          [[iRefByte2:%\d+]] = OpBitcast %int [[refByte2]]
// CHECK-NEXT:         [[iSrc3Byte2:%\d+]] = OpBitcast %int [[src3Byte2]]
// CHECK-NEXT:              [[isub2:%\d+]] = OpISub %int [[iRefByte2]] [[iSrc3Byte2]]
// CHECK-NEXT:           [[iSub2Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub2]]
// CHECK-NEXT:     [[isRefByte2Zero:%\d+]] = OpIEqual %bool [[refByte2]] %uint_0
// CHECK-NEXT:           [[uSub2Abs:%\d+]] = OpBitcast %uint [[iSub2Abs]]
// CHECK-NEXT:              [[diff2:%\d+]] = OpSelect %uint [[isRefByte2Zero]] %uint_0 [[uSub2Abs]]
// CHECK-NEXT:  [[accum3PlusDiff012:%\d+]] = OpIAdd %uint [[accum3PlusDiff01]] [[diff2]]

// MSAD3 BYTE 3
//
// CHECK-NEXT:           [[refByte3:%\d+]] = OpBitFieldUExtract %uint [[ref]] %uint_24 %uint_8
// CHECK-NEXT:          [[src3Byte3:%\d+]] = OpBitFieldUExtract %uint [[bfi2]] %uint_24 %uint_8
// CHECK-NEXT:          [[iRefByte3:%\d+]] = OpBitcast %int [[refByte3]]
// CHECK-NEXT:         [[iSrc3Byte3:%\d+]] = OpBitcast %int [[src3Byte3]]
// CHECK-NEXT:              [[isub3:%\d+]] = OpISub %int [[iRefByte3]] [[iSrc3Byte3]]
// CHECK-NEXT:           [[iSub3Abs:%\d+]] = OpExtInst %int [[glsl]] SAbs [[isub3]]
// CHECK-NEXT:     [[isRefByte3Zero:%\d+]] = OpIEqual %bool [[refByte3]] %uint_0
// CHECK-NEXT:           [[uSub3Abs:%\d+]] = OpBitcast %uint [[iSub3Abs]]
// CHECK-NEXT:              [[diff3:%\d+]] = OpSelect %uint [[isRefByte3Zero]] %uint_0 [[uSub3Abs]]
// CHECK-NEXT: [[accum3PlusDiff0123:%\d+]] = OpIAdd %uint [[accum3PlusDiff012]] [[diff3]]


// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %v4uint [[accum0PlusDiff0123]] [[accum1PlusDiff0123]] [[accum2PlusDiff0123]] [[accum3PlusDiff0123]]

  uint4 result = msad4(reference, source, accum);
  return result;
}
