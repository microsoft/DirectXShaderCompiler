// Failing on dxc but addition works on vanilla clang
// Look at CheckBinOpForHLSL on SemaHLSL.cpp
enum Vertex {
    FIRST,
    SECOND,
    THIRD
};


int4 main(float4 col : COLOR) : SV_Target {
    return col.x + Vertex::FIRST; 
}
