// RUN: %dxc -E main -T ps_6_0 %s -HV 2021  | FileCheck -check-prefix=HV2021 %s

// RUN: %dxc -E main -T ps_6_0 %s  | FileCheck %s

// CHECK:keyword 'using' may not be supported by compilers prior to HLSL2021
// CHECK:error: control reaches end of non-void function

// HV2021-NOT:keyword 'using' may not be supported by compilers prior to HLSL2021
// HV2021:error: control reaches end of non-void function
namespace n {
    using f = float;
    namespace n2 {
       f foo(f a) { return sin(a); }
    }

}

using namespace n;
using f32 = n::f;


using n2::foo;

f32 main(f32 a:A) : SV_Target {

  f32 r = foo(a);
}