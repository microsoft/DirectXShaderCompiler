// RUN: %dxc -E main -T vs_6_2 %s | FileCheck %s
// TODO: No check lines found, we should update this

// Repro of GitHub #1970

struct S2 { int a, b; };
struct S3 { int a, b, c; };
void main()
{
    S3 s;
    (S2)s;
}