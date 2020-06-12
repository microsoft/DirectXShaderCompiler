// RUN: %dxc -E main -fcgl -structurize-returns -T ps_6_0 %s | FileCheck %s

int i;

float main(float4 a:A) : SV_Target {
// Init bReturned.
// CHECK:%[[bReturned:.*]] = alloca i1
// CHECK:store i1 false, i1* %[[bReturned]]
  float r = a.w;

// CHECK: [[label:.*]] ; preds =
  if (a.z > 0) {
// CHECK: [[for_cond:.*]] ; preds =
    for (int j=0;j<i;j++) {
// CHECK: [[for_body:.*]] ; preds =
// set bReturned to true.
// CHECK:store i1 true, i1* %[[bReturned]]
       return log(i);

// CHECK: [[for_inc:.*]] ; No predecessors!
    }
// guard rest of scope with !bReturned
// CHECK: [[label_bRet_cmp_false:.*]] ; preds =
// CHECK:%[[RET:.*]] = load i1, i1* %[[bReturned]]
// CHECK:%[[NRET:.*]] = xor i1 %[[RET]], true
// CHECK:br i1 %[[NRET]],

// CHECK: [[for_end:.*]]  ; preds =
    r += sin(a.y);
// set bReturned to true.
// CHECK:store i1 true, i1* %[[bReturned]]
    return sin(a.x * a.z + r);
  } else {
// CHECK: [[else:.*]]  ; preds =
// set bReturned to true.
// CHECK:store i1 true, i1* %[[bReturned]]
    return cos(r + a.z);
  }

// dead code which not has code generated.
  return a.x + a.y;

// CHECK: [[exit:.*]]  ; preds =
// CHECK-NOT:preds
// CHECK:ret float
}