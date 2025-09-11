#ifndef LONGVECTORS_H
#define LONGVECTORS_H

#include <WexTestClass.h>

#include <optional>
#include <string>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include "LongVectorTestData.h"

namespace LongVector {

class OpTest {
public:
  BEGIN_TEST_CLASS(OpTest)
  END_TEST_CLASS()

  TEST_CLASS_SETUP(classSetup);

  BEGIN_TEST_METHOD(unaryMathOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#UnaryMathOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(binaryOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#BinaryOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(binaryMathOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#BinaryMathOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(binaryComparisonOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#BinaryComparisonOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(bitwiseOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#bitwiseOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(ternaryMathOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#TernaryMathOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(trigonometricOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#TrigonometricOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(unaryOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#UnaryOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(asTypeOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#AsTypeOpTable")
  END_TEST_METHOD()

private:
  bool Initialized = false;
  bool VerboseLogging = false;
};

} // namespace LongVector

#endif // LONGVECTORS_H
