#define _create(x) create_##x
#define _add(x) add_##x
#define _sub(x) sub_##x
#define _scale(x) scale_##x
#define _dot(x) dot_##x
#define _magnitude(x) magnitude_##x
#define _normalize(x) normalize_##x
#define create(x) _create(x)
#define add(x) _add(x)
#define sub(x) _sub(x)
#define dot(x) _dot(x)
#define magnitude(x) _magnitude(x)
#define normalize(x) _normalize(x)
#define scale(x) _scale(x)

typedef struct {
    NUMERIC_TYPE x;
    NUMERIC_TYPE y;
} TYPE_NAME;

TYPE_NAME create(TYPE_NAME)(NUMERIC_TYPE x, NUMERIC_TYPE y) {
    TYPE_NAME v = {x, y};
    return v;
}

TYPE_NAME add(TYPE_NAME)(TYPE_NAME v1, TYPE_NAME v2) {
    TYPE_NAME v = {v1.x + v2.x, v1.y + v2.y};
    return v;
}

TYPE_NAME sub(TYPE_NAME)(TYPE_NAME v1, TYPE_NAME v2) {
    TYPE_NAME v = {v1.x - v2.x, v1.y - v2.y};
    return v;
}

TYPE_NAME scale(TYPE_NAME)(TYPE_NAME v, NUMERIC_TYPE scalar) {
    TYPE_NAME r = {v.x * scalar, v.y * scalar};
    return r;
}

NUMERIC_TYPE dot(TYPE_NAME)(TYPE_NAME v1, TYPE_NAME v2) {
    return v1.x * v2.x + v1.y * v2.y;
}

NUMERIC_TYPE magnitude(TYPE_NAME)(TYPE_NAME v1) {
    return sqrt(v1.x * v1.x + v1.y * v1.y);
}

TYPE_NAME normalize(TYPE_NAME)(TYPE_NAME v1) {
    double mag = magnitude_v2f(v1);
    assert(mag > 0);
    return scale(TYPE_NAME)(v1, 1/mag);
}

#undef create
#undef add
#undef sub
#undef scale
#undef dot
#undef magnitude
#undef normalize
#undef _create
#undef _add
#undef _sub
#undef _scale
#undef _dot
#undef _magnitude
#undef _normalize
