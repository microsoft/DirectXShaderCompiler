// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

mutable int globalValue; // expected-error {{'mutable' can only be applied to member variables}}

struct S {
  mutable void function(); // expected-error {{'mutable' cannot be applied to functions}}

  void local() {
    mutable int localValue; // expected-error {{'mutable' can only be applied to member variables}}
  }
};

struct InvalidMutableFields {
  mutable const int constValue; // expected-error {{'const' is not a valid modifier for a field}}
};

cbuffer DirectCBuffer {
  mutable int directCBufferMember; // expected-error {{'mutable' can only be applied to member variables}}
};

tbuffer DirectTBuffer {
  mutable int directTBufferMember; // expected-error {{'mutable' can only be applied to member variables}}
};

struct MutableData {
  mutable int value; // expected-note 8 {{'int' field declared here}}
};

struct NestedMutableData {
  MutableData data;
};

cbuffer CBufferWithMutableType {
  MutableData data; // expected-error {{'mutable' field 'value' is not allowed in constant or texture buffer data}}
};

tbuffer TBufferWithMutableType {
  MutableData data; // expected-error {{'mutable' field 'value' is not allowed in constant or texture buffer data}}
};

cbuffer CBufferWithNestedMutableType {
  NestedMutableData data; // expected-error {{'mutable' field 'value' is not allowed in constant or texture buffer data}}
};

MutableData implicitCBufferData; // expected-error {{'mutable' field 'value' is not allowed in constant or texture buffer data}}
MutableData implicitCBufferArray[2]; // expected-error {{'mutable' field 'value' is not allowed in constant or texture buffer data}}
ConstantBuffer<MutableData> constantBuffer; // expected-error {{'mutable' field 'value' is not allowed in constant or texture buffer data}}
TextureBuffer<MutableData> textureBuffer; // expected-error {{'mutable' field 'value' is not allowed in constant or texture buffer data}}

StructuredBuffer<MutableData> readOnlyStructuredBuffer; // expected-error {{'mutable' field 'value' is not allowed in read-only structured buffer data}}

groupshared MutableData sharedData;
RWStructuredBuffer<MutableData> structuredBuffer;
