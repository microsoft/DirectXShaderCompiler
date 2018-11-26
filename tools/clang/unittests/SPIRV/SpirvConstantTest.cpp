//===- unittests/SPIRV/SpirvConstantTest.cpp --- SPIR-V Constant tests ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "clang/SPIRV/SPIRVContext.h"
#include "clang/SPIRV/SpirvInstruction.h"

namespace {
using namespace clang::spirv;

TEST(SpirvConstant, BoolFalse) {
  SpirvContext ctx;
  const bool val = false;
  SpirvConstantBoolean constant(ctx.getBoolType(), val);
  EXPECT_EQ(val, constant.getValue());
}

TEST(SpirvConstant, BoolTrue) {
  SpirvContext ctx;
  const bool val = true;
  SpirvConstantBoolean constant(ctx.getBoolType(), val);
  EXPECT_EQ(val, constant.getValue());
}

TEST(SpirvConstant, Uint16) {
  SpirvContext ctx;
  const uint16_t u16 = 12;
  SpirvConstantInteger constant(ctx.getUIntType(16), u16);
  EXPECT_EQ(u16, constant.getUnsignedInt16Value());
}

TEST(SpirvConstant, Int16) {
  SpirvContext ctx;
  const int16_t i16 = -12;
  SpirvConstantInteger constant(ctx.getSIntType(16), i16);
  EXPECT_EQ(i16, constant.getSignedInt16Value());
}

TEST(SpirvConstant, Uint32) {
  SpirvContext ctx;
  const uint32_t u32 = 65536;
  SpirvConstantInteger constant(ctx.getUIntType(32), u32);
  EXPECT_EQ(u32, constant.getUnsignedInt32Value());
}

TEST(SpirvConstant, Int32) {
  SpirvContext ctx;
  const int32_t i32 = -65536;
  SpirvConstantInteger constant(ctx.getSIntType(32), i32);
  EXPECT_EQ(i32, constant.getSignedInt32Value());
}

TEST(SpirvConstant, Uint64) {
  SpirvContext ctx;
  const uint64_t u64 = 4294967296;
  SpirvConstantInteger constant(ctx.getUIntType(64), u64);
  EXPECT_EQ(u64, constant.getUnsignedInt64Value());
}

TEST(SpirvConstant, Int64) {
  SpirvContext ctx;
  const int64_t i64 = -4294967296;
  SpirvConstantInteger constant(ctx.getSIntType(64), i64);
  EXPECT_EQ(i64, constant.getSignedInt64Value());
}

TEST(SpirvConstant, Float16) {
  SpirvContext ctx;
  const uint16_t f16 = 12;
  SpirvConstantFloat constant(ctx.getFloatType(16), f16);
  EXPECT_EQ(f16, constant.getValue16());
}

TEST(SpirvConstant, Float32) {
  SpirvContext ctx;
  const float f32 = 1.5;
  SpirvConstantFloat constant(ctx.getFloatType(32), f32);
  EXPECT_EQ(f32, constant.getValue32());
}

TEST(SpirvConstant, Float64) {
  SpirvContext ctx;
  const double f64 = 3.14;
  SpirvConstantFloat constant(ctx.getFloatType(64), f64);
  EXPECT_EQ(f64, constant.getValue64());
}

TEST(SpirvConstant, CheckOperatorEqualOnBool) {
  SpirvContext ctx;
  const bool val = true;
  SpirvConstantBoolean constant1(ctx.getBoolType(), val);
  SpirvConstantBoolean constant2(ctx.getBoolType(), val);
  EXPECT_TRUE(constant1 == constant2);
}

TEST(SpirvConstant, CheckOperatorEqualOnInt) {
  SpirvContext ctx;
  const int32_t i32 = -65536;
  SpirvConstantInteger constant1(ctx.getSIntType(32), i32);
  SpirvConstantInteger constant2(ctx.getSIntType(32), i32);
  EXPECT_TRUE(constant1 == constant2);
}

TEST(SpirvConstant, CheckOperatorEqualOnFloat) {
  SpirvContext ctx;
  const double f64 = 3.14;
  SpirvConstantFloat constant1(ctx.getFloatType(64), f64);
  SpirvConstantFloat constant2(ctx.getFloatType(64), f64);
  EXPECT_TRUE(constant1 == constant2);
}

TEST(SpirvConstant, CheckOperatorEqualOnNull) {
  SpirvContext ctx;
  SpirvConstantNull constant1(ctx.getSIntType(32));
  SpirvConstantNull constant2(ctx.getSIntType(32));
  EXPECT_TRUE(constant1 == constant2);
}

TEST(SpirvConstant, CheckOperatorEqualOnBool2) {
  SpirvContext ctx;
  SpirvConstantBoolean constant1(ctx.getBoolType(), true);
  SpirvConstantBoolean constant2(ctx.getBoolType(), false);
  EXPECT_FALSE(constant1 == constant2);
}

TEST(SpirvConstant, CheckOperatorEqualOnInt2) {
  SpirvContext ctx;
  SpirvConstantInteger constant1(ctx.getSIntType(32), 5);
  SpirvConstantInteger constant2(ctx.getSIntType(32), 7);
  EXPECT_FALSE(constant1 == constant2);
}

TEST(SpirvConstant, CheckOperatorEqualOnFloat2) {
  SpirvContext ctx;
  SpirvConstantFloat constant1(ctx.getFloatType(64), 3.14);
  SpirvConstantFloat constant2(ctx.getFloatType(64), 3.15);
  EXPECT_FALSE(constant1 == constant2);
}

TEST(SpirvConstant, CheckOperatorEqualOnNull2) {
  SpirvContext ctx;
  SpirvConstantNull constant1(ctx.getSIntType(32));
  SpirvConstantNull constant2(ctx.getUIntType(32));
  EXPECT_FALSE(constant1 == constant2);
}

TEST(SpirvConstant, CheckOperatorEqualOnComposite) {
  // Make a constant array of size 2.
  // Each array element is a vector of 4 floats.
  SpirvContext ctx;
  const FloatType *f32Type = ctx.getFloatType(32);
  const VectorType *vecType = ctx.getVectorType(f32Type, 4);
  const ArrayType *arrType = ctx.getArrayType(vecType, 2, llvm::None);
  SpirvConstantFloat f1(f32Type, 3.14);
  SpirvConstantFloat f2(f32Type, 5.f);
  SpirvConstantFloat f3(f32Type, -1.f);
  SpirvConstantFloat f4(f32Type, 0.f);
  llvm::SmallVector<SpirvConstant *, 4> vectorValues = {&f1, &f2, &f3, &f4};
  SpirvConstantComposite vec4(vecType, vectorValues);
  llvm::SmallVector<SpirvConstant *, 2> arrayValues = {&vec4, &vec4};
  SpirvConstantComposite arrayConstant1(arrType, arrayValues);
  SpirvConstantComposite arrayConstant2(arrType, arrayValues);
  EXPECT_TRUE(arrayConstant1 == arrayConstant2);
}

TEST(SpirvConstant, CheckOperatorEqualOnComposite2) {
  // Make a constant array of size 1.
  // Each array element is a vector of 4 floats.
  SpirvContext ctx;
  const FloatType *f32Type = ctx.getFloatType(32);
  const VectorType *vecType = ctx.getVectorType(f32Type, 4);
  const ArrayType *arrType = ctx.getArrayType(vecType, 1, llvm::None);
  SpirvConstantFloat f1(f32Type, 3.14);
  SpirvConstantFloat f2(f32Type, 5.f);
  SpirvConstantFloat f3(f32Type, -1.f);
  SpirvConstantFloat f4(f32Type, 0.f);
  // Make the first component of the two vectors different intentionally.
  llvm::SmallVector<SpirvConstant *, 4> vectorValues1 = {&f1, &f2, &f3, &f4};
  llvm::SmallVector<SpirvConstant *, 4> vectorValues2 = {&f2, &f2, &f3, &f4};
  SpirvConstantComposite vecConstant1(vecType, vectorValues1);
  SpirvConstantComposite vecConstant2(vecType, vectorValues2);
  llvm::SmallVector<SpirvConstant *, 2> arrayValues1 = {&vecConstant1};
  llvm::SmallVector<SpirvConstant *, 2> arrayValues2 = {&vecConstant2};
  SpirvConstantComposite arrayConstant1(arrType, arrayValues1);
  SpirvConstantComposite arrayConstant2(arrType, arrayValues2);
  EXPECT_FALSE(arrayConstant1 == arrayConstant2);
}

TEST(SpirvConstant, BoolConstNotEqualSpecConst) {
  SpirvContext ctx;
  SpirvConstantBoolean constant1(ctx.getBoolType(), true, /*SpecConst*/ true);
  SpirvConstantBoolean constant2(ctx.getBoolType(), false, /*SpecConst*/ false);
  EXPECT_FALSE(constant1 == constant2);
}

TEST(SpirvConstant, IntConstNotEqualSpecConst) {
  SpirvContext ctx;
  SpirvConstantInteger constant1(ctx.getSIntType(32), 5, /*SpecConst*/ true);
  SpirvConstantInteger constant2(ctx.getSIntType(32), 7, /*SpecConst*/ false);
  EXPECT_FALSE(constant1 == constant2);
}

TEST(SpirvConstant, FloatConstNotEqualSpecConst) {
  SpirvContext ctx;
  SpirvConstantFloat constant1(ctx.getFloatType(64), 3.14, /*SpecConst*/ true);
  SpirvConstantFloat constant2(ctx.getFloatType(64), 3.15, /*SpecConst*/ false);
  EXPECT_FALSE(constant1 == constant2);
}

TEST(SpirvConstant, CompositeConstNotEqualSpecConstComposite) {
  // Make a constant array of size 2.
  // Each array element is a vector of 4 floats.
  SpirvContext ctx;
  const FloatType *f32Type = ctx.getFloatType(32);
  const VectorType *vecType = ctx.getVectorType(f32Type, 4);
  const ArrayType *arrType = ctx.getArrayType(vecType, 2, llvm::None);
  SpirvConstantFloat f1(f32Type, 3.14);
  SpirvConstantFloat f2(f32Type, 5.f);
  SpirvConstantFloat f3(f32Type, -1.f);
  SpirvConstantFloat f4(f32Type, 0.f);
  llvm::SmallVector<SpirvConstant *, 4> vectorValues = {&f1, &f2, &f3, &f4};
  SpirvConstantComposite vec4(vecType, vectorValues);
  llvm::SmallVector<SpirvConstant *, 2> arrayValues = {&vec4, &vec4};
  SpirvConstantComposite arrayConstant1(arrType, arrayValues,
                                        /*SpecConst*/ true);
  SpirvConstantComposite arrayConstant2(arrType, arrayValues,
                                        /*SpecConst*/ false);
  EXPECT_FALSE(arrayConstant1 == arrayConstant2);
}

} // anonymous namespace
