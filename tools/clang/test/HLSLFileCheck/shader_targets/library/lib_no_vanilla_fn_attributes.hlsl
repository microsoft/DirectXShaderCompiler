// RUN: %dxc -T lib_6_3 %s | FileCheck %s

// Make sure a bunch of vanilla function attributes are not added to the functions which are not inlined. 

// CHECK-NOT: "disable-tail-calls"="false"
// CHECK-NOT: "less-precise-fpmad"="false"
// CHECK-NOT: "no-frame-pointer-elim"="false"
// CHECK-NOT: "no-infs-fp-math"="false" 
// CHECK-NOT: "no-nans-fp-math"="false" 
// CHECK-NOT: "no-realign-stack" 
// CHECK-NOT: "stack-protector-buffer-size"="0" 
// CHECK-NOT: "unsafe-fp-math"="false" 
// CHECK-NOT: "use-soft-float"="false"

export float foo(float a) {
    return a*a;
}
