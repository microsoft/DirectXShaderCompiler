// RUN: %dxc -Wno-conversion -T cs_6_9       %s | FileCheck %s --check-prefixes=CHECK,F32
// RUN: %dxc -Wno-conversion -T cs_6_9 -DF64 %s | FileCheck %s --check-prefixes=CHECK,F64

RWByteAddressBuffer buf;

// "TYPE" is the mainly focused test type.
// "UNTYPE" is the other type used for mixed precision testing.
#ifdef F64
typedef double TYPE;
typedef float UNTYPE;
#else
typedef float TYPE;
typedef double UNTYPE;
#endif

// Two main test function overloads. One expects matching element types.
// The other uses different types to test ops and overload resolution.
template <typename T, int N> vector<T, N> dostuff(vector<T, N> thing1, vector<T, N> thing2, vector<T, N> thing3);
vector<TYPE, 8> dostuff(vector<TYPE, 8> thing1, vector<UNTYPE, 8> thing2, vector<TYPE, 8> thing3);

// Just a trick to capture the needed type spellings since the DXC version of FileCheck can't do that explicitly.
// F32-DAG: %dx.types.ResRet.[[TY:v8f32]] = type { [[TYPE:<8 x float>]]
// F32-DAG: %dx.types.ResRet.[[UNTY:v8f64]] = type { [[UNTYPE:<8 x double>]]
// F64-DAG: %dx.types.ResRet.[[TY:v8f64]] = type { [[TYPE:<8 x double>]]
// F64-DAG: %dx.types.ResRet.[[UNTY:v8f32]] = type { [[UNTYPE:<8 x float>]]

// Verify that groupshared vectors are kept as aggregates
// CHECK: @"\01?gs_vec1@@3V?$vector@{{M|N}}$07@@A" = external addrspace(3) global [[TYPE]]
// CHECK: @"\01?gs_vec2@@3V?$vector@{{M|N}}$07@@A" = external addrspace(3) global [[TYPE]]
// CHECK: @"\01?gs_vec3@@3V?$vector@{{M|N}}$07@@A" = external addrspace(3) global [[TYPE]]
groupshared vector<TYPE, 8> gs_vec1, gs_vec2, gs_vec3;

[numthreads(8,1,1)]
void main() {
  // CHECK: [[buf:%.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4107, i32 0 })  ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer

  // CHECK: [[vec1_res:%.*]] = call %dx.types.ResRet.[[TY]] @dx.op.rawBufferVectorLoad.[[TY]](i32 303, %dx.types.Handle [[buf]], i32 0
  // CHECK-DAG: [[vec1:%.*]] = extractvalue %dx.types.ResRet.[[TY]] [[vec1_res]], 0
  // F32-DAG: [[vec1_32:%.*]] = extractvalue %dx.types.ResRet.[[TY]] [[vec1_res]], 0
  // F64-DAG: [[vec1_64:%.*]] = extractvalue %dx.types.ResRet.[[TY]] [[vec1_res]], 0
  vector<TYPE, 8> vec1 = buf.Load<vector<TYPE, 8> >(0);

  // CHECK: [[vec2_res:%.*]] = call %dx.types.ResRet.[[TY]] @dx.op.rawBufferVectorLoad.[[TY]](i32 303, %dx.types.Handle [[buf]], i32 60
  // CHECK-DAG: [[vec2:%.*]] = extractvalue %dx.types.ResRet.[[TY]] [[vec2_res]], 0
  // F32-DAG: [[vec2_32:%.*]] = extractvalue %dx.types.ResRet.[[TY]] [[vec2_res]], 0
  // F64-DAG: [[vec2_64:%.*]] = extractvalue %dx.types.ResRet.[[TY]] [[vec2_res]], 0
  vector<TYPE, 8> vec2 = buf.Load<vector<TYPE, 8> >(60);

  // CHECK: [[vec3_res:%.*]] = call %dx.types.ResRet.[[TY]] @dx.op.rawBufferVectorLoad.[[TY]](i32 303, %dx.types.Handle [[buf]], i32 120
  // CHECK-DAG: [[vec3:%.*]] = extractvalue %dx.types.ResRet.[[TY]] [[vec3_res]], 0
  // F64-DAG: [[vec3_64:%.*]] = extractvalue %dx.types.ResRet.[[TY]] [[vec3_res]], 0
  vector<TYPE, 8> vec3 = buf.Load<vector<TYPE, 8> >(120);

  // CHECK: [[unvec_res:%.*]] = call %dx.types.ResRet.[[UNTY]] @dx.op.rawBufferVectorLoad.[[UNTY]](i32 303, %dx.types.Handle [[buf]], i32 180
  // CHECK-DAG: [[unvec:%.*]] = extractvalue %dx.types.ResRet.[[UNTY]] [[unvec_res]], 0
  // F32-DAG: [[unvec_64:%.*]] = extractvalue %dx.types.ResRet.[[UNTY]] [[unvec_res]], 0
  // F64-DAG: [[unvec_32:%.*]] = extractvalue %dx.types.ResRet.[[UNTY]] [[unvec_res]], 0
  vector<UNTYPE, 8> unvec = buf.Load<vector<UNTYPE, 8> >(180);

  vec1 = dostuff(vec1, vec2, vec3);

  // Test mixed type operations
  vec2 = dostuff(vec2, unvec, vec3);

  gs_vec2 = dostuff(gs_vec1, gs_vec2, gs_vec3);

  // mix groupshared and non
  //vec1 = dostuff(vec1, gs_vec2, vec3);

  buf.Store<vector<TYPE, 8> >(240, vec1 * vec2 - vec3 * gs_vec1 + gs_vec2 / gs_vec3);
}

//  Test the required ops on long vectors and confirm correct lowering.
template <typename T, int N>
vector<T, N> dostuff(vector<T, N> thing1, vector<T, N> thing2, vector<T, N> thing3) {
  vector<T, N> res = 0;

  // CHECK: call [[TYPE]] @dx.op.binary.[[TY]](i32 36, [[TYPE]] [[vec1]], [[TYPE]] [[vec2]])  ; FMin(a,b)
  res += min(thing1, thing2);
  // CHECK: call [[TYPE]] @dx.op.binary.[[TY]](i32 35, [[TYPE]] [[vec1]], [[TYPE]] [[vec3]])  ; FMax(a,b)
  res += max(thing1, thing3);

  // CHECK: [[tmp:%.*]] = call [[TYPE]] @dx.op.binary.[[TY]](i32 35, [[TYPE]] [[vec1]], [[TYPE]] [[vec2]])  ; FMax(a,b)
  // CHECK: call [[TYPE]] @dx.op.binary.[[TY]](i32 36, [[TYPE]] [[tmp]], [[TYPE]] [[vec3]])  ; FMin(a,b)
  res += clamp(thing1, thing2, thing3);

  // F32: [[vec3_64:%.*]] = fpext <8 x float> [[vec3]] to <8 x double>
  // F32: [[vec2_64:%.*]] = fpext <8 x float> [[vec2]] to <8 x double>
  // F32: [[vec1_64:%.*]] = fpext <8 x float> [[vec1]] to <8 x double>
  // CHECK: call <8 x double> @dx.op.tertiary.v8f64(i32 47, <8 x double> [[vec1_64]], <8 x double> [[vec2_64]], <8 x double> [[vec3_64]]) ; Fma(a,b,c)
  res += (vector<T, N>)fma((vector<double, N>)thing1, (vector<double, N>)(thing2), (vector<double, N>)thing3);

  // Even in the double test, these will be downconverted because these builtins only take floats.
  // F64: [[vec2_32:%.*]] = fptrunc <8 x double> [[vec2]] to <8 x float>
  // F64: [[vec1_32:%.*]] = fptrunc <8 x double> [[vec1]] to <8 x float>

  // CHECK: [[tmp:%.*]] = fcmp fast olt <8 x float> [[vec2_32]], [[vec1_32]]
  // CHECK: select <8 x i1> [[tmp]], [[TYPE]] zeroinitializer, [[TYPE]]
  res += step(thing1, thing2);

  // CHECK: [[tmp:%.*]] = fmul fast <8 x float> [[vec1_32]], <float 0x
  // CHECK: call <8 x float> @dx.op.unary.v8f32(i32 21, <8 x float> [[tmp]])  ; Exp(value)
  res += exp(thing1);

  // CHECK: [[tmp:%.*]] = call <8 x float> @dx.op.unary.v8f32(i32 23, <8 x float> [[vec1_32]])  ; Log(value)
  // CHECK: fmul fast <8 x float> [[tmp]], <float 0x
  res += log(thing1);

  // CHECK: call <8 x float> @dx.op.unary.v8f32(i32 20, <8 x float> [[vec1_32]])  ; Htan(value)
  res += tanh(thing1);
  // CHECK: call <8 x float> @dx.op.unary.v8f32(i32 17, <8 x float> [[vec1_32]])  ; Atan(value)
  res += atan(thing1);

  return res;
}

// A mixed-type overload to test overload resolution and mingle different vector element types in ops
vector<TYPE, 8> dostuff(vector<TYPE, 8> thing1, vector<UNTYPE, 8> thing2, vector<TYPE, 8> thing3) {
  vector<TYPE, 8> res = 0;

  // F64: [[unvec_64:%.*]] = fpext <8 x float> [[unvec]] to <8 x double>
  // CHECK: call <8 x double> @dx.op.binary.v8f64(i32 36, <8 x double> [[vec2_64]], <8 x double> [[unvec_64]])  ; FMin(a,b)
  res += min(thing1, thing2);

  // CHECK: call [[TYPE]] @dx.op.binary.[[TY]](i32 35, [[TYPE]] [[vec2]], [[TYPE]] [[vec3]]) ; FMax(a,b)
  res += max(thing1, thing3);

  // CHECK: [[tmp:%.*]] = call <8 x double> @dx.op.binary.v8f64(i32 35, <8 x double> [[vec2_64]], <8 x double> [[unvec_64]])  ; FMax(a,b)
  // CHECK: call <8 x double> @dx.op.binary.v8f64(i32 36, <8 x double> [[tmp]], <8 x double> [[vec3_64]])  ; FMin(a,b)
  res += clamp(thing1, thing2, thing3);

  // CHECK: call <8 x double> @dx.op.tertiary.v8f64(i32 47, <8 x double> [[vec2_64]], <8 x double> [[unvec_64]], <8 x double> [[vec3_64]]) ; Fma(a,b,c)
  res += (vector<TYPE, 8>)fma((vector<double,8>)thing1, (vector<double,8>)(thing2), (vector<double,8>)thing3);

  // F32: [[unvec_32:%.*]] = fptrunc <8 x double> [[unvec]] to <8 x float>
  // CHECK: [[tmp:%.*]] = fcmp fast olt <8 x float> [[unvec_32]], [[vec2_32]]
  // CHECK: select <8 x i1> [[tmp]], [[TYPE]] zeroinitializer, [[TYPE]]
  res += step(thing1, thing2);

  // CHECK: [[tmp:%.*]] = fmul fast <8 x float> [[vec2_32]], <float 0x
  // CHECK: call <8 x float> @dx.op.unary.v8f32(i32 21, <8 x float> [[tmp]])  ; Exp(value)
  res += exp(thing1);

  // CHECK: [[tmp:%.*]] = call <8 x float> @dx.op.unary.v8f32(i32 23, <8 x float> [[vec2_32]])  ; Log(value)
  // CHECK: fmul fast <8 x float> [[tmp]], <float 0x
  res += log(thing1);

  // CHECK: call <8 x float> @dx.op.unary.v8f32(i32 20, <8 x float> [[vec2_32]])  ; Htan(value)
  res += tanh(thing1);
  // CHECK: call <8 x float> @dx.op.unary.v8f32(i32 17, <8 x float> [[vec2_32]])  ; Atan(value)
  res += atan(thing1);

  return res;
}
