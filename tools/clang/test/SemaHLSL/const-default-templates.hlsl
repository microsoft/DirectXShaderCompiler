// RUN: %dxc -T ps_6_0 -HV 2021 -verify %s
// RUN: %dxc -T ps_6_0 -HV 202x -verify %s

// Globals and cbuffer members are implicitly const. Verify this also applies
// when the type is spelled as a template-id (including the built-in vector and
// matrix templates), and when a class template specialization has not been
// instantiated before the declaration.

template <typename T> struct W { T v; };
template <typename T> struct R { T res; float f; };

typedef W<uint> WU; // Not instantiated until a variable is declared.

W<int> g_w;               // expected-note {{variable 'g_w' declared const here}}
WU g_wu;                  // expected-note {{variable 'g_wu' declared const here}}
W<W<float> > g_nested;    // expected-note {{variable 'g_nested' declared const here}}
W<float4> g_arr[2];
vector<float, 4> g_vec;   // expected-note {{variable 'g_vec' declared const here}}
matrix<float, 2, 2> g_mat; // expected-note {{variable 'g_mat' declared const here}}

// Types containing resources are not implicitly const, but must still be
// usable.
R<Texture2D<float4> > g_res;
Texture2D<float4> g_tex;

cbuffer CB {
  W<int> cb_w;            // expected-note {{variable 'cb_w' declared const here}}
};

tbuffer TB {
  W<int> tb_w;            // expected-note {{variable 'tb_w' declared const here}}
};

static W<int> s_w;
groupshared W<int> gs_w;

float4 main() : SV_Target {
  g_w.v = 1;         // expected-error {{cannot assign to variable 'g_w' with const-qualified type 'const W<int>'}}
  g_wu.v = 1;        // expected-error {{cannot assign to variable 'g_wu' with const-qualified type 'const WU'}}
  g_nested.v.v = 1;  // expected-error {{cannot assign to variable 'g_nested' with const-qualified type 'const W<W<float> >'}}
  g_arr[0].v = 1;    // expected-error {{read-only variable is not assignable}}
  g_vec = 1;         // expected-error {{cannot assign to variable 'g_vec' with const-qualified type 'const vector<float, 4>'}}
  g_mat = 1;         // expected-error {{cannot assign to variable 'g_mat' with const-qualified type 'const matrix<float, 2, 2>'}}
  cb_w.v = 1;        // expected-error {{cannot assign to variable 'cb_w' with const-qualified type 'const W<int>'}}
  tb_w.v = 1;        // expected-error {{cannot assign to variable 'tb_w' with const-qualified type 'const W<int>'}}

  // Not implicitly const.
  s_w.v = 1;
  gs_w.v = 1;

  return g_tex.Load(0) + g_res.res.Load(0) + g_w.v + g_wu.v + g_nested.v.v +
         g_arr[0].v + g_vec + g_mat[0].xyxy + cb_w.v + tb_w.v + s_w.v +
         gs_w.v;
}
