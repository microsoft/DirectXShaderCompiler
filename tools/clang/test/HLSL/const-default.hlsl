// RUN: %clang_cc1 -fsyntax-only -Wno-unused-value  -ffreestanding -verify %s

float g_float1;                                             /* expected-note {{variable 'g_float1' declared const here}} expected-note {{variable 'g_float1' declared const here}} */
int4 g_vec1;                                                /* expected-note {{variable 'g_vec1' declared const here}} expected-note {{variable 'g_vec1' declared const here}} */
uint64_t3x4 g_mat1;

cbuffer g_cbuffer {
    min12int m_buffer_min12int;                             /* expected-note {{variable 'm_buffer_min12int' declared const here}} expected-warning {{min12int is promoted to min16int}} */
    float4 m_buffer_float4;                                 /* expected-note {{variable 'm_buffer_float4' declared const here}} */
    int3x4 m_buffer_int3x4;
}

tbuffer g_tbuffer {
    float m_tbuffer_float;                                  /* expected-note {{variable 'm_tbuffer_float' declared const here}} */
    int3 m_tbuffer_int3;                                    /* expected-note {{variable 'm_tbuffer_int3' declared const here}} */
    double2x1 m_tbuffer_double2x1;                          /* expected-note {{variable 'm_tbuffer_double2x1' declared const here}} */
}

struct MyStruct {
    float3 my_float3;
    int3x4 my_int3x4;
};

ConstantBuffer<MyStruct> g_const_buffer;
TextureBuffer<MyStruct> g_texture_buffer;                

float4 main() : SV_TARGET
{
    g_float1 = g_float1 + 10.0;                             /* expected-error {{cannot assign to variable 'g_float1' with const-qualified type 'const float'}} */
    g_float1 += 3.5;                                         /* expected-error {{cannot assign to variable 'g_float1' with const-qualified type 'const float'}} */
    g_vec1 = g_vec1 + 3;                                    /* expected-error {{cannot assign to variable 'g_vec1' with const-qualified type 'const int4'}} */
    g_vec1 -= 1;                                            /* expected-error {{cannot assign to variable 'g_vec1' with const-qualified type 'const int4'}} */
    g_mat1._12 = 3;                                         /* expected-error {{read-only variable is not assignable}} */
    g_mat1._34 *= 4;                                        /* expected-error {{read-only variable is not assignable}} */
    m_buffer_min12int = 12;                                 /* expected-error {{cannot assign to variable 'm_buffer_min12int' with const-qualified type 'const min12int'}} */
    m_buffer_float4 += 3.4;                                 /* expected-error {{cannot assign to variable 'm_buffer_float4' with const-qualified type 'const float4'}} */
    m_buffer_int3x4._m01 -= 10;                             /* expected-error {{read-only variable is not assignable}} */
    m_tbuffer_float *= 2;                                   /* expected-error {{cannot assign to variable 'm_tbuffer_float' with const-qualified type 'const float'}} */
    m_tbuffer_int3 = 10;                                    /* expected-error {{cannot assign to variable 'm_tbuffer_int3' with const-qualified type 'const int3'}} */
    m_tbuffer_double2x1 *= 3;                               /* expected-error {{cannot assign to variable 'm_tbuffer_double2x1' with const-qualified type 'const double2x1'}} */

    g_const_buffer.my_float3.x = 1.5;                       /* expected-error {{read-only variable is not assignable}} */
    g_const_buffer.my_int3x4._10 -= 2;                      /* expected-error {{the digit '0' is used in '_10', but the syntax is for one-based rows and columns}} */
    g_texture_buffer.my_float3.y += 2.0;                      /* expected-error {{read-only variable is not assignable}} */
    g_texture_buffer.my_int3x4._02 = 3;                     /* expected-error {{the digit '0' is used in '_02', but the syntax is for one-based rows and columns}} */

    return (float4)g_float1;
}