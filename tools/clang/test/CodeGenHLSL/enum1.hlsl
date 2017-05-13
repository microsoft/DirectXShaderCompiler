enum Vertex {
    FIRST,
    SECOND,
    THIRD
};

int getNumber(Vertex v) {
    switch (v) {
        case Vertex::FIRST:
            return 1;
        case Vertex::SECOND:
            return 2;
        case Vertex::THIRD:
            return 3;
        default:
            return 0;
    }
}

int4 main() : SV_Target {
    return getNumber(Vertex::FIRST); 
}