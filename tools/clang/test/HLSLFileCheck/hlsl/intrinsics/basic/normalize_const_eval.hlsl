// RUN: %dxc -T vs_6_0 %s -E main | %FileCheck %s
// RUN: %dxc -T vs_6_0 %s -E main -DNO_FOLD | %FileCheck %s -check-prefixes=NO_FOLD

// Ensure normalize is constant evaluated during codegen, or dxil const eval
// TODO: handle fp specials properly!

RWBuffer<float4> results : register(u0);

[shader("vertex")]
void main(bool b : B) {
    uint i = 0;

    // Literal float
    // CHECK: call void @dx.op.bufferStore.f32(i32 69, %dx.types.Handle %{{.+}}, i32 0, i32 undef, float 0x3FE6A09E60000000, float 0xBFE279A740000000, float 0x7FF8000000000000, float 0x7FF8000000000000, i8 15)
    results[i++] = float4(normalize(0.5.xx).x,
                          normalize(-1.5.xxx).x,
                          normalize(0.0.xxxx).x,
                          normalize(-0.0.xxxx).x);

    // Explicit float
    // CHECK: call void @dx.op.bufferStore.f32(i32 69, %dx.types.Handle %{{.+}}, i32 1, i32 undef, float 0x3FE6A09E60000000, float 0xBFE279A740000000, float 0x7FF8000000000000, float 0x7FF8000000000000, i8 15)
    results[i++] = float4(normalize(0.5F.xx).x,
                          normalize(-1.5F.xxx).x,
                          normalize(0.0F.xxxx).x,
                          normalize(-0.0F.xxxx).x);

#ifdef NO_FOLD
    // Currently, we rely on constant folding of DXIL ops to get rid of illegal
    // double overloads. If this doesn't happen, we expect a validation error.
    // Ternary operator can return literal type, while not being foldable due
    // non-constant condition.
    // NO_FOLD: error: validation errors
    // NO_FOLD: error: DXIL intrinsic overload must be valid.
    // The next check isn't entirely necessary, since the invalid overload
    // reported depends on intrinsic function declaration order, and this case
    // will report one of two intrinsics: dot4 or sqrt.  The above HLSL causes
    // the double unary op to be declared before the dot4 intrinsic.
    // NO_FOLD: note: at '%{{.+}} = call double @dx.op.unary.f64(i32 25, double %{{.+}})' in block '#0' of function 'main'.
    results[i++] = normalize((b ? 1.25 : 2.5).xxxx);
#endif // NO_FOLD
}
