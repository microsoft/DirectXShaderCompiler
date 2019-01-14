// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// Test implicit conversions between numerical shapes (scalars, vectors, matrices of different sizes)

// These functions are used to trigger implicit conversions,
// they return the last element of their input (to test splats).
int last_of_s(int s) { return s; }
int last_of_v1(int1 v) { return v.x; }
int last_of_v2(int2 v) { return v.y; }
int last_of_v3(int3 v) { return v.z; }
int last_of_v4(int4 v) { return v.w; }
int last_of_m1x1(int1x1 m) { return m._11; }
int last_of_m1x2(int1x2 m) { return m._12; }
int last_of_m2x1(int2x1 m) { return m._21; }
int last_of_m2x2(int2x2 m) { return m._22; }
int last_of_m3x3(int3x3 m) { return m._33; }

AppendStructuredBuffer<int> output;

void main()
{
    // Use values that will be easy to identify in the output
    int s = 42;
    int1 v1 = int1(-1);
    int2 v2 = int2(-1, -2);
    int3 v3 = int3(-1, -2, -3);
    int4 v4 = int4(-1, -2, -3, -4);
    int1x1 m1x1 = int1x1(11);
    int1x2 m1x2 = int1x2(11, 12);
    int2x1 m2x1 = int2x1(11, 21);
    int2x2 m2x2 = int2x2(11, 12, 21, 22);
    int3x3 m3x3 = int3x3(11, 12, 13, 21, 22, 23, 31, 32, 33);

    // ICK_HLSLVector_Scalar
    // CHECK: i32 -1,
    output.Append(last_of_s(v1));
    // CHECK: i32 -1,
    output.Append(last_of_s(v2));
    // CHECK: i32 11,
    output.Append(last_of_s(m1x1));
    // NCHECK: i32 11,
    // output.Append(last_of_s(m2x2)); // GitHub #1845

    // ICK_HLSLVector_Conversion (vector/matrix, element-preserving)
    // NCHECK: i32 12,
    // output.Append(last_of_v2(m1x2)); // GitHub #1844
    // NCHECK: i32 21,
    // output.Append(last_of_v2(m2x1)); // GitHub #1844
    // NCHECK: i32 22,
    // output.Append(last_of_v4(m2x2)); // GitHub #1844
    // NCHECK: i32 -2,
    // output.Append(last_of_m1x2(v2)); // GitHub #1844
    // NCHECK: i32 -2,
    // output.Append(last_of_m2x1(v2)); // GitHub #1844
    // NCHECK: i32 -4,
    // output.Append(last_of_m2x2(v4)); // GitHub #1844

    // ICK_HLSLVector_Splat (single element duplicated)
    // CHECK: i32 42,
    output.Append(last_of_v3(s));
    // CHECK: i32 -1,
    output.Append(last_of_v3(v1));
    // CHECK: i32 11,
    output.Append(last_of_v3(m1x1));
    // CHECK: i32 42,
    output.Append(last_of_m3x3(s));
    // CHECK: i32 -1,
    output.Append(last_of_m3x3(v1));
    // CHECK: i32 11,
    output.Append(last_of_m3x3(m1x1));

    // ICK_HLSLVector_Truncation (vector to smaller vector or matrix to smaller matrix)
    // CHECK: i32 -1,
    output.Append(last_of_v1(v2));
    // CHECK: i32 -2,
    output.Append(last_of_v2(v3));
    // NCHECK: i32 11,
    // output.Append(last_of_v1(m2x2)); // GitHub #1845
    // NCHECK: i32 11,
    // output.Append(last_of_m1x1(m2x2)); // GitHub #1845
    // CHECK: i32 12,
    output.Append(last_of_m1x2(m2x2));
    // CHECK: i32 21,
    output.Append(last_of_m2x1(m2x2));
    // CHECK: i32 22,
    output.Append(last_of_m2x2(m3x3));
}