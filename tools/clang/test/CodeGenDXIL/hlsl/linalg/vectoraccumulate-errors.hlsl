// RUN: %dxc -T lib_6_9 %s | FileCheck %s

RWByteAddressBuffer RWBuf;

export void Test5(vector<float, 128> Input) {
  using namespace dx::linalg;

  RWBuf.Store<vector<half, 128> >(0, Input);

  // clang-format off
  // CHECK: Something about an error due to illegal conversions
  VectorAccumulate(Input, RWBuf, 0);
  // clang-format on
}
