// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// Repro of GitHub #1881

bool main() : OUT
{
    static const struct { bool2x2 x; } sm = { false, true, false, true };
    return sm.x._11;
}