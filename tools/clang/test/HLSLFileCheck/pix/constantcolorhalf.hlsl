// RUN: %dxc -enable-16bit-types -Emain -Tps_6_2 %s | %opt -S -hlsl-dxil-constantColor,constant-red=0.5,constant-green=0.25,constant-blue=0.125,constant-alpha=1 | %FileCheck %s

// A native half SV_Target lowers to dx.op.storeOutput.f16.

// The override values are 0.5, 0.25, 0.125 and 1.0 as half:
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 0, i32 0, i8 0, half 0xH3800)
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 0, i32 0, i8 1, half 0xH3400)
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 0, i32 0, i8 2, half 0xH3000)
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 0, i32 0, i8 3, half 0xH3C00)

// Unused storeOutput overloads must not remain as external declarations.
// CHECK-NOT: declare void @dx.op.storeOutput.f32
// CHECK-NOT: declare void @dx.op.storeOutput.i16
// CHECK-NOT: declare void @dx.op.storeOutput.i32

[RootSignature("")]
half4 main() : SV_Target {
    return half4(0, 0, 0, 0);
}
