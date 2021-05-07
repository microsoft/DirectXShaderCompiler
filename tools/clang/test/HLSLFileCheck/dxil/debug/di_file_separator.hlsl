// RUN: %dxc -E main -T ps_6_0 %s -Od /Zi | FileCheck %s

// CHECK: !{{[0-9]+}} = !DIFile(filename: "{{.+}}\5Cmy_header\5Cmy_include\5Cheader1.hlsli
// CHECK: !{{[0-9]+}} = !DIFile(filename: "{{.+}}\5Cmy_header\5Cmy_include\5Cheader2.hlsli
// CHECK: !{{[0-9]+}} = !DIFile(filename: "{{.+}}\5Cmy_header\5Cmy_include\5Cheader3.hlsli
// CHECK: !{{[0-9]+}} = !DIFile(filename: "{{.+}}\5Cmy_header\5Cheader4.hlsli
// CHECK: !{{[0-9]+}} = !DIFile(filename: "{{.+}}\5Cmy_header\5Cheader5.hlsli
// CHECK: !{{[0-9]+}} = !DIFile(filename: "{{.+}}\5Cmy_header\5Cheader6.hlsli
// CHECK: !{{[0-9]+}} = !DIFile(filename: "{{.+}}\5Cheader7.hlsli

// CHECK-NOT: "{{.+}}/my_header/my_include/header1.hlsli"
// CHECK-NOT: "{{.+}}/my_include/header1.hlsli"
// CHECK-NOT: "{{.+}}/header1.hlsli"

// CHECK-NOT: "{{.+}}/my_header/my_include/header2.hlsli"
// CHECK-NOT: "{{.+}}/my_include/header2.hlsli"
// CHECK-NOT: "{{.+}}/header2.hlsli"

// CHECK-NOT: "{{.+}}/my_header/my_include/header3.hlsli"
// CHECK-NOT: "{{.+}}/my_include/header3.hlsli"
// CHECK-NOT: "{{.+}}/header3.hlsli"

// CHECK-NOT: "{{.+}}/my_header/header4.hlsli"
// CHECK-NOT: "{{.+}}/header4.hlsli"

// CHECK-NOT: "{{.+}}/my_header/header5.hlsli"
// CHECK-NOT: "{{.+}}/header5.hlsli"

// CHECK-NOT: "{{.+}}/my_header/header6.hlsli"
// CHECK-NOT: "{{.+}}/header6.hlsli"

// CHECK-NOT: "{{.+}}/header7.hlsli"

#include "my_header\my_include/header1.hlsli"
#include "my_header/my_include\header2.hlsli"
#include "my_header/my_include/header3.hlsli"
#include "my_header\header4.hlsli"
#include "my_header/header5.hlsli"
#include "my_header/header6.hlsli"
#include "header7.hlsli"

[RootSignature("CBV(b0)")]
float main() : SV_Target {
  return 
    foo_1 +
    foo_2 +
    foo_3 +
    foo_4 +
    foo_5 +
    foo_6 +
    foo_7;
}

