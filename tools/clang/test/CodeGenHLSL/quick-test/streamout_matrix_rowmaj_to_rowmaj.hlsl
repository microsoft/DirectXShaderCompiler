// RUN: %dxc -E main -T gs_6_0 %s | FileCheck %s

// CHECK: call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 0)
// CHECK: call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 0)
// CHECK: call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 0)
// CHECK: call float @dx.op.loadInput.f32(i32 4, i32 0, i32 1, i8 0, i32 0)
// CHECK: call float @dx.op.loadInput.f32(i32 4, i32 0, i32 1, i8 1, i32 0)
// CHECK: call float @dx.op.loadInput.f32(i32 4, i32 0, i32 1, i8 2, i32 0)
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float {{.*}})
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float {{.*}})
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float {{.*}})
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 1, i8 0, float {{.*}})
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 1, i8 1, float {{.*}})
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 1, i8 2, float {{.*}})

struct GSIn { row_major float2x3 value : TEXCOORD0; };
struct GSOut { row_major float2x3 value : TEXCOORD0; };

[maxvertexcount(1)]
void main(point GSIn input[1], inout PointStream<GSOut> output)
{
    GSOut result;
    result.value = input[0].value;
    output.Append(result);
}