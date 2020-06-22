// Run: %dxc -T cs_6_0 -E main

string first = "first string";

[numthreads(1,1,1)]
void main() {
  // CHECK: 8:3: error: string variables are immutable in SPIR-V.
  first = "second string";
}

