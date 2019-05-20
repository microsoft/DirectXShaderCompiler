///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// VerifierTest.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <vector>
#include <string>
#include "CompilationResult.h"
#include "HLSLTestData.h"

#include <fstream>

#ifdef _WIN32
#include "WexTestClass.h"
#define TEST_CLASS_DERIVATION
#else
#define TEST_CLASS_DERIVATION : public ::testing::Test
#endif
#include "HlslTestUtils.h"

using namespace std;

// The test fixture.
class VerifierTest TEST_CLASS_DERIVATION {
public:
  BEGIN_TEST_CLASS(VerifierTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_METHOD(RunArrayIndexOutOfBounds)
  TEST_METHOD(RunArrayLength)
  TEST_METHOD(RunAttributes)
  TEST_METHOD(RunBuiltinTypesNoInheritance)
  TEST_METHOD(RunConstExpr)
  TEST_METHOD(RunConstAssign)
  TEST_METHOD(RunConstDefault)
  TEST_METHOD(RunConversionsBetweenTypeShapes)
  TEST_METHOD(RunConversionsNonNumericAggregates)
  TEST_METHOD(RunCppErrors)
  TEST_METHOD(RunCppErrorsHV2015)
  TEST_METHOD(RunCXX11Attributes)
  TEST_METHOD(RunEnums)
  TEST_METHOD(RunFunctions)
  TEST_METHOD(RunIncompleteType)
  TEST_METHOD(RunIndexingOperator)
  TEST_METHOD(RunIntrinsicExamples)
  TEST_METHOD(RunMatrixAssignments)
  TEST_METHOD(RunMatrixSyntax)
  TEST_METHOD(RunMatrixSyntaxExactPrecision)
  TEST_METHOD(RunMintypesPromotionWarnings)
  TEST_METHOD(RunMoreOperators)
  TEST_METHOD(RunObjectOperators)
  TEST_METHOD(RunPackReg)
  TEST_METHOD(RunRayTracings)
  TEST_METHOD(RunScalarAssignments)
  TEST_METHOD(RunScalarAssignmentsExactPrecision)
  TEST_METHOD(RunScalarOperatorsAssign)
  TEST_METHOD(RunScalarOperatorsAssignExactPrecision)
  TEST_METHOD(RunScalarOperators)
  TEST_METHOD(RunScalarOperatorsExactPrecision)
  TEST_METHOD(RunSizeof)
  TEST_METHOD(RunString)
  TEST_METHOD(RunStructAssignments)
  TEST_METHOD(RunSubobjects)
  TEST_METHOD(RunIncompleteArray)
  TEST_METHOD(RunTemplateChecks)
  TEST_METHOD(RunVarmodsSyntax)
  TEST_METHOD(RunVectorAssignments)
  TEST_METHOD(RunVectorSyntaxMix)
  TEST_METHOD(RunVectorSyntax)
  TEST_METHOD(RunVectorSyntaxExactPrecision)
  TEST_METHOD(RunTypemodsSyntax)
  TEST_METHOD(RunSemantics)
  TEST_METHOD(RunImplicitCasts)
  TEST_METHOD(RunDerivedToBaseCasts)
  TEST_METHOD(RunLiterals)
  TEST_METHOD(RunEffectsSyntax)
  TEST_METHOD(RunVectorConditional)
  TEST_METHOD(RunUint4Add3)
  TEST_METHOD(RunBadInclude)
  TEST_METHOD(RunWave)
  TEST_METHOD(RunBinopDims)

  void CheckVerifies(const wchar_t* path) {
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    const char startMarker[] = "%clang_cc1";
    const char endMarker[] = "%s";

    char firstLine[200];
    memset(firstLine, 0, sizeof(firstLine));

    char* commandLine;

    //
    // Very simple processing for now.
    // See utils\lit\lit\TestRunner.py for the more thorough implementation.
    //
    // The first line for HLSL tests will always look like this:
    // // RUN: %clang_cc1 -fsyntax-only -Wno-unused-value -ffreestanding -verify %s
    //
    // We turn this into ' -fsyntax-only -Wno-unused-value -ffreestanding -verify ' by offseting after %clang_cc1
    // and chopping off everything after '%s'.
    //

    {
      ifstream infile(CW2A(path).m_psz);
      ASSERT_EQ(false, infile.bad());

      infile.getline(firstLine, _countof(firstLine));
      char* found = strstr(firstLine, startMarker);
      ASSERT_NE(nullptr, found);

      commandLine = found + strlen(startMarker);

      char* fileArgument = strstr(commandLine, endMarker);
      ASSERT_NE(nullptr, fileArgument);
      *fileArgument = '\0';
    }

    CW2A asciiPath(path);
    CompilationResult result = CompilationResult::CreateForCommandLine(commandLine, asciiPath);
    if (!result.ParseSucceeded()) {
      std::stringstream ss;
      ss << "for program " << asciiPath << " with errors:\n" << result.GetTextForErrors();
      CA2W pszW(ss.str().c_str());
      ::WEX::Logging::Log::Comment(pszW);
    }
    VERIFY_IS_TRUE(result.ParseSucceeded());
  }

  void CheckVerifiesHLSL(LPCWSTR name) {
    // Having a test per file makes it very easy to filter from the command line.
    CheckVerifies(hlsl_test::GetPathToHlslDataFile(name).c_str());
  }
};

TEST_F(VerifierTest, RunArrayIndexOutOfBounds) {
  CheckVerifiesHLSL(L"array-index-out-of-bounds.hlsl");
  CheckVerifiesHLSL(L"array-index-out-of-bounds-HV-2016.hlsl");
}

TEST_F(VerifierTest, RunArrayLength) {
  CheckVerifiesHLSL(L"array-length.hlsl");
}

TEST_F(VerifierTest, RunAttributes) {
  CheckVerifiesHLSL(L"attributes.hlsl");
}

TEST_F(VerifierTest, RunBuiltinTypesNoInheritance) {
  CheckVerifiesHLSL(L"builtin-types-no-inheritance.hlsl");
}

TEST_F(VerifierTest, RunConstExpr) {
  CheckVerifiesHLSL(L"const-expr.hlsl");
}

TEST_F(VerifierTest, RunConstAssign) {
  CheckVerifiesHLSL(L"const-assign.hlsl");
}

TEST_F(VerifierTest, RunConstDefault) {
  CheckVerifiesHLSL(L"const-default.hlsl");
}

TEST_F(VerifierTest, RunConversionsBetweenTypeShapes) {
  CheckVerifiesHLSL(L"conversions-between-type-shapes.hlsl");
}

TEST_F(VerifierTest, RunConversionsNonNumericAggregates) {
  CheckVerifiesHLSL(L"conversions-non-numeric-aggregates.hlsl");
}

TEST_F(VerifierTest, RunCppErrors) {
  CheckVerifiesHLSL(L"cpp-errors.hlsl");
}

TEST_F(VerifierTest, RunCppErrorsHV2015) {
  CheckVerifiesHLSL(L"cpp-errors-hv2015.hlsl");
}

TEST_F(VerifierTest, RunCXX11Attributes) {
  CheckVerifiesHLSL(L"cxx11-attributes.hlsl");
}

TEST_F(VerifierTest, RunEnums) {
  CheckVerifiesHLSL(L"enums.hlsl");
}

TEST_F(VerifierTest, RunFunctions) {
  CheckVerifiesHLSL(L"functions.hlsl");
}

TEST_F(VerifierTest, RunIncompleteType) {
  CheckVerifiesHLSL(L"incomplete-type.hlsl");
}

TEST_F(VerifierTest, RunIndexingOperator) {
  CheckVerifiesHLSL(L"indexing-operator.hlsl");
}

TEST_F(VerifierTest, RunIntrinsicExamples) {
  CheckVerifiesHLSL(L"intrinsic-examples.hlsl");
}

TEST_F(VerifierTest, RunMatrixAssignments) {
  CheckVerifiesHLSL(L"matrix-assignments.hlsl");
}

TEST_F(VerifierTest, RunMatrixSyntax) {
  CheckVerifiesHLSL(L"matrix-syntax.hlsl");
}

TEST_F(VerifierTest, RunMatrixSyntaxExactPrecision) {
  CheckVerifiesHLSL(L"matrix-syntax-exact-precision.hlsl");
}

TEST_F(VerifierTest, RunMintypesPromotionWarnings) {
  CheckVerifiesHLSL(L"mintypes-promotion-warnings.hlsl");
}

TEST_F(VerifierTest, RunMoreOperators) {
  CheckVerifiesHLSL(L"more-operators.hlsl");
}

TEST_F(VerifierTest, RunObjectOperators) {
  CheckVerifiesHLSL(L"object-operators.hlsl");
}

TEST_F(VerifierTest, RunPackReg) {
  CheckVerifiesHLSL(L"packreg.hlsl");
}

TEST_F(VerifierTest, RunRayTracings) {
  CheckVerifiesHLSL(L"raytracings.hlsl");
}

TEST_F(VerifierTest, RunScalarAssignments) {
  CheckVerifiesHLSL(L"scalar-assignments.hlsl");
}

TEST_F(VerifierTest, RunScalarAssignmentsExactPrecision) {
  CheckVerifiesHLSL(L"scalar-assignments-exact-precision.hlsl");
}

TEST_F(VerifierTest, RunScalarOperatorsAssign) {
  CheckVerifiesHLSL(L"scalar-operators-assign.hlsl");
}

TEST_F(VerifierTest, RunScalarOperatorsAssignExactPrecision) {
  CheckVerifiesHLSL(L"scalar-operators-assign-exact-precision.hlsl");
}

TEST_F(VerifierTest, RunScalarOperators) {
  CheckVerifiesHLSL(L"scalar-operators.hlsl");
}

TEST_F(VerifierTest, RunScalarOperatorsExactPrecision) {
  CheckVerifiesHLSL(L"scalar-operators-exact-precision.hlsl");
}

TEST_F(VerifierTest, RunSizeof) {
  CheckVerifiesHLSL(L"sizeof.hlsl");
}

TEST_F(VerifierTest, RunString) {
  CheckVerifiesHLSL(L"string.hlsl");
}

TEST_F(VerifierTest, RunStructAssignments) {
  CheckVerifiesHLSL(L"struct-assignments.hlsl");
}

TEST_F(VerifierTest, RunSubobjects) {
  CheckVerifiesHLSL(L"subobjects-syntax.hlsl");
}

TEST_F(VerifierTest, RunIncompleteArray) {
  CheckVerifiesHLSL(L"incomp_array_err.hlsl");
}

TEST_F(VerifierTest, RunTemplateChecks) {
  CheckVerifiesHLSL(L"template-checks.hlsl");
}

TEST_F(VerifierTest, RunVarmodsSyntax) {
  CheckVerifiesHLSL(L"varmods-syntax.hlsl");
}

TEST_F(VerifierTest, RunVectorAssignments) {
  CheckVerifiesHLSL(L"vector-assignments.hlsl");
}

TEST_F(VerifierTest, RunVectorSyntaxMix) {
  CheckVerifiesHLSL(L"vector-syntax-mix.hlsl");
}

TEST_F(VerifierTest, RunVectorSyntax) {
  CheckVerifiesHLSL(L"vector-syntax.hlsl");
}

TEST_F(VerifierTest, RunVectorSyntaxExactPrecision) {
  CheckVerifiesHLSL(L"vector-syntax-exact-precision.hlsl");
}

TEST_F(VerifierTest, RunTypemodsSyntax) {
  CheckVerifiesHLSL(L"typemods-syntax.hlsl");
}

TEST_F(VerifierTest, RunSemantics) {
  CheckVerifiesHLSL(L"semantics.hlsl");
}

TEST_F(VerifierTest, RunImplicitCasts) {
  CheckVerifiesHLSL(L"implicit-casts.hlsl");
}

TEST_F(VerifierTest, RunDerivedToBaseCasts) {
  CheckVerifiesHLSL(L"derived-to-base.hlsl");
}

TEST_F(VerifierTest, RunLiterals) {
  CheckVerifiesHLSL(L"literals.hlsl");
}

TEST_F(VerifierTest, RunEffectsSyntax) {
  CheckVerifiesHLSL(L"effects-syntax.hlsl");
}

TEST_F(VerifierTest, RunVectorConditional) {
  CheckVerifiesHLSL(L"vector-conditional.hlsl");
}

TEST_F(VerifierTest, RunUint4Add3) {
  CheckVerifiesHLSL(L"uint4_add3.hlsl");
}

TEST_F(VerifierTest, RunBadInclude) {
  CheckVerifiesHLSL(L"bad-include.hlsl");
}

TEST_F(VerifierTest, RunWave) {
  CheckVerifiesHLSL(L"wave.hlsl");
}

TEST_F(VerifierTest, RunBinopDims) {
  CheckVerifiesHLSL(L"binop-dims.hlsl");
}
