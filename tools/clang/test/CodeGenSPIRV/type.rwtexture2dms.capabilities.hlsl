// RUN: %dxc -T cs_6_7 -E main -fcgl  %s -spirv | FileCheck %s --implicit-check-not="OpCapability ImageMSArray"

// RWTexture2DMS requires StorageImageMultisample but not ImageMSArray.
// CHECK: OpCapability StorageImageMultisample

// Expect Arrayed=0, MS=1, Sampled=2 and the format of the scalar type. The
// sample count template argument does not change the image type.
// CHECK-DAG: OpTypeImage %uint 2D 2 0 1 2 R32ui
// CHECK-DAG: OpTypeImage %float 2D 2 0 1 2 R32f

RWTexture2DMS<uint, 8> t1 : register(u1);
RWTexture2DMS<float> t2 : register(u2);

[numthreads(1, 1, 1)]
void main() {
}
