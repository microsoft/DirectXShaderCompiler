// RUN: %dxc -E main -T cs_6_0 %s

groupshared int a;
[numthreads(64, 1, 1)]
void main() {
  a = 123;
  int4 x = (a).xxxx;
}
