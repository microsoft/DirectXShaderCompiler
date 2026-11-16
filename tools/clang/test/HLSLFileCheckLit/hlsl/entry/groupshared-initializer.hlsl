// RUN: %dxc -T cs_6_0 -E main -fcgl -verify %s

uint getValue() { return 2; }

groupshared uint Constant = 1;
// expected-warning@-1 {{initializer of 'groupshared' variable will be ignored}}

groupshared uint Dynamic = getValue();
// expected-warning@-1 {{initializer of 'groupshared' variable will be ignored}}

// DXIL retains initializers on static groupshared variables.
static groupshared uint Static = 3;

[numthreads(1, 1, 1)]
void main() {
  Constant += Dynamic + Static;
}
