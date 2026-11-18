// RUN: %dxc -E main -T ps_6_0 -HV 202x -fcgl %s -spirv | FileCheck %s

// Verify SPIR-V code generation for function parameter packs, pack expansions,
// and sizeof...().

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

// CHECK-LABEL: %src_main = OpFunction %float None
// CHECK: OpFunctionCall %float %Sum
// CHECK: OpFunctionCall %uint %CountArgs
// CHECK-LABEL: %Sum = OpFunction %float None
// CHECK: OpFunctionCall %float %Sum_0
// CHECK-LABEL: %CountArgs = OpFunction %uint None
// CHECK-NEXT: %args = OpFunctionParameter
// CHECK-NEXT: %args_0 = OpFunctionParameter
// CHECK-NEXT: %args_1 = OpFunctionParameter
// CHECK: OpReturnValue %uint_3
// CHECK-LABEL: %Sum_0 = OpFunction %float None
// CHECK: OpFunctionCall %float %Sum_1
// CHECK-LABEL: %Sum_1 = OpFunction %float None
// CHECK: OpFunctionCall %float %Sum_2
// CHECK-LABEL: %Sum_2 = OpFunction %float None
float main() : SV_Target {
  float total = Sum(1.0, 2.0, 3.0, 4.0);
  // Keep CountArgs in the unoptimized SPIR-V without changing the result.
  total += 0 * (float)CountArgs(1, 2, 3);
  return total;
}
