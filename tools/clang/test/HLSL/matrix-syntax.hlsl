// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s


// To test with the classic compiler, run
// %sdxroot%\tools\x86\fxc.exe /T vs_5_1 matrix-syntax.hlsl

matrix m;

void abs_without_using_result() {
    matrix<float, 4, 4> mymatrix;
    abs(mymatrix);            /* expected-warning {{ignoring return value of function declared with const attribute}} fxc-pass {{}} */

    matrix<float, 1, 4> mymatrix2;
    abs(mymatrix2);           /* expected-warning {{ignoring return value of function declared with const attribute}} fxc-pass {{}} */
}

void abs_with_assignment() {
    matrix<float, 4, 4> mymatrix;
    matrix<float, 4, 4> absMatrix;
    absMatrix = abs(mymatrix);
}

matrix<float, 4, 4> abs_for_result(matrix<float, 4, 4> value) {
    return abs(value);
}

void fn_use_matrix(matrix<float, 4, 4> value) { }

void abs_in_argument() {
    matrix<float, 4, 4> mymatrix;
    fn_use_matrix(abs(mymatrix));
    /*verify-ast
      CallExpr <col:5, col:32> 'void'
      |-ImplicitCastExpr <col:5> 'void (*)(matrix<float, 4, 4>)' <FunctionToPointerDecay>
      | `-DeclRefExpr <col:5> 'void (matrix<float, 4, 4>)' lvalue Function 'fn_use_matrix' 'void (matrix<float, 4, 4>)'
      `-CallExpr <col:19, col:31> 'matrix<float, 4, 4>':'matrix<float, 4, 4>'
        |-ImplicitCastExpr <col:19> 'matrix<float, 4, 4> (*)(matrix<float, 4, 4>)' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:19> 'matrix<float, 4, 4> (matrix<float, 4, 4>)' lvalue Function 'abs' 'matrix<float, 4, 4> (matrix<float, 4, 4>)'
        `-ImplicitCastExpr <col:23> 'matrix<float, 4, 4>':'matrix<float, 4, 4>' <LValueToRValue>
          `-DeclRefExpr <col:23> 'matrix<float, 4, 4>':'matrix<float, 4, 4>' lvalue Var 'mymatrix' 'matrix<float, 4, 4>':'matrix<float, 4, 4>'
    */
}

void matrix_on_demand() {
    float4x4 thematrix;
    float4x4 anotherMatrix;
    bool2x1 boolMatrix;   
}

void abs_on_demand() {
   float1x2 f12;
   float1x2 result = abs(f12);
}

void matrix_out_of_bounds() {
  matrix<float, 1, 8> matrix_oob_0; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}} fxc-error {{X3053: matrix dimensions must be between 1 and 4}}
  matrix<float, 0, 1> matrix_oob_1; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}} fxc-error {{X3053: matrix dimensions must be between 1 and 4}}
  matrix<float, -1, 1> matrix_oob_2; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}} fxc-error {{X3053: matrix dimensions must be between 1 and 4}}
}

void main() {
    // Multiple assignments in a chain.
    matrix<float, 4, 4> mymatrix;
    matrix<float, 4, 4> absMatrix = abs(mymatrix);
    matrix<float, 4, 4> absMatrix2 = abs(absMatrix);
    const matrix<float, 4, 4> myConstMatrix = mymatrix;                /* expected-note {{variable 'myConstMatrix' declared const here}} expected-note {{variable 'myConstMatrix' declared const here}} fxc-pass {{}} */

    matrix<float, 2, 4> f24;
    float f;
    float2 f2;
    float3 f3;
    float4 f4;
    float farr2[2];

    // zero-based positions.
    f = mymatrix._m00;
    f2 = mymatrix._m00_m11;
    f4 = mymatrix._m00_m11_m00_m11;
    /*verify-ast
      BinaryOperator <col:5, col:19> 'float4':'vector<float, 4>' '='
      |-DeclRefExpr <col:5> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
      `-ExtMatrixElementExpr <col:10, col:19> 'vector<float, 4>':'vector<float, 4>' _m00_m11_m00_m11
        `-DeclRefExpr <col:10> 'matrix<float, 4, 4>':'matrix<float, 4, 4>' lvalue Var 'mymatrix' 'matrix<float, 4, 4>':'matrix<float, 4, 4>'
    */
    //fxc error X3018 : invalid subscript '_m00_11'
    f2 = mymatrix._m00_11; // expected-error {{matrix subscript '_m00_11' mixes one-based and zero-based references}} fxc-error {{X3018: invalid subscript '_m00_11'}}
    //fxc error X3018: invalid subscript '_m00_m11_m00_m11_m00_m11_m00_m11'
    f24 = mymatrix._m00_m11_m00_m11_m00_m11_m00_m11; // expected-error {{more than four positions are referenced in '_m00_m11_m00_m11_m00_m11_m00_m11'}} fxc-error {{X3018: invalid subscript '_m00_m11_m00_m11_m00_m11_m00_m11'}}
    //fxc error X3017: cannot convert from 'float2' to 'float[2]'
    farr2 = mymatrix._m00_m01; // expected-error {{cannot implicitly convert from 'vector<float, 2>' to 'float [2]'}} fxc-error {{X3017: cannot convert from 'float2' to 'float[2]'}}
    //fxc error X3018: invalid subscript '_m04'
    f = mymatrix._m04; // expected-error {{the digit '4' is used in '_m04', but the syntax is for zero-based rows and columns}} fxc-error {{X3018: invalid subscript '_m04'}}
    f2 = mymatrix._m00_m01;
    //fxc error X3017: cannot implicitly convert from 'float2' to 'float3'
    f3 = mymatrix._m00_m01; // expected-error {{cannot convert from 'vector<float, 2>' to 'float3'}} fxc-error {{X3017: cannot implicitly convert from 'float2' to 'float3'}}
    //fxc warning X3206: implicit truncation of vector type
    f2 = mymatrix._m00_m01_m00; // expected-warning {{implicit truncation of vector type}} fxc-warning {{X3206: implicit truncation of vector type}}
    mymatrix._m00 = mymatrix._m01;
    mymatrix._m00_m11_m02_m13 = mymatrix._m10_m21_m10_m21;
    /*verify-ast
      BinaryOperator <col:5, col:42> 'vector<float, 4>':'vector<float, 4>' '='
      |-ExtMatrixElementExpr <col:5, col:14> 'vector<float, 4>':'vector<float, 4>' lvalue vectorcomponent _m00_m11_m02_m13
      | `-DeclRefExpr <col:5> 'matrix<float, 4, 4>':'matrix<float, 4, 4>' lvalue Var 'mymatrix' 'matrix<float, 4, 4>':'matrix<float, 4, 4>'
      `-ExtMatrixElementExpr <col:33, col:42> 'vector<float, 4>':'vector<float, 4>' _m10_m21_m10_m21
        `-DeclRefExpr <col:33> 'matrix<float, 4, 4>':'matrix<float, 4, 4>' lvalue Var 'mymatrix' 'matrix<float, 4, 4>':'matrix<float, 4, 4>'
    */
    //fxc error X3025: l-value specifies const object
    mymatrix._m00_m11_m00_m11 = mymatrix._m10_m21_m10_m21; // expected-error {{matrix is not assignable (contains duplicate components)}} fxc-error {{X3025: l-value specifies const object}}

    // one-based positions.
    //fxc error X3018: invalid subscript '_00'
    f = mymatrix._00; // expected-error {{the digit '0' is used in '_00', but the syntax is for one-based rows and columns}} fxc-error {{X3018: invalid subscript '_00'}}
    f = mymatrix._11;
    f2 = mymatrix._11_11;
    f4 = mymatrix._11_11_44_44;
    /*verify-ast
      BinaryOperator <col:5, col:19> 'float4':'vector<float, 4>' '='
      |-DeclRefExpr <col:5> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
      `-ExtMatrixElementExpr <col:10, col:19> 'vector<float, 4>':'vector<float, 4>' _11_11_44_44
        `-DeclRefExpr <col:10> 'matrix<float, 4, 4>':'matrix<float, 4, 4>' lvalue Var 'mymatrix' 'matrix<float, 4, 4>':'matrix<float, 4, 4>'
    */
    // member assignment using subscript syntax
    f = mymatrix[0][0];
    f = mymatrix[1][1];
    f2 = mymatrix[1].xx;
    f4 = mymatrix[2];

    f = mymatrix[0][4];                                     /* expected-error {{matrix index '4' is out of bounds}} fxc-pass {{}} */
    f = mymatrix[-1][3];                                    /* expected-error {{matrix index '-1' is out of bounds}} fxc-pass {{}} */
    f4 = mymatrix[10];                                      /* expected-error {{matrix index '10' is out of bounds}} fxc-pass {{}} */

    // accessing const member
    f = myConstMatrix[0][0];
    f = myConstMatrix[1][1];
    f2 = myConstMatrix[1].xx;
    f4 = myConstMatrix[2];

    myConstMatrix[0][0] = 3;                                /* expected-error {{cannot assign to variable 'myConstMatrix' with const-qualified type 'const matrix<float, 4, 4>'}} fxc-error {{X3025: l-value specifies const object}} */
    myConstMatrix[3] = float4(1,2,3,4);                     /* expected-error {{cannot assign to variable 'myConstMatrix' with const-qualified type 'const matrix<float, 4, 4>'}} fxc-error {{X3025: l-value specifies const object}} */

}