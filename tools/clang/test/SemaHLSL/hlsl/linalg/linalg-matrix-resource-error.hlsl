// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -Wno-hlsl-availability -verify %s

// LinAlg matrices are opaque, thread-local values and cannot be stored in any
// resource, node record, patch, or stream, either directly or nested in a
// struct.

#include <dx/linalg.h>
using namespace dx::linalg;

using MatrixTy =
    Matrix<ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave>;

using HandleTy = __builtin_LinAlgMatrix
    [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A,
                                MatrixScope::Wave)]];

// The Matrix class holds its handle in a private field.
// expected-note@dx/linalg.h:* 9 {{field declared here}}

struct MatrixState {
  MatrixTy Mat;
};

struct HandleState {
  HandleTy Handle; // expected-note 6 {{field declared here}}
};

struct RawHandleState {
  __builtin_LinAlgMatrix Handle; // expected-note 6 {{field declared here}}
};

struct DerivedState : MatrixState {
  float F;
};

struct Numeric {
  float4 V;
};

// ConstantBuffer and TextureBuffer.
// expected-error@+1 {{object 'HandleT' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in ConstantBuffers or TextureBuffers}}
ConstantBuffer<MatrixTy> CBMat;
// expected-error@+1 {{object 'HandleT' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in ConstantBuffers or TextureBuffers}}
ConstantBuffer<MatrixState> CBState;
// expected-error@+1 {{object 'HandleT' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in ConstantBuffers or TextureBuffers}}
ConstantBuffer<DerivedState> CBDerived;
// expected-error@+1 {{object 'HandleTy' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in ConstantBuffers or TextureBuffers}}
ConstantBuffer<HandleTy> CBHandle;
// expected-error@+1 {{object 'HandleTy' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in ConstantBuffers or TextureBuffers}}
ConstantBuffer<HandleState> CBHandleState;
// expected-error@+1 {{object '__builtin_LinAlgMatrix' is not allowed in ConstantBuffers or TextureBuffers}}
ConstantBuffer<__builtin_LinAlgMatrix> CBRawHandle;
// expected-error@+1 {{object '__builtin_LinAlgMatrix' is not allowed in ConstantBuffers or TextureBuffers}}
ConstantBuffer<RawHandleState> CBRawHandleState;
// expected-error@+1 {{object 'HandleT' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in ConstantBuffers or TextureBuffers}}
TextureBuffer<MatrixState> TBState;
// expected-error@+1 {{object 'HandleTy' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in ConstantBuffers or TextureBuffers}}
TextureBuffer<HandleState> TBHandleState;
// expected-error@+1 {{object '__builtin_LinAlgMatrix' is not allowed in ConstantBuffers or TextureBuffers}}
TextureBuffer<RawHandleState> TBRawHandleState;

// Structured buffers.
// expected-error@+1 {{object 'HandleT' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in structured buffers}}
StructuredBuffer<MatrixTy> SBMat;
// expected-error@+1 {{object 'HandleT' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in structured buffers}}
RWStructuredBuffer<MatrixState> RWSBState;
// expected-error@+1 {{object 'HandleTy' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in structured buffers}}
AppendStructuredBuffer<HandleState> ASBHandleState;
// expected-error@+1 {{object '__builtin_LinAlgMatrix' is not allowed in structured buffers}}
ConsumeStructuredBuffer<RawHandleState> CSBRawHandleState;
// expected-error@+1 {{object '__builtin_LinAlgMatrix' is not allowed in structured buffers}}
RasterizerOrderedStructuredBuffer<__builtin_LinAlgMatrix> ROSBRawHandle;

// Typed buffers and textures.
// expected-error@+1 {{'HandleTy' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') cannot be used as a type parameter}}
Buffer<HandleTy> BufHandle;
// expected-error@+1 {{'__builtin_LinAlgMatrix' cannot be used as a type parameter}}
RWBuffer<__builtin_LinAlgMatrix> RWBufRawHandle;
// expected-error@+2 {{'HandleT' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') cannot be used as a type parameter}}
// expected-note@+1 {{usage of 'HandleT' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') found in field '__handle' of type 'MatrixTy' (aka 'Matrix<ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave>')}}
Texture2D<MatrixTy> TexMat;
// expected-error@+2 {{'__builtin_LinAlgMatrix' cannot be used as a type parameter}}
// expected-note@+1 {{usage of '__builtin_LinAlgMatrix' found in field 'Handle' of type 'RawHandleState'}}
RWTexture3D<RawHandleState> RWTexRawHandleState;

// Numeric element types remain valid.
ConstantBuffer<Numeric> CBNumeric;
TextureBuffer<Numeric> TBNumeric;
StructuredBuffer<Numeric> SBNumeric;
Buffer<float4> BufNumeric;

ByteAddressBuffer BAB;
RWByteAddressBuffer RWBAB;

void ByteAddressBufferTemplates() {
  // expected-error@+2 {{Explicit template arguments on intrinsic Load must be a single numeric type}}
  // expected-error@+1 {{object 'HandleT' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in builtin template parameters}}
  MatrixState A = BAB.Load<MatrixState>(0);
  // expected-error@+2 {{Explicit template arguments on intrinsic Load must be a single numeric type}}
  // expected-error@+1 {{object '__builtin_LinAlgMatrix' is not allowed in builtin template parameters}}
  __builtin_LinAlgMatrix B = BAB.Load<__builtin_LinAlgMatrix>(0);
  RawHandleState C;
  // expected-error@+2 {{Explicit template arguments on intrinsic Store must be a single numeric type}}
  // expected-error@+1 {{object '__builtin_LinAlgMatrix' is not allowed in builtin template parameters}}
  RWBAB.Store<RawHandleState>(0, C);
  HandleState D;
  // expected-error@+2 {{Explicit template arguments on intrinsic Store must be a single numeric type}}
  // expected-error@+1 {{object 'HandleTy' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in builtin template parameters}}
  RWBAB.Store<HandleState>(0, D);

  Numeric N = BAB.Load<Numeric>(0);
  RWBAB.Store<Numeric>(0, N);
}

// Node records.
// expected-error@+1 {{object 'HandleT' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in node records}}
void NodeInputMat(DispatchNodeInputRecord<MatrixState> In);
// expected-error@+1 {{object 'HandleTy' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in node records}}
void NodeOutputHandle(NodeOutput<HandleState> Out);
// expected-error@+1 {{object '__builtin_LinAlgMatrix' is not allowed in node records}}
void NodeOutputRawHandle(NodeOutput<RawHandleState> Out);
void NodeNumeric(DispatchNodeInputRecord<Numeric> In, NodeOutput<Numeric> Out);

// Tessellation patches and geometry streams.
// expected-error@+1 {{object 'HandleT' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in tessellation patches}}
void PatchMat(InputPatch<MatrixState, 3> P);
// expected-error@+1 {{object 'HandleTy' (aka '__builtin_LinAlgMatrix [[__LinAlgMatrix_Attributes(ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Wave)]]') is not allowed in tessellation patches}}
void PatchHandle(OutputPatch<HandleState, 3> P);
// expected-error@+1 {{object '__builtin_LinAlgMatrix' is not allowed in geometry streams}}
void StreamRawHandle(inout PointStream<RawHandleState> S);
void PatchAndStreamNumeric(InputPatch<Numeric, 3> P,
                           inout TriangleStream<Numeric> S);
