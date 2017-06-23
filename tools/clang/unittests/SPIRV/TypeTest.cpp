//===- unittests/SPIRV/TypeTest.cpp ----- Type tests ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "gmock/gmock.h"
#include "clang/SPIRV/SPIRVContext.h"
#include "clang/SPIRV/Type.h"
#include "gtest/gtest.h"

using namespace clang::spirv;

namespace {

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

  const Type *struct_1 = Type::getType(
      ctx, spv::Op::OpTypeStruct, {intt_id, boolt_id},
      {relaxed, bufferblock, mem_0_offset, mem_1_offset, mem_0_position});

  const Type *struct_2 = Type::getType(
      ctx, spv::Op::OpTypeStruct, {intt_id, boolt_id},
      {relaxed, bufferblock, mem_0_offset, mem_1_offset, mem_0_position});

  const Type *struct_3 = Type::getType(
      ctx, spv::Op::OpTypeStruct, {intt_id, boolt_id},
      {bufferblock, mem_0_offset, mem_0_position, mem_1_offset, relaxed});

  // 2 types with the same signature. We should get the same pointer.
  EXPECT_EQ(struct_1, struct_2);

  // The order of decorations does not matter.
  EXPECT_EQ(struct_1, struct_3);
}

// TODO: Add Type tests for all types

} // anonymous namespace
