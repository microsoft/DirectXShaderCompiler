// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -verify %s

#include <dx/linalg.h>
using namespace dx::linalg;

using MatrixTy =
    Matrix<ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Thread>;

void validTypes(float4 FloatVec, uint4 UintVec,
                vector<uint8_t4_packed, 1> PackedVec,
                vector<int8_t4_packed, 1> SignedPackedVec) {
  MakeInterpretedVector<ComponentType::F32>(FloatVec);
  MakeInterpretedVector<ComponentType::F8_E4M3FN>(UintVec);
  MakeInterpretedVector<ComponentType::F8_E4M3FN>(PackedVec);
  MakeInterpretedVector<ComponentType::F8_E4M3FN>(SignedPackedVec);

  Convert<ComponentType::U32, ComponentType::F32>(FloatVec);
  Convert<ComponentType::F32, ComponentType::U32>(UintVec);
  Convert<ComponentType::F32, ComponentType::F8_E4M3FN>(PackedVec);
  Convert<ComponentType::F32, ComponentType::F32>(FloatVec);
  Convert<ComponentType::F8_E4M3FN, ComponentType::F8_E4M3FN>(PackedVec);
}

void invalidFactories(float4 FloatVec, uint4 UintVec) {
  // expected-error@+1{{no matching function for call to 'MakeInterpretedVector'}}
  MakeInterpretedVector<ComponentType::F32>(UintVec);
  // expected-error@+1{{no matching function for call to 'MakeInterpretedVector'}}
  MakeInterpretedVector<ComponentType::F8_E4M3FN>(FloatVec);

  // expected-error@+1{{no matching function for call to 'Convert'}}
  Convert<ComponentType::U32, ComponentType::F32>(UintVec);
  // expected-error@+1{{no matching function for call to 'Convert'}}
  Convert<ComponentType::F32, ComponentType::F8_E4M3FN>(FloatVec);
  // expected-error@+1{{no matching function for call to 'Convert'}}
  Convert<ComponentType::F32, ComponentType::F32>(UintVec);
  // expected-error@+1{{no matching function for call to 'Convert'}}
  Convert<ComponentType::F8_E4M3FN, ComponentType::F8_E4M3FN>(FloatVec);
}

void invalidConsumers(MatrixTy Mat, ByteAddressBuffer Buf, float4 Bias) {
  InterpretedVector<float, 1, ComponentType::F8_E4M3FN> Invalid = {0};

  // expected-error@+1{{no matching function for call to 'Multiply'}}
  Multiply<float>(Mat, Invalid);
  // expected-error@+1{{no matching function for call to 'MultiplyAdd'}}
  MultiplyAdd<float>(Mat, Invalid, Bias);

  VectorRef<ComponentType::F32, 4> Ref = {Buf, 0};
  // expected-error@+1{{no matching function for call to 'MultiplyAdd'}}
  MultiplyAdd<float>(Mat, Invalid, Ref);
}

// expected-note@dx/linalg.h:* 24{{candidate template ignored}}
