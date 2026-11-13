// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials %s > %t.gen.hlsl
// RUN: cat %t.gen.hlsl %S/Inputs/active_runtime_for_generic_intrinsic_caller.hlsl > %t.run.hlsl
// RUN: %dxc -T cs_6_9 -E testMain -HV 2021 -Fo %t.dxil %t.run.hlsl
// RUN: %dxc -dumpbin %t.dxil | FileCheck %s --check-prefix=EXEC

// The intrinsic consumes x version 1, produced earlier in the iteration. Its
// generic pullback reconstructs that operand from the versioned tape.

// CHECK: float __dxc_ad_loop_0_version_1_primal_tape[8];
// CHECK: x.value += y.value;
// CHECK: __dxc_ad_loop_0_version_1_primal_tape[iteration] = x.value;
// CHECK: y.value = ::sin(x.value);
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_1_adjoint * cos(__dxc_ad_loop_0_version_1_primal_tape[iteration]));

// One iteration produces 3 + sin(3) + 6. With seed 2, both x and y have
// gradient 2 * (1 + cos(3)), while z has gradient 4.
// EXEC: rawBufferStore.f32{{.*}}i32 0, i32 0, float 0x40224840E0000000
// EXEC: rawBufferStore.f32{{.*}}i32 1, i32 0, float 0x3F947ED000000000
// EXEC: rawBufferStore.f32{{.*}}i32 2, i32 0, float 0x3F947ED000000000
// EXEC: rawBufferStore.f32{{.*}}i32 3, i32 0, float 4.000000e+00

[[dxc::AutoDiff(bwd)]]
float f(float x, float y, float z, [[dxc::NoDiff]] uint count,
        [[dxc::NoDiff]] float factor) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x += y;
    y = sin(x);
    z *= factor;
  }
  return x + y + z;
}

// CHECK: y.value = ::cos(x.value);
// CHECK: __dxc_ad_loop_0_state_0_adjoint += -(__dxc_ad_loop_0_update_1_adjoint * sin(__dxc_ad_loop_0_version_1_primal_tape[iteration]));
[[dxc::AutoDiff(bwd)]]
float cos_f(float x, float y, float z, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x += y;
    y = cos(x);
    z *= 2.0f;
  }
  return x + y + z;
}

// CHECK: y.value = ::exp(x.value);
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_1_adjoint * exp(__dxc_ad_loop_0_version_1_primal_tape[iteration]));
[[dxc::AutoDiff(bwd)]]
float exp_f(float x, float y, float z, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x += y;
    y = exp(x);
    z *= 2.0f;
  }
  return x + y + z;
}

// CHECK: y.value = ::log(x.value);
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_1_adjoint / __dxc_ad_loop_0_version_1_primal_tape[iteration]);
[[dxc::AutoDiff(bwd)]]
float log_f(float x, float y, float z, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x += y;
    y = log(x);
    z *= 2.0f;
  }
  return x + y + z;
}

// CHECK: y.value = ::sqrt(x.value);
// CHECK: __dxc_ad_loop_0_state_0_adjoint += (__dxc_ad_loop_0_update_1_adjoint * (0.5f / sqrt(__dxc_ad_loop_0_version_1_primal_tape[iteration])));
[[dxc::AutoDiff(bwd)]]
float sqrt_f(float x, float y, float z, [[dxc::NoDiff]] uint count) {
  for (uint iteration = 0; iteration < min(count, 8); ++iteration) {
    x += y;
    y = sqrt(x);
    z *= 2.0f;
  }
  return x + y + z;
}
