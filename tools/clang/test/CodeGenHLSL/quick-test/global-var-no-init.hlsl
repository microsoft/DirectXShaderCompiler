// RUN: %dxc -E main -T ps_6_0 > %s | FileCheck %s

// CBuffer-promoted global variables should not have initializers
// CHECK-NOT: {{.*var.*}} = constant float 0.000000e+00, align 4
// CHECK-NOT: {{.*var_init.*}} = constant float 0.000000e+00, align 4
// CHECK-NOT: {{.*const_var.*}} = constant float 0.000000e+00, align 4
// CHECK-NOT: {{.*const_var_init.*}} = constant float 0.000000e+00, align 4
// CHECK-NOT: {{.*extern_var.*}} = constant float 0.000000e+00, align 4
// CHECK-NOT: {{.*extern_var_init.*}} = constant float 0.000000e+00, align 4
// CHECK-NOT: {{.*extern_const_var.*}} = constant float 0.000000e+00, align 4
// CHECK-NOT: {{.*extern_const_var_init.*}} = constant float 0.000000e+00, align 4

// ... they should only exist in their CBuffer declaration
// CHECK: cbuffer $Globals
// CHECK: float var;
// CHECK: float var_init;
// CHECK: float const_var;
// CHECK: float const_var_init;
// CHECK: float extern_var;
// CHECK: float extern_var_init;
// CHECK: float extern_const_var;
// CHECK: float extern_const_var_init;

Texture2D tex;
float var;
float var_init = 1;
const float const_var;
const float const_var_init = 1;
extern float extern_var;
extern float extern_var_init = 1;
extern const float extern_const_var;
extern const float extern_const_var_init = 1;

// Those get optimized away
static float static_var;
static float static_var_init = 1;
static const float static_const_var;
static const float static_const_var_init = 1;

struct s
{
  // Those get optimized away
  static float struct_static_var;
  // static float struct_static_var_init = 1; // error: struct/class members cannot have default values
  static const float struct_static_const_var;
  static const float struct_static_const_var_init = 1;
};

float s::struct_static_var = 1;
const float s::struct_static_const_var = 1;

float main() : SV_Target {
  static float func_static_var;
  static float func_static_var_init = 1;
  static const float func_static_const_var;
  static const float func_static_const_var_init = 1;
  return tex.Load((int3)0).x
    + var + var_init
    + const_var + const_var_init
    + extern_var + extern_var_init
    + extern_const_var + extern_const_var_init
    + static_var + static_var_init
    + static_const_var + static_const_var_init
    + s::struct_static_var + /*s::struct_static_var_init*/
    + s::struct_static_const_var + s::struct_static_const_var_init
    + func_static_var + func_static_var_init
    + func_static_const_var + func_static_const_var_init;
}