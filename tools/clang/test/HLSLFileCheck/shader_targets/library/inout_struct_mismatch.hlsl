// RUN: %dxc -T lib_6_x -default-linkage external -HV 2018 %s | FileCheck %s

// CHECK: define <4 x float>
// CHECK-SAME: main
// CHECK: [[local:%(local)|([0-9]+)]] = alloca %struct.CallStruct
// CHECK: [[param:%[0-9]+]] = bitcast %struct.CallStruct* [[local]] to %struct.ParamStruct*
// CHEKC: call void @"\01?modify_ext{{.*}}(%struct.ParamStruct* dereferenceable(8) [[param]])

struct ParamStruct {
  int i;
  float f;
};

struct CallStruct {
  int i;
  float f;
};

void modify(inout ParamStruct s) {
  s.f += 1;
}

void modify_ext(inout ParamStruct s);

CallStruct g_struct;

float4 main() : SV_Target {
  CallStruct local = g_struct;
  modify(local);
  modify_ext(local);
  return local.f;
}
