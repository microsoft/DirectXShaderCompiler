// RUN: %dxc -T cs_6_0 -E main %s | FileCheck %s
// RUN: %dxc -T cs_6_0 -E main -fcgl %s | FileCheck %s

// String containment must visit distinct types, not every path through this
// shared type graph, even when no strings are present.
// CHECK: define void @main()

struct Empty {};
struct S1 { Empty A, B; };
struct S2 { S1 A, B; };
struct S3 { S2 A, B; };
struct S4 { S3 A, B; };
struct S5 { S4 A, B; };
struct S6 { S5 A, B; };
struct S7 { S6 A, B; };
struct S8 { S7 A, B; };
struct S9 { S8 A, B; };
struct S10 { S9 A, B; };
struct S11 { S10 A, B; };
struct S12 { S11 A, B; };
struct S13 { S12 A, B; };
struct S14 { S13 A, B; };
struct S15 { S14 A, B; };
struct S16 { S15 A, B; };
struct S17 { S16 A, B; };
struct S18 { S17 A, B; };
struct S19 { S18 A, B; };
struct S20 { S19 A, B; };
struct S21 { S20 A, B; };
struct S22 { S21 A, B; };
struct S23 { S22 A, B; };
struct S24 { S23 A, B; };
struct S25 { S24 A, B; };
struct S26 { S25 A, B; };
struct S27 { S26 A, B; };
struct S28 { S27 A, B; };
struct S29 { S28 A, B; };
struct S30 { S29 A, B; };
struct S31 { S30 A, B; };
struct S32 { S31 A, B; };

static S32 State;

[numthreads(1, 1, 1)]
void main() {
  S32 Local = State;
}
