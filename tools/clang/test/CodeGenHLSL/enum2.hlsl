enum Vertex {
    FIRST,
    SECOND,
    THIRD
};

int4 main(float4 col : COLOR) : SV_Target {
    return float4(FIRST, SECOND, THIRD, 4);
}