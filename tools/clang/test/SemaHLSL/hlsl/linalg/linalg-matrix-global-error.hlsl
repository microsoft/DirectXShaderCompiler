// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main -verify %s

// Globals containing LinAlg matrices are mutable state, not constant buffer
// data, so they must be explicitly declared 'static'.

#include <dx/linalg.h>
using namespace dx::linalg;

using MatrixTy =
    Matrix<ComponentType::F32, 4, 4, MatrixUse::Accumulator,
           MatrixScope::Wave>;

using HandleTy = __builtin_LinAlgMatrix
    [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4,
                                MatrixUse::Accumulator, MatrixScope::Wave)]];

struct MatrixState {
  MatrixTy Mat;
};

struct DerivedState : MatrixState {
  float F;
};

struct NumericState {
  float4 V;
};

template <typename T> struct Wrapper {
  T Val;
};

// expected-error@+1 {{global variable 'GMat' containing a linear algebra matrix must be declared 'static'}}
MatrixTy GMat;
// expected-error@+1 {{global variable 'GMatArr' containing a linear algebra matrix must be declared 'static'}}
MatrixTy GMatArr[2];
// expected-error@+1 {{global variable 'GMatArr2D' containing a linear algebra matrix must be declared 'static'}}
MatrixTy GMatArr2D[2][3];
// expected-error@+1 {{global variable 'GState' containing a linear algebra matrix must be declared 'static'}}
MatrixState GState;
// expected-error@+1 {{global variable 'GDerived' containing a linear algebra matrix must be declared 'static'}}
DerivedState GDerived;
// expected-error@+1 {{global variable 'GWrapped' containing a linear algebra matrix must be declared 'static'}}
Wrapper<MatrixTy> GWrapped;
// expected-error@+1 {{global variable 'GHandle' containing a linear algebra matrix must be declared 'static'}}
HandleTy GHandle;
// expected-error@+1 {{global variable 'GRawHandle' containing a linear algebra matrix must be declared 'static'}}
__builtin_LinAlgMatrix GRawHandle;

// LinAlg matrices cannot be stored in groupshared memory, even when static.
// expected-error@+1 {{object 'HandleT' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::Accumulator, MatrixScope::Wave)]]') is not allowed in groupshared variables}}
groupshared MatrixTy GSMat;
// expected-error@+1 {{object 'HandleT' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::Accumulator, MatrixScope::Wave)]]') is not allowed in groupshared variables}}
static groupshared MatrixTy SGSMatArr[2];
// expected-error@+1 {{object 'HandleT' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::Accumulator, MatrixScope::Wave)]]') is not allowed in groupshared variables}}
groupshared DerivedState GSDerived;
// expected-note@dx/linalg.h:* 3 {{field declared here}}
// expected-error@+1 {{object 'HandleTy' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::Accumulator, MatrixScope::Wave)]]') is not allowed in groupshared variables}}
static groupshared HandleTy SGSHandle;
// expected-error@+1 {{object '__builtin_LinAlgMatrix' is not allowed in groupshared variables}}
groupshared __builtin_LinAlgMatrix GSRawHandle;
groupshared float4 GSNumeric;

cbuffer CB {
  // expected-error@+1 {{global variable 'CBMat' containing a linear algebra matrix must be declared 'static'}}
  MatrixTy CBMat;
  static MatrixTy CBStaticMat;
};

namespace NS {
// expected-error@+1 {{global variable 'NSMat' containing a linear algebra matrix must be declared 'static'}}
MatrixTy NSMat;
static MatrixTy NSStaticMat;
}

static MatrixTy SMat;
static MatrixTy SMatArr[2];
static MatrixState SState;
static DerivedState SDerived;
static Wrapper<MatrixTy> SWrapped;
static HandleTy SHandle;
static __builtin_LinAlgMatrix SRawHandle;

// Globals without LinAlg matrices are unaffected.
NumericState GNumeric;
Wrapper<float> GWrappedFloat;
RWByteAddressBuffer Output;

struct StaticMember {
  static MatrixTy Mat;
};

typedef MatrixTy MatrixTypedef;

[numthreads(1, 1, 1)]
void main() {
  MatrixTy LocalMat = MatrixTy::Splat(1.0f);
  static MatrixTy LocalStaticMat;
  SMat = LocalMat;
  Output.Store(0, SMat.Get(0) + GNumeric.V.x + GWrappedFloat.Val);
}
