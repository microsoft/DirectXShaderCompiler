// RUN: not %dxc -T lib_6_3 -HV 2016 -DCAST=static_cast %s 2>&1 | FileCheck %s
// RUN: not %dxc -T lib_6_3 -HV 2017 -DCAST=static_cast %s 2>&1 | FileCheck %s
// RUN: not %dxc -T lib_6_3 -HV 2018 -DCAST=static_cast %s 2>&1 | FileCheck %s
// RUN: not %dxc -T lib_6_3 -HV 2021 -DCAST=static_cast %s 2>&1 | FileCheck %s
// RUN: not %dxc -T lib_6_3 -HV 202x -DCAST=const_cast %s 2>&1 | FileCheck %s
// RUN: not %dxc -T lib_6_3 -HV 202x -DCAST=dynamic_cast %s 2>&1 | FileCheck %s
// RUN: not %dxc -T lib_6_3 -HV 202x -DCAST=reinterpret_cast %s 2>&1 | FileCheck %s

// CHECK: error: C++-style cast is unsupported in HLSL
float convert(int value) {
  return CAST<float>(value);
}
