// RUN: not %dxc -T lib_6_4 %s 2>&1 | FileCheck %s

float not_a_function;

// CHECK-DAG: BackwardDerivative{{.*}}attribute argument must name a function
[[dxc::BackwardDerivative(not_a_function)]]
float bad_name(float x) { return x; }

float bad_return_pullback(float x, float dResult, out float dX) { return x; }

// CHECK-DAG: backward derivative 'bad_return_pullback' must return void
[[dxc::BackwardDerivative(bad_return_pullback)]]
float bad_return(float x) { return x; }

void bad_count_pullback(float x, float dResult) {}

// CHECK-DAG: backward derivative 'bad_count_pullback' requires 3 parameters but has 2
[[dxc::BackwardDerivative(bad_count_pullback)]]
float bad_count(float x) { return x; }

void bad_seed_pullback(float x, float2 dResult, out float dX) {}

// CHECK-DAG: backward derivative 'bad_seed_pullback' parameter 2 must have type 'float'
[[dxc::BackwardDerivative(bad_seed_pullback)]]
float bad_seed(float x) { return x; }

void missing_out_pullback(float x, float dResult, float dX) {}

// CHECK-DAG: backward derivative 'missing_out_pullback' parameter 3 must be an out parameter
[[dxc::BackwardDerivative(missing_out_pullback)]]
float missing_out(float x) { return x; }

// CHECK-DAG: BackwardDerivative{{.*}}attribute argument must name a function
[[dxc::BackwardDerivative(unknown_pullback)]]
float missing_name(float x) { return x; }

// CHECK-DAG: function 'recursive' cannot be its own backward derivative
[[dxc::BackwardDerivative(recursive)]]
float recursive(float x) { return x; }

void bad_input_pullback(out float x, float dResult, out float dX) {}

// CHECK-DAG: backward derivative 'bad_input_pullback' parameter 1 must be an input parameter
[[dxc::BackwardDerivative(bad_input_pullback)]]
float bad_input(float x) { return x; }

void inout_output_pullback(float x, float dResult, inout float dX) {}

// CHECK-DAG: backward derivative 'inout_output_pullback' parameter 3 must be an out parameter
[[dxc::BackwardDerivative(inout_output_pullback)]]
float inout_output(float x) { return x; }

void no_match_pullback(float2 x, float2 dResult, out float2 dX) {}
void no_match_pullback(float3 x, float3 dResult, out float3 dX) {}

// CHECK-DAG: no overload of backward derivative 'no_match_pullback' matches function 'no_match'
[[dxc::BackwardDerivative(no_match_pullback)]]
float no_match(float x) { return x; }

void first_pullback(float x, float dResult, out float dX) {}
void second_pullback(float x, float dResult, out float dX) {}

// CHECK-DAG: function 'multiple' has multiple backward derivative associations
[[dxc::BackwardDerivative(first_pullback),
  dxc::BackwardDerivative(second_pullback)]]
float multiple(float x) { return x; }

struct MethodMismatch {
  static void pullback(float x, float dResult, out float dX) {}

  // CHECK-DAG: backward derivative for method 'method' must be a method of the same type with matching staticness
  [[dxc::BackwardDerivative(pullback)]]
  float method(float x) { return x; }
};
