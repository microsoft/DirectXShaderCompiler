// RUN: %dxc -T lib_6_8 -HV 2021 -verify %s
// RUN: %dxc -T lib_6_8 -HV 202x -verify %s

struct StringState {
  string Text; // expected-note 19 {{field declared here}}
};

struct NestedState {
  StringState Values[2];
};

struct DerivedState : StringState {
  uint Value;
};

struct Numeric {
  uint Value;
};

template <typename T>
struct Wrapper {
  T Value;
};

// expected-error@+1 {{object 'const string' is not allowed in ConstantBuffers or TextureBuffers}}
ConstantBuffer<StringState> CB;
// expected-error@+1 {{object 'const string' is not allowed in ConstantBuffers or TextureBuffers}}
TextureBuffer<NestedState> TB;
// expected-error@+1 {{object 'const string' is not allowed in structured buffers}}
StructuredBuffer<StringState> SB;
// expected-error@+1 {{object 'const string' is not allowed in structured buffers}}
RWStructuredBuffer<NestedState> RWSB;
// expected-error@+1 {{object 'const string' is not allowed in structured buffers}}
AppendStructuredBuffer<DerivedState> ASB;
// expected-error@+1 {{object 'const string' is not allowed in structured buffers}}
ConsumeStructuredBuffer<StringState> CSB;
// expected-error@+1 {{object 'const string' is not allowed in structured buffers}}
RasterizerOrderedStructuredBuffer<DerivedState> ROSB;
// expected-error@+1 {{object 'const string' is not allowed in structured buffers}}
StructuredBuffer<string> DirectSB;
// expected-error@+1 {{object 'const string' is not allowed in structured buffers}}
RWStructuredBuffer<Wrapper<StringState> > Wrapped;

struct ForwardStringState;
typedef StructuredBuffer<ForwardStringState> ForwardStringBuffer;
typedef RWStructuredBuffer<ForwardStringState> ForwardRWStringBuffer;

struct ForwardStringState {
  string Text; // expected-note 2 {{field declared here}}
};

// expected-error@+1 {{object 'const string' is not allowed in structured buffers}}
ForwardStringBuffer ForwardSB;
// expected-error@+1 {{object 'const string' is not allowed in structured buffers}}
ForwardRWStringBuffer ForwardRWSB;

// expected-error@+1 {{'const string' cannot be used as a type parameter}}
Buffer<string> DirectBuffer;
// expected-error@+2 {{'const string' cannot be used as a type parameter}}
// expected-note@+1 {{usage of 'const string' found in field 'Text'}}
RWTexture2D<StringState> Texture;

// expected-error@+1 {{object 'const string' is not allowed in cbuffers or tbuffers}}
NestedState Global;
cbuffer Constants {
  // expected-error@+1 {{object 'const string' is not allowed in cbuffers or tbuffers}}
  StringState State;
};
tbuffer TextureConstants {
  // expected-error@+1 {{object 'const string' is not allowed in cbuffers or tbuffers}}
  DerivedState Derived;
};

// expected-error@+1 {{object 'const string' is not allowed in groupshared variables}}
groupshared string SharedString;
// expected-error@+1 {{object 'const string' is not allowed in groupshared variables}}
groupshared StringState Shared;
// expected-error@+1 {{object 'const string' is not allowed in groupshared variables}}
groupshared NestedState SharedArray[2];
// expected-error@+1 {{object 'const string' is not allowed in groupshared variables}}
groupshared DerivedState SharedDerived;

// expected-error@+1 {{object 'const string' is not allowed in groupshared variables}}
void SharedParameter(groupshared StringState State);
#if __HLSL_VERSION <= 2021
// expected-warning@-2 {{Support for groupshared parameter annotation not added until HLSL 202x}}
#endif

ByteAddressBuffer Bytes;
RWByteAddressBuffer RWBytes;

void ByteAddressAccess(StringState A, NestedState B, DerivedState C) {
  // expected-error@+2 {{Explicit template arguments on intrinsic Load must be a single numeric type}}
  // expected-error@+1 {{object 'const string' is not allowed in builtin template parameters}}
  A = Bytes.Load<StringState>(0);
  // expected-error@+2 {{Explicit template arguments on intrinsic Load must be a single numeric type}}
  // expected-error@+1 {{object 'const string' is not allowed in builtin template parameters}}
  B = RWBytes.Load<NestedState>(0);
  // expected-error@+2 {{Explicit template arguments on intrinsic Store must be a single numeric type}}
  // expected-error@+1 {{object 'const string' is not allowed in builtin template parameters}}
  RWBytes.Store<DerivedState>(0, C);
  // expected-error@+2 {{object 'const string' is not allowed in builtin template parameters}}
  // expected-error@+1 {{no matching member function for call to 'Store'}}
  RWBytes.Store(0, C);
  // expected-note@? {{candidate template ignored: couldn't infer template argument 'TResult'}}
}

ConstantBuffer<Numeric> NumericCB;
RWStructuredBuffer<Numeric> NumericBuffer;
groupshared Numeric SharedNumeric[2];
static StringState PrivateState;

void PrivateStrings() {
  string Text = "hello";
  StringState Local = {Text};
  NestedState Nested = {{Local, Local}};
  Text = "goodbye";
  Local.Text = Text;
  PrivateState = Local;
}

[shader("pixel")]
float4 StringInput(string Text : TEXCOORD0) : SV_Target {
  // expected-error@-1 {{object 'const string' is not allowed in entry function parameters}}
  return 0;
}

[shader("pixel")]
float4 StringOutput(out string Text : SV_Target1) : SV_Target0 {
  // expected-error@-1 {{object 'const string' is not allowed in entry function parameters}}
  Text = "hello";
  return 0;
}

[shader("pixel")]
float4 StringInOut(inout string Text : TEXCOORD0) : SV_Target {
  // expected-error@-1 {{object 'const string' is not allowed in entry function parameters}}
  Text = "hello";
  return 0;
}
