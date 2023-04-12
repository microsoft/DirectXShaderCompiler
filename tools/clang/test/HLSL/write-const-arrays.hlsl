// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s

// Array subscripts lost their qualifiers including const due to taking a different
// path for HLSL on account of having no array to ptr decay
// This test verifies that subscripts of constant and cbuf arrays can't be assigned
// or otherwise altered by a few mechanisms

/* Expected note with no locations (implicit built-in):
  expected-note@? {{function 'operator[]<const unsigned int &>' which returns const-qualified type 'const unsigned int &' declared here}}
*/

StructuredBuffer<uint> g_robuf;
Texture1D<uint> g_tex;
uint g_cbuf[4];

const groupshared uint gs_val[4];                           /* fxc-error {{X3012: 'gs_val': missing initial value}} */

[NumThreads(1, 1, 1)]
void main() {
  const uint local[4];                                      /* fxc-error {{X3012: 'local': missing initial value}} */

  // Assigning using assignment operator
  local[0] = 0;                                             /* expected-error {{read-only variable is not assignable}} fxc-error {{X3004: undeclared identifier 'local'}} */
  gs_val[0] = 0;                                            /* expected-error {{read-only variable is not assignable}} fxc-error {{X3004: undeclared identifier 'gs_val'}} */
  g_cbuf[0] = 0;                                            /* expected-error {{read-only variable is not assignable}} fxc-error {{X3025: global variables are implicitly constant, enable compatibility mode to allow modification}} */
  g_robuf[0] = 0;                                           /* expected-error {{cannot assign to return value because function 'operator[]<const unsigned int &>' returns a const value}} fxc-error {{X3025: l-value specifies const object}} */

  // Assigning using out param of builtin function
  double d = 1.0;
  asuint(d, local[0], local[1]);                          /* expected-error {{no matching function for call to 'asuint'}} expected-note {{candidate function not viable: 2nd argument ('const uint') would lose const qualifier}} fxc-error {{X3004: undeclared identifier 'local'}} */
  asuint(d, gs_val[0], gs_val[1]);                        /* expected-error {{no matching function for call to 'asuint'}} expected-note {{candidate function not viable: 2nd argument ('const uint') would lose const qualifier}} fxc-error {{X3004: undeclared identifier 'gs_val'}} */
  asuint(d, g_cbuf[0], g_cbuf[1]);                        /* expected-error {{no matching function for call to 'asuint'}} expected-note {{candidate function not viable: 2nd argument ('const uint') would lose const qualifier}} fxc-error {{X3013:     asuint(double, out uint x, out uint y)}} fxc-error {{X3013:     asuint(float|half|int|uint)}} fxc-error {{X3013: 'asuint': no matching 3 parameter intrinsic function}} fxc-error {{X3013: Possible intrinsic functions are:}} */
  asuint(d, g_robuf[0], g_robuf[1]);                      /* expected-error {{no matching function for call to 'asuint'}} expected-note {{candidate function not viable: 2nd argument ('const unsigned int') would lose const qualifier}} fxc-error {{X3013:     asuint(double, out uint x, out uint y)}} fxc-error {{X3013:     asuint(float|half|int|uint)}} fxc-error {{X3013: 'asuint': no matching 3 parameter intrinsic function}} fxc-error {{X3013: Possible intrinsic functions are:}} */


  // Assigning using out param of method
  g_tex.GetDimensions(local[0]);                            /* expected-error {{no matching member function for call to 'GetDimensions'}} expected-note {{candidate function template not viable: requires 3 arguments, but 1 was provided}} fxc-error {{X3004: undeclared identifier 'local'}} */
  g_tex.GetDimensions(gs_val[0]);                           /* expected-error {{no matching member function for call to 'GetDimensions'}} expected-note {{candidate function template not viable: requires 3 arguments, but 1 was provided}} fxc-error {{X3004: undeclared identifier 'gs_val'}} */
  g_tex.GetDimensions(g_cbuf[0]);                           /* expected-error {{no matching member function for call to 'GetDimensions'}} expected-note {{candidate function template not viable: requires 3 arguments, but 1 was provided}} fxc-error {{X3013:     Texture1D<uint>.GetDimensions(out float|half|min10float|min16float width)}} fxc-error {{X3013:     Texture1D<uint>.GetDimensions(out uint width)}} fxc-error {{X3013:     Texture1D<uint>.GetDimensions(uint, out float|half|min10float|min16float width, out float|half|min10float|min16float levels)}} fxc-error {{X3013:     Texture1D<uint>.GetDimensions(uint, out uint width, out uint levels)}} fxc-error {{X3013: 'GetDimensions': no matching 1 parameter intrinsic method}} fxc-error {{X3013: Possible intrinsic methods are:}} */
  g_tex.GetDimensions(g_robuf[0]);                          /* expected-error {{no matching member function for call to 'GetDimensions'}} expected-note {{candidate function template not viable: requires 3 arguments, but 1 was provided}} fxc-error {{X3013:     Texture1D<uint>.GetDimensions(out float|half|min10float|min16float width)}} fxc-error {{X3013:     Texture1D<uint>.GetDimensions(out uint width)}} fxc-error {{X3013:     Texture1D<uint>.GetDimensions(uint, out float|half|min10float|min16float width, out float|half|min10float|min16float levels)}} fxc-error {{X3013:     Texture1D<uint>.GetDimensions(uint, out uint width, out uint levels)}} fxc-error {{X3013: 'GetDimensions': no matching 1 parameter intrinsic method}} fxc-error {{X3013: Possible intrinsic methods are:}} */


  // Assigning using dest param of atomics
  // Distinct because of special handling of atomics dest param
  InterlockedAdd(local[0], 1);                              /* expected-error {{no matching function for call to 'InterlockedAdd'}} expected-note {{candidate function not viable: 1st argument ('const uint') would lose const qualifier}} expected-note {{candidate function not viable: no known conversion from 'const uint' to 'unsigned long long &' for 1st argument}} fxc-error {{X3004: undeclared identifier 'local'}} */
  InterlockedAdd(gs_val[0], 1);                             /* expected-error {{no matching function for call to 'InterlockedAdd'}} expected-note {{candidate function not viable: 1st argument ('const uint') would lose const qualifier}} expected-note {{candidate function not viable: no known conversion from 'const uint' to 'unsigned long long' for 1st argument}} fxc-error {{X3004: undeclared identifier 'gs_val'}} */
  InterlockedAdd(g_cbuf[0], 1);                             /* expected-error {{no matching function for call to 'InterlockedAdd'}} expected-note {{candidate function not viable: 1st argument ('const uint') would lose const qualifier}} expected-note {{candidate function not viable: no known conversion from 'const uint' to 'unsigned long long' for 1st argument}} fxc-pass {{}} */
  InterlockedAdd(g_robuf[0], 1);                            /* expected-error {{no matching function for call to 'InterlockedAdd'}} expected-note {{candidate function not viable: 1st argument ('const unsigned int') would lose const qualifier}} expected-note {{candidate function not viable: no known conversion from 'const unsigned int' to 'unsigned long long' for 1st argument}} fxc-pass {{}} */

}
