// RUN: %dxc -T vs_6_2 -E main -enable-16bit-types %s | FileCheck %s

// Tests all flat conversions from structs/arrays to scalar/vector/matrices

AppendStructuredBuffer<int4> buffer;

void output_s(int value) { buffer.Append(int4(value, 0, 0, 0)); }
void output_v(int4 value) { buffer.Append(value); }
void output_m(int2x2 value) { buffer.Append(int4(value._11, value._12, value._21, value._22)); }

void main() {
    // To scalar case
    {
        // CHECK: i32 1, i32 0, i32 0, i32 0
        struct { int x; } value = { 1 };
        output_s((int)value);

        // CHECK: i32 1, i32 0, i32 0, i32 0
        int array[1] = { 1 };
        output_s((int)array);
    }

    // Direct to vector/matrix cases
    {
        // CHECK: i32 1, i32 2, i32 3, i32 4
        // CHECK: i32 1, i32 2, i32 3, i32 4
        struct { int x, y, z, w; } value = { 1, 2, 3, 4 };
        output_v((int4)value);
        output_m((int2x2)value);

        // CHECK: i32 1, i32 2, i32 3, i32 4
        // CHECK: i32 1, i32 2, i32 3, i32 4
        int array[4] = { 1, 2, 3, 4 };
        output_v((int4)array);
        output_m((int2x2)array);
    }
    
    // With numerical conversions
    {
        // CHECK: i32 0, i32 1, i32 65535, i32 1
        // CHECK: i32 0, i32 1, i32 65535, i32 1
        struct { int64_t x; float y; uint16_t z; bool w; } value = { 0xFFFFFFFF00000000, 1.5f, 0xFFFF, true };
        output_v((int4)value);
        output_m((int2x2)value);
        
        // CHECK: i32 0, i32 0, i32 0, i32 0
        // CHECK: i32 1, i32 0, i32 0, i32 0
        // CHECK: i32 65535, i32 0, i32 0, i32 0
        // CHECK: i32 1, i32 0, i32 0, i32 0
        int64_t array_i64[1] = { 0xFFFFFFFF00000000 };
        float array_f[1] = { 1.5f };
        uint16_t array_u16[1] = { 0xFFFF };
        bool array_b[1] = { true };
        output_s((int64_t)array_i64);
        output_s((int)array_f);
        output_s((int)array_u16);
        output_s((int)array_b);
    }
    
    // With vectors in source
    {
        // CHECK: i32 1, i32 2, i32 3, i32 4
        // CHECK: i32 1, i32 2, i32 3, i32 4
        struct { int2 xy; int2 zw; } value = { int2(1, 2), int2(3, 4) };
        output_v((int4)value);
        output_m((int2x2)value);

        // CHECK: i32 1, i32 2, i32 3, i32 4
        // CHECK: i32 1, i32 2, i32 3, i32 4
        int2 array[2] = { int2(1, 2), int2(3, 4) };
        output_v((int4)array);
        output_m((int2x2)array);
    }
    
    // With matrices in source
    {
        // CHECK: i32 11, i32 12, i32 21, i32 22
        // CHECK: i32 11, i32 12, i32 21, i32 22
        struct { int2x2 mat; } value = { int2x2(11, 12, 21, 22) };
        output_v((int4)value);
        output_m((int2x2)value);

        // CHECK: i32 11, i32 12, i32 21, i32 22
        // CHECK: i32 11, i32 12, i32 21, i32 22
        int2x2 array[1] = { int2x2(11, 12, 21, 22) };
        output_v((int4)array);
        output_m((int2x2)array);
    }
    
    // With homogeneous nesting (struct of structs, array of arrays)
    {
        // CHECK: i32 1, i32 2, i32 3, i32 4
        // CHECK: i32 1, i32 2, i32 3, i32 4
        struct
        {
            struct { int x, y; } xy;
            struct { int x, y; } zw;
        } value = { { 1, 2 }, { 3, 4 } };
        output_v((int4)value);
        output_m((int2x2)value);

        // CHECK: i32 1, i32 2, i32 3, i32 4
        // CHECK: i32 1, i32 2, i32 3, i32 4
        int array[2][2] = { { 1, 2 }, { 3, 4 } };
        output_v((int4)array);
        output_m((int2x2)array);
    }
    
    // With heterogeneous nesting (struct of arrays, array of structs)
    {
        // CHECK: i32 1, i32 2, i32 3, i32 4
        // CHECK: i32 1, i32 2, i32 3, i32 4
        struct { int xy[2]; int zw[2]; } value = { { 1, 2 }, { 3, 4 } };
        output_v((int4)value);
        output_m((int2x2)value);

        // CHECK: i32 1, i32 2, i32 3, i32 4
        // CHECK: i32 1, i32 2, i32 3, i32 4
        struct { int x, y; } array[2] = { { 1, 2 }, { 3, 4 } };
        output_v((int4)array);
        output_m((int2x2)array);
    }

    // With nested empty struct
    {
        // CHECK: i32 1, i32 2, i32 3, i32 4
        // CHECK: i32 1, i32 2, i32 3, i32 4
        struct { int x, y; struct {} _; int z, w; } value = { 1, 2, 3, 4 };
        output_v((int4)value);
        output_m((int2x2)value);
    }

    // Truncation case
    {
        // CHECK: i32 1, i32 2, i32 3, i32 4
        // CHECK: i32 1, i32 2, i32 3, i32 4
        struct { int x, y, z, w, a; } value = { 1, 2, 3, 4, 5 };
        output_v((int4)value);
        output_m((int2x2)value);

        // CHECK: i32 1, i32 2, i32 3, i32 4
        // CHECK: i32 1, i32 2, i32 3, i32 4
        int array[5] = { 1, 2, 3, 4, 5 };
        output_v((int4)array);
        output_m((int2x2)array);
    }
}