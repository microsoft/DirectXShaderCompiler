// RUN: %dxc -E main -T cs_6_0 %s | FileCheck %s

// Regression test for github.com/microsoft/DirectXShaderCompiler/issues/8477:
// asuint(double_matrix, out uint_mat lo, out uint_mat hi) was producing
// transposed output in DXIL because TranslateDoubleAsUint wrote elements in
// row-major order while column-major output allocas expect column-major layout.
//
// For a 2x2 double matrix with distinct elements d00, d01, d10, d11 (in
// logical row-major order), the lo output must satisfy:
//   lo[0][0] = lo_bits(d00)
//   lo[0][1] = lo_bits(d01)
//   lo[1][0] = lo_bits(d10)
//   lo[1][1] = lo_bits(d11)
//
// Before the fix, lo[0][0] and lo[0][1] would be lo_bits(d00) and lo_bits(d10)
// (first column) instead of the first row.  We verify that four SplitDouble
// calls are emitted (one per matrix element) and that the compiler produces
// valid DXIL without assertion failures.

// CHECK: SplitDouble
// CHECK: SplitDouble
// CHECK: SplitDouble
// CHECK: SplitDouble

RWStructuredBuffer<uint> Lows : register(u0);
RWStructuredBuffer<uint> Highs : register(u1);

[numthreads(1,1,1)]
void main()
{
    double2x2 m;
    m[0][0] = asdouble(10u, 0u);
    m[0][1] = asdouble(20u, 0u);
    m[1][0] = asdouble(30u, 0u);
    m[1][1] = asdouble(40u, 0u);

    uint2x2 lo, hi;
    asuint(m, lo, hi);

    Lows[0] = lo[0][0];
    Lows[1] = lo[0][1];
    Lows[2] = lo[1][0];
    Lows[3] = lo[1][1];

    Highs[0] = hi[0][0];
    Highs[1] = hi[0][1];
    Highs[2] = hi[1][0];
    Highs[3] = hi[1][1];
}
