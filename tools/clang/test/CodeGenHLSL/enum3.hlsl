enum Vertex {
    FIRST,
    SECOND,
    THIRD
};

int4 main(nointerpolation float4 col : COLOR) : SV_Target {
    return GetAttributeAtVertex(col, Vertex::THIRD);
}