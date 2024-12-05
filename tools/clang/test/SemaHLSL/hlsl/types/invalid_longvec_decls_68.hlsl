// RUN: %dxc  -DTYPE=float -DNUM=7 -T ps_6_8 -verify %s

// CHECK: %struct.LongVec = type { <4 x float>, <7 x [[STY:[a-z0-9]*]]> }
struct LongVec {
  float4 f;
  vector<TYPE,NUM> vec; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
};

static vector<TYPE, NUM> static_vec; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
static vector<TYPE, NUM> static_vec_arr[10]; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}

groupshared vector<TYPE, NUM> gs_vec; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
groupshared vector<TYPE, NUM> gs_vec_arr[10]; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}

export vector<TYPE, NUM> lv_param_passthru(vector<TYPE, NUM> vec1) { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}} expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vector<TYPE, NUM> ret = vec1; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  return ret;
}

export void lv_param_in_out(in vector<TYPE, NUM> vec1, out vector<TYPE, NUM> vec2) { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}} expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vec2 = vec1;
}

export void lv_param_inout(inout vector<TYPE, NUM> vec1, inout vector<TYPE, NUM> vec2) { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}} expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vector<TYPE, NUM> tmp = vec1; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vec1 = vec2;
  vec2 = tmp;
}

export void lv_global_assign(vector<TYPE, NUM> vec) { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  static_vec = vec;
}

export vector<TYPE, NUM> lv_global_ret() { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vector<TYPE, NUM> ret = static_vec; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  return ret;
}

export void lv_gs_assign(vector<TYPE, NUM> vec) { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  gs_vec = vec;
}

export vector<TYPE, NUM> lv_gs_ret() { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vector<TYPE, NUM> ret = gs_vec; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  return ret;
}

export vector<TYPE, NUM> lv_param_arr_passthru(vector<TYPE, NUM> vec)[10] { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}} expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vector<TYPE, NUM> ret[10]; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  for (int i = 0; i < 10; i++)
    ret[i] = vec;
  return ret;
}

export void lv_global_arr_assign(vector<TYPE, NUM> vec[10]) { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  for (int i = 0; i < 10; i++)
    static_vec_arr[i] = vec[i];
}

export vector<TYPE, NUM> lv_global_arr_ret()[10] { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vector<TYPE, NUM> ret[10]; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  for (int i = 0; i < 10; i++)
    ret[i] = static_vec_arr[i];
  return ret;
}

export void lv_gs_arr_assign(vector<TYPE, NUM> vec[10]) { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  for (int i = 0; i < 10; i++)
    gs_vec_arr[i] = vec[i];
}

export vector<TYPE, NUM> lv_gs_arr_ret()[10] { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vector<TYPE, NUM> ret[10]; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  for (int i = 0; i < 10; i++)
    ret[i] = gs_vec_arr[i];
  return ret;
}

export vector<TYPE,NUM> lv_splat(TYPE scalar) { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vector<TYPE,NUM> ret = scalar; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  return ret;
}

export vector<TYPE, 6> lv_initlist() { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vector<TYPE, 6> ret = {1, 2, 3, 4, 5, 6}; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  return ret;
}

export vector<TYPE, 6> lv_initlist_vec(vector<TYPE, 3> vec) { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vector<TYPE, 6> ret = {vec, 4.0, 5.0, 6.0}; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  return ret;
}

export vector<TYPE, 6> lv_vec_vec(vector<TYPE, 3> vec1, vector<TYPE, 3> vec2) { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vector<TYPE, 6> ret = {vec1, vec2}; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  return ret;
}

export vector<TYPE, NUM> lv_array_cast(TYPE arr[NUM]) { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vector<TYPE, NUM> ret = (vector<TYPE,NUM>)arr; // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  return ret;
}

export vector<TYPE, 6> lv_ctor(TYPE s) { // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  vector<TYPE, 6> ret = vector<TYPE,6>(1.0, 2.0, 3.0, 4.0, 5.0, s); // expected-error {{invalid value, valid range is between 1 and 4 inclusive}}
  return ret;
}

