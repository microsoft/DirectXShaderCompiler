// This compiles on dxc but fails on vanilla clang complaining that
// note: candidate function not viable: no known conversion from 'int' to 'Vertex' for 1st argument
// Look at CanConvert on semahlsl.cpp
enum Vertex {
    FIRST,
    SECOND,
    THIRD
};

int4 getValueVertex(Vertex v) {
    switch (v) {
        case FIRST:
            return int4(1,1,1,1);
        case SECOND:
            return int4(2,2,2,2);
        case THIRD:
            return int4(3,3,3,3);
    }
}

float4 main(float4 col : COLOR) : SV_Target {
    return getValueVertex(1); 
}
