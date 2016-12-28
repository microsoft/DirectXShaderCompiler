// RUN: %dxc -E plain -T ps_5_0 %s

// This file includes operator tests that target specific cases that are not
// otherwise covered by the other generated files.

// without also putting them in a static assertion

#define OVERLOAD_IMPLEMENTED

// __decltype is the GCC way of saying 'decltype', but doesn't require C++11
#ifdef VERIFY_FXC
#endif

// To test with the classic compiler, run
// %sdxroot%\tools\x86\fxc.exe /T vs_6_0 more-operators.hlsl
// with vs_2_0 (the default) min16float usage produces a complaint that it's not supported

struct f3_s    { float3 f3; };
struct mixed_s { float3 f3; uint s; };
struct many_s  { float4 f4; double4 d4; };
struct u3_s    { uint u3; };

SamplerState  g_SamplerState;
f3_s          g_f3_s;
mixed_s       g_mixed_s;
many_s        g_many_s;
u3_s          g_u3_s;

// Until initialization lists are complete, this is the easiest way to initialize these.
int1x1 g_i11;
int1x2 g_i12;
int2x1 g_i21;
int2x2 g_i22;

float  int_to_float(int i)    { return i; }
int1x1 float_to_i11(float f)  { return f; }
float  i11_to_float(int1x1 v) { return v; }

void into_out_i(out int i) { i = g_i11; }
void into_out_i3(out int3 i3) { i3 = int3(1, 2, 3); } // expected-note {{candidate function not viable}} expected-note {{candidate function not viable}} fxc-pass {{}}
void into_out_f(out float i) { i = g_i11; }
void into_out_f3_s(out f3_s i) { }
void into_out_ss(out SamplerState ss) { ss = g_SamplerState; }

float4 plain(float4 param4 /* : FOO */): SV_Target /*: FOO */{
    bool bools = 0;
    int ints = 0;
    const int intc = 1;
    int1 i1 = 0;
    int2 i2 = { 0, 0 };
    int3 i3 = { 1, 2, 3 };
    int4 i4 = { 1, 2, 3, 4 };
    int1x1 i11 = g_i11;
    int1x2 i12 = g_i12;
    int2x1 i21 = g_i21;
    int2x2 i22 = g_i22;
    int ari1[1] = { 0 };
    int ari2[2] = { 0, 1 };
    float floats = 0;
    SamplerState SamplerStates = g_SamplerState;
    f3_s f3_ss = g_f3_s;
    mixed_s mixed_ss = g_mixed_s;


    // Tests for complex objects.

    // Tests with vectors of one.
    ints = i1; // assign to scalar
    i2 = i1;   // assign to wider type
    i1 = ints; // assign from scalar
    i12 = i1;  // assign to matrix type (one row)
    i21 = i1;  // assign to matrix type (one col)
    i22 = i1;  // assign to matrix type (square)
    floats = i1; // assign to scalar of different but compatible type
    floats = i11; // assign to scalar of different but compatible type
    i11 = floats; // assign from scalar of different but compatible type

    // Tests with 1x1 matrices.
    ints = i11;         // assign to scalar
    i22 = i11;          // assign to wider type

    // Tests with arrays of one and one-dimensional arrays.
    ari1 = (int[1])ints; // explicit conversion works
    ari1 = ari1; // assign to same-sized array
    ari1 = (int[1])ari2; // explicit conversion to smaller size
    floats = (float)ari1; // assign to scalar of compatible type

    // Tests that introduce const into expression.
    ints = intc;
    i2 = intc;

    // Tests that perform a conversion by virtue of making a function call.
    // Same rules, mostly ensuring that the same codepath is taken.
    floats = int_to_float(ints);
    ints = int_to_float(floats);

    // Tests that perform a conversion by virtue of returning a type.
    // Same rules, mostly ensuring that the same codepath is taken.
    floats = int_to_float(ints);
    ints = int_to_float(floats);
    i11 = float_to_i11(floats);
    floats = i11_to_float(i11);

    // Tests that perform equality checks on object types. (not supported in prior compiler)

    // Tests that perform the conversion into an out parameter.
    into_out_i(ints);
    into_out_f(floats);
    into_out_f3_s(f3_ss);
    into_out_ss(SamplerStates);
    into_out_i3((int3)i4);

    // Tests that perform casts that yield lvalues.
    ((int3)i4).x = 1;

    // Tests that work with unary operators.
    bool bool_l = 1;
    int int_l = 1;
    uint uint_l = 1;
    half half_l = 1;
    float float_l = 1;
    double double_l = 1;
    min16float min16float_l = 1;
    min10float min10float_l = 1;
    min16int min16int_l = 1;
    min12int min12int_l = 1;
    min16uint min16uint_l = 1;
    SamplerState SamplerState_l = g_SamplerState;
    bool1 bool1_l = 0;
    bool2 bool2_l = 0;
    float3 float3_l = 0;
    double4 double4_l = 0;
    min10float1x2 min10float1x2_l = 0;
    uint2x3 uint2x3_l = 0;
    min16uint4x4 min16uint4x4_l = 0;
    int3x2 int3x2_l = 0;
    f3_s f3_s_l = g_f3_s;
    mixed_s mixed_s_l = g_mixed_s;

    // An artifact of the code generation is that some results were classified
    // as int or bool2 when int1 and int2 were more accurate (but the overload was missing
    // in the test).
    //
    // Some 'int or unsigned int type require' messages are 'scalar, vector, or matrix expected'
    // when both might apply.
    //
    // pre/post inc/dec on unsupported types was silently ignored and resulted in
    // 'scalar, vector, or matrix expected'.

#ifdef OVERLOAD_IMPLEMENTED
    (!bool_l); //
    (!int_l); //
    (!uint_l); //
    (!half_l); //
    (!float_l); //
    (!double_l); //
    (!min16float_l); //
    (!min10float_l); //
    (!min16int_l); //
    (!min12int_l); //
    (!min16uint_l); //
    (!bool1_l); //
#endif

    // Tests with multidimensional arrays.
    int arr23[2][3] = { 1, 2, 3, 4, 5, 6 };

    float farr23[2][3];
    // arithmetic
    // order comparison
    // equality comparison
    // bitwise
    // logical
    farr23 = farr23;

    float farr3[3];
    farr3 = farr23[0];
    farr3 = farr23[1];

    // Tests with structures with all primitives.
    many_s many_l = g_many_s;
    u3_s u3_l = g_u3_s;



    // Tests with comma operator.

    // Note that this would work in C/C++.
};