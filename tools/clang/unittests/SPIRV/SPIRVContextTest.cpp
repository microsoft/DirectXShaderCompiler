//===- unittests/SPIRV/SPIRVContextTest.cpp ----- SPIRVContext tests ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "gmock/gmock.h"
#include "clang/SPIRV/Decoration.h"
#include "clang/SPIRV/SPIRVContext.h"
#include "clang/SPIRV/Type.h"
#include "gtest/gtest.h"

using namespace clang::spirv;

namespace {

TEST(ValidateSPIRVContext, ValidateGetNextId) {
  SPIRVContext context;
  // Check that the first ID is 1.
  EXPECT_EQ(context.getNextId(), 1u);
  // Check that calling getNextId() multiple times does not increment the ID
  EXPECT_EQ(context.getNextId(), 1u);
}

TEST(ValidateSPIRVContext, ValidateTakeNextId) {
  SPIRVContext context;
  EXPECT_EQ(context.takeNextId(), 1u);
  EXPECT_EQ(context.takeNextId(), 2u);
  EXPECT_EQ(context.getNextId(), 3u);
}

TEST(ValidateSPIRVContext, ValidateUniqueIdForUniqueNonAggregateType) {
  SPIRVContext ctx;
  const Type *intt = Type::getInt32(ctx);
  uint32_t intt_id = ctx.getResultIdForType(intt);
  uint32_t intt_id_again = ctx.getResultIdForType(intt);
  // We should get the same ID for the same non-aggregate type.
  EXPECT_EQ(intt_id, intt_id_again);
}

TEST(ValidateSPIRVContext, ValidateUniqueIdForUniqueAggregateType) {
  SPIRVContext ctx;
  // In this test we construct a struct which includes an integer member and
  // a boolean member.
  // We also assign RelaxedPrecision decoration to the struct as a whole.
  // We also assign BufferBlock decoration to the struct as a whole.
  // We also assign Offset decoration to each member of the struct.
  // We also assign a BuiltIn decoration to the first member of the struct.
  const Type *intt = Type::getInt32(ctx);
  const Type *boolt = Type::getBool(ctx);
  const uint32_t intt_id = ctx.getResultIdForType(intt);
  const uint32_t boolt_id = ctx.getResultIdForType(boolt);
  const auto relaxed = Decoration::getRelaxedPrecision(ctx);
  const auto bufferblock = Decoration::getBufferBlock(ctx);
  const auto mem_0_offset = Decoration::getOffset(ctx, 0u, 0);
  const auto mem_1_offset = Decoration::getOffset(ctx, 0u, 1);
  const auto mem_0_position =
      Decoration::getBuiltIn(ctx, spv::BuiltIn::Position, 0);

  const Type *struct_1 = Type::getType(
      ctx, spv::Op::OpTypeStruct, {intt_id, boolt_id},
      {relaxed, bufferblock, mem_0_offset, mem_1_offset, mem_0_position});

  const Type *struct_2 = Type::getType(
      ctx, spv::Op::OpTypeStruct, {intt_id, boolt_id},
      {relaxed, bufferblock, mem_0_offset, mem_1_offset, mem_0_position});

  const uint32_t struct_1_id = ctx.getResultIdForType(struct_1);
  const uint32_t struct_2_id = ctx.getResultIdForType(struct_2);

  // We should be able to retrieve the same ID for the same Type.
  EXPECT_EQ(struct_1_id, struct_2_id);
}

// TODO: Add more SPIRVContext tests

} // anonymous namespace
