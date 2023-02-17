// RUN: %dxc -Od -T lib_6_6 %s | %opt -S -dxil-annotate-with-virtual-regs | FileCheck %s


/* To run locally run:
%dxc -Od -T lib_6_6 %s -Fc %t.ll
%opt %t.ll -S -dxil-annotate-with-virtual-regs | FileCheck %s
*/

RaytracingAccelerationStructure scene : register(t0);

struct RayPayload
{
    int3 color;
};

[shader("raygeneration")]
void ENTRY()
{
    RayDesc ray = {{0,0,0}, {0,0,1}, 0.05, 1000.0};
    RayPayload pld;
    TraceRay(scene, 0 /*rayFlags*/, 0xFF /*rayMask*/, 0 /*sbtRecordOffset*/, 1 /*sbtRecordStride*/, 0 /*missIndex*/, ray, pld);
}

// CHECK: {{.*}} = alloca %struct.RayDesc, align 4, !pix-dxil-inst-num {{.*}}, !pix-alloca-reg [[RDAlloca:![0-9]+]]
// CHECK: {{.*}} = alloca %struct.RayPayload, align 4, !pix-dxil-inst-num {{.*}}, !pix-alloca-reg [[RPAlloca:![0-9]+]]
// CHECK: {{.*}} = getelementptr inbounds %struct.RayDesc, %struct.RayDesc* {{.*}}, i32 0, i32 0, !pix-dxil-inst-num {{.*}}, !pix-dxil-reg [[RDGEP:![0-9]+]]
// CHECK: {{.*}} = load i32, i32* getelementptr inbounds ([1 x i32], [1 x i32]* @dx.nothing.a, i32 0, i32 0), !pix-dxil-inst-num {{.*}}, !pix-dxil-reg [[NothGEP:![0-9]+]]
// CHECK: {{.*}} = getelementptr inbounds %struct.RayDesc, %struct.RayDesc* {{.*}}, i32 0, i32 1, !pix-dxil-inst-num {{.*}}, !pix-dxil-reg [[RDGEP2:![0-9]+]]
// CHECK: {{.*}} = load i32, i32* getelementptr inbounds ([1 x i32], [1 x i32]* @dx.nothing.a, i32 0, i32 0), !pix-dxil-inst-num {{.*}}, !pix-dxil-reg [[NothGEP2:![0-9]+]]
// CHECK: {{.*}} = getelementptr inbounds %struct.RayDesc, %struct.RayDesc* {{.*}}, i32 0, i32 2, !pix-dxil-inst-num {{.*}}, !pix-dxil-reg [[RDGEP3:![0-9]+]]
// CHECK: {{.*}} = load i32, i32* getelementptr inbounds ([1 x i32], [1 x i32]* @dx.nothing.a, i32 0, i32 0), !pix-dxil-inst-num {{.*}}, !pix-dxil-reg [[NothGEP3:![0-9]+]]

// CHECK-DAG: [[RDAlloca]] = !{i32 1, i32 0, i32 8}
// CHECK-DAG: [[RPAlloca]] = !{i32 1, i32 8, i32 3}
// CHECK-DAG: [[RDGEP]] = !{i32 0, i32 0}
// CHECK-DAG: [[NothGEP]] = !{i32 0, i32 11}
// CHECK-DAG: [[RDGEP2]] = !{i32 0, i32 3}
// CHECK-DAG: [[NothGEP2]] = !{i32 0, i32 12}
// CHECK-DAG: [[RDGEP3]] = !{i32 0, i32 4}
// CHECK-DAG: [[NothGEP3]] = !{i32 0, i32 13}
