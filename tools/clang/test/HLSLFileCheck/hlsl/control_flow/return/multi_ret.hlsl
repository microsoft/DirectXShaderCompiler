// RUN: %dxc -E main -fcgl -structurize-returns -T ps_6_0 %s | FileCheck %s

// CHECK:entry
// Init bReturned.
// CHECK:bReturned = alloca i1
// CHECK:store i1 false, i1* %bReturned

// CHECK:if.then:
// CHECK:if.then.5:
// return inside if.
// set bReturned to true.
// CHECK:store i1 true, i1* %bReturned

// guard rest of scope with !bReturned
// CHECK:bReturned.cmp.false:
// CHECK:%[[RET:.*]] = load i1, i1* %bReturned
// CHECK:%[[NRET:.*]] = xor i1 %[[RET]], true
// CHECK:br i1 %[[NRET]], label %if.end, label %bReturned.cmp.false.

// CHECK:if.end
// CHECK:br label %bReturned.cmp.false.

// CHECK:if.else
// CHECK:br i1 {{.*}}, label %if.then.{{[0-9]+}}, label %bReturned.cmp.false


// CHECK:if.then
// return inside else
// set bIsReturn to true
// CHECK:store i1 true, i1* %bReturned
// CHECK:br label %bReturned.cmp.false

// CHECK:bReturned.cmp.false.{{[0-9]+}}:
// CHECK:%[[RET2:.*]] = load i1, i1* %bReturned
// CHECK:%[[NRET2:.*]] = xor i1 %[[RET2]], true
// CHECK:br i1 %[[NRET2]], label %if.end.10, label %bReturned.cmp.false.

// CHECK:if.end

// guard after endif.
// CHECK:bReturned.cmp.false.{{[0-9]+}}:
// CHECK:%[[RET3:.*]] = load i1, i1* %bReturned
// CHECK:%[[NRET3:.*]] = xor i1 %[[RET3]], true
// CHECK:br i1 %[[NRET3]], label %bReturned.cmp.false.{{[0-9]+}}, label %return
// guard after endif for else.
// CHECK:bReturned.cmp.false.{{[0-9]+}}:
// CHECK:%[[RET4:.*]] = load i1, i1* %bReturned
// CHECK:%[[NRET4:.*]] = xor i1 %[[RET4]], true
// CHECK:br i1 %[[NRET4]], label %if.end.{{[0-9]+}}, label %return

// CHECK:for.cond:
// CHECK:for.body:
// CHECK:if.then.{{[0-9]+}}:
// set bIsReturn to true
// CHECK:store i1 true, i1* %bReturned
// CHECK:br label %bReturned.cmp.true


// CHECK:bReturned.cmp.true:
// CHECK:%[[RET5:.*]] = load i1, i1* %bReturned
// CHECK:br i1 %[[RET5]], label %bReturned.break, label %if.end.
// CHECK:bReturned.break:
// CHECK:br label %bReturned.cmp.false.

// CHECK:for.inc:
// CHECK:br label %for.cond

// Guard after loop.
// CHECK:bReturned.cmp.false.{{[0-9]+}}:
// CHECK:%[[RET6:.*]] = load i1, i1* %bReturned
// CHECK:%[[NRET6:.*]] = xor i1 %[[RET6]], true
// CHECK:br i1 %[[NRET6]], label %for.end, label %return

// CHECK:for.end:
// CHECK:switch i32

// CHECK:if.then.{{[0-9]+}}:
// set bIsReturn to true
// CHECK:store i1 true, i1* %bReturned
// CHECK:br label %bReturned.cmp.true

// out of switch
// CHECK:bReturned.cmp.true.{{[0-9]+}}:
// CHECK:%[[RET7:.*]] = load i1, i1* %bReturned
// CHECK:br i1 %[[RET7]], label %bReturned.break.{{[0-9]+}}, label %if.end.
// CHECK:bReturned.break.{{[0-9]+}}:
// CHECK:br label %bReturned.cmp.false.

// guard code after return
// CHECK:if.end


// guard code after switch.
// CHECK:bReturned.cmp.false.{{[0-9]+}}:
// CHECK:%[[RET8:.*]] = load i1, i1* %bReturned
// CHECK:%[[NRET8:.*]] = xor i1 %[[RET8]], true
// CHECK:br i1 %[[NRET8]], label %sw.epilog, label %return

// CHECK:sw.epilog:
// CHECK:return:

int i;

float main(float4 a:A) : SV_Target {

  float c = 0;
  if (i < 0) {
    if (a.w > 2)
      return -1;
    c += sin(a.z);
  }
  else {
    if (a.z > 3)
      return -5;
    c *= cos(a.w);
  }

  for (int j=0;j<i;j++) {
    c += pow(2,j);
    if (c > 10)
      return -2;
  }

  switch (i) {
    case 1:
     c += log(a.x);
     break;
    case 2:

        c += cos(a.y);
     break;
    case 3:
         if (c < 10)
         return -3;
       c += sin(a.x);
     break;
  }

  return c;
}