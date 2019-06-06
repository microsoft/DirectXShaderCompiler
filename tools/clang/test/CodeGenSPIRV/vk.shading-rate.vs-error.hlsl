// Run: %dxc -T vs_6_4 -E main

void main(out uint rate : SV_ShadingRate) {
    rate = 0;
}

// CHECK:  :3:27: error: semantic ShadingRate currently unsupported in non-PS shader stages
