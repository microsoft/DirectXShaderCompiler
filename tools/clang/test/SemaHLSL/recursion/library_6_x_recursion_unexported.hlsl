// RUN: %dxc -Tlib_6_x -verify %s 
// functions with unspecified linkage will default to internal, except for when
// the target library has shader model 6_x.
// So, we expect an error on unreachable functions
// that are recursive without export or static keywords. 
// unreachable_unexported_recurse_external suffices as an example.

// expected-error@+1{{recursive functions are not allowed: export function calls recursive function 'unreachable_unexported_recurse_external'}}
void unreachable_unexported_recurse_external(inout float4 f, float a) 
{
    if (a > 1)
      unreachable_unexported_recurse_external(f, a-1);
    f = abs(f+a);
}
