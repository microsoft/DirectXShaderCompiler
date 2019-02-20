// RUN: %dxc -E main -T cs_6_0 -Zpr %s | FileCheck %s

// CHECK: %[[cb0:[^ ]+]] = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %{{.*}}, i32 0)
// CHECK: %[[cb0x:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[cb0]], 0
// CHECK: %[[cb0y:[^ ]+]] = extractvalue %dx.types.CBufRet.f32 %[[cb0]], 1

// CHECK: store float %[[cb0x]], float addrspace(3)* getelementptr inbounds ([16 x float], [16 x float] addrspace(3)* @[[obj:[^,]+]], i32 0, i32 0), align 16
// CHECK: store float %[[cb0y]], float addrspace(3)* getelementptr inbounds ([16 x float], [16 x float] addrspace(3)* @[[obj]], i32 0, i32 1), align 4

// CHECK: %[[_25:[^ ]+]] = getelementptr [16 x float], [16 x float] addrspace(3)* @[[obj]], i32 0, i32 %{{.+}}
// CHECK: %[[_26:[^ ]+]] = load float, float addrspace(3)* %[[_25]], align 16
// CHECK: %[[_27:[^ ]+]] = getelementptr [16 x float], [16 x float] addrspace(3)* @[[obj]], i32 0, i32 %{{.+}}
// CHECK: %[[_28:[^ ]+]] = load float, float addrspace(3)* %[[_27]], align 4

// CHECK: %[[_33:[^ ]+]] = bitcast float %[[_26]] to i32
// CHECK: %[[_34:[^ ]+]] = bitcast float %[[_28]] to i32

// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %{{.*}}, i32 %{{.+}}, i32 undef, i32 %[[_33]], i32 %[[_34]], i32 %{{.+}}, i32 %{{.+}}, i8 15)

float4 rows[4];

void set_row(inout float4 row, uint i) {
  row = rows[i];
}

class Obj {
  float4x4 mat;
  void set() {
    set_row(mat[0], 0);
    set_row(mat[1], 1);
    set_row(mat[2], 2);
    set_row(mat[3], 3);
  }
};

RWByteAddressBuffer RWBuf;
groupshared Obj obj;

[numthreads(4, 1, 1)]
void main(uint3 groupThreadID: SV_GroupThreadID) {
  if (groupThreadID.x == 0) {
    obj.set();
  }
  GroupMemoryBarrierWithGroupSync();
  float4 row = obj.mat[groupThreadID.x];
  uint addr = groupThreadID.x * 4;
  RWBuf.Store4(addr, uint4(asuint(row.x), asuint(row.y), asuint(row.z), asuint(row.w)));
}
