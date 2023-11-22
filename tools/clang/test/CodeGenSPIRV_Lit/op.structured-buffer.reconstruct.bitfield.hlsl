// RUN: %dxc -T cs_6_0 -E main -HV 2021

struct Base {
  uint base;
};

struct Derived : Base {
  uint a;
  uint b : 3;
  uint c : 3;
  uint d;
};

RWStructuredBuffer<Derived> g_probes : register(u0);

[numthreads(64u, 1u, 1u)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID) {

// CHECK:     [[p:%\w+]] = OpVariable %_ptr_Function_Derived_0 Function
  Derived p;

// CHECK:   [[tmp:%\d+]] = OpAccessChain %_ptr_Function_Base_0 [[p]] %uint_0
// CHECK:   [[tmp:%\d+]] = OpAccessChain %_ptr_Function_uint [[tmp]] %int_0
// CHECK:                  OpStore [[tmp]] %uint_5
  p.base = 5;

// CHECK:   [[tmp:%\d+]] = OpAccessChain %_ptr_Function_uint [[p]] %int_1
// CHECK:                  OpStore [[tmp]] %uint_1
  p.a = 1;

// CHECK:   [[tmp:%\d+]] = OpAccessChain %_ptr_Function_uint [[p]] %int_2
// CHECK: [[value:%\d+]] = OpLoad %uint [[tmp]]
// CHECK: [[value:%\d+]] = OpBitFieldInsert %uint [[value]] %uint_2 %uint_0 %uint_3
// CHECK:                  OpStore [[tmp]] [[value]]
  p.b = 2;

// CHECK:   [[tmp:%\d+]] = OpAccessChain %_ptr_Function_uint [[p]] %int_2
// CHECK: [[value:%\d+]] = OpLoad %uint [[tmp]]
// CHECK: [[value:%\d+]] = OpBitFieldInsert %uint [[value]] %uint_3 %uint_3 %uint_3
// CHECK:                  OpStore [[tmp]] [[value]]
  p.c = 3;

// CHECK:   [[tmp:%\d+]] = OpAccessChain %_ptr_Function_uint [[p]] %int_3
// CHECK:                  OpStore [[tmp]] %uint_4
  p.d = 4;


// CHECK:     [[p:%\d+]] = OpLoad %Derived_0 [[p]]
// CHECK:   [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_Derived %g_probes %int_0 %uint_0
// CHECK:   [[tmp:%\d+]] = OpCompositeExtract %Base_0 [[p]] 0
// CHECK:   [[tmp:%\d+]] = OpCompositeExtract %uint [[tmp]] 0
// CHECK:  [[base:%\d+]] = OpCompositeConstruct %Base [[tmp]]
// CHECK:  [[mem1:%\d+]] = OpCompositeExtract %uint [[p]] 1
// CHECK:  [[mem2:%\d+]] = OpCompositeExtract %uint [[p]] 2
// CHECK:  [[mem3:%\d+]] = OpCompositeExtract %uint [[p]] 3
// CHECK:   [[tmp:%\d+]] = OpCompositeConstruct %Derived [[base]] [[mem1]] [[mem2]] [[mem3]]
// CHECK:                  OpStore [[ptr]] [[tmp]]
	g_probes[0] = p;
}

