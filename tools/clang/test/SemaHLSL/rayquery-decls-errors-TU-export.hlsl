// RUN: %dxc -T lib_6_5 -verify %s

export float4 MyExportedFunction(float4 color) {
    // expected-warning@+1{{potential misuse of built-in constant 'RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS' in shader model lib_6_5; introduced in shader model 6.9}}
    return color * RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS;
}
