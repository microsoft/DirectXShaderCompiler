// RUN: %dxc -E main -T ps_6_0 -HV 202x %s | FileCheck %s
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 1.300000e+01)

// Verify template and function parameter packs, pack expansions, and
// sizeof...() in DXIL.

template <typename T>
T Sum(T First) {
  return First;
}

template <typename T, typename U, typename... Rest>
T Sum(T First, U Second, Rest... Others) {
  return First + Sum(Second, Others...);
}

template <typename... Args>
uint CountArgs(Args... args) {
  return sizeof...(Args);
}

float main() : SV_Target {
  return Sum(1.0, 2.0, 3.0, 4.0) + CountArgs(1, 2, 3);
}
