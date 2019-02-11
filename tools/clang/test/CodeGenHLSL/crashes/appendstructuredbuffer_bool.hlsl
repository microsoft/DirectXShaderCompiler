// RUN: %dxc -E main -T vs_6_2 %s | FileCheck %s

// Repro of GitHub #1882

AppendStructuredBuffer<bool> buf;
void main() { buf.Append(true); }