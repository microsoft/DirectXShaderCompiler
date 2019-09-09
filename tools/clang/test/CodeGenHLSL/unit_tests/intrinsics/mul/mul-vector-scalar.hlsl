// RUN: %dxc -T vs_6_0 -E main -Od %s  | FileCheck %s

void main() {

    float3 fvec1 = { 0.1, 0.2, 0.3};
    float4 fvec2 = { 1.1, 1.2, 1.3, 1.4};
    float fx1 = 0.5;
    float fx2 = 1.5;

// CHECK: call float @dx.op.dot3.f32
    float4 a = mul(fvec1, fvec2);

// CHECK: fmul fast float
// CHECK: fmul fast float
// CHECK: fmul fast float
    float3 b = mul(fvec1, fx1);

// CHECK: fmul fast float
// CHECK: fmul fast float
// CHECK: fmul fast float
    float3 c = mul(fx1, fvec1);

// CHECK: fmul fast float
    float d = mul(fx1, fx2);
    
    int4 ivec1 = { 1, 2, 3, 4};
    int3 ivec2 = { 4, 5, 6};
    int i1 = 1;
    int i2 = 2;   

// CHECK: mul i32
// CHECK: call i32 @dx.op.tertiary.i32(i32 48,
// CHECK: call i32 @dx.op.tertiary.i32(i32 48,
    int e = mul(ivec1, ivec2);

// CHECK: mul i32
// CHECK: mul i32
// CHECK: mul i32
// CHECK: mul i32
    int4 f = mul(ivec1, i1);

// CHECK: mul i32
// CHECK: mul i32
// CHECK: mul i32
// CHECK: mul i32
    int4 g = mul(i1, ivec1);

// CHECK: mul i32
    int h = mul(i1, i2);
}
