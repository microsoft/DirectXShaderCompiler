// Run: %dxc -T ps_6_0 -E main

struct S {
  ByteAddressBuffer foo;
  uint bar;

  void AccessMember() {
//CHECK:      [[foo:%\d+]] = OpAccessChain %_ptr_Function__ptr_Uniform_type_ByteAddressBuffer %param_this %int_0
//CHECK-NEXT:                OpLoad %_ptr_Uniform_type_ByteAddressBuffer [[foo]]
    foo.Load(0);
  }
};

struct T {
  S first;
  uint second;

  void AccessMemberRecursive() {
//CHECK:      [[foo:%\d+]] = OpAccessChain %_ptr_Function__ptr_Uniform_type_ByteAddressBuffer %param_this_0 %int_0 %int_0
//CHECK-NEXT:                OpLoad %_ptr_Uniform_type_ByteAddressBuffer [[foo]]
    first.foo.Load(0);
  }
};

void AccessParam(S input) {
//CHECK:      [[foo:%\d+]] = OpAccessChain %_ptr_Function__ptr_Uniform_type_ByteAddressBuffer %input %int_0
//CHECK-NEXT:                OpLoad %_ptr_Uniform_type_ByteAddressBuffer [[foo]]
  input.foo.Load(0);
}

void main() {
  S a;
  T b;

  a.AccessMember();
  b.AccessMemberRecursive();
  AccessParam(a);
}
