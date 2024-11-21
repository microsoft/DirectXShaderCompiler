// RUN: %dxc -fcgl -T lib_6_9 -DTYPE=float     -DNUM=7 %s | FileCheck %s
// RUN: %dxc -fcgl -T lib_6_9 -DTYPE=bool      -DNUM=7 %s | FileCheck %s
// RUN: %dxc -fcgl -T lib_6_9 -DTYPE=uint64_t  -DNUM=7 %s | FileCheck %s
// RUN: %dxc -fcgl -T lib_6_9 -DTYPE=double    -DNUM=7 %s | FileCheck %s
// RUN: %dxc -fcgl -T lib_6_9 -DTYPE=float16_t -DNUM=7 -enable-16bit-types %s | FileCheck %s
// RUN: %dxc -fcgl -T lib_6_9 -DTYPE=int16_t   -DNUM=7 -enable-16bit-types %s | FileCheck %s

// A test to verify that declarations of longvecs are permitted in all the accepted places.
// Only tests for acceptance, most codegen is ignored for now.

// CHECK: %struct.LongVec = type { <4 x float>, <7 x [[STY:[a-z0-9]*]]> }
struct LongVec {
  float4 f;
  vector<TYPE,NUM> vec;
};


// Just some dummies to capture the types and mangles.
// CHECK: @"\01?dummy@@3[[MNG:F|M|N|_N|_K|\$f16@]]A" = external addrspace(3) global [[STY]]
groupshared TYPE dummy;

// CHECK-DAG: @"\01?gs_vec@@3V?$vector@[[MNG]]$06@@A" = external addrspace(3) global <7 x [[STY]]>
// CHECK-DAG: @"\01?gs_vec_arr@@3PAV?$vector@[[MNG]]$06@@A" = external addrspace(3) global [10 x <7 x [[STY]]>]
// CHECK-DAG: @"\01?gs_vec_rec@@3ULongVec@@A" = external addrspace(3) global %struct.LongVec
groupshared vector<TYPE, NUM> gs_vec;
groupshared vector<TYPE, NUM> gs_vec_arr[10];
groupshared LongVec gs_vec_rec;

// CHECK-DAG: @static_vec = internal global <7 x [[STY]]>
// CHECK-DAG: @static_vec_arr = internal global [10 x <7 x [[STY]]>] zeroinitializer
// CHECK-DAG: @static_vec_rec = internal global %struct.LongVec
static vector<TYPE, NUM> static_vec;
static vector<TYPE, NUM> static_vec_arr[10];
static LongVec static_vec_rec;

// CHECK: define [[RTY:[a-z0-9]*]] @"\01?getVal@@YA[[MNG]][[MNG]]@Z"([[RTY]] {{.*}}%t)
export TYPE getVal(TYPE t) {TYPE ret = dummy; dummy = t; return ret;}

// CHECK: define <7 x [[RTY]]>
// CHECK-LABEL: @"\01?lv_param_passthru
// CHECK-SAME: @@YA?AV?$vector@[[MNG]]$06@@V1@@Z"(<7 x [[RTY]]> %vec1)
// CHECK:   ret <7 x [[RTY]]>
export vector<TYPE, NUM> lv_param_passthru(vector<TYPE, NUM> vec1) {
  vector<TYPE, NUM> ret = vec1;
  return ret;
}

// CHECK-LABEL: define void @"\01?lv_param_in_out
// CHECK-SAME: @@YAXV?$vector@[[MNG]]$06@@AIAV1@@Z"(<7 x [[RTY]]> %vec1, <7 x [[STY]]>* noalias dereferenceable({{[0-9]*}}) %vec2)
// CHECK:   store <7 x [[STY]]> {{%.*}}, <7 x [[STY]]>* %vec2, align 4
// CHECK:   ret void
export void lv_param_in_out(in vector<TYPE, NUM> vec1, out vector<TYPE, NUM> vec2) {
  vec2 = vec1;
}

// CHECK-LABEL: define void @"\01?lv_param_inout
// CHECK-SAME: @@YAXAIAV?$vector@[[MNG]]$06@@0@Z"(<7 x [[STY]]>* noalias dereferenceable({{[0-9]*}}) %vec1, <7 x [[STY]]>* noalias dereferenceable({{[0-9]*}}) %vec2)
// CHECK:   load <7 x [[STY]]>, <7 x [[STY]]>* %vec1, align 4
// CHECK:   load <7 x [[STY]]>, <7 x [[STY]]>* %vec2, align 4
// CHECK:   store <7 x [[STY]]> {{%.*}}, <7 x [[STY]]>* %vec1, align 4
// CHECK:   store <7 x [[STY]]> {{%.*}}, <7 x [[STY]]>* %vec2, align 4
// CHECK:   ret void
export void lv_param_inout(inout vector<TYPE, NUM> vec1, inout vector<TYPE, NUM> vec2) {
  vector<TYPE, NUM> tmp = vec1;
  vec1 = vec2;
  vec2 = tmp;
}

// CHECK-LABEL: define void @"\01?lv_param_in_out_rec@@YAXULongVec@@U1@@Z"(%struct.LongVec* %vec1, %struct.LongVec* noalias %vec2)
// CHECK: memcpy
// CHECK:   ret void
export void lv_param_in_out_rec(in LongVec vec1, out LongVec vec2) {
  vec2 = vec1;
}

// CHECK-LABEL: define void @"\01?lv_param_inout_rec@@YAXULongVec@@0@Z"(%struct.LongVec* noalias %vec1, %struct.LongVec* noalias %vec2)
// CHECK: memcpy
// CHECK:   ret void
export void lv_param_inout_rec(inout LongVec vec1, inout LongVec vec2) {
  LongVec tmp = vec1;
  vec1 = vec2;
  vec2 = tmp;
}

// CHECK-LABEL: define void @"\01?lv_global_assign
// CHECK-SAME: @@YAXV?$vector@[[MNG]]$06@@@Z"(<7 x [[RTY]]> %vec)
// CHECK:   store <7 x [[STY]]> {{%.*}}, <7 x [[STY]]>* @static_vec
// CHECK:   ret void
export void lv_global_assign(vector<TYPE, NUM> vec) {
  static_vec = vec;
}

// CHECK: define <7 x [[RTY]]>
// CHECK-LABEL: @"\01?lv_global_ret
// CHECK-SAME: @@YA?AV?$vector@[[MNG]]$06@@XZ"()
// CHECK:   load <7 x [[STY]]>, <7 x [[STY]]>* @static_vec
// CHECK:   ret <7 x [[RTY]]>
export vector<TYPE, NUM> lv_global_ret() {
  vector<TYPE, NUM> ret = static_vec;
  return ret;
}

// CHECK-LABEL: define void @"\01?lv_gs_assign
// CHECK-SAME: @@YAXV?$vector@[[MNG]]$06@@@Z"(<7 x [[RTY]]> %vec)
// CHECK:   store <7 x [[STY]]> {{%.*}}, <7 x [[STY]]> addrspace(3)* @"\01?gs_vec@@3V?$vector@[[MNG]]$06@@A"
// CHECK:   ret void
export void lv_gs_assign(vector<TYPE, NUM> vec) {
  gs_vec = vec;
}

// CHECK: define <7 x [[RTY]]>
// CHECK-LABEL: @"\01?lv_gs_ret
// CHECK-SAME: @@YA?AV?$vector@[[MNG]]$06@@XZ"()
// CHECK:   load <7 x [[STY]]>, <7 x [[STY]]> addrspace(3)* @"\01?gs_vec@@3V?$vector@[[MNG]]$06@@A"
// CHECK:   ret <7 x [[RTY]]>
export vector<TYPE, NUM> lv_gs_ret() {
  vector<TYPE, NUM> ret = gs_vec;
  return ret;
}

#define DIMS 10

// CHECK-LABEL: define void @"\01?lv_param_arr_passthru
// CHECK-SAME: @@YA$$BY09V?$vector@[[MNG]]$06@@V1@@Z"([10 x <7 x [[STY]]>]* noalias sret %agg.result, <7 x [[RTY]]> %vec)
// Arrays are returned in the params
// CHECK: ret void
export vector<TYPE, NUM> lv_param_arr_passthru(vector<TYPE, NUM> vec)[10] {
  vector<TYPE, NUM> ret[10];
  for (int i = 0; i < DIMS; i++)
    ret[i] = vec;
  return ret;
}

// CHECK-LABEL: define void @"\01?lv_global_arr_assign
// CHECK-SAME: @@YAXY09V?$vector@[[MNG]]$06@@@Z"([10 x <7 x [[STY]]>]* %vec)
// CHECK: ret void
export void lv_global_arr_assign(vector<TYPE, NUM> vec[10]) {
  for (int i = 0; i < DIMS; i++)
    static_vec_arr[i] = vec[i];
}

// CHECK-LABEL: define void @"\01?lv_global_arr_ret
// CHECK-SAME: @@YA$$BY09V?$vector@[[MNG]]$06@@XZ"([10 x <7 x [[STY]]>]* noalias sret %agg.result)
// Arrays are returned in the params
// CHECK: ret void
export vector<TYPE, NUM> lv_global_arr_ret()[10] {
  vector<TYPE, NUM> ret[10];
  for (int i = 0; i < DIMS; i++)
    ret[i] = static_vec_arr[i];
  return ret;
}

// CHECK-LABEL: define void @"\01?lv_gs_arr_assign
// CHECK-SAME: @@YAXY09V?$vector@[[MNG]]$06@@@Z"([10 x <7 x [[STY]]>]* %vec)
// ret void
export void lv_gs_arr_assign(vector<TYPE, NUM> vec[10]) {
  for (int i = 0; i < DIMS; i++)
    gs_vec_arr[i] = vec[i];
}

// CHECK-LABEL: define void @"\01?lv_gs_arr_ret
// CHECK-SAME: @@YA$$BY09V?$vector@[[MNG]]$06@@XZ"([10 x <7 x [[STY]]>]* noalias sret %agg.result)
export vector<TYPE, NUM> lv_gs_arr_ret()[10] {
  vector<TYPE, NUM> ret[10];
  for (int i = 0; i < DIMS; i++)
    ret[i] = gs_vec_arr[i];
  return ret;
}

// CHECK-LABEL: define void @"\01?lv_param_rec_passthru@@YA?AULongVec@@U1@@Z"(%struct.LongVec* noalias sret %agg.result, %struct.LongVec* %vec)
// CHECK: memcpy
// Aggregates are returned in the params
// CHECK:   ret void
export LongVec lv_param_rec_passthru(LongVec vec) {
  LongVec ret = vec;
  return ret;
}

// CHECK-LABEL: define void @"\01?lv_global_rec_assign@@YAXULongVec@@@Z"(%struct.LongVec* %vec)
// CHECK: memcpy
// CHECK:   ret void
export void lv_global_rec_assign(LongVec vec) {
  static_vec_rec = vec;
}

// CHECK-LABEL: define void @"\01?lv_global_rec_ret@@YA?AULongVec@@XZ"(%struct.LongVec* noalias sret %agg.result)
// CHECK: memcpy
// Aggregates are returned in the params
// CHECK:   ret void
export LongVec lv_global_rec_ret() {
  LongVec ret = static_vec_rec;
  return ret;
}

// CHECK-LABEL: define void @"\01?lv_gs_rec_assign@@YAXULongVec@@@Z"(%struct.LongVec* %vec)
// CHECK: memcpy
// CHECK:   ret void
export void lv_gs_rec_assign(LongVec vec) {
  gs_vec_rec = vec;
}

// CHECK-LABEL: define void @"\01?lv_gs_rec_ret@@YA?AULongVec@@XZ"(%struct.LongVec* noalias sret %agg.result)
// CHECK: memcpy
// Aggregates are returned in the params
// CHECK:   ret void
export LongVec lv_gs_rec_ret() {
  LongVec ret = gs_vec_rec;
  return ret;
}

// CHECK: define <7 x [[RTY]]>
// CHECK-LABEL: @"\01?lv_splat
// CHECK-SAME: @@YA?AV?$vector@[[MNG]]$06@@[[MNG]]@Z"([[RTY]] {{.*}}%scalar)
// CHECK:   ret <7 x [[RTY]]>
export vector<TYPE,NUM> lv_splat(TYPE scalar) {
  vector<TYPE,NUM> ret = scalar;
  return ret;
}

// CHECK: define <6 x [[RTY]]>
// CHECK-LABEL: @"\01?lv_initlist
// CHECK-SAME: @@YA?AV?$vector@[[MNG]]$05@@XZ"()
// CHECK:   ret <6 x [[RTY]]>
export vector<TYPE, 6> lv_initlist() {
  vector<TYPE, 6> ret = {1, 2, 3, 4, 5, 6};
  return ret;
}

// CHECK: define <6 x [[RTY]]>
// CHECK-LABEL: @"\01?lv_initlist_vec
// CHECK-SAME: @@YA?AV?$vector@[[MNG]]$05@@V?$vector@[[MNG]]$02@@@Z"(<3 x [[RTY]]> %vec)
// CHECK:   ret <6 x [[RTY]]>
export vector<TYPE, 6> lv_initlist_vec(vector<TYPE, 3> vec) {
  vector<TYPE, 6> ret = {vec, 4.0, 5.0, 6.0};
  return ret;
}

// CHECK: define <6 x [[RTY]]>
// CHECK-LABEL: @"\01?lv_vec_vec
// CHECK-SAME: @@YA?AV?$vector@[[MNG]]$05@@V?$vector@[[MNG]]$02@@0@Z"(<3 x [[RTY]]> %vec1, <3 x [[RTY]]> %vec2)
// CHECK:   ret <6 x [[RTY]]>
export vector<TYPE, 6> lv_vec_vec(vector<TYPE, 3> vec1, vector<TYPE, 3> vec2) {
  vector<TYPE, 6> ret = {vec1, vec2};
  return ret;
}

// CHECK: define <7 x [[RTY]]>
// CHECK-LABEL: @"\01?lv_array_cast
// CHECK-SAME: @@YA?AV?$vector@[[MNG]]$06@@Y06[[MNG]]@Z"([7 x [[STY]]]* %arr)
// CHECK:   ret <7 x [[RTY]]>
export vector<TYPE, NUM> lv_array_cast(TYPE arr[NUM]) {
  vector<TYPE, NUM> ret = (vector<TYPE,NUM>)arr;
  return ret;
}

// CHECK: define <6 x [[RTY]]>
// CHECK-LABEL: @"\01?lv_ctor
// CHECK-SAME: @@YA?AV?$vector@[[MNG]]$05@@[[MNG]]@Z"([[RTY]] {{.*}}%s)
// CHECK:  ret <6 x [[RTY]]>
export vector<TYPE, 6> lv_ctor(TYPE s) {
  vector<TYPE, 6> ret = vector<TYPE,6>(1.0, 2.0, 3.0, 4.0, 5.0, s);
  return ret;
}
