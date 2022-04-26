// RUN: %dxc -T cs_6_0 _HV 2021 %s | FileCheck %s

// Array subscripts lost their qualifiers including const due to taking a different
// path for HLSL on account of having no array to ptr decay
// This test verifies that subscripts of constant and cbuf arrays can't be assigned
// or otherwise altered by a few mechanisms

StructuredBuffer<uint> g_robuf;
Texture1D<uint> g_tex;
uint g_cbuf[4];

const groupshared uint gs_val[4];

[NumThreads(1, 1, 1)]
void main() {
  const uint local[4];

  //CHECK: error: read-only variable is not assignable
  //CHECK: error: read-only variable is not assignable
  //CHECK: error: read-only variable is not assignable
  //CHECK: error: cannot assign to return value
  // Assigning using assignment operator
  local[0] = 0;
  gs_val[0] = 0;
  g_cbuf[0] = 0;
  g_robuf[0] = 0;

  // Assigning using out param of builtin function
  //CHECK: error: no matching function for call to 'sincos'
  //CHECK: error: no matching function for call to 'sincos'
  //CHECK: error: no matching function for call to 'sincos'
  //CHECK: error: no matching function for call to 'sincos'
  sincos(0.0, local[0], local[0]);
  sincos(0.0, gs_val[0], gs_val[0]);
  sincos(0.0, g_cbuf[0], g_cbuf[0]);
  sincos(0.0, g_robuf[0], g_robuf[0]);


  // Assigning using out param of method
  //CHECK: error: no matching member function for call to 'GetDimensions'
  //CHECK: error: no matching member function for call to 'GetDimensions'
  //CHECK: error: no matching member function for call to 'GetDimensions'
  //CHECK: error: no matching member function for call to 'GetDimensions'
  g_tex.GetDimensions(local[0]);
  g_tex.GetDimensions(gs_val[0]);
  g_tex.GetDimensions(g_cbuf[0]);
  g_tex.GetDimensions(g_robuf[0]);


  // Assigning using dest param of atomics
  // Distinct because of special handling of atomics dest param
  //CHECK: error: no matching function for call to 'InterlockedAdd'
  //CHECK: error: no matching function for call to 'InterlockedAdd'
  //CHECK: error: no matching function for call to 'InterlockedAdd'
  //CHECK: error: no matching function for call to 'InterlockedAdd'
  InterlockedAdd(local[0], 1);
  InterlockedAdd(gs_val[0], 1);
  InterlockedAdd(g_cbuf[0], 1);
  InterlockedAdd(g_robuf[0], 1);

}
