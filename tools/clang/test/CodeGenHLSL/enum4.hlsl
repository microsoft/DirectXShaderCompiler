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

int4 getValueInt(int i) {
    switch (i) {
        case 0:
            return int4(1,1,1,1);
        case 1:
            return int4(2,2,2,2);
        case 2:
            return int4(3,3,3,3);
    }
}

int4 main(float4 col : COLOR) : SV_Target {
    return getValueInt(Vertex::FIRST);
}
