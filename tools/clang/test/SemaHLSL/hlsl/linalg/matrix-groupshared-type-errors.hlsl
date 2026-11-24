// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -verify %s

#include <dx/linalg.h>
using namespace dx::linalg;

using Mat = Matrix<ComponentType::U8, 8, 8, MatrixUse::A, MatrixScope::Wave>;
groupshared float Floats[64];
groupshared float2 FloatVectors[64];
groupshared bool Bools[64];

void invalidLoads() {
  // expected-error@+1{{no matching function for call to 'Load'}}
  Mat M = Mat::Load(Floats, 0, 8, MatrixLayout::RowMajor);
  // expected-error@+1{{no matching function for call to 'Load'}}
  M = Mat::Load(FloatVectors, 0, 8, MatrixLayout::RowMajor);
  // expected-error@+1{{no matching function for call to 'Load'}}
  M = Mat::Load(Bools, 0, 8, MatrixLayout::RowMajor);
}

void invalidStores(Mat M) {
  // expected-error@+1{{no matching member function for call to 'Store'}}
  M.Store(Floats, 0, 8, MatrixLayout::RowMajor);
  // expected-error@+1{{no matching member function for call to 'Store'}}
  M.Store(FloatVectors, 0, 8, MatrixLayout::RowMajor);
  // expected-error@+1{{no matching member function for call to 'Store'}}
  M.Store(Bools, 0, 8, MatrixLayout::RowMajor);
}

// expected-note@dx/linalg.h:* 9{{candidate function}}
// expected-note@dx/linalg.h:* 6{{candidate template ignored}}
