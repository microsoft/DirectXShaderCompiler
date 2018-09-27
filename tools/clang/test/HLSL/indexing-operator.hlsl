// RUN: %clang_cc1 -fsyntax-only -Wno-unused-value -ffreestanding -verify %s

// To test with the classic compiler, run
// %sdxroot%\tools\x86\fxc.exe /T vs_5_1 indexing-operator.hlsl

Buffer g_b;
StructuredBuffer<float4> g_sb;
Texture1D g_t1d;
Texture1DArray g_t1da;
Texture2D g_t2d;
Texture2DArray g_t2da;
// fxc error X3000: syntax error: unexpected token 'g_t2dms_err'
Texture2DMS g_t2dms_err; // expected-error {{too few template arguments for class template 'Texture2DMS'}} fxc-error {{X3000: syntax error: unexpected token 'g_t2dms_err'}}
Texture2DMS<float4, 8> g_t2dms;
// fxc error X3000: syntax error: unexpected token 'g_t2dmsa_err'
Texture2DMSArray g_t2dmsa_err; // expected-error {{too few template arguments for class template 'Texture2DMSArray'}} fxc-error {{X3000: syntax error: unexpected token 'g_t2dmsa_err'}}
Texture2DMSArray<float4, 8> g_t2dmsa;
Texture3D g_t3d;
TextureCube g_tc;
TextureCubeArray g_tca;

RWStructuredBuffer<float4> g_rw_sb;
// fxc error X3000: syntax error: unexpected token 'g_rw_b_err'
RWBuffer g_rw_b_err; // expected-error {{too few template arguments for class template 'RWBuffer'}} fxc-error {{X3000: syntax error: unexpected token 'g_rw_b_err'}}
RWBuffer<float4> g_rw_b;
// fxc error X3000: syntax error: unexpected token 'g_rw_t1d_err'
RWTexture1D g_rw_t1d_err; // expected-error {{too few template arguments for class template 'RWTexture1D'}} fxc-error {{X3000: syntax error: unexpected token 'g_rw_t1d_err'}}
RWTexture1D<float4> g_rw_t1d;
// fxc error X3000: syntax error: unexpected token 'g_rw_t1da_err'
RWTexture1DArray g_rw_t1da_err; // expected-error {{too few template arguments for class template 'RWTexture1DArray'}} fxc-error {{X3000: syntax error: unexpected token 'g_rw_t1da_err'}}
RWTexture1DArray<float4> g_rw_t1da;
// fxc error X3000: syntax error: unexpected token 'g_rw_t2d_err'
RWTexture2D g_rw_t2d_err; // expected-error {{too few template arguments for class template 'RWTexture2D'}} fxc-error {{X3000: syntax error: unexpected token 'g_rw_t2d_err'}}
RWTexture2D<float4> g_rw_t2d;
// fxc error X3000: syntax error: unexpected token 'g_rw_t2da_err'
RWTexture2DArray g_rw_t2da_err; // expected-error {{too few template arguments for class template 'RWTexture2DArray'}} fxc-error {{X3000: syntax error: unexpected token 'g_rw_t2da_err'}}
RWTexture2DArray<float4> g_rw_t2da;
// fxc error X3000: syntax error: unexpected token 'g_rw_t3d_err'
RWTexture3D g_rw_t3d_err; // expected-error {{too few template arguments for class template 'RWTexture3D'}} fxc-error {{X3000: syntax error: unexpected token 'g_rw_t3d_err'}}
RWTexture3D<float4> g_rw_t3d;

// No such thing as these:
// RWTexture2DMS g_rw_t2dms;
// RWTexture2DMSArray g_rw_t2dmsa;
// RWTextureCube g_rw_tc;
// RWTextureCubeArray g_rw_tca;

float test_vector_indexing()
{
  float4 f4 = { 1, 2, 3, 4 };
  float f = f4[0];
  /*verify-ast
    DeclStmt <col:3, col:18>
    `-VarDecl <col:3, col:17> col:9 used f 'float' cinit
      `-ImplicitCastExpr <col:13, col:17> 'float':'float' <LValueToRValue>
        `-CXXOperatorCallExpr <col:13, col:17> 'float':'float' lvalue
          |-ImplicitCastExpr <col:15, col:17> 'float &(*)(unsigned int)' <FunctionToPointerDecay>
          | `-DeclRefExpr <col:15, col:17> 'float &(unsigned int)' lvalue CXXMethod 'operator[]' 'float &(unsigned int)'
          |-DeclRefExpr <col:13> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
          `-ImplicitCastExpr <col:16> 'unsigned int' <IntegralCast>
            `-IntegerLiteral <col:16> 'literal int' 0
  */
  // fxc error X3121: array, matrix, vector, or indexable object type expected in index expression
  //f = f4[0][1];
  return f;
}

float4 test_scalar_indexing()
{
  float4 f4 = 0;
  f4 += g_b[0];
  /*verify-ast
    CompoundAssignOperator <col:3, col:14> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:14> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:14> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:12, col:14> 'const vector<float, 4> &(*)(unsigned int) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:12, col:14> 'const vector<float, 4> &(unsigned int) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(unsigned int) const'
        |-ImplicitCastExpr <col:9> 'const Buffer<vector<float, 4> >' lvalue <NoOp>
        | `-DeclRefExpr <col:9> 'Buffer':'Buffer<vector<float, 4> >' lvalue Var 'g_b' 'Buffer':'Buffer<vector<float, 4> >'
        `-ImplicitCastExpr <col:13> 'unsigned int' <IntegralCast>
          `-IntegerLiteral <col:13> 'literal int' 0
  */
  f4 += g_t1d[0];
  /*verify-ast
    CompoundAssignOperator <col:3, col:16> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:16> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:16> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:14, col:16> 'const vector<float, 4> &(*)(unsigned int) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:14, col:16> 'const vector<float, 4> &(unsigned int) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(unsigned int) const'
        |-ImplicitCastExpr <col:9> 'const Texture1D<vector<float, 4> >' lvalue <NoOp>
        | `-DeclRefExpr <col:9> 'Texture1D':'Texture1D<vector<float, 4> >' lvalue Var 'g_t1d' 'Texture1D':'Texture1D<vector<float, 4> >'
        `-ImplicitCastExpr <col:15> 'unsigned int' <IntegralCast>
          `-IntegerLiteral <col:15> 'literal int' 0
  */
  f4 += g_sb[0];
  /*verify-ast
    CompoundAssignOperator <col:3, col:15> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:15> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:15> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:13, col:15> 'const vector<float, 4> &(*)(unsigned int) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:13, col:15> 'const vector<float, 4> &(unsigned int) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(unsigned int) const'
        |-ImplicitCastExpr <col:9> 'const StructuredBuffer<vector<float, 4> >' lvalue <NoOp>
        | `-DeclRefExpr <col:9> 'StructuredBuffer<float4>':'StructuredBuffer<vector<float, 4> >' lvalue Var 'g_sb' 'StructuredBuffer<float4>':'StructuredBuffer<vector<float, 4> >'
        `-ImplicitCastExpr <col:14> 'unsigned int' <IntegralCast>
          `-IntegerLiteral <col:14> 'literal int' 0
  */
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t1da[0]; // expected-error {{no viable overloaded operator[] for type 'Texture1DArray'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'literal int' to 'vector<uint, 2>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  // fxc error X3120 : invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t2d[0]; // expected-error {{no viable overloaded operator[] for type 'Texture2D'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'literal int' to 'vector<uint, 2>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  // fxc error X3120 : invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t2da[0]; // expected-error {{no viable overloaded operator[] for type 'Texture2DArray'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'literal int' to 'vector<uint, 3>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  // fxc error X3120 : invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t2dms[0]; // expected-error {{no viable overloaded operator[] for type 'Texture2DMS<float4, 8>'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'literal int' to 'vector<uint, 2>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  // fxc error X3120 : invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t2dmsa[0]; // expected-error {{no viable overloaded operator[] for type 'Texture2DMSArray<float4, 8>'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'literal int' to 'vector<uint, 3>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  // fxc error X3120 : invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t3d[0]; // expected-error {{no viable overloaded operator[] for type 'Texture3D'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'literal int' to 'vector<uint, 3>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  // fxc  error X3121: array, matrix, vector, or indexable object type expected in index expression
  f4 += g_tc[0]; // expected-error {{type 'TextureCube' does not provide a subscript operator}} fxc-error {{X3121: array, matrix, vector, or indexable object type expected in index expression}}
  // fxc  error X3121: array, matrix, vector, or indexable object type expected in index expression
  f4 += g_tca[0]; // expected-error {{type 'TextureCubeArray' does not provide a subscript operator}} fxc-error {{X3121: array, matrix, vector, or indexable object type expected in index expression}}
  return f4;
}

float4 test_vector2_indexing()
{
  int2 offset = { 1, 2 };
  float4 f4 = 0;
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_b[offset]; // expected-error {{no viable overloaded operator[] for type 'Buffer'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'int2' to 'unsigned int' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t1d[offset]; // expected-error {{no viable overloaded operator[] for type 'Texture1D'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'int2' to 'unsigned int' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_sb[offset]; // expected-error {{no viable overloaded operator[] for type 'StructuredBuffer<float4>'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'int2' to 'unsigned int' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  f4 += g_t1da[offset];
  /*verify-ast
    CompoundAssignOperator <col:3, col:22> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:22> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:22> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:15, col:22> 'const vector<float, 4> &(*)(vector<uint, 2>) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:15, col:22> 'const vector<float, 4> &(vector<uint, 2>) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(vector<uint, 2>) const'
        |-ImplicitCastExpr <col:9> 'const Texture1DArray<vector<float, 4> >' lvalue <NoOp>
        | `-DeclRefExpr <col:9> 'Texture1DArray':'Texture1DArray<vector<float, 4> >' lvalue Var 'g_t1da' 'Texture1DArray':'Texture1DArray<vector<float, 4> >'
        `-ImplicitCastExpr <col:16> 'vector<unsigned int, 2>' <HLSLCC_IntegralCast>
          `-ImplicitCastExpr <col:16> 'int2':'vector<int, 2>' <LValueToRValue>
            `-DeclRefExpr <col:16> 'int2':'vector<int, 2>' lvalue Var 'offset' 'int2':'vector<int, 2>'
  */
  f4 += g_t2d[offset];
  /*verify-ast
    CompoundAssignOperator <col:3, col:21> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:21> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:21> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:14, col:21> 'const vector<float, 4> &(*)(vector<uint, 2>) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:14, col:21> 'const vector<float, 4> &(vector<uint, 2>) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(vector<uint, 2>) const'
        |-ImplicitCastExpr <col:9> 'const Texture2D<vector<float, 4> >' lvalue <NoOp>
        | `-DeclRefExpr <col:9> 'Texture2D':'Texture2D<vector<float, 4> >' lvalue Var 'g_t2d' 'Texture2D':'Texture2D<vector<float, 4> >'
        `-ImplicitCastExpr <col:15> 'vector<unsigned int, 2>' <HLSLCC_IntegralCast>
          `-ImplicitCastExpr <col:15> 'int2':'vector<int, 2>' <LValueToRValue>
            `-DeclRefExpr <col:15> 'int2':'vector<int, 2>' lvalue Var 'offset' 'int2':'vector<int, 2>'
  */
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t2da[offset]; // expected-error {{no viable overloaded operator[] for type 'Texture2DArray'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'vector<int, 2>' to 'vector<uint, 3>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  f4 += g_t2dms[offset];
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t2dmsa[offset]; // expected-error {{no viable overloaded operator[] for type 'Texture2DMSArray<float4, 8>'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'vector<int, 2>' to 'vector<uint, 3>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t3d[offset]; // expected-error {{no viable overloaded operator[] for type 'Texture3D'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'vector<int, 2>' to 'vector<uint, 3>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  // fxc error X3121: array, matrix, vector, or indexable object type expected in index expression
  f4 += g_tc[offset]; // expected-error {{type 'TextureCube' does not provide a subscript operator}} fxc-error {{X3121: array, matrix, vector, or indexable object type expected in index expression}}
  // fxc error X3121: array, matrix, vector, or indexable object type expected in index expression
  f4 += g_tca[offset]; // expected-error {{type 'TextureCubeArray' does not provide a subscript operator}} fxc-error {{X3121: array, matrix, vector, or indexable object type expected in index expression}}
  return f4;
}

float4 test_vector3_indexing()
{
  int3 offset = { 1, 2, 3 };
  float4 f4 = 0;
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_b[offset]; // expected-error {{no viable overloaded operator[] for type 'Buffer'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'int3' to 'unsigned int' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t1d[offset]; // expected-error {{no viable overloaded operator[] for type 'Texture1D'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'int3' to 'unsigned int' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_sb[offset]; // expected-error {{no viable overloaded operator[] for type 'StructuredBuffer<float4>'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'int3' to 'unsigned int' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t1da[offset]; // expected-error {{no viable overloaded operator[] for type 'Texture1DArray'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'vector<int, 3>' to 'vector<uint, 2>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t2d[offset]; // expected-error {{no viable overloaded operator[] for type 'Texture2D'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'vector<int, 3>' to 'vector<uint, 2>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  f4 += g_t2da[offset];
  /*verify-ast
    CompoundAssignOperator <col:3, col:22> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:22> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:22> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:15, col:22> 'const vector<float, 4> &(*)(vector<uint, 3>) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:15, col:22> 'const vector<float, 4> &(vector<uint, 3>) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(vector<uint, 3>) const'
        |-ImplicitCastExpr <col:9> 'const Texture2DArray<vector<float, 4> >' lvalue <NoOp>
        | `-DeclRefExpr <col:9> 'Texture2DArray':'Texture2DArray<vector<float, 4> >' lvalue Var 'g_t2da' 'Texture2DArray':'Texture2DArray<vector<float, 4> >'
        `-ImplicitCastExpr <col:16> 'vector<unsigned int, 3>' <HLSLCC_IntegralCast>
          `-ImplicitCastExpr <col:16> 'int3':'vector<int, 3>' <LValueToRValue>
            `-DeclRefExpr <col:16> 'int3':'vector<int, 3>' lvalue Var 'offset' 'int3':'vector<int, 3>'
  */
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t2dms[offset]; // expected-error {{no viable overloaded operator[] for type 'Texture2DMS<float4, 8>'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'vector<int, 3>' to 'vector<uint, 2>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  f4 += g_t2dmsa[offset];
  /*verify-ast
    CompoundAssignOperator <col:3, col:24> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:24> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:24> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:17, col:24> 'const vector<float, 4> &(*)(vector<uint, 3>) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:17, col:24> 'const vector<float, 4> &(vector<uint, 3>) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(vector<uint, 3>) const'
        |-ImplicitCastExpr <col:9> 'const Texture2DMSArray<vector<float, 4>, 8>' lvalue <NoOp>
        | `-DeclRefExpr <col:9> 'Texture2DMSArray<float4, 8>':'Texture2DMSArray<vector<float, 4>, 8>' lvalue Var 'g_t2dmsa' 'Texture2DMSArray<float4, 8>':'Texture2DMSArray<vector<float, 4>, 8>'
        `-ImplicitCastExpr <col:18> 'vector<unsigned int, 3>' <HLSLCC_IntegralCast>
          `-ImplicitCastExpr <col:18> 'int3':'vector<int, 3>' <LValueToRValue>
            `-DeclRefExpr <col:18> 'int3':'vector<int, 3>' lvalue Var 'offset' 'int3':'vector<int, 3>'
  */
  f4 += g_t3d[offset];
  /*verify-ast
    CompoundAssignOperator <col:3, col:21> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:21> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:21> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:14, col:21> 'const vector<float, 4> &(*)(vector<uint, 3>) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:14, col:21> 'const vector<float, 4> &(vector<uint, 3>) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(vector<uint, 3>) const'
        |-ImplicitCastExpr <col:9> 'const Texture3D<vector<float, 4> >' lvalue <NoOp>
        | `-DeclRefExpr <col:9> 'Texture3D':'Texture3D<vector<float, 4> >' lvalue Var 'g_t3d' 'Texture3D':'Texture3D<vector<float, 4> >'
        `-ImplicitCastExpr <col:15> 'vector<unsigned int, 3>' <HLSLCC_IntegralCast>
          `-ImplicitCastExpr <col:15> 'int3':'vector<int, 3>' <LValueToRValue>
            `-DeclRefExpr <col:15> 'int3':'vector<int, 3>' lvalue Var 'offset' 'int3':'vector<int, 3>'
  */
  // fxc error X3121: array, matrix, vector, or indexable object type expected in index expression
  f4 += g_tc[offset]; // expected-error {{type 'TextureCube' does not provide a subscript operator}} fxc-error {{X3121: array, matrix, vector, or indexable object type expected in index expression}}
  // fxc error X3121: array, matrix, vector, or indexable object type expected in index expression
  f4 += g_tca[offset]; // expected-error {{type 'TextureCubeArray' does not provide a subscript operator}} fxc-error {{X3121: array, matrix, vector, or indexable object type expected in index expression}}
  return f4;
}

float4 test_mips_indexing()
{
  // .mips[uint mipSlice, uint pos]
  uint offset = 1;
  float4 f4;
  // fxc error X3018: invalid subscript 'mips'
  f4 += g_b.mips[offset]; // expected-error {{no member named 'mips' in 'Buffer<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'mips'}}
  // fxc error X3022 : scalar, vector, or matrix expected
  f4 += g_t1d.mips[offset]; // expected-error {{scalar, vector, or matrix expected}} fxc-error {{X3022: scalar, vector, or matrix expected}}
  // fxc error X3018: invalid subscript 'mips'
  f4 += g_sb.mips[offset]; // expected-error {{no member named 'mips' in 'StructuredBuffer<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'mips'}}
  // fxc error X3022 : scalar, vector, or matrix expected
  f4 += g_t1da.mips[offset]; // expected-error {{scalar, vector, or matrix expected}} fxc-error {{X3022: scalar, vector, or matrix expected}}
  // fxc error X3022 : scalar, vector, or matrix expected
  f4 += g_t2d.mips[offset]; // expected-error {{scalar, vector, or matrix expected}} fxc-error {{X3022: scalar, vector, or matrix expected}}
  // fxc error X3022 : scalar, vector, or matrix expected
  f4 += g_t2da.mips[offset]; // expected-error {{scalar, vector, or matrix expected}} fxc-error {{X3022: scalar, vector, or matrix expected}}
  // fxc error X3018: invalid subscript 'mips'
  f4 += g_t2dms.mips[offset]; // expected-error {{no member named 'mips' in 'Texture2DMS<vector<float, 4>, 8>'}} fxc-error {{X3018: invalid subscript 'mips'}}
  // fxc error X3018: invalid subscript 'mips'
  f4 += g_t2dmsa.mips[offset]; // expected-error {{no member named 'mips' in 'Texture2DMSArray<vector<float, 4>, 8>'}} fxc-error {{X3018: invalid subscript 'mips'}}
  // fxc error X3022 : scalar, vector, or matrix expected
  f4 += g_t3d.mips[offset]; // expected-error {{scalar, vector, or matrix expected}} fxc-error {{X3022: scalar, vector, or matrix expected}}
  // fxc error X3018: invalid subscript 'mips'
  f4 += g_tc.mips[offset]; // expected-error {{no member named 'mips' in 'TextureCube<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'mips'}}
  // fxc error X3018: invalid subscript 'mips'
  f4 += g_tca.mips[offset]; // expected-error {{no member named 'mips' in 'TextureCubeArray<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'mips'}}
  return f4;
}

float4 test_mips_double_indexing()
{
  // .mips[uint mipSlice, uint pos]
  uint mipSlice = 1;
  uint pos = 1;
  uint2 pos2 = { 1, 2 };
  uint3 pos3 = { 1, 2, 3 };
  float4 f4;
  // fxc error X3018: invalid subscript 'mips'
  f4 += g_b.mips[mipSlice][pos]; // expected-error {{no member named 'mips' in 'Buffer<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'mips'}}
  f4 += g_t1d.mips[mipSlice][pos];
  /*verify-ast
    CompoundAssignOperator <col:3, col:33> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:33> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:33> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:29, col:33> 'const vector<float, 4> &(*)(unsigned int) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:29, col:33> 'const vector<float, 4> &(unsigned int) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(unsigned int) const'
        |-ImplicitCastExpr <col:9, col:28> 'const Texture1D<vector<float, 4> >::mips_slice_type' xvalue <NoOp>
        | `-CXXOperatorCallExpr <col:9, col:28> 'Texture1D<vector<float, 4> >::mips_slice_type' xvalue
        |   |-ImplicitCastExpr <col:19, col:28> 'Texture1D<vector<float, 4> >::mips_slice_type &&(*)(unsigned int) const' <FunctionToPointerDecay>
        |   | `-DeclRefExpr <col:19, col:28> 'Texture1D<vector<float, 4> >::mips_slice_type &&(unsigned int) const' lvalue CXXMethod 'operator[]' 'Texture1D<vector<float, 4> >::mips_slice_type &&(unsigned int) const'
        |   |-ImplicitCastExpr <col:9, col:15> 'const Texture1D<vector<float, 4> >::mips_type' lvalue <NoOp>
        |   | `-MemberExpr <col:9, col:15> 'Texture1D<vector<float, 4> >::mips_type' lvalue .mips
        |   |   `-DeclRefExpr <col:9> 'Texture1D':'Texture1D<vector<float, 4> >' lvalue Var 'g_t1d' 'Texture1D':'Texture1D<vector<float, 4> >'
        |   `-ImplicitCastExpr <col:20> 'uint':'unsigned int' <LValueToRValue>
        |     `-DeclRefExpr <col:20> 'uint':'unsigned int' lvalue Var 'mipSlice' 'uint':'unsigned int'
        `-ImplicitCastExpr <col:30> 'uint':'unsigned int' <LValueToRValue>
          `-DeclRefExpr <col:30> 'uint':'unsigned int' lvalue Var 'pos' 'uint':'unsigned int'
  */
  // fxc error X3018: invalid subscript 'mips'
  f4 += g_sb.mips[mipSlice][pos]; // expected-error {{no member named 'mips' in 'StructuredBuffer<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'mips'}}
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t1da.mips[mipSlice][pos]; // expected-error {{no viable overloaded operator[] for type 'Texture1DArray<vector<float, 4> >::mips_slice_type'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'uint' to 'vector<uint, 2>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  f4 += g_t1da.mips[mipSlice][pos2];
  /*verify-ast
    CompoundAssignOperator <col:3, col:35> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:35> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:35> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:30, col:35> 'const vector<float, 4> &(*)(vector<uint, 2>) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:30, col:35> 'const vector<float, 4> &(vector<uint, 2>) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(vector<uint, 2>) const'
        |-ImplicitCastExpr <col:9, col:29> 'const Texture1DArray<vector<float, 4> >::mips_slice_type' xvalue <NoOp>
        | `-CXXOperatorCallExpr <col:9, col:29> 'Texture1DArray<vector<float, 4> >::mips_slice_type' xvalue
        |   |-ImplicitCastExpr <col:20, col:29> 'Texture1DArray<vector<float, 4> >::mips_slice_type &&(*)(unsigned int) const' <FunctionToPointerDecay>
        |   | `-DeclRefExpr <col:20, col:29> 'Texture1DArray<vector<float, 4> >::mips_slice_type &&(unsigned int) const' lvalue CXXMethod 'operator[]' 'Texture1DArray<vector<float, 4> >::mips_slice_type &&(unsigned int) const'
        |   |-ImplicitCastExpr <col:9, col:16> 'const Texture1DArray<vector<float, 4> >::mips_type' lvalue <NoOp>
        |   | `-MemberExpr <col:9, col:16> 'Texture1DArray<vector<float, 4> >::mips_type' lvalue .mips
        |   |   `-DeclRefExpr <col:9> 'Texture1DArray':'Texture1DArray<vector<float, 4> >' lvalue Var 'g_t1da' 'Texture1DArray':'Texture1DArray<vector<float, 4> >'
        |   `-ImplicitCastExpr <col:21> 'uint':'unsigned int' <LValueToRValue>
        |     `-DeclRefExpr <col:21> 'uint':'unsigned int' lvalue Var 'mipSlice' 'uint':'unsigned int'
        `-ImplicitCastExpr <col:31> 'uint2':'vector<unsigned int, 2>' <LValueToRValue>
          `-DeclRefExpr <col:31> 'uint2':'vector<unsigned int, 2>' lvalue Var 'pos2' 'uint2':'vector<unsigned int, 2>'
  */
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t2d.mips[mipSlice][pos]; // expected-error {{no viable overloaded operator[] for type 'Texture2D<vector<float, 4> >::mips_slice_type'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'uint' to 'vector<uint, 2>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  f4 += g_t2d.mips[mipSlice][pos2];
  /*verify-ast
    CompoundAssignOperator <col:3, col:34> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:34> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:34> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:29, col:34> 'const vector<float, 4> &(*)(vector<uint, 2>) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:29, col:34> 'const vector<float, 4> &(vector<uint, 2>) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(vector<uint, 2>) const'
        |-ImplicitCastExpr <col:9, col:28> 'const Texture2D<vector<float, 4> >::mips_slice_type' xvalue <NoOp>
        | `-CXXOperatorCallExpr <col:9, col:28> 'Texture2D<vector<float, 4> >::mips_slice_type' xvalue
        |   |-ImplicitCastExpr <col:19, col:28> 'Texture2D<vector<float, 4> >::mips_slice_type &&(*)(unsigned int) const' <FunctionToPointerDecay>
        |   | `-DeclRefExpr <col:19, col:28> 'Texture2D<vector<float, 4> >::mips_slice_type &&(unsigned int) const' lvalue CXXMethod 'operator[]' 'Texture2D<vector<float, 4> >::mips_slice_type &&(unsigned int) const'
        |   |-ImplicitCastExpr <col:9, col:15> 'const Texture2D<vector<float, 4> >::mips_type' lvalue <NoOp>
        |   | `-MemberExpr <col:9, col:15> 'Texture2D<vector<float, 4> >::mips_type' lvalue .mips
        |   |   `-DeclRefExpr <col:9> 'Texture2D':'Texture2D<vector<float, 4> >' lvalue Var 'g_t2d' 'Texture2D':'Texture2D<vector<float, 4> >'
        |   `-ImplicitCastExpr <col:20> 'uint':'unsigned int' <LValueToRValue>
        |     `-DeclRefExpr <col:20> 'uint':'unsigned int' lvalue Var 'mipSlice' 'uint':'unsigned int'
        `-ImplicitCastExpr <col:30> 'uint2':'vector<unsigned int, 2>' <LValueToRValue>
          `-DeclRefExpr <col:30> 'uint2':'vector<unsigned int, 2>' lvalue Var 'pos2' 'uint2':'vector<unsigned int, 2>'
  */
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t2da.mips[mipSlice][pos]; // expected-error {{no viable overloaded operator[] for type 'Texture2DArray<vector<float, 4> >::mips_slice_type'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'uint' to 'vector<uint, 3>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  f4 += g_t2da.mips[mipSlice][pos3];
  /*verify-ast
    CompoundAssignOperator <col:3, col:35> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:35> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:35> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:30, col:35> 'const vector<float, 4> &(*)(vector<uint, 3>) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:30, col:35> 'const vector<float, 4> &(vector<uint, 3>) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(vector<uint, 3>) const'
        |-ImplicitCastExpr <col:9, col:29> 'const Texture2DArray<vector<float, 4> >::mips_slice_type' xvalue <NoOp>
        | `-CXXOperatorCallExpr <col:9, col:29> 'Texture2DArray<vector<float, 4> >::mips_slice_type' xvalue
        |   |-ImplicitCastExpr <col:20, col:29> 'Texture2DArray<vector<float, 4> >::mips_slice_type &&(*)(unsigned int) const' <FunctionToPointerDecay>
        |   | `-DeclRefExpr <col:20, col:29> 'Texture2DArray<vector<float, 4> >::mips_slice_type &&(unsigned int) const' lvalue CXXMethod 'operator[]' 'Texture2DArray<vector<float, 4> >::mips_slice_type &&(unsigned int) const'
        |   |-ImplicitCastExpr <col:9, col:16> 'const Texture2DArray<vector<float, 4> >::mips_type' lvalue <NoOp>
        |   | `-MemberExpr <col:9, col:16> 'Texture2DArray<vector<float, 4> >::mips_type' lvalue .mips
        |   |   `-DeclRefExpr <col:9> 'Texture2DArray':'Texture2DArray<vector<float, 4> >' lvalue Var 'g_t2da' 'Texture2DArray':'Texture2DArray<vector<float, 4> >'
        |   `-ImplicitCastExpr <col:21> 'uint':'unsigned int' <LValueToRValue>
        |     `-DeclRefExpr <col:21> 'uint':'unsigned int' lvalue Var 'mipSlice' 'uint':'unsigned int'
        `-ImplicitCastExpr <col:31> 'uint3':'vector<unsigned int, 3>' <LValueToRValue>
          `-DeclRefExpr <col:31> 'uint3':'vector<unsigned int, 3>' lvalue Var 'pos3' 'uint3':'vector<unsigned int, 3>'
  */
  // fxc error X3018: invalid subscript 'mips'
  f4 += g_t2dms.mips[mipSlice][pos]; // expected-error {{no member named 'mips' in 'Texture2DMS<vector<float, 4>, 8>'}} fxc-error {{X3018: invalid subscript 'mips'}}
  // fxc error X3018: invalid subscript 'mips'
  f4 += g_t2dmsa.mips[mipSlice][pos]; // expected-error {{no member named 'mips' in 'Texture2DMSArray<vector<float, 4>, 8>'}} fxc-error {{X3018: invalid subscript 'mips'}}
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t3d.mips[mipSlice][pos]; // expected-error {{no viable overloaded operator[] for type 'Texture3D<vector<float, 4> >::mips_slice_type'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'uint' to 'vector<uint, 3>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  f4 += g_t3d.mips[mipSlice][pos3];
  /*verify-ast
    CompoundAssignOperator <col:3, col:34> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:34> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:34> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:29, col:34> 'const vector<float, 4> &(*)(vector<uint, 3>) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:29, col:34> 'const vector<float, 4> &(vector<uint, 3>) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(vector<uint, 3>) const'
        |-ImplicitCastExpr <col:9, col:28> 'const Texture3D<vector<float, 4> >::mips_slice_type' xvalue <NoOp>
        | `-CXXOperatorCallExpr <col:9, col:28> 'Texture3D<vector<float, 4> >::mips_slice_type' xvalue
        |   |-ImplicitCastExpr <col:19, col:28> 'Texture3D<vector<float, 4> >::mips_slice_type &&(*)(unsigned int) const' <FunctionToPointerDecay>
        |   | `-DeclRefExpr <col:19, col:28> 'Texture3D<vector<float, 4> >::mips_slice_type &&(unsigned int) const' lvalue CXXMethod 'operator[]' 'Texture3D<vector<float, 4> >::mips_slice_type &&(unsigned int) const'
        |   |-ImplicitCastExpr <col:9, col:15> 'const Texture3D<vector<float, 4> >::mips_type' lvalue <NoOp>
        |   | `-MemberExpr <col:9, col:15> 'Texture3D<vector<float, 4> >::mips_type' lvalue .mips
        |   |   `-DeclRefExpr <col:9> 'Texture3D':'Texture3D<vector<float, 4> >' lvalue Var 'g_t3d' 'Texture3D':'Texture3D<vector<float, 4> >'
        |   `-ImplicitCastExpr <col:20> 'uint':'unsigned int' <LValueToRValue>
        |     `-DeclRefExpr <col:20> 'uint':'unsigned int' lvalue Var 'mipSlice' 'uint':'unsigned int'
        `-ImplicitCastExpr <col:30> 'uint3':'vector<unsigned int, 3>' <LValueToRValue>
          `-DeclRefExpr <col:30> 'uint3':'vector<unsigned int, 3>' lvalue Var 'pos3' 'uint3':'vector<unsigned int, 3>'
  */
  // fxc error X3018: invalid subscript 'mips'
  f4 += g_tc.mips[mipSlice][pos]; // expected-error {{no member named 'mips' in 'TextureCube<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'mips'}}
  // fxc error X3018: invalid subscript 'mips'
  f4 += g_tca.mips[mipSlice][pos]; // expected-error {{no member named 'mips' in 'TextureCubeArray<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'mips'}}
  return f4;
}

float4 test_sample_indexing()
{
  // .sample[uint sampleSlice, uint pos]
  uint offset = 1;
  float4 f4;
  // fxc error X3018: invalid subscript 'sample'
  f4 += g_b.sample[offset]; // expected-error {{no member named 'sample' in 'Buffer<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'sample'}}
  // fxc error X3018: invalid subscript 'sample'
  f4 += g_t1d.sample[offset]; // expected-error {{no member named 'sample' in 'Texture1D<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'sample'}}
  // fxc error X3018: invalid subscript 'sample'
  f4 += g_sb.sample[offset]; // expected-error {{no member named 'sample' in 'StructuredBuffer<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'sample'}}
  // fxc error X3018: invalid subscript 'sample'
  f4 += g_t1da.sample[offset]; // expected-error {{no member named 'sample' in 'Texture1DArray<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'sample'}}
  // fxc error X3018: invalid subscript 'sample'
  f4 += g_t2d.sample[offset]; // expected-error {{no member named 'sample' in 'Texture2D<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'sample'}}
  // fxc error X3018: invalid subscript 'sample'
  f4 += g_t2da.sample[offset]; // expected-error {{no member named 'sample' in 'Texture2DArray<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'sample'}}
  // fxc error X3022: scalar, vector, or matrix expected
  f4 += g_t2dms.sample[offset]; // expected-error {{scalar, vector, or matrix expected}} fxc-error {{X3022: scalar, vector, or matrix expected}}
  // fxc error X3022: scalar, vector, or matrix expected
  f4 += g_t2dmsa.sample[offset]; // expected-error {{scalar, vector, or matrix expected}} fxc-error {{X3022: scalar, vector, or matrix expected}}
  // fxc error X3018: invalid subscript 'sample'
  f4 += g_t3d.sample[offset]; // expected-error {{no member named 'sample' in 'Texture3D<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'sample'}}
  // fxc error X3018: invalid subscript 'sample'
  f4 += g_tc.sample[offset]; // expected-error {{no member named 'sample' in 'TextureCube<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'sample'}}
  // fxc error X3018: invalid subscript 'sample'
  f4 += g_tca.sample[offset]; // expected-error {{no member named 'sample' in 'TextureCubeArray<vector<float, 4> >'}} fxc-error {{X3018: invalid subscript 'sample'}}
  return f4;
}

float4 test_sample_double_indexing()
{
  // .sample[uint sampleSlice, uint pos]
  uint sampleSlice = 1;
  uint pos = 1;
  uint2 pos2 = { 1, 2 };
  uint3 pos3 = { 1, 2, 3 };
  float4 f4;
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t2dms.sample[sampleSlice][pos]; // expected-error {{no viable overloaded operator[] for type 'Texture2DMS<vector<float, 4>, 8>::sample_slice_type'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'uint' to 'vector<uint, 2>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  f4 += g_t2dms.sample[sampleSlice][pos2];
  /*verify-ast
    CompoundAssignOperator <col:3, col:41> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:41> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:41> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:36, col:41> 'const vector<float, 4> &(*)(vector<uint, 2>) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:36, col:41> 'const vector<float, 4> &(vector<uint, 2>) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(vector<uint, 2>) const'
        |-ImplicitCastExpr <col:9, col:35> 'const Texture2DMS<vector<float, 4>, 8>::sample_slice_type' xvalue <NoOp>
        | `-CXXOperatorCallExpr <col:9, col:35> 'Texture2DMS<vector<float, 4>, 8>::sample_slice_type' xvalue
        |   |-ImplicitCastExpr <col:23, col:35> 'Texture2DMS<vector<float, 4>, 8>::sample_slice_type &&(*)(unsigned int) const' <FunctionToPointerDecay>
        |   | `-DeclRefExpr <col:23, col:35> 'Texture2DMS<vector<float, 4>, 8>::sample_slice_type &&(unsigned int) const' lvalue CXXMethod 'operator[]' 'Texture2DMS<vector<float, 4>, 8>::sample_slice_type &&(unsigned int) const'
        |   |-ImplicitCastExpr <col:9, col:17> 'const Texture2DMS<vector<float, 4>, 8>::sample_type' lvalue <NoOp>
        |   | `-MemberExpr <col:9, col:17> 'Texture2DMS<vector<float, 4>, 8>::sample_type' lvalue .sample
        |   |   `-DeclRefExpr <col:9> 'Texture2DMS<float4, 8>':'Texture2DMS<vector<float, 4>, 8>' lvalue Var 'g_t2dms' 'Texture2DMS<float4, 8>':'Texture2DMS<vector<float, 4>, 8>'
        |   `-ImplicitCastExpr <col:24> 'uint':'unsigned int' <LValueToRValue>
        |     `-DeclRefExpr <col:24> 'uint':'unsigned int' lvalue Var 'sampleSlice' 'uint':'unsigned int'
        `-ImplicitCastExpr <col:37> 'uint2':'vector<unsigned int, 2>' <LValueToRValue>
          `-DeclRefExpr <col:37> 'uint2':'vector<unsigned int, 2>' lvalue Var 'pos2' 'uint2':'vector<unsigned int, 2>'
  */
  // fxc error X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions
  f4 += g_t2dmsa.sample[sampleSlice][pos]; // expected-error {{no viable overloaded operator[] for type 'Texture2DMSArray<vector<float, 4>, 8>::sample_slice_type'}} expected-note {{candidate function [with element = const vector<float, 4> &] not viable: no known conversion from 'uint' to 'vector<uint, 3>' for 1st argument}} fxc-error {{X3120: invalid type for index - index must be a scalar, or a vector with the correct number of dimensions}}
  f4 += g_t2dmsa.sample[sampleSlice][pos3];
  /*verify-ast
    CompoundAssignOperator <col:3, col:42> 'float4':'vector<float, 4>' lvalue '+=' ComputeLHSTy='float4':'vector<float, 4>' ComputeResultTy='float4':'vector<float, 4>'
    |-DeclRefExpr <col:3> 'float4':'vector<float, 4>' lvalue Var 'f4' 'float4':'vector<float, 4>'
    `-ImplicitCastExpr <col:9, col:42> 'vector<float, 4>' <LValueToRValue>
      `-CXXOperatorCallExpr <col:9, col:42> 'const vector<float, 4>' lvalue
        |-ImplicitCastExpr <col:37, col:42> 'const vector<float, 4> &(*)(vector<uint, 3>) const' <FunctionToPointerDecay>
        | `-DeclRefExpr <col:37, col:42> 'const vector<float, 4> &(vector<uint, 3>) const' lvalue CXXMethod 'operator[]' 'const vector<float, 4> &(vector<uint, 3>) const'
        |-ImplicitCastExpr <col:9, col:36> 'const Texture2DMSArray<vector<float, 4>, 8>::sample_slice_type' xvalue <NoOp>
        | `-CXXOperatorCallExpr <col:9, col:36> 'Texture2DMSArray<vector<float, 4>, 8>::sample_slice_type' xvalue
        |   |-ImplicitCastExpr <col:24, col:36> 'Texture2DMSArray<vector<float, 4>, 8>::sample_slice_type &&(*)(unsigned int) const' <FunctionToPointerDecay>
        |   | `-DeclRefExpr <col:24, col:36> 'Texture2DMSArray<vector<float, 4>, 8>::sample_slice_type &&(unsigned int) const' lvalue CXXMethod 'operator[]' 'Texture2DMSArray<vector<float, 4>, 8>::sample_slice_type &&(unsigned int) const'
        |   |-ImplicitCastExpr <col:9, col:18> 'const Texture2DMSArray<vector<float, 4>, 8>::sample_type' lvalue <NoOp>
        |   | `-MemberExpr <col:9, col:18> 'Texture2DMSArray<vector<float, 4>, 8>::sample_type' lvalue .sample
        |   |   `-DeclRefExpr <col:9> 'Texture2DMSArray<float4, 8>':'Texture2DMSArray<vector<float, 4>, 8>' lvalue Var 'g_t2dmsa' 'Texture2DMSArray<float4, 8>':'Texture2DMSArray<vector<float, 4>, 8>'
        |   `-ImplicitCastExpr <col:25> 'uint':'unsigned int' <LValueToRValue>
        |     `-DeclRefExpr <col:25> 'uint':'unsigned int' lvalue Var 'sampleSlice' 'uint':'unsigned int'
        `-ImplicitCastExpr <col:38> 'uint3':'vector<unsigned int, 3>' <LValueToRValue>
          `-DeclRefExpr <col:38> 'uint3':'vector<unsigned int, 3>' lvalue Var 'pos3' 'uint3':'vector<unsigned int, 3>'
  */
  return f4;
}

// Verify subscript access through a const parameter.
struct my_struct { float2 f2; float4 f4; };
float fn_sb(const StructuredBuffer < my_struct > sb) {
  float4 f4 = sb[0].f4; // works fine
  /*verify-ast
    DeclStmt <col:3, col:23>
    `-VarDecl <col:3, col:21> col:10 used f4 'float4':'vector<float, 4>' cinit
      `-ImplicitCastExpr <col:15, col:21> 'float4':'vector<float, 4>' <LValueToRValue>
        `-MemberExpr <col:15, col:21> 'const float4':'const vector<float, 4>' lvalue .f4
          `-CXXOperatorCallExpr <col:15, col:19> 'const my_struct' lvalue
            |-ImplicitCastExpr <col:17, col:19> 'const my_struct &(*)(unsigned int) const' <FunctionToPointerDecay>
            | `-DeclRefExpr <col:17, col:19> 'const my_struct &(unsigned int) const' lvalue CXXMethod 'operator[]' 'const my_struct &(unsigned int) const'
            |-DeclRefExpr <col:15> 'const StructuredBuffer<my_struct>':'const StructuredBuffer<my_struct>' lvalue ParmVar 'sb' 'const StructuredBuffer<my_struct>':'const StructuredBuffer<my_struct>'
            `-ImplicitCastExpr <col:18> 'unsigned int' <IntegralCast>
              `-IntegerLiteral <col:18> 'literal int' 0
  */
  sb[0].f4.x = 1; // expected-error {{read-only variable is not assignable}} fxc-error {{X3025: l-value specifies const object}}
  return f4.x;
}

Texture1D t1d;

void my_subscripts()
{
  int i;
  int2 i2;
  int ai2[2];
  int2x2 i22;

  // fxc error X3059: array dimension must be between 1 and 65536
  int ai0[0]; // expected-error {{array dimension must be between 1 and 65536}} fxc-error {{X3059: array dimension must be between 1 and 65536}}
  int ai1[1];
  int ai65536[65536];
  // fxc error X3059: array dimension must be between 1 and 65536
  int ai65537[65537]; // expected-error {{array dimension must be between 1 and 65536}} fxc-error {{X3059: array dimension must be between 1 and 65536}}

  // fxc error X3121: array, matrix, vector, or indexable object type expected in index expression
  i[0] = 1; // expected-error {{subscripted value is not an array, matrix, or vector}} fxc-error {{X3121: array, matrix, vector, or indexable object type expected in index expression}}
  i2[0] = 1;
  ai2[0] = 1;
  i22[0][0] = 2;

  // TODO: these warnings are issued for the operator[] overloads defined automatically; it would be better to (a)
  // not emit warnings while considering for overloads, and instead only emit (a single one) when noting why candidates
  // were rejected.
  //
  i22[1.5][0] = 2; // expected-warning {{implicit conversion from 'literal float' to 'unsigned int' changes value from 1.5 to 1}} fxc-pass {{}}
  /*verify-ast
    BinaryOperator <col:3, col:17> 'int':'int' '='
    |-CXXOperatorCallExpr <col:3, col:13> 'int':'int' lvalue
    | |-ImplicitCastExpr <col:11, col:13> 'int &(*)(unsigned int)' <FunctionToPointerDecay>
    | | `-DeclRefExpr <col:11, col:13> 'int &(unsigned int)' lvalue CXXMethod 'operator[]' 'int &(unsigned int)'
    | |-CXXOperatorCallExpr <col:3, col:10> 'vector<int, 2>':'vector<int, 2>' lvalue
    | | |-ImplicitCastExpr <col:6, col:10> 'vector<int, 2> &(*)(unsigned int)' <FunctionToPointerDecay>
    | | | `-DeclRefExpr <col:6, col:10> 'vector<int, 2> &(unsigned int)' lvalue CXXMethod 'operator[]' 'vector<int, 2> &(unsigned int)'
    | | |-DeclRefExpr <col:3> 'int2x2':'matrix<int, 2, 2>' lvalue Var 'i22' 'int2x2':'matrix<int, 2, 2>'
    | | `-ImplicitCastExpr <col:7> 'unsigned int' <FloatingToIntegral>
    | |   `-FloatingLiteral <col:7> 'literal float' 1.500000e+00
    | `-ImplicitCastExpr <col:12> 'unsigned int' <IntegralCast>
    |   `-IntegerLiteral <col:12> 'literal int' 0
    `-ImplicitCastExpr <col:17> 'int':'int' <IntegralCast>
      `-IntegerLiteral <col:17> 'literal int' 2
  */
  i22[0] = i2;
  /*verify-ast
    BinaryOperator <col:3, col:12> 'vector<int, 2>':'vector<int, 2>' '='
    |-CXXOperatorCallExpr <col:3, col:8> 'vector<int, 2>':'vector<int, 2>' lvalue
    | |-ImplicitCastExpr <col:6, col:8> 'vector<int, 2> &(*)(unsigned int)' <FunctionToPointerDecay>
    | | `-DeclRefExpr <col:6, col:8> 'vector<int, 2> &(unsigned int)' lvalue CXXMethod 'operator[]' 'vector<int, 2> &(unsigned int)'
    | |-DeclRefExpr <col:3> 'int2x2':'matrix<int, 2, 2>' lvalue Var 'i22' 'int2x2':'matrix<int, 2, 2>'
    | `-ImplicitCastExpr <col:7> 'unsigned int' <IntegralCast>
    |   `-IntegerLiteral <col:7> 'literal int' 0
    `-ImplicitCastExpr <col:12> 'int2':'vector<int, 2>' <LValueToRValue>
      `-DeclRefExpr <col:12> 'int2':'vector<int, 2>' lvalue Var 'i2' 'int2':'vector<int, 2>'
  */

  // Floats are fine.
  i2[1.5f] = 1; // expected-warning {{implicit conversion from 'float' to 'unsigned int' changes value from 1.5 to 1}} fxc-pass {{}}
  /*verify-ast
    BinaryOperator <col:3, col:14> 'int':'int' '='
    |-CXXOperatorCallExpr <col:3, col:10> 'int':'int' lvalue
    | |-ImplicitCastExpr <col:5, col:10> 'int &(*)(unsigned int)' <FunctionToPointerDecay>
    | | `-DeclRefExpr <col:5, col:10> 'int &(unsigned int)' lvalue CXXMethod 'operator[]' 'int &(unsigned int)'
    | |-DeclRefExpr <col:3> 'int2':'vector<int, 2>' lvalue Var 'i2' 'int2':'vector<int, 2>'
    | `-ImplicitCastExpr <col:6> 'unsigned int' <FloatingToIntegral>
    |   `-FloatingLiteral <col:6> 'float' 1.500000e+00
    `-ImplicitCastExpr <col:14> 'int':'int' <IntegralCast>
      `-IntegerLiteral <col:14> 'literal int' 1
  */
  float fone = 1;
  i2[fone] = 1;
  /*verify-ast
    BinaryOperator <col:3, col:14> 'int':'int' '='
    |-CXXOperatorCallExpr <col:3, col:10> 'int':'int' lvalue
    | |-ImplicitCastExpr <col:5, col:10> 'int &(*)(unsigned int)' <FunctionToPointerDecay>
    | | `-DeclRefExpr <col:5, col:10> 'int &(unsigned int)' lvalue CXXMethod 'operator[]' 'int &(unsigned int)'
    | |-DeclRefExpr <col:3> 'int2':'vector<int, 2>' lvalue Var 'i2' 'int2':'vector<int, 2>'
    | `-ImplicitCastExpr <col:6> 'unsigned int' <FloatingToIntegral>
    |   `-ImplicitCastExpr <col:6> 'float' <LValueToRValue>
    |     `-DeclRefExpr <col:6> 'float' lvalue Var 'fone' 'float'
    `-ImplicitCastExpr <col:14> 'int':'int' <IntegralCast>
      `-IntegerLiteral <col:14> 'literal int' 1
  */
  // fxc error X3121: array, matrix, vector, or indexable object type expected in index expression
  i = 1[ai2]; // expected-error {{HLSL does not support having the base of a subscript operator in brackets}} fxc-error {{X3121: array, matrix, vector, or indexable object type expected in index expression}}
}

float4 plain(float4 param4 /* : FOO */) /*: FOO */{
  return 0; //  test_mips_double_indexing();
}