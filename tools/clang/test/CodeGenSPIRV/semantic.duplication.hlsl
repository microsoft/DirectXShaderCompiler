// Run: %dxc -T vs_6_0 -E main

struct VSIn {
    float4 f: ABC;
};

struct VSOut {
    float4 g: ABC;
};

void main(    int a: ABC,     int b: ABC,     VSIn c,
          out int u: ABC, out int v: ABC, out VSOut w) {
}

// CHECK: error: input semantic 'ABC' used more than once
// CHECK: error: input semantic 'ABC' used more than once

// CHECK: error: output semantic 'ABC' used more than once
// CHECK: error: output semantic 'ABC' used more than once