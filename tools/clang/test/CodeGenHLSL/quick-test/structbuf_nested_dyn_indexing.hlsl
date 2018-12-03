// RUN: %dxc -E main -T ps_6_3 -enable-16bit-types %s | FileCheck %s

// &sb[idx.x] + 4 + idx.y*16 + 4 + idx.z*4

// CHECK: %0 = call i32 @dx.op.loadInput.i32(i32 4, i32 0, i32 0, i8 0, i32 undef)
// CHECK: %1 = call i32 @dx.op.loadInput.i32(i32 4, i32 0, i32 0, i8 1, i32 undef)
// CHECK: %2 = call i32 @dx.op.loadInput.i32(i32 4, i32 0, i32 0, i8 2, i32 undef)
// CHECK: %3 = shl i32 %1, 4
// CHECK: %4 = or i32 %3, 4
// CHECK: %5 = add nsw i32 %4, 4
// CHECK: %6 = shl i32 %2, 2
// CHECK: %7 = add i32 %5, %6
// CHECK: @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %sb_texture_structbuf, i32 %0, i32 %7, i8 1, i32 4)
// CHECK: extractvalue %dx.types.ResRet.i32 {{.*}}, 0

struct Inner // 4 + 4*3 = 16 bytes
{
    uint16_t unalign_next_field;
    uint values[3];
};

struct Outer // 4 + 16*2 = 36 bytes
{
    uint16_t unalign_next_field;
    Inner inner[2];
};

StructuredBuffer<Outer> sb;

float main(uint3 idx : IDX) : SV_Target
{
    return sb[idx.x].inner[idx.y].values[idx.z]; 
}