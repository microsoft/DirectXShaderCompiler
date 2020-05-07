// Run: %dxc -T cs_6_0 -E main

// CHECK: {{%\d+}} = OpString "first string"
string first = "first string";
// CHECK: {{%\d+}} = OpString "second string"
string second = "second string";
// CHECK: {{%\d+}} = OpString "third string"
const string third = "third string";
// CHECK-NOT: {{%\d+}} = OpString "first string"
const string a = "first string";

[numthreads(1,1,1)]
void main() {
}

