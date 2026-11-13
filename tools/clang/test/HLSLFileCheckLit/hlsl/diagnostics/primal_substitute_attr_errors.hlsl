// RUN: not %dxc -T lib_6_4 %s 2>&1 | FileCheck %s

float not_a_function;

// CHECK-DAG: PrimalSubstituteOf attribute argument must name a function
[[dxc::PrimalSubstituteOf(not_a_function)]]
[[dxc::AutoDiff(bwd)]]
float bad_name(float x) { return x; }

// CHECK-DAG: PrimalSubstituteOf attribute argument must name a function
[[dxc::PrimalSubstituteOf(unknown_primal)]]
[[dxc::AutoDiff(bwd)]]
float missing_name(float x) { return x; }

float return_original(float x) { return x; }

// CHECK-DAG: primal substitute 'return_substitute' must return 'float'
[[dxc::PrimalSubstituteOf(return_original)]]
[[dxc::AutoDiff(bwd)]]
float2 return_substitute(float x) { return x.xx; }

float count_original(float x, float y) { return x + y; }

// CHECK-DAG: primal substitute 'count_substitute' requires 2 parameters but has 1
[[dxc::PrimalSubstituteOf(count_original)]]
[[dxc::AutoDiff(bwd)]]
float count_substitute(float x) { return x; }

float type_original(float x) { return x; }

// CHECK-DAG: primal substitute 'type_substitute' parameter 1 must have type 'float'
[[dxc::PrimalSubstituteOf(type_original)]]
[[dxc::AutoDiff(bwd)]]
float type_substitute(float2 x) { return x.x; }

float activity_original([[dxc::NoDiff]] float scale, float x) {
  return scale * x;
}

// CHECK-DAG: primal substitute 'activity_substitute' parameter 1 must have the same NoDiff annotation as the primal
[[dxc::PrimalSubstituteOf(activity_original)]]
[[dxc::AutoDiff(bwd)]]
float activity_substitute(float scale, float x) { return scale * x; }

float direction_original(out float x) {
  x = 1.0f;
  return x;
}

// CHECK-DAG: primal substitute 'direction_substitute' parameter 1 must have the same direction as the primal
[[dxc::PrimalSubstituteOf(direction_original)]]
[[dxc::AutoDiff(bwd)]]
float direction_substitute(float x) { return x; }

float mode_original(float x) { return x; }

// CHECK-DAG: primal substitute 'mode_substitute' must request backward-mode autodiff or provide a backward derivative
[[dxc::PrimalSubstituteOf(mode_original)]]
float mode_substitute(float x) { return x; }

float duplicate_original(float x) { return x; }

[[dxc::PrimalSubstituteOf(duplicate_original)]]
[[dxc::AutoDiff(bwd)]]
float duplicate_one(float x) { return x; }

// CHECK-DAG: function 'duplicate_original' has multiple primal substitutes
[[dxc::PrimalSubstituteOf(duplicate_original)]]
[[dxc::AutoDiff(bwd)]]
float duplicate_two(float x) { return x; }

// CHECK-DAG: function 'self_substitute' cannot substitute for itself
[[dxc::PrimalSubstituteOf(self_substitute)]]
[[dxc::AutoDiff(bwd)]]
float self_substitute(float x) { return x; }

// CHECK-DAG: primal substitute association for '{{cycle_a|cycle_b}}' forms a cycle
[[dxc::PrimalSubstituteOf(cycle_b)]]
[[dxc::AutoDiff(bwd)]]
float cycle_a(float x) { return x; }

[[dxc::PrimalSubstituteOf(cycle_a)]]
[[dxc::AutoDiff(bwd)]]
float cycle_b(float x) { return x; }

void overloaded_original(float2 x) {}
void overloaded_original(float3 x) {}

// CHECK-DAG: no overload of primal 'overloaded_original' matches substitute 'overload_substitute'
[[dxc::PrimalSubstituteOf(overloaded_original)]]
[[dxc::AutoDiff(bwd)]]
void overload_substitute(float x) {}

struct MethodMismatch {
  static float original(float x) { return x; }

  // CHECK-DAG: primal substitute 'substitute' must match the primal's method receiver and staticness
  [[dxc::PrimalSubstituteOf(original)]]
  [[dxc::AutoDiff(bwd)]]
  float substitute(float x) { return x; }
};
