// RUN: %dxc -enable-16bit-types /Tps_6_2 /Emain > %s | FileCheck %s
// CHECK: error: redefinition of 'foo'
// CHECK: note: previous definition is here

// When '-enable-16bit-types' flag is enabled, types 'short', 'min16int', and 'min12int' all map to 'short'.
// Similarly, types 'ushort' and 'min16uint' both map to 'ushort'. So it is expected to get 'redefinition' 
// compilation error. Note that the '-enable-16bit-types' flag is only available in dxc (and not in fxc),
// so this behavior doesn't break compat between dxc and fxc.

int4 foo(int v0, int v1, int v2, int v3) { return int4(v0, v0 * v1, v0 * v1 * v2, v0 * v1 * v2 * v3); }
uint4 foo(uint v0, uint v1, uint v2, uint v3) { return uint4(v0, v0 * v1, v0 * v1 * v2, v0 * v1 * v2 * v3); }
min16int4 foo(min16int v0, min16int v1, min16int v2, min16int v3) { return min16int4(v0, v0 * v1, v0 * v1 * v2, v0 * v1 * v2 * v3); }
min12int4 foo(min12int v0, min12int v1, min12int v2, min12int v3) { return min12int4(v0, v0 * v1, v0 * v1 * v2, v0 * v1 * v2 * v3); }
min16uint4 foo(min16uint v0, min16uint v1, min16uint v2, min16uint v3) { return min16uint4(v0, v0 * v1, v0 * v1 * v2, v0 * v1 * v2 * v3); }

float4 main(int vi
            : A, uint vui
            : B, min16int vm16i
            : C, min12int vm12i
            : D, min16uint vm16ui
            : E) : SV_Target {
  return foo(vi, vi, vi, vi) + foo(vui, vui, vui, vui) + foo(vm16i, vm16i, vm16i, vm16i) + foo(vm12i, vm12i, vm12i, vm12i) + foo(vm16ui, vm16ui, vm16ui, vm16ui);
}