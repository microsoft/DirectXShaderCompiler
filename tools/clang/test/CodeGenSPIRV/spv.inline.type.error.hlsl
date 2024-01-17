// RUN: not %dxc -T ps_6_0 -E main -fcgl  %s -spirv 2>&1 | FileCheck %s

template<typename T>
class SomeTemplate {};

void main(uint iid : SV_InstanceID) {
  typedef vk::SpirvOpaqueType</* OpTypeInt */ 21, iid, 0> BadType;
  // CHECK: error: template argument for 'SpirvOpaqueType' must evaluate to a constant rvalue
  BadType val;

  typedef vk::SpirvType</* OpTypeInt */ 21, 8, 8, iid, 0> BadType2;
  // CHECK: error: template argument for 'SpirvType' must evaluate to a constant rvalue
  BadType2 val2;

  typedef vk::SpirvOpaqueType</* OpTypeInt */ 21, SomeTemplate, 0> BadTemplate;
  // CHECK: error: 'SomeTemplate' is not a valid HLSL type (did you forget the template arguments?)
  BadTemplate val3;
}
