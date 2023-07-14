// RUN: %clang_cc1 -fsyntax-only -ffreestanding -HV 2018 -verify %s -ast-dump | FileCheck %s

float overload1(float f) { return 1; }
double overload1(double f) { return 2; }
int overload1(int i) { return 3; }
uint overload1(uint i) { return 4; }
min12int overload1(min12int i) { return 5; }                   /* expected-warning {{'min12int' is promoted to 'min16int'}} expected-warning {{'min12int' is promoted to 'min16int'}} */


static const float2 g_f2_arr[8] =
{
  float2 (-0.1234,  0.4321) / 0.8750,
  float2 ( 0.0000, -0.0012) / 0.8750,
  float2 ( 0.5555,  0.5555) / 0.8750,
  float2 (-0.6666,  0.0000) / 0.8750,
  float2 ( 0.3333, -0.0000) / 0.8750,
  float2 ( 0.0000,  0.3213) / 0.8750,
  float2 (-0.1234, -0.4567) / 0.8750,
  float2 ( 0.1255,  0.0000) / 0.8750,
};

float2 g_f2;
float2 rotation;
float fn_f_f(float r)
{
  // 1-digit integers go through separate code path, so also check multi-digit:

  int tap = 0;
  float2x2 rotationMatrix = { rotation.x, rotation.y, -rotation.y, rotation.x };

  //CHECK: DeclStmt 0x{{[0-9a-f]+}} <line:29:3, col:80>
  //CHECK-NEXT:  `-VarDecl 0x{{[0-9a-f]+}} <col:3, col:79> col:12 used rotationMatrix 'float2x2':'matrix<float, 2, 2>'
  //CHECK-NEXT:    `-InitListExpr  0x{{[0-9a-f]+}} <col:29, col:79> 'float2x2':'matrix<float, 2, 2>'
  //CHECK-NEXT:      |-ImplicitCastExpr 0x{{[0-9a-f]+}}  <col:31, col:40> 'float' <LValueToRValue>
  //CHECK-NEXT:      | `-HLSLVectorElementExpr 0x{{[0-9a-f]+}}  <col:31, col:40> 'const float' lvalue vectorcomponent x
  //CHECK-NEXT:      |   `-DeclRefExpr  0x{{[0-9a-f]+}} <col:31> 'const float2':'const vector<float, 2>' lvalue Var 0x{{[0-9a-f]+}} 'rotation' 'const float2':'const vector<float, 2>'
  //CHECK-NEXT:      |-ImplicitCastExpr  0x{{[0-9a-f]+}} <col:43, col:52> 'float' <LValueToRValue>
  //CHECK-NEXT:      | `-HLSLVectorElementExpr  0x{{[0-9a-f]+}} <col:43, col:52> 'const float' lvalue vectorcomponent y
  //CHECK-NEXT:      |   `-DeclRefExpr  0x{{[0-9a-f]+}} <col:43> 'const float2':'const vector<float, 2>' lvalue Var 0x{{[0-9a-f]+}} 'rotation' 'const float2':'const vector<float, 2>'
  //CHECK-NEXT:      |-UnaryOperator  0x{{[0-9a-f]+}} <col:55, col:65> 'float' prefix '-'
  //CHECK-NEXT:      | `-ImplicitCastExpr  0x{{[0-9a-f]+}} <col:56, col:65> 'float' <LValueToRValue>
  //CHECK-NEXT:      |   `-HLSLVectorElementExpr  0x{{[0-9a-f]+}} <col:56, col:65> 'const float' lvalue vectorcomponent y
  //CHECK-NEXT:      |     `-DeclRefExpr  0x{{[0-9a-f]+}} <col:56> 'const float2':'const vector<float, 2>' lvalue Var 0x{{[0-9a-f]+}} 'rotation' 'const float2':'const vector<float, 2>'
  //CHECK-NEXT:      `-ImplicitCastExpr  0x{{[0-9a-f]+}} <col:68, col:77> 'float' <LValueToRValue>
  //CHECK-NEXT:        `-HLSLVectorElementExpr  0x{{[0-9a-f]+}} <col:68, col:77> 'const float' lvalue vectorcomponent x
  //CHECK-NEXT:          `-DeclRefExpr  0x{{[0-9a-f]+}} <col:68> 'const float2':'const vector<float, 2>' lvalue Var 0x{{[0-9a-f]+}} 'rotation' 'const float2':'const vector<float, 2>'

  float2 offs = mul(g_f2_arr[tap], rotationMatrix) * r;
  //CHECK: DeclStmt 0x{{[0-9a-f]+}} <line:48:3, col:55>
  //CHECK-NEXT:  `-VarDecl 0x{{[0-9a-f]+}} <col:3, col:54> col:10 used offs 'float2':'vector<float, 2>'
  //CHECK-NEXT:    `-BinaryOperator 0x{{[0-9a-f]+}} <col:17, col:54> 'vector<float, 2>':'vector<float, 2>' '*'
  //CHECK-NEXT:      |-CallExpr 0x{{[0-9a-f]+}} <col:17, col:50> 'vector<float, 2>':'vector<float, 2>'
  //CHECK-NEXT:      | |-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:17> 'vector<float, 2> (*)(vector<float, 2>, matrix<float, 2, 2>)' <FunctionToPointerDecay>
  //CHECK-NEXT:      | | `-DeclRefExpr 0x{{[0-9a-f]+}} <col:17> 'vector<float, 2> (vector<float, 2>, matrix<float, 2, 2>)' lvalue Function 0x{{[0-9a-f]+}} 'mul' 'vector<float, 2> (vector<float, 2>, matrix<float, 2, 2>)'
  //CHECK-NEXT:      | |-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:21, col:33> 'float2':'vector<float, 2>' <LValueToRValue>
  //CHECK-NEXT:      | | `-ArraySubscriptExpr 0x{{[0-9a-f]+}} <col:21, col:33> 'const float2':'const vector<float, 2>' lvalue
  //CHECK-NEXT:      | | |-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:21> 'const float2 [8]' <LValueToRValue>
  //CHECK-NEXT:      | | | `-DeclRefExpr 0x{{[0-9a-f]+}} <col:21> 'const float2 [8]' lvalue Var 0x{{[0-9a-f]+}} 'g_f2_arr' 'const float2 [8]'
  //CHECK-NEXT:      | | `-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:30> 'int' <LValueToRValue>
  //CHECK-NEXT:      | |   `-DeclRefExpr 0x{{[0-9a-f]+}} <col:30> 'int' lvalue Var 0x{{[0-9a-f]+}} 'tap' 'int'
  //CHECK-NEXT:      | `-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:36> 'float2x2':'matrix<float, 2, 2>' <LValueToRValue>
  //CHECK-NEXT:      | `-DeclRefExpr 0x{{[0-9a-f]+}} <col:36> 'float2x2':'matrix<float, 2, 2>' lvalue Var 0x{{[0-9a-f]+}} 'rotationMatrix' 'float2x2':'matrix<float, 2, 2>'
  //CHECK-NEXT:      `-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:54> 'vector<float, 2>':'vector<float, 2>' <HLSLVectorSplat>
  //CHECK-NEXT:        `-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:54> 'float' <LValueToRValue>
  //CHECK-NEXT:          `-DeclRefExpr 0x{{[0-9a-f]+}} <col:54> 'float' lvalue ParmVar 0x{{[0-9a-f]+}} 'r' 'float'
  return offs.x;
}

uint fn_f3_f3io_u(float3 wn, inout float3 tsn)
{
  uint  e3 = 0;
  float d1 = (wn.x + wn.y + wn.z) * 0.5;

    //CHECK: DeclStmt 0x{{[0-9a-f]+}} <line:72:3, col:40>
    //CHECK-NEXT:`-VarDecl 0x{{[0-9a-f]+}} <col:3, col:37> col:9 used d1 'float'
    //CHECK-NEXT:  `-BinaryOperator 0x{{[0-9a-f]+}} <col:14, col:37> 'float' '*'
    //CHECK-NEXT:    |-ParenExpr 0x{{[0-9a-f]+}} <col:14, col:33> 'float'
    //CHECK-NEXT:    | `-BinaryOperator 0x{{[0-9a-f]+}} <col:15, col:32> 'float' '+'
    //CHECK-NEXT:    |   |-BinaryOperator 0x{{[0-9a-f]+}} <col:15, col:25> 'float' '+'
    //CHECK-NEXT:    |   | |-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:15, col:18> 'float' <LValueToRValue>
    //CHECK-NEXT:    |   | | `-HLSLVectorElementExpr 0x{{[0-9a-f]+}} <col:15, col:18> 'float' lvalue vectorcomponent x
    //CHECK-NEXT:    |   | |   `-DeclRefExpr 0x{{[0-9a-f]+}} <col:15> 'float3':'vector<float, 3>' lvalue ParmVar 0x{{[0-9a-f]+}} 'wn' 'float3':'vector<float, 3>'
    //CHECK-NEXT:    |   | `-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:22, col:25> 'float' <LValueToRValue>
    //CHECK-NEXT:    |   |   `-HLSLVectorElementExpr 0x{{[0-9a-f]+}} <col:22, col:25> 'float' lvalue vectorcomponent y
    //CHECK-NEXT:    |   |     `-DeclRefExpr 0x{{[0-9a-f]+}} <col:22> 'float3':'vector<float, 3>' lvalue ParmVar 0x{{[0-9a-f]+}} 'wn' 'float3':'vector<float, 3>'
    //CHECK-NEXT:    |   `-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:29, col:32> 'float' <LValueToRValue>
    //CHECK-NEXT:    |     `-HLSLVectorElementExpr 0x{{[0-9a-f]+}} <col:29, col:32> 'float' lvalue vectorcomponent z
    //CHECK-NEXT:    |       `-DeclRefExpr 0x{{[0-9a-f]+}} <col:29> 'float3':'vector<float, 3>' lvalue ParmVar 0x{{[0-9a-f]+}} 'wn' 'float3':'vector<float, 3>'
    //CHECK-NEXT:    `-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:37> 'float' <FloatingCast>
    //CHECK-NEXT:      `-FloatingLiteral 0x{{[0-9a-f]+}} <col:37> 'literal float' 5.000000e-01

  float d2 = wn.x - d1;
  float d3 = wn.y - d1;
  float d4 = wn.z - d1;
  float dm = max(max(d1, d2), max(d3, d4));

  float3 nn = tsn;
  if (d2 == dm) { e3 = 1; nn *= float3 (1, -1, -1); dm += 2; }
  if (d3 == dm) { e3 = 2; nn *= float3 (-1, 1, -1); dm += 2; }
  if (d4 == dm) { e3 = 3; nn *= float3 (-1, -1, 1); }

  tsn.z = nn.x + nn.y + nn.z;
  tsn.y = nn.z - nn.x;
  tsn.x = tsn.z - 3 * nn.y;

  const float sqrt_2 = 1.414213562373f;
  const float sqrt_3 = 1.732050807569f;
  const float sqrt_6 = 2.449489742783f;

  tsn *= float3 (1.0 / sqrt_6, 1.0 / sqrt_2, 1.0 / sqrt_3);

    //CHECK: CompoundAssignOperator 0x{{[0-9a-f]+}} <line:110:3, col:58> 'float3':'vector<float, 3>' lvalue '*=' ComputeLHSTy='float3':'vector<float, 3>' ComputeResultTy='float3':'vector<float, 3>'
    //CHECK-NEXT:|-DeclRefExpr 0x{{[0-9a-f]+}} <col:3> 'float3':'vector<float, 3>' lvalue ParmVar 0x{{[0-9a-f]+}} 'tsn' 'float3 &
    //CHECK-NEXT:`-CXXFunctionalCastExpr 0x{{[0-9a-f]+}} <col:10, col:58> 'float3':'vector<float, 3>' functional cast to float3 <NoOp>
    //CHECK-NEXT:  `-InitListExpr 0x{{[0-9a-f]+}} <col:17, col:58> 'float3':'vector<float, 3>'
    //CHECK-NEXT:    |-BinaryOperator 0x{{[0-9a-f]+}} <col:18, col:24> 'float' '/'
    //CHECK-NEXT:    | |-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:18> 'float' <FloatingCast>
    //CHECK-NEXT:    | | `-FloatingLiteral 0x{{[0-9a-f]+}} <col:18> 'literal float' 1.000000e+00
    //CHECK-NEXT:    | `-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:24> 'float' <LValueToRValue>
    //CHECK-NEXT:    |   `-DeclRefExpr 0x{{[0-9a-f]+}} <col:24> 'const float' lvalue Var 0x{{[0-9a-f]+}} 'sqrt_6' 'const float'
    //CHECK-NEXT:    |-BinaryOperator 0x{{[0-9a-f]+}} <col:32, col:38> 'float' '/'
    //CHECK-NEXT:    | |-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:32> 'float' <FloatingCast>
    //CHECK-NEXT:    | | `-FloatingLiteral 0x{{[0-9a-f]+}} <col:32> 'literal float' 1.000000e+00
    //CHECK-NEXT:    | `-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:38> 'float' <LValueToRValue>
    //CHECK-NEXT:    |   `-DeclRefExpr 0x{{[0-9a-f]+}} <col:38> 'const float' lvalue Var 0x{{[0-9a-f]+}} 'sqrt_2' 'const float'
    //CHECK-NEXT:    `-BinaryOperator 0x{{[0-9a-f]+}} <col:46, col:52> 'float' '/'
    //CHECK-NEXT:      |-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:46> 'float' <FloatingCast>
    //CHECK-NEXT:      | `-FloatingLiteral 0x{{[0-9a-f]+}} <col:46> 'literal float' 1.000000e+00
    //CHECK-NEXT:      `-ImplicitCastExpr 0x{{[0-9a-f]+}} <col:52> 'float' <LValueToRValue>
    //CHECK-NEXT:        `-DeclRefExpr 0x{{[0-9a-f]+}} <col:52> 'const float' lvalue Var 0x{{[0-9a-f]+}} 'sqrt_3' 'const float'


  return e3;
}


[numthreads(8,8,1)]
void cs_main() {
}