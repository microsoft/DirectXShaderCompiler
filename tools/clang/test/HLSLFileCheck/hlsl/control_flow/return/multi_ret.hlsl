// RUN: %dxc -E main -fcgl -structurize-returns -T ps_6_0 %s | FileCheck %s

int i;

float main(float4 a:A) : SV_Target {
// Init bReturned.
// CHECK:%[[bReturned:.*]] = alloca i1
// CHECK:store i1 false, i1* %[[bReturned]]

  float c = 0;

// CHECK: [[label:.*]] ; preds =
  if (i < 0) {
// CHECK: [[label2:.*]] ; preds =
    if (a.w > 2)
// return inside if.
// set bReturned to true.
// CHECK:store i1 true, i1* %[[bReturned]]
      return -1;
// guard rest of scope with !bReturned
// CHECK: [[label_bRet_cmp_false:.*]] ; preds =
// CHECK:%[[RET:.*]] = load i1, i1* %[[bReturned]]
// CHECK:%[[NRET:.*]] = xor i1 %[[RET]], true
// CHECK:br i1 %[[NRET]],

// CHECK: [[label3:.*]]  ; preds =
    c += sin(a.z);
  }
  else {
// CHECK: [[else:.*]] ; preds =
    if (a.z > 3)
// return inside else
// set bIsReturn to true
// CHECK:store i1 true, i1* %[[bReturned]]
      return -5;
// CHECK: [[label_bRet_cmp_false2:.*]] ; preds =
// CHECK:%[[RET2:.*]] = load i1, i1* %[[bReturned]]
// CHECK:%[[NRET2:.*]] = xor i1 %[[RET2]], true
// CHECK:br i1 %[[NRET2]],

// CHECK: [[label4:.*]] ; preds =
    c *= cos(a.w);
// guard after endif.
// CHECK: [[label_bRet_cmp_false3:.*]] ; preds =
// CHECK:%[[RET3:.*]] = load i1, i1* %[[bReturned]]
// CHECK:%[[NRET3:.*]] = xor i1 %[[RET3]], true
// CHECK:br i1 %[[NRET3]],
// guard after endif for else.
// CHECK: [[label_bRet_cmp_false4:.*]] ; preds =
// CHECK:%[[RET4:.*]] = load i1, i1* %[[bReturned]]
// CHECK:%[[NRET4:.*]] = xor i1 %[[RET4]], true
// CHECK:br i1 %[[NRET4]],
  }
// CHECK: [[endif:.*]] ; preds =

// CHECK: [[forCond:.*]]; preds =


// CHECK: [[forBody:.*]] ; preds =
  for (int j=0;j<i;j++) {
    c += pow(2,j);
// CHECK: [[if_in_loop:.*]] ; preds =
    if (c > 10)
// set bIsReturn to true
// CHECK:store i1 true, i1* %[[bReturned]]
      return -2;

// CHECK: [[label_bRet_cmp_true:.*]] ; preds =
// CHECK:%[[RET5:.*]] = load i1, i1* %[[bReturned]]
// CHECK:br i1 %[[RET5]],
// CHECK: [[label_bRet_break:.*]] ; preds =
// dxBreak
// CHECK:br i1 true,
// CHECK: [[endif_in_loop:.*]] ; preds =

// CHECK: [[for_inc:.*]] ; preds =

// Guard after loop.
// CHECK: [[label_bRet_cmp_false5:.*]] ; preds =
// CHECK:%[[RET6:.*]] = load i1, i1* %[[bReturned]]
// CHECK:%[[NRET6:.*]] = xor i1 %[[RET6]], true
// CHECK:br i1 %[[NRET6]],

  }
// CHECK: [[for_end:.*]] ; preds =
// CHECK:switch i32
  switch (i) {
// CHECK: [[case1:.*]] ; preds =
    case 1:
     c += log(a.x);
     break;

// CHECK: [[case2:.*]] ; preds =
    case 2:

        c += cos(a.y);
     break;

// CHECK: [[case3:.*]] ; preds =
    case 3:

// CHECK: [[if_in_switch:.*]]  ; preds =
         if (c < 10)
// set bIsReturn to true
// CHECK:store i1 true, i1* %[[bReturned]]
         return -3;
// CHECK: [[label_bRet_cmp_true2:.*]] ; preds =
// CHECK:%[[RET7:.*]] = load i1, i1* %[[bReturned]]
// CHECK:br i1 %[[RET7]],
// CHECK: [[label_bRet_break2:.*]] ; preds =
// normal break
// CHECK:br label

// CHECK: [[endif_in_switch:.*]] ; preds =
       c += sin(a.x);
     break;
  }
// guard code after switch.
// CHECK: [[label_bRet_cmp_false6:.*]] ; preds =
// CHECK:%[[RET8:.*]] = load i1, i1* %[[bReturned]]
// CHECK:%[[NRET8:.*]] = xor i1 %[[RET8]], true
// CHECK:br i1 %[[NRET8]]

// CHECK: [[end_switch:.*]]; preds =

// CHECK: [[return:.*]] ; preds =
// CHECK-NOT:preds
// CHECK:ret

  return c;
}