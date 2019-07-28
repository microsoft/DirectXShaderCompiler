// Run: %dxc -T ps_6_0 -E main -Zi

// CHECK:      [[file:%\d+]] = OpString
// CHECK-SAME: spirv.debug.opline.function.hlsl

void foo(in float4 a, out float3 b);

// CHECK:              OpLine [[file]] 30 1
// CHECK-NEXT: %main = OpFunction %void None

void bar(int a, in float b, inout bool2 c, const float3 d, out uint4 e) {
}

float4 getV4f(float x, int y, bool z);

struct R {
  int a;
  void incr();
  void decr() { --a; }
};

RWStructuredBuffer<R> rwsb;

void decr(inout R a, in R b, out R c, R d, const R e);

groupshared R r[5];

R getR(uint i);

void main() {
  float4 v4f;
  float3 v3f;

// CHECK:                     OpLine [[file]] 36 7
// CHECK-NEXT: %param_var_a = OpVariable %_ptr_Function_v4float Function
  foo(v4f, v3f);
// CHECK:                     OpLine [[file]] 39 14
// CHECK-NEXT: %param_var_x = OpVariable %_ptr_Function_float Function
  foo(getV4f(v4f.x,
// CHECK:                     OpLine [[file]] 46 21
// CHECK-NEXT: %param_var_x_0 = OpVariable %_ptr_Function_float Function
// CHECK-NEXT:                OpLine [[file]] 46 28
// CHECK-NEXT: %param_var_y = OpVariable %_ptr_Function_int Function
// CHECK-NEXT:                OpLine [[file]] 46 35
// CHECK-NEXT: %param_var_z = OpVariable %_ptr_Function_bool Function
             getV4f(v4f.y, v4f.z, v4f.w).z,
// CHECK:                     OpLine [[file]] 46 14
// CHECK-NEXT: %param_var_y_0 = OpVariable %_ptr_Function_int Function
// CHECK-NEXT:                OpLine [[file]] 51 14
// CHECK-NEXT: %param_var_z_0 = OpVariable %_ptr_Function_bool Function
             v3f.y),
      v3f);
// CHECK-NEXT:                OpLine [[file]] 39 7
// CHECK-NEXT: %param_var_a_0 = OpVariable %_ptr_Function_v4float Function

  r[0].incr();
  decr(r[0],
// CHECK-NEXT:                OpLine [[file]] 62 13
// CHECK-NEXT: %param_var_i = OpVariable %_ptr_Function_uint Function
// CHECK-NEXT:                OpLine [[file]] 62 8
// CHECK-NEXT: %param_var_b = OpVariable %_ptr_Function_R_0 Function
       getR(1),
       r[2],
// CHECK-NEXT:                OpLine [[file]] 68 13
// CHECK-NEXT: %param_var_i_0 = OpVariable %_ptr_Function_uint Function
// CHECK-NEXT:                OpLine [[file]] 68 8
// CHECK-NEXT: %param_var_d = OpVariable %_ptr_Function_R_0 Function
       getR(3),
// CHECK-NEXT:                OpLine [[file]] 73 13
// CHECK-NEXT: %param_var_i_1 = OpVariable %_ptr_Function_uint Function
// CHECK-NEXT:                OpLine [[file]] 73 8
// CHECK-NEXT: %param_var_e = OpVariable %_ptr_Function_R_0 Function
       getR(4));

  rwsb[0].incr();

  decr(rwsb[1],
// CHECK-NEXT:                OpLine [[file]] 80 8
// CHECK-NEXT: %param_var_b_0 = OpVariable %_ptr_Function_R_0 Function
       rwsb[2],
       rwsb[3],
// CHECK-NEXT:                OpLine [[file]] 84 8
// CHECK-NEXT: %param_var_d_0 = OpVariable %_ptr_Function_R_0 Function
       rwsb[4],
// CHECK-NEXT:                OpLine [[file]] 87 8
// CHECK-NEXT: %param_var_e_0 = OpVariable %_ptr_Function_R_0 Function
       rwsb[5]);
}

// CHECK:             OpLine [[file]] 96 1
// CHECK-NEXT: %foo = OpFunction %void None
// CHECK-NEXT:        OpLine [[file]] 96 20
// CHECK-NEXT:   %a = OpFunctionParameter %_ptr_Function_v4float
// CHECK-NEXT:        OpLine [[file]] 96 34
// CHECK-NEXT:   %b = OpFunctionParameter %_ptr_Function_v3float
void foo(in float4 a, out float3 b) {
  a = b.xxzz;
  b = a.yzw;
// CHECK:                     OpLine [[file]] 103 7
// CHECK-NEXT: %param_var_a_1 = OpVariable %_ptr_Function_int Function
// CHECK-NEXT:                OpLine [[file]] 103 12
// CHECK-NEXT: %param_var_b_1 = OpVariable %_ptr_Function_float Function
  bar(a.x, b.y, a.yz, b, a);
}

// CHECK:                     OpLine [[file]] 112 1
// CHECK-NEXT:      %getV4f = OpFunction %v4float None
// CHECK-NEXT:                OpLine [[file]] 112 21
// CHECK-NEXT:           %x = OpFunctionParameter %_ptr_Function_float
// CHECK-NEXT:                OpLine [[file]] 112 28
// CHECK-NEXT:           %y = OpFunctionParameter %_ptr_Function_int
float4 getV4f(float x, int y, bool z) { return float4(x, y, z, z); }

// CHECK:                     OpLine [[file]] 117 1
// CHECK-NEXT:      %R_incr = OpFunction %void None
// CHECK-NEXT:  %param_this = OpFunctionParameter %_ptr_Function_R_0
void R::incr() { ++a; }

// CHECK:                     OpLine [[file]] 123 1
// CHECK-NEXT:        %getR = OpFunction %R_0 None
// CHECK-NEXT:                OpLine [[file]] 123 13
// CHECK-NEXT:           %i = OpFunctionParameter %_ptr_Function_uint
R getR(uint i) { return r[i]; }

// CHECK:                     OpLine [[file]] 131 1
// CHECK-NEXT:        %decr = OpFunction %void None
// CHECK-NEXT:                OpLine [[file]] 131 19
// CHECK-NEXT:         %a_0 = OpFunctionParameter %_ptr_Function_R_0
// CHECK-NEXT:                OpLine [[file]] 131 27
// CHECK-NEXT:         %b_0 = OpFunctionParameter %_ptr_Function_R_0
void decr(inout R a, in R b, out R c, R d, const R e) { a.a--; }

// CHECK:             OpLine [[file]] 11 1
// CHECK-NEXT: %bar = OpFunction %void None
// CHECK-NEXT:        OpLine [[file]] 11 14
// CHECK-NEXT: %a_1 = OpFunctionParameter %_ptr_Function_int
// CHECK-NEXT:        OpLine [[file]] 11 26
// CHECK-NEXT: %b_1 = OpFunctionParameter %_ptr_Function_float
// CHECK-NEXT:        OpLine [[file]] 11 41
// CHECK-NEXT: %c_0 = OpFunctionParameter %_ptr_Function_v2bool
// CHECK-NEXT:        OpLine [[file]] 11 57
// CHECK-NEXT: %d_0 = OpFunctionParameter %_ptr_Function_v3float
// CHECK-NEXT:        OpLine [[file]] 11 70
// CHECK-NEXT: %e_0 = OpFunctionParameter %_ptr_Function_v4uint
