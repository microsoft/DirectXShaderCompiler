// RUN: %dxc -E main -T ps_6_0 -O0 %s | FileCheck %s

// Ensure that bools are converted from/to their mem representation when loaded/stored in buffers

// Constant buffer loads
// CHECK: call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32
// CHECK: extractvalue %dx.types.CBufRet.i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32
// CHECK: extractvalue %dx.types.CBufRet.i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32
// CHECK: extractvalue %dx.types.CBufRet.i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32
// CHECK: extractvalue %dx.types.CBufRet.i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32
// CHECK: extractvalue %dx.types.CBufRet.i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32
// CHECK: extractvalue %dx.types.CBufRet.i32
// CHECK: icmp ne i32 {{.*}}, 0

// Structured buffer loads
// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK: extractvalue %dx.types.ResRet.i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK: extractvalue %dx.types.ResRet.i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK: extractvalue %dx.types.ResRet.i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK: extractvalue %dx.types.ResRet.i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK: extractvalue %dx.types.ResRet.i32
// CHECK: icmp ne i32 {{.*}}, 0
// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK: extractvalue %dx.types.ResRet.i32
// CHECK: icmp ne i32 {{.*}}, 0

// Structured buffer stores
// CHECK: icmp eq i32 {{.*}}, 42
// CHECK: zext i1 {{.*}} to i32
// CHECK: call void @dx.op.bufferStore.i32
// CHECK: icmp eq i32 {{.*}}, 42
// CHECK: zext i1 {{.*}} to i32
// CHECK: call void @dx.op.bufferStore.i32
// CHECK: icmp eq i32 {{.*}}, 42
// CHECK: zext i1 {{.*}} to i32
// CHECK: call void @dx.op.bufferStore.i32
// CHECK: icmp eq i32 {{.*}}, 42
// CHECK: zext i1 {{.*}} to i32
// CHECK: call void @dx.op.bufferStore.i32
// CHECK: icmp eq i32 {{.*}}, 42
// CHECK: zext i1 {{.*}} to i32
// CHECK: call void @dx.op.bufferStore.i32
// CHECK: icmp eq i32 {{.*}}, 42
// CHECK: zext i1 {{.*}} to i32
// CHECK: call void @dx.op.bufferStore.i32
  

struct AllTheBools
{
    bool2x2 m;
    bool2 v;
    bool s;
    bool2x2 ma[2];
    bool2 va[2];
    bool sa[2];
};

ConstantBuffer<AllTheBools> cb;
StructuredBuffer<AllTheBools> sb;
RWStructuredBuffer<AllTheBools> rwsb;

float main(int i : I) : SV_Target
{
    float result = 0;

    // Constant buffer loads
    if (cb.m._22 && cb.v.y && cb.s
        && cb.ma[1]._22 && cb.va[1].y && cb.sa[1])
    {
        result++;
    }
    
    // Structured buffer loads
    if (sb[0].m._22 && sb[0].v.y && sb[0].s
        && sb[0].ma[1]._22 && sb[0].va[1].y && sb[0].sa[1])
    {
        result++;
    }

    // Structured buffer stores
    if (result >= 1.0f)
    {
        rwsb[0].m._22 = i == 42;
        rwsb[0].v.y = i == 42;
        rwsb[0].s = i == 42;
        rwsb[0].ma[1]._22 = i == 42;
        rwsb[0].va[1].y = i == 42;
        rwsb[0].sa[1] = i == 42;
    }

    return 0;
}