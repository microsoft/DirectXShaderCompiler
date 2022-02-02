//===-- Predefines.h - Predefines for SPIR-V ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===--------------------------------------------------------------===//
//
//  This file includes predefined struct/class, functions, variables,
//  types, and constants for SPIR-V as constant strings.
//
//===--------------------------------------------------------------===//

#include "clang/SPIRV/Predefines.h"

static const char* kGLMatrixTypes = R"(
#define DefineGLMatrix(type, rows, cols) \
    struct gl_##type##rows##x##cols { \
      type##rows##x##cols _m; \
      type##rows##x1 operator *(type##cols##x1 rhs); \
      type##rows##x2 operator *(type##cols##x2 rhs); \
      type##rows##x3 operator *(type##cols##x3 rhs); \
      type##rows##x4 operator *(type##cols##x4 rhs); \
      type##rows##x1 operator *(gl_##type##cols##x1 rhs); \
      type##rows##x2 operator *(gl_##type##cols##x2 rhs); \
      type##rows##x3 operator *(gl_##type##cols##x3 rhs); \
      type##rows##x4 operator *(gl_##type##cols##x4 rhs); \
    }

#define DefineGLMatrixMul(type, rows, cols) \
    type##rows##x1 gl_##type##rows##x##cols::operator *(type##cols##x1 rhs) { \
      return mul(_m, rhs); \
    } \
    type##rows##x2 gl_##type##rows##x##cols::operator *(type##cols##x2 rhs) { \
      return mul(_m, rhs); \
    } \
    type##rows##x3 gl_##type##rows##x##cols::operator *(type##cols##x3 rhs) { \
      return mul(_m, rhs); \
    } \
    type##rows##x4 gl_##type##rows##x##cols::operator *(type##cols##x4 rhs) { \
      return mul(_m, rhs); \
    } \
    type##rows##x1 gl_##type##rows##x##cols::operator *(gl_##type##cols##x1 rhs) { \
      return mul(_m, rhs._m); \
    } \
    type##rows##x2 gl_##type##rows##x##cols::operator *(gl_##type##cols##x2 rhs) { \
      return mul(_m, rhs._m); \
    } \
    type##rows##x3 gl_##type##rows##x##cols::operator *(gl_##type##cols##x3 rhs) { \
      return mul(_m, rhs._m); \
    } \
    type##rows##x4 gl_##type##rows##x##cols::operator *(gl_##type##cols##x4 rhs) { \
      return mul(_m, rhs._m); \
    }

DefineGLMatrix(float, 1, 1);
DefineGLMatrix(float, 1, 2);
DefineGLMatrix(float, 1, 3);
DefineGLMatrix(float, 1, 4);

DefineGLMatrix(float, 2, 1);
DefineGLMatrix(float, 2, 2);
DefineGLMatrix(float, 2, 3);
DefineGLMatrix(float, 2, 4);

DefineGLMatrix(float, 3, 1);
DefineGLMatrix(float, 3, 2);
DefineGLMatrix(float, 3, 3);
DefineGLMatrix(float, 3, 4);

DefineGLMatrix(float, 4, 1);
DefineGLMatrix(float, 4, 2);
DefineGLMatrix(float, 4, 3);
DefineGLMatrix(float, 4, 4);

DefineGLMatrixMul(float, 1, 1);
DefineGLMatrixMul(float, 1, 2);
DefineGLMatrixMul(float, 1, 3);
DefineGLMatrixMul(float, 1, 4);

DefineGLMatrixMul(float, 2, 1);
DefineGLMatrixMul(float, 2, 2);
DefineGLMatrixMul(float, 2, 3);
DefineGLMatrixMul(float, 2, 4);

DefineGLMatrixMul(float, 3, 1);
DefineGLMatrixMul(float, 3, 2);
DefineGLMatrixMul(float, 3, 3);
DefineGLMatrixMul(float, 3, 4);

DefineGLMatrixMul(float, 4, 1);
DefineGLMatrixMul(float, 4, 2);
DefineGLMatrixMul(float, 4, 3);
DefineGLMatrixMul(float, 4, 4);
)";

static const char* kRawBufferLoad = R"(
namespace vk {

template<typename T>
T RawBufferLoad(uint64_t addr) {
    T output;
    vk::RawBufferLoadToParam(output, addr);
    return output;
}

}
)";

namespace clang {
namespace spirv {

void BuildPredefinesForSPIRV(llvm::raw_ostream &Output,
                             bool isTemplateEnabled) {
  if (isTemplateEnabled) {
    Output << kRawBufferLoad;
  }
  Output << kGLMatrixTypes;
}

} // end namespace spirv
} // end namespace clang
