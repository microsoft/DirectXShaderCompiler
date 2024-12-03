// RUN: %dxc -HV 2018 -T lib_6_9   -DTYPE=uint     -DNUM=5 %s | FileCheck %s
// RUN: %dxc -HV 2018 -T lib_6_9   -DTYPE=int64_t  -DNUM=3 %s | FileCheck %s
// RUN: %dxc -HV 2018 -T lib_6_9   -DTYPE=uint16_t -DNUM=9 -enable-16bit-types %s | FileCheck %s

// Test bitwise operators on an assortment vector sizes and integer types with 6.9 native vectors.

// Test bit twiddling operators.
// CHECK-LABEL: define void @"\01?bittwiddlers
// CHECK-SAME: ([10 x <[[NUM:[0-9][0-9]*]] x [[TYPE:[a-z0-9]*]]>]*
export void bittwiddlers(inout vector<TYPE, NUM> things[10]) {
  // CHECK: [[add1:%[0-9]*]] = getelementptr inbounds [10 x <[[NUM]] x [[TYPE]]>], [10 x <[[NUM]] x [[TYPE]]>]* %things, i32 0, i32 1
  // CHECK: [[vec1:%[0-9]*]] = load <[[NUM]] x [[TYPE]]>, <[[NUM]] x [[TYPE]]>* [[add1]]
  // CHECK: [[res1:%[0-9]*]] = xor <[[NUM]] x [[TYPE]]> [[vec1]], <[[TYPE]] -1,
  // CHECK: [[add0:%[0-9]*]] = getelementptr inbounds [10 x <[[NUM]] x [[TYPE]]>], [10 x <[[NUM]] x [[TYPE]]>]* %things, i32 0, i32 0
  // CHECK: store <[[NUM]] x [[TYPE]]> [[res1]], <[[NUM]] x [[TYPE]]>* [[add0]]
  things[0] = ~things[1];

  // CHECK: [[add2:%[0-9]*]] = getelementptr inbounds [10 x <[[NUM]] x [[TYPE]]>], [10 x <[[NUM]] x [[TYPE]]>]* %things, i32 0, i32 2
  // CHECK: [[vec2:%[0-9]*]] = load <[[NUM]] x [[TYPE]]>, <[[NUM]] x [[TYPE]]>* [[add2]]

  // CHECK: [[add3:%[0-9]*]] = getelementptr inbounds [10 x <[[NUM]] x [[TYPE]]>], [10 x <[[NUM]] x [[TYPE]]>]* %things, i32 0, i32 3
  // CHECK: [[vec3:%[0-9]*]] = load <[[NUM]] x [[TYPE]]>, <[[NUM]] x [[TYPE]]>* [[add3]]
  // CHECK: [[res1:%[0-9]*]] = or <[[NUM]] x [[TYPE]]> [[vec3]], [[vec2]]
  // CHECK: store <[[NUM]] x [[TYPE]]> [[res1]], <[[NUM]] x [[TYPE]]>* [[add1]]
  things[1] = things[2] | things[3];

  // CHECK: [[add4:%[0-9]*]] = getelementptr inbounds [10 x <[[NUM]] x [[TYPE]]>], [10 x <[[NUM]] x [[TYPE]]>]* %things, i32 0, i32 4
  // CHECK: [[vec4:%[0-9]*]] = load <[[NUM]] x [[TYPE]]>, <[[NUM]] x [[TYPE]]>* [[add4]]
  // CHECK: [[res2:%[0-9]*]] = and <[[NUM]] x [[TYPE]]> [[vec4]], [[vec3]]
  // CHECK: store <[[NUM]] x [[TYPE]]> [[res2]], <[[NUM]] x [[TYPE]]>* [[add2]]
  things[2] = things[3] & things[4];

  // CHECK: [[add5:%[0-9]*]] = getelementptr inbounds [10 x <[[NUM]] x [[TYPE]]>], [10 x <[[NUM]] x [[TYPE]]>]* %things, i32 0, i32 5
  // CHECK: [[vec5:%[0-9]*]] = load <[[NUM]] x [[TYPE]]>, <[[NUM]] x [[TYPE]]>* [[add5]]
  // CHECK: [[res3:%[0-9]*]] = xor <[[NUM]] x [[TYPE]]> [[vec4]], [[vec5]]
  // CHECK: store <[[NUM]] x [[TYPE]]> [[res3]], <[[NUM]] x [[TYPE]]>* [[add3]]
  things[3] = things[4] ^ things[5];

  // CHECK: [[add6:%[0-9]*]] = getelementptr inbounds [10 x <[[NUM]] x [[TYPE]]>], [10 x <[[NUM]] x [[TYPE]]>]* %things, i32 0, i32 6
  // CHECK: [[vec6:%[0-9]*]] = load <[[NUM]] x [[TYPE]]>, <[[NUM]] x [[TYPE]]>* [[add6]]
  // CHECK: [[res4:%[0-9]*]] = or <[[NUM]] x [[TYPE]]> [[vec6]], [[vec4]]
  // CHECK: store <[[NUM]] x [[TYPE]]> [[res4]], <[[NUM]] x [[TYPE]]>* [[add4]]
  things[4] |= things[6];

  // CHECK: [[add7:%[0-9]*]] = getelementptr inbounds [10 x <[[NUM]] x [[TYPE]]>], [10 x <[[NUM]] x [[TYPE]]>]* %things, i32 0, i32 7
  // CHECK: [[vec7:%[0-9]*]] = load <[[NUM]] x [[TYPE]]>, <[[NUM]] x [[TYPE]]>* [[add7]]
  // CHECK: [[res5:%[0-9]*]] = and <[[NUM]] x [[TYPE]]> [[vec7]], [[vec5]]
  // CHECK: store <[[NUM]] x [[TYPE]]> [[res5]], <[[NUM]] x [[TYPE]]>* [[add5]]
  things[5] &= things[7];

  // CHECK: [[add8:%[0-9]*]] = getelementptr inbounds [10 x <[[NUM]] x [[TYPE]]>], [10 x <[[NUM]] x [[TYPE]]>]* %things, i32 0, i32 8
  // CHECK: [[vec8:%[0-9]*]] = load <[[NUM]] x [[TYPE]]>, <[[NUM]] x [[TYPE]]>* [[add8]]
  // CHECK: [[res6:%[0-9]*]] = xor <[[NUM]] x [[TYPE]]> [[vec6]], [[vec8]]
  // CHECK: store <[[NUM]] x [[TYPE]]> [[res6]], <[[NUM]] x [[TYPE]]>* [[add6]]
  things[6] ^= things[8];

  // CHECK: ret void
}
