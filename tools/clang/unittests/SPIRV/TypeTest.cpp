//===- unittests/SPIRV/TypeTest.cpp ----- Type tests ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/Type.h"
#include "SPIRVTestUtils.h"
#include "clang/SPIRV/SPIRVContext.h"
#include "clang/SPIRV/String.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace clang::spirv;

namespace {
using ::testing::ContainerEq;
using ::testing::ElementsAre;

TEST(Type, SameTypeWoParameterShouldHaveSameAddress) {
  SPIRVContext context;
  const Type *pIntFirst = Type::getInt32(context);
  const Type *pIntSecond = Type::getInt32(context);
  EXPECT_EQ(pIntFirst, pIntSecond);
}

TEST(Type, SameAggregateTypeWithDecorationsShouldHaveSameAddress) {
  clang::spirv::SPIRVContext ctx;
  // In this test we will build a struct which includes an integer member and
  // a boolean member.
  // We also assign RelaxedPrecision decoration to the struct as a whole.
  // We also assign BufferBlock decoration to the struct as a whole.
  // We also assign Offset decoration to each member of the struct.
  // We also assign a BuiltIn decoration to the first member of the struct.
  const Type *intt = Type::getInt32(ctx);
  const Type *boolt = Type::getBool(ctx);
  const uint32_t intt_id = ctx.getResultIdForType(intt);
  const uint32_t boolt_id = ctx.getResultIdForType(boolt);
  const Decoration *relaxed = Decoration::getRelaxedPrecision(ctx);
  const Decoration *bufferblock = Decoration::getBufferBlock(ctx);
  const Decoration *mem_0_offset = Decoration::getOffset(ctx, 0u, 0);
  const Decoration *mem_1_offset = Decoration::getOffset(ctx, 0u, 1);
  const Decoration *mem_0_position =
      Decoration::getBuiltIn(ctx, spv::BuiltIn::Position, 0);

  const Type *struct_1 = Type::getStruct(
      ctx, {intt_id, boolt_id}, "",
      {relaxed, bufferblock, mem_0_offset, mem_1_offset, mem_0_position});

  const Type *struct_2 = Type::getStruct(
      ctx, {intt_id, boolt_id}, "",
      {relaxed, bufferblock, mem_0_offset, mem_1_offset, mem_0_position});

  const Type *struct_3 = Type::getStruct(
      ctx, {intt_id, boolt_id}, "",
      {bufferblock, mem_0_offset, mem_0_position, mem_1_offset, relaxed});

  const Type *struct_4 = Type::getStruct(
      ctx, {intt_id, boolt_id}, "name",
      {bufferblock, mem_0_offset, mem_0_position, mem_1_offset, relaxed});

  // 2 types with the same signature. We should get the same pointer.
  EXPECT_EQ(struct_1, struct_2);

  // The order of decorations does not matter.
  EXPECT_EQ(struct_1, struct_3);

  // Struct with different names are different.
  EXPECT_NE(struct_3, struct_4);
}

TEST(Type, Void) {
  SPIRVContext ctx;
  const Type *t = Type::getVoid(ctx);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeVoid);
  EXPECT_TRUE(t->getArgs().empty());
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Bool) {
  SPIRVContext ctx;
  const Type *t = Type::getBool(ctx);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeBool);
  EXPECT_TRUE(t->getArgs().empty());
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Int8) {
  SPIRVContext ctx;
  const Type *t = Type::getInt8(ctx);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeInt);
  EXPECT_THAT(t->getArgs(), ElementsAre(8, 1));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Uint8) {
  SPIRVContext ctx;
  const Type *t = Type::getUint8(ctx);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeInt);
  EXPECT_THAT(t->getArgs(), ElementsAre(8, 0));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Int16) {
  SPIRVContext ctx;
  const Type *t = Type::getInt16(ctx);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeInt);
  EXPECT_THAT(t->getArgs(), ElementsAre(16, 1));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Uint16) {
  SPIRVContext ctx;
  const Type *t = Type::getUint16(ctx);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeInt);
  EXPECT_THAT(t->getArgs(), ElementsAre(16, 0));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Int32) {
  SPIRVContext ctx;
  const Type *t = Type::getInt32(ctx);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeInt);
  EXPECT_THAT(t->getArgs(), ElementsAre(32, 1));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Uint32) {
  SPIRVContext ctx;
  const Type *t = Type::getUint32(ctx);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeInt);
  EXPECT_THAT(t->getArgs(), ElementsAre(32, 0));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Int64) {
  SPIRVContext ctx;
  const Type *t = Type::getInt64(ctx);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeInt);
  EXPECT_THAT(t->getArgs(), ElementsAre(64, 1));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Uint64) {
  SPIRVContext ctx;
  const Type *t = Type::getUint64(ctx);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeInt);
  EXPECT_THAT(t->getArgs(), ElementsAre(64, 0));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Float16) {
  SPIRVContext ctx;
  const Type *t = Type::getFloat16(ctx);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeFloat);
  EXPECT_THAT(t->getArgs(), ElementsAre(16));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Float32) {
  SPIRVContext ctx;
  const Type *t = Type::getFloat32(ctx);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeFloat);
  EXPECT_THAT(t->getArgs(), ElementsAre(32));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Float64) {
  SPIRVContext ctx;
  const Type *t = Type::getFloat64(ctx);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeFloat);
  EXPECT_THAT(t->getArgs(), ElementsAre(64));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Vec2) {
  SPIRVContext ctx;
  const Type *t = Type::getVec2(ctx, 1);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeVector);
  EXPECT_THAT(t->getArgs(), ElementsAre(1, 2));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Vec3) {
  SPIRVContext ctx;
  const Type *t = Type::getVec3(ctx, 1);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeVector);
  EXPECT_THAT(t->getArgs(), ElementsAre(1, 3));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Vec4) {
  SPIRVContext ctx;
  const Type *t = Type::getVec4(ctx, 1);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeVector);
  EXPECT_THAT(t->getArgs(), ElementsAre(1, 4));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, Matrix) {
  SPIRVContext ctx;
  const Type *t = Type::getMatrix(ctx, /*type-id*/ 7, /*column-count*/ 4);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeMatrix);
  EXPECT_THAT(t->getArgs(), ElementsAre(7, 4));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, ImageWithoutAccessQualifier) {
  SPIRVContext ctx;
  const Type *t = Type::getImage(ctx, /*sampled-type*/ 5, spv::Dim::Cube,
                                 /*depth*/ 1, /*arrayed*/ 1, /*multisampled*/ 0,
                                 /*sampled*/ 2, spv::ImageFormat::Rgba32f);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeImage);
  EXPECT_THAT(t->getArgs(),
              ElementsAre(5, static_cast<uint32_t>(spv::Dim::Cube), 1, 1, 0, 2,
                          static_cast<uint32_t>(spv::ImageFormat::Rgba32f)));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, DecoratedImageWithoutAccessQualifier) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getAliased(ctx);
  const Type *t =
      Type::getImage(ctx, /*sampled-type*/ 5, spv::Dim::Cube, /*depth*/ 1,
                     /*arrayed*/ 1, /*multisampled*/ 0, /*sampled*/ 2,
                     spv::ImageFormat::Rgba32f, llvm::None, {d});
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeImage);
  EXPECT_THAT(t->getArgs(),
              ElementsAre(5, static_cast<uint32_t>(spv::Dim::Cube), 1, 1, 0, 2,
                          static_cast<uint32_t>(spv::ImageFormat::Rgba32f)));
  EXPECT_THAT(t->getDecorations(), ElementsAre(d));
}

TEST(Type, ImageWithAccessQualifier) {
  SPIRVContext ctx;
  const Type *t = Type::getImage(
      ctx, /*sampled-type*/ 5, spv::Dim::Cube, /*depth*/ 1, /*arrayed*/ 1,
      /*multisampled*/ 0, /*sampled*/ 2, spv::ImageFormat::Rgba32f,
      /*access-qualifier*/ spv::AccessQualifier::ReadWrite);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeImage);
  EXPECT_THAT(
      t->getArgs(),
      ElementsAre(5, static_cast<uint32_t>(spv::Dim::Cube), 1, 1, 0, 2,
                  static_cast<uint32_t>(spv::ImageFormat::Rgba32f),
                  static_cast<uint32_t>(spv::AccessQualifier::ReadWrite)));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, DecoratedImageWithAccessQualifier) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getAliased(ctx);
  const Type *t = Type::getImage(
      ctx, /*sampled-type*/ 5, spv::Dim::Cube, /*depth*/ 1, /*arrayed*/ 1,
      /*multisampled*/ 0, /*sampled*/ 2, spv::ImageFormat::Rgba32f,
      /*access-qualifier*/ spv::AccessQualifier::ReadWrite, {d});
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeImage);
  EXPECT_THAT(
      t->getArgs(),
      ElementsAre(5, static_cast<uint32_t>(spv::Dim::Cube), 1, 1, 0, 2,
                  static_cast<uint32_t>(spv::ImageFormat::Rgba32f),
                  static_cast<uint32_t>(spv::AccessQualifier::ReadWrite)));
  EXPECT_THAT(t->getDecorations(), ElementsAre(d));
}

TEST(Type, ImageWithAndWithoutAccessQualifierAreDifferentTypes) {
  SPIRVContext ctx;
  const Type *img1 =
      Type::getImage(ctx, /*sampled-type*/ 5, spv::Dim::Cube,
                     /*depth*/ 1, /*arrayed*/ 1, /*multisampled*/ 0,
                     /*sampled*/ 2, spv::ImageFormat::Rgba32f);
  const Type *img2 =
      Type::getImage(ctx, /*sampled-type*/ 5, spv::Dim::Cube,
                     /*depth*/ 1, /*arrayed*/ 1, /*multisampled*/ 0,
                     /*sampled*/ 2, spv::ImageFormat::Rgba32f,
                     /*access-qualifier*/ spv::AccessQualifier::ReadWrite);

  // The only difference between these two types is the Access Qualifier which
  // is an optional argument.
  EXPECT_NE(img1, img2);
}

TEST(Type, Sampler) {
  SPIRVContext ctx;
  const Type *t = Type::getSampler(ctx);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeSampler);
  EXPECT_TRUE(t->getArgs().empty());
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, DecoratedSampler) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getAliased(ctx);
  const Type *t = Type::getSampler(ctx, {d});
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeSampler);
  EXPECT_TRUE(t->getArgs().empty());
  EXPECT_THAT(t->getDecorations(), ElementsAre(d));
}

TEST(Type, SampledImage) {
  SPIRVContext ctx;
  const Type *t = Type::getSampledImage(ctx, 1);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeSampledImage);
  EXPECT_THAT(t->getArgs(), ElementsAre(1));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, DecoratedSampledImage) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getAliased(ctx);
  const Type *t = Type::getSampledImage(ctx, 1, {d});
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeSampledImage);
  EXPECT_THAT(t->getArgs(), ElementsAre(1));
  EXPECT_THAT(t->getDecorations(), ElementsAre(d));
}

TEST(Type, Array) {
  SPIRVContext ctx;
  const Type *t = Type::getArray(ctx, 2, 4);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeArray);
  EXPECT_THAT(t->getArgs(), ElementsAre(2, 4));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, DecoratedArray) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getAliased(ctx);
  const Type *t = Type::getArray(ctx, 2, 4, {d});
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeArray);
  EXPECT_THAT(t->getArgs(), ElementsAre(2, 4));
  EXPECT_THAT(t->getDecorations(), ElementsAre(d));
}

TEST(Type, RuntimeArray) {
  SPIRVContext ctx;
  const Type *t = Type::getRuntimeArray(ctx, 2);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeRuntimeArray);
  EXPECT_THAT(t->getArgs(), ElementsAre(2));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, DecoratedRuntimeArray) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getAliased(ctx);
  const Type *t = Type::getRuntimeArray(ctx, 2, {d});
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeRuntimeArray);
  EXPECT_THAT(t->getArgs(), ElementsAre(2));
  EXPECT_THAT(t->getDecorations(), ElementsAre(d));
}

TEST(Type, StructBasic) {
  SPIRVContext ctx;
  const Type *t = Type::getStruct(ctx, {2, 3, 4});
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeStruct);
  EXPECT_THAT(t->getArgs(), ElementsAre(2, 3, 4));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, StructWithDecoration) {
  SPIRVContext ctx;
  const Decoration *bufferblock = Decoration::getBufferBlock(ctx);
  const Type *t = Type::getStruct(ctx, {2, 3, 4}, "", {bufferblock});
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeStruct);
  EXPECT_THAT(t->getArgs(), ElementsAre(2, 3, 4));
  EXPECT_THAT(t->getDecorations(), ElementsAre(bufferblock));
}

TEST(Type, StructWithDecoratedMembers) {
  SPIRVContext ctx;
  const Decoration *relaxed = Decoration::getRelaxedPrecision(ctx);
  const Decoration *bufferblock = Decoration::getBufferBlock(ctx);
  const Decoration *mem_0_offset = Decoration::getOffset(ctx, 0u, 0);
  const Decoration *mem_1_offset = Decoration::getOffset(ctx, 0u, 1);
  const Decoration *mem_0_position =
      Decoration::getBuiltIn(ctx, spv::BuiltIn::Position, 0);

  const Type *t = Type::getStruct(
      ctx, {2, 3, 4}, "",
      {relaxed, bufferblock, mem_0_position, mem_0_offset, mem_1_offset});
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeStruct);
  EXPECT_THAT(t->getArgs(), ElementsAre(2, 3, 4));
  // Since decorations are an ordered set of pointers, it's better not to use
  // ElementsAre()
  EXPECT_EQ(t->getDecorations().size(), 5u);
  EXPECT_TRUE(t->hasDecoration(relaxed));
  EXPECT_TRUE(t->hasDecoration(bufferblock));
  EXPECT_TRUE(t->hasDecoration(mem_0_offset));
  EXPECT_TRUE(t->hasDecoration(mem_0_position));
  EXPECT_TRUE(t->hasDecoration(mem_1_offset));
}

TEST(Type, Pointer) {
  SPIRVContext ctx;
  const Type *t = Type::getPointer(ctx, spv::StorageClass::Uniform, 2);
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypePointer);
  EXPECT_THAT(
      t->getArgs(),
      ElementsAre(static_cast<uint32_t>(spv::StorageClass::Uniform), 2));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, DecoratedPointer) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getAliased(ctx);
  const Type *t = Type::getPointer(ctx, spv::StorageClass::Uniform, 2, {d});
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypePointer);
  EXPECT_THAT(
      t->getArgs(),
      ElementsAre(static_cast<uint32_t>(spv::StorageClass::Uniform), 2));
  EXPECT_THAT(t->getDecorations(), ElementsAre(d));
}

TEST(Type, Function) {
  SPIRVContext ctx;
  const Type *t = Type::getFunction(ctx, 1, {2, 3, 4});
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeFunction);
  EXPECT_THAT(t->getArgs(), ElementsAre(1, 2, 3, 4));
  EXPECT_TRUE(t->getDecorations().empty());
}

TEST(Type, DecoratedFunction) {
  SPIRVContext ctx;
  const Decoration *d = Decoration::getAliased(ctx);
  const Type *t = Type::getFunction(ctx, 1, {2, 3, 4}, {d});
  EXPECT_EQ(t->getOpcode(), spv::Op::OpTypeFunction);
  EXPECT_THAT(t->getArgs(), ElementsAre(1, 2, 3, 4));
  EXPECT_THAT(t->getDecorations(), ElementsAre(d));
}

TEST(Type, BoolWithResultId) {
  SPIRVContext ctx;
  const Type *t = Type::getBool(ctx);
  const auto words = t->withResultId(1);
  EXPECT_THAT(words, ContainerEq(constructInst(spv::Op::OpTypeBool, {1})));
}

TEST(Type, IntWithResultId) {
  SPIRVContext ctx;
  const Type *t = Type::getInt32(ctx);
  const auto words = t->withResultId(42);
  EXPECT_THAT(words,
              ContainerEq(constructInst(spv::Op::OpTypeInt, {42, 32, 1})));
}

} // anonymous namespace
