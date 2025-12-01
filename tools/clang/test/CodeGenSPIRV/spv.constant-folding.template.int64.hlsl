// RUN: %dxc -O3 -T lib_6_8 -HV 2021 -spirv -fcgl -fspv-target-env=universal1.5 %s | FileCheck %s

template <typename S> struct Trait;

template <> struct Trait<int64_t> {
  using type = int64_t;
  static const type value = -42;
};

Trait<int64_t>::type get_s64_value() {
  return Trait<int64_t>::value;
}

int64_t cvt_s64(int64_t x) { return int64_t(x); }

template <> struct Trait<uint64_t> {
  using type = uint64_t;
  static const type value = 1337ull;
};

Trait<uint64_t>::type get_u64_value() {
  return Trait<uint64_t>::value;
}

uint64_t cvt_u64(uint64_t x) { return uint64_t(x); }

// CHECK-LABEL: %testcase_s64 = OpFunction %long
export int64_t testcase_s64(int x) {
  if (x == 2) {
    // CHECK: OpReturnValue %long_n42
    return int64_t(Trait<int64_t>::value);
  }
  else if (x == 4) {
    // CHECK: OpStore %param_var_x %long_n42
    // CHECK-NEXT: OpFunctionCall %long %cvt_s64 %param_var_x
    return cvt_s64(Trait<int64_t>::value);
  } else if (x == 8) {
    return get_s64_value();
  }
  return 0ll;
}

// CHECK-LABEL: %testcase_u64 = OpFunction %ulong
export uint64_t testcase_u64(int x) {
  if (x == 2) {
    // CHECK: OpReturnValue %ulong_1337
    return uint64_t(Trait<uint64_t>::value);
  }
  else if (x == 4) {
    // CHECK: OpStore %param_var_x_0 %ulong_1337
    // CHECK-NEXT: OpFunctionCall %ulong %cvt_u64 %param_var_x_0
    return cvt_u64(Trait<uint64_t>::value);
  } else if (x == 8) {
    return get_u64_value();
  }
  return 0ull;
}

// CHECK-LABEL: %get_s64_value = OpFunction %long
// CHECK-LABEL: %get_u64_value = OpFunction %ulong
