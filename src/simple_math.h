#ifndef SIMPLE_MATH_H
#define SIMPLE_MATH_H

#include <math.h>

#ifndef PI
    #define PI    3.14159265358979323846f
#endif

#ifndef PI2
    #define PI2   6.28318530717958647693f
#endif

#ifndef PIDIV2
    #define PIDIV2 1.5707963267948966192f
#endif

#ifndef EPSILON
    #define EPSILON 0.000001f
#endif

#ifndef DEG2RAD
    #define DEG2RAD (PI/180.0f)
#endif

#ifndef RAD2DEG
    #define RAD2DEG (180.0f/PI)
#endif

#ifndef ABS
    #define ABS(X) ((X) >= 0 ? X : -(X))
#endif

#define CLAMP(V, Min, Max) ((V) < (Min) ? (Min) : (V) > (Max) ? (Max) : (V))

inline float32 lerp(float32 a, float32 b, float32 t) {
    float32 result = (1.0f - t) * a + b * t;
    return result;
}

union v2 {
    float32 e[2];
    struct {
        float32 x, y;
    };
    struct {
        float32 u, v;
    };
    struct {
        float32 width, height;
    };
};

union v3 {
    float32 e[3];
    struct {
        float32 x, y, z;
    };
    struct {
        float32 u, v, w;
    };
    struct {
        float32 r, g, b;
    };
    struct {
        float32 width, height, depth;
    };
    struct {
        v2 xy;
        float32 _unused0;
    };
};

union v4 {
    float32 e[4];
    struct {
        float32 x, y, z, w;
    };
    struct {
        float32 r, g, b, a;
    };
    struct {
        v2 xy;
        float32 _unused0;
        float32 _unused1;
    };
    struct {
        v3 xyz;
        float32 _unused2;
    };
};

union m4 {
    float32 e[4][4];
    v4 columns[4];
};

inline v2 V2() {
    v2 result;
    result.x = 0.0f;
    result.y = 0.0f;
    return result;
}

inline v2 V2(float32 x, float32 y) {
    v2 result;
    result.x = x;
    result.y = y;
    return result;
}

inline v3 V3() {
    v3 result;
    result.x = 0.0f;
    result.y = 0.0f;
    result.z = 0.0f;
    return result;
}

inline v3 V3(float32 x, float32 y, float32 z) {
    v3 result;
    result.x = x;
    result.y = y;
    result.z = z;
    return result;
}

inline v3 V3(v2 xy, float32 z) {
    v3 result;
    result.x = xy.x;
    result.y = xy.y;
    result.z = z;
    return result;
}

inline v4 V4() {
    v4 result;
    result.x = 0.0f;
    result.y = 0.0f;
    result.z = 0.0f;
    return result;
}

inline v4 V4(float32 x, float32 y, float32 z, float32 w) {
    v4 result;
    result.x = x;
    result.y = y;
    result.z = z;
    result.w = w;
    return result;
}

inline v4 V4(v3 v, float32 w) {
    v4 result;
    result.x = v.x;
    result.y = v.y;
    result.z = v.z;
    result.w = w;
    return result;
}

inline v2 add_v2(v2 a, v2 b) {
    v2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

inline v2 sub_v2(v2 a, v2 b) {
    v2 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

inline v2 negate_v2(v2 v) {
    v2 result;
    result.x = -v.x;
    result.y = -v.y;
    return result;
}

inline v2 mul_v2_s(v2 v, float32 s) {
    v2 result;
    result.x = v.x * s;
    result.y = v.y * s;
    return result;
}

inline v2 div_v2_s(v2 v, float32 s) {
    v2 result;
    result.x = v.x / s;
    result.y = v.y / s;
    return result;
}

inline v3 add_v3(v3 a, v3 b) {
    v3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result; 
}

inline v3 sub_v3(v3 a, v3 b) {
    v3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result; 
}

inline v3 negate_v3(v3 v) {
    v3 result;
    result.x = -v.x;
    result.y = -v.y;
    result.z = -v.z;
    return result;
}

inline v3 mul_v3_s(v3 v, float32 s) {
    v3 result;
    result.x = v.x * s;
    result.y = v.y * s;
    result.z = v.z * s;
    return result;
}

inline v3 div_v3_s(v3 v, float32 s) {
    v3 result;
    result.x = v.x / s;
    result.y = v.y / s;
    result.z = v.z / s;
    return result;
}

inline v4 add_v4(v4 a, v4 b) {
    v4 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    result.w = a.w + b.w;
    return result;
}

inline v4 sub_v4(v4 a, v4 b) {
    v4 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    result.w = a.w - b.w;
    return result;
}

inline v4 negate_v4(v4 v) {
    v4 result;
    result.x = -v.x;
    result.y = -v.y;
    result.z = -v.z;
    result.w = -v.w;
    return result;
}

inline v4 mul_v4_s(v4 v, float32 s) {
    v4 result;
    result.x = v.x * s;
    result.y = v.y * s;
    result.z = v.z * s;
    result.w = v.w * s;
    return result;
}

inline v4 div_v4_s(v4 v, float32 s) {
    v4 result;
    result.x = v.x / s;
    result.y = v.y / s;
    result.z = v.z / s;
    result.w = v.w / s;
    return result;
}

inline float32 length2_v2(v2 v) {
    float32 result;
    result = v.x * v.x + v.y * v.y;
    return result;
}

inline float32 length_v2(v2 v) {
    float32 result;
    result = sqrtf(v.x * v.x + v.y * v.y);
    return result;
}

inline v2 normalize_v2(v2 v) {
    float32 length = length_v2(v);
    v2 result = div_v2_s(v, length);
    return result;
}

inline float32 dot_v2(v2 a, v2 b) {
    float32 result = a.x * b.x + a.y * b.y;
    return result;
}

inline v2 lerp_v2(v2 a, v2 b, float32 t) {
    v2 result;
    result = add_v2(mul_v2_s(a, (1.0f - t)), mul_v2_s(b, t));
    return result;
}

inline float32 length2_v3(v3 v) {
    float32 result = v.x * v.x + v.y * v.y + v.z * v.z;
    return result;
}

inline float32 length_v3(v3 v) {
    float32 result;
    result = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    return result;
}

inline v3 normalize_v3(v3 v) {
    float32 length = length_v3(v);
    v3 result = div_v3_s(v, length);
    return result;
}

inline float32 dot_v3(v3 a, v3 b) {
    float32 result;
    result = a.x * b.x + a.y * b.y + a.z * b.z;
    return result; 
}

inline v3 lerp_v3(v3 a, v3 b, float32 t) {
    v3 result;
    result = add_v3(mul_v3_s(a, (1.0f - t)), mul_v3_s(b, t));
    return result;
}

inline v3 cross_v3(v3 a, v3 b) {
    v3 result;
    result.x = (a.y * b.z) - (a.z * b.y);
    result.y = (a.z * b.x) - (a.x * b.z);
    result.z = (a.x * b.y) - (a.y * b.x);
    return result;
}

inline float32 length2_v4(v4 v) {
    float32 result = v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
    return result;
}

inline float32 length_v4(v4 v) {
    float32 result;
    result = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
    return result;
}

inline v4 normalize_v4(v4 v) {
    float32 length = length_v4(v);
    v4 result = mul_v4_s(v, length);
    return result;
}

inline float32 dot_v4(v4 a, v4 b) {
    float32 result;
    result = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    return result; 
}

inline v4 lerp_v4(v4 a, v4 b, float32 t) {
    v4 result;
    result = add_v4(mul_v4_s(a, (1.0f - t)), mul_v4_s(b, t));
    return result;
}

//

inline m4 identity_m4() {
    m4 result{};
    result.e[0][0] = 1.0f;
    result.e[1][1] = 1.0f;
    result.e[2][2] = 1.0f;
    result.e[3][3] = 1.0f;
    return result;
}

inline m4 transpose_m4(m4 m) {
    m4 result = m;
    result.e[0][1] = m.e[1][0];
    result.e[0][2] = m.e[2][0];
    result.e[0][3] = m.e[3][0];
    result.e[1][0] = m.e[0][1];
    result.e[1][2] = m.e[2][1];
    result.e[1][3] = m.e[3][1];
    result.e[2][0] = m.e[0][2];
    result.e[2][1] = m.e[1][2];
    result.e[2][3] = m.e[3][2];
    result.e[3][0] = m.e[0][3];
    result.e[3][1] = m.e[1][3];
    result.e[3][2] = m.e[2][3];
    return result;
}

inline m4 mul_m4(m4 b, m4 a) {
    m4 result{};
    result.e[0][0] = a.e[0][0] * b.e[0][0] + a.e[0][1] * b.e[1][0] + a.e[0][2] * b.e[2][0] + a.e[0][3] * b.e[3][0]; 
    result.e[0][1] = a.e[0][0] * b.e[0][1] + a.e[0][1] * b.e[1][1] + a.e[0][2] * b.e[2][1] + a.e[0][3] * b.e[3][1]; 
    result.e[0][2] = a.e[0][0] * b.e[0][2] + a.e[0][1] * b.e[1][2] + a.e[0][2] * b.e[2][2] + a.e[0][3] * b.e[3][2]; 
    result.e[0][3] = a.e[0][0] * b.e[0][3] + a.e[0][1] * b.e[1][3] + a.e[0][2] * b.e[2][3] + a.e[0][3] * b.e[3][3]; 
    result.e[1][0] = a.e[1][0] * b.e[0][0] + a.e[1][1] * b.e[1][0] + a.e[1][2] * b.e[2][0] + a.e[1][3] * b.e[3][0]; 
    result.e[1][1] = a.e[1][0] * b.e[0][1] + a.e[1][1] * b.e[1][1] + a.e[1][2] * b.e[2][1] + a.e[1][3] * b.e[3][1]; 
    result.e[1][2] = a.e[1][0] * b.e[0][2] + a.e[1][1] * b.e[1][2] + a.e[1][2] * b.e[2][2] + a.e[1][3] * b.e[3][2]; 
    result.e[1][3] = a.e[1][0] * b.e[0][3] + a.e[1][1] * b.e[1][3] + a.e[1][2] * b.e[2][3] + a.e[1][3] * b.e[3][3]; 
    result.e[2][0] = a.e[2][0] * b.e[0][0] + a.e[2][1] * b.e[1][0] + a.e[2][2] * b.e[2][0] + a.e[2][3] * b.e[3][0]; 
    result.e[2][1] = a.e[2][0] * b.e[0][1] + a.e[2][1] * b.e[1][1] + a.e[2][2] * b.e[2][1] + a.e[2][3] * b.e[3][1]; 
    result.e[2][2] = a.e[2][0] * b.e[0][2] + a.e[2][1] * b.e[1][2] + a.e[2][2] * b.e[2][2] + a.e[2][3] * b.e[3][2]; 
    result.e[2][3] = a.e[2][0] * b.e[0][3] + a.e[2][1] * b.e[1][3] + a.e[2][2] * b.e[2][3] + a.e[2][3] * b.e[3][3]; 
    result.e[3][0] = a.e[3][0] * b.e[0][0] + a.e[3][1] * b.e[1][0] + a.e[3][2] * b.e[2][0] + a.e[3][3] * b.e[3][0]; 
    result.e[3][1] = a.e[3][0] * b.e[0][1] + a.e[3][1] * b.e[1][1] + a.e[3][2] * b.e[2][1] + a.e[3][3] * b.e[3][1]; 
    result.e[3][2] = a.e[3][0] * b.e[0][2] + a.e[3][1] * b.e[1][2] + a.e[3][2] * b.e[2][2] + a.e[3][3] * b.e[3][2]; 
    result.e[3][3] = a.e[3][0] * b.e[0][3] + a.e[3][1] * b.e[1][3] + a.e[3][2] * b.e[2][3] + a.e[3][3] * b.e[3][3]; 
    return result;
}

inline v4 mul_m4_v4(m4 m, v4 v) {
    v4 result;
    result.e[0] = v.e[0] * m.e[0][0] + v.e[1] * m.e[0][1] + v.e[2] * m.e[0][2] + v.e[3] * m.e[0][3];
    result.e[1] = v.e[0] * m.e[1][0] + v.e[1] * m.e[1][1] + v.e[2] * m.e[1][2] + v.e[3] * m.e[1][3];
    result.e[2] = v.e[0] * m.e[2][0] + v.e[1] * m.e[2][1] + v.e[2] * m.e[2][2] + v.e[3] * m.e[2][3];
    result.e[3] = v.e[0] * m.e[3][0] + v.e[1] * m.e[3][1] + v.e[2] * m.e[3][2] + v.e[3] * m.e[3][3];
    return result;
}

inline m4 translate_m4(v3 t) {
    m4 result = identity_m4();
    result.e[3][0] = t.x;
    result.e[3][1] = t.y;
    result.e[3][2] = t.z;
    return result;
}

inline m4 inv_translate_m4(v3 t) {
    t = negate_v3(t);
    m4 result = translate_m4(t);
    return result;
}

inline m4 scale_m4(v3 s) {
    m4 result{};
    result.e[0][0] = s.x;
    result.e[1][1] = s.y;
    result.e[2][2] = s.z;
    result.e[3][3] = 1.0f;
    return result;
}

inline m4 rotate_m4(v3 axis, float32 theta) {
    m4 result = identity_m4();
    axis = normalize_v3(axis);

    float32 sin_t = sinf(theta);
    float32 cos_t = cosf(theta);
    float32 cos_inv = 1.0f - cos_t;

    result.e[0][0] = (cos_inv * axis.x * axis.x) + cos_t;
    result.e[0][1] = (cos_inv * axis.x * axis.y) + (axis.z * sin_t);
    result.e[0][2] = (cos_inv * axis.x * axis.z) - (axis.y * sin_t);

    result.e[1][0] = (cos_inv * axis.y * axis.x) - (axis.z * sin_t);
    result.e[1][1] = (cos_inv * axis.y * axis.y) - cos_t;
    result.e[1][2] = (cos_inv * axis.y * axis.z) + (axis.x * sin_t);

    result.e[2][0] = (cos_inv * axis.z * axis.x) + (axis.y * sin_t);
    result.e[2][1] = (cos_inv * axis.z * axis.y) - (axis.x * sin_t);
    result.e[2][2] = (cos_inv * axis.z * axis.z) + cos_t;

    return result; 
}

inline m4 perspective_projection_rh(float32 fov, float32 aspect_ratio, float32 near, float32 far) {
    m4 result{};
    float32 c = 1.0f / tanf(fov / 2.0f);
    result.e[0][0] = c / aspect_ratio;
    result.e[1][1] = c;
    result.e[2][3] = -1.0f;

    result.e[2][2] = (far) / (near - far);
    result.e[3][2] = (near * far) / (near - far);
    
    return result;
}

inline m4 look_at_rh(v3 eye, v3 target, v3 up) {
    v3 F = normalize_v3(sub_v3(target, eye));
    v3 R = normalize_v3(cross_v3(F, up));
    v3 U = cross_v3(R, F);

    m4 result = identity_m4();
    result.e[0][0] = R.x;
    result.e[1][0] = R.y;
    result.e[2][0] = R.z;
    result.e[0][1] = U.x;
    result.e[1][1] = U.y;
    result.e[2][1] = U.z;
    result.e[0][2] = -F.x;
    result.e[1][2] = -F.y;
    result.e[2][2] = -F.z;
    result.e[3][0] = -dot_v3(R, eye);
    result.e[3][1] = -dot_v3(U, eye);
    result.e[3][2] =  dot_v3(F, eye);
    return result;
}

inline m4 look_at_lh(v3 eye, v3 target, v3 up) {
    v3 F = normalize_v3(sub_v3(target, eye));
    v3 R = normalize_v3(cross_v3(up, F));
    v3 U = cross_v3(F, R);

    m4 result = identity_m4();
    result.e[0][0] = R.x;
    result.e[1][0] = R.y;
    result.e[2][0] = R.z;
    result.e[0][1] = U.x;
    result.e[1][1] = U.y;
    result.e[2][1] = U.z;
    result.e[0][2] = F.x;
    result.e[1][2] = F.y;
    result.e[2][2] = F.z;
    result.e[3][0] = -dot_v3(R, eye);
    result.e[3][1] = -dot_v3(U, eye);
    result.e[3][2] = -dot_v3(F, eye);
    return result;
}

inline m4 ortho_rh_zo(float32 left, float32 right, float32 bottom, float32 top, float32 near, float32 far) {
    m4 result = identity_m4();
    result.e[0][0] = 2.0f / (right - left);
    result.e[1][1] = 2.0f / (top - bottom);
    result.e[2][2] = 1.0f / (far - near);
    result.e[3][0] = - (right + left) / (right - left);
    result.e[3][1] = - (top + bottom) / (top - bottom);
    result.e[3][2] = - near / (far - near);
    return result;
}


// OVERLOADED FUNCTIONS
#ifdef __cplusplus
inline float32 dot(v2 a, v2 b) {
    float32 result = dot_v2(a, b);
    return result;
}

inline float32 length2(v2 v) {
    float32 result = length2_v2(v);
    return result;
}

inline float32 length(v2 v) {
    float32 result = length_v2(v);
    return result;
}

inline v2 normalize(v2 v) {
    v2 result = normalize_v2(v);
    return result;
}

inline float32 dot(v3 a, v3 b) {
    float32 result = dot_v3(a, b);
    return result;
}

inline v2 lerp(v2 a, v2 b, float32 t) {
    v2 result = lerp_v2(a, b, t);
    return result; 
}

inline float32 length2(v3 v) {
    float32 result = length2_v3(v);
    return result;
}

inline float32 length(v3 v) {
    float32 result = length_v3(v);
    return result;
}

inline v3 normalize(v3 v) {
    v3 result = normalize_v3(v);
    return result;
}

inline v3 lerp(v3 a, v3 b, float32 t) {
    v3 result = lerp_v3(a, b, t);
    return result; 
}

inline float32 dot(v4 a, v4 b) {
    float32 result = dot_v4(a, b);
    return result;
}

inline float32 length2(v4 v) {
    float32 result = length2_v4(v);
    return result;
}

inline float32 length(v4 v) {
    float32 result = length_v4(v);
    return result;
}

inline v4 normalize(v4 v) {
    v4 result = normalize_v4(v);
    return result;
}

inline v4 lerp(v4 a, v4 b, float32 t) {
    v4 result = lerp_v4(a, b, t);
    return result; 
}

inline m4 mul(m4 a, m4 b) {
    m4 result = mul_m4(a, b);
    return result;
}

inline v4 mul(m4 m, v4 v) {
    v4 result = mul_m4_v4(m, v);
    return result;
}

inline m4 translate(v3 v) {
    m4 result = translate_m4(v);
    return result;
}

inline m4 inv_translate(v3 v) {
    m4 result = inv_translate_m4(v);
    return result;
}

inline m4 scale(v3 v) {
    m4 result = scale_m4(v);
    return result;
}

inline m4 rotate(v3 v, float32 theta) {
    m4 result = rotate_m4(v, theta);
    return result;
}

#endif // __cplusplus


// OPERATORS
#ifdef __cplusplus
inline v2 operator+(v2 a, v2 b) {
    v2 result = add_v2(a, b);
    return result;
}

inline v3 operator+(v3 a, v3 b) {
    v3 result = add_v3(a, b);
    return result;
}

inline v4 operator+(v4 a, v4 b) {
    v4 result = add_v4(a, b);
    return result;
}

inline v2 operator-(v2 v) {
    v2 result = negate_v2(v);
    return result;
}

inline v3 operator-(v3 v) {
    v3 result = negate_v3(v);
    return result;
}

inline v4 operator-(v4 v) {
    v4 result = negate_v4(v);
    return result;
}


inline v2 operator-(v2 a, v2 b) {
    v2 result = sub_v2(a, b);
    return result;
}

inline v3 operator-(v3 a, v3 b) {
    v3 result = sub_v3(a, b);
    return result;
}

inline v4 operator-(v4 a, v4 b) {
    v4 result = sub_v4(a, b);
    return result;
}

inline v2 operator+=(v2 &a, v2 b) {
    a = a + b;
    return a;
}

inline v3 operator+=(v3 &a, v3 b) {
    a = a + b;
    return a;
}

inline v4 operator+=(v4 &a, v4 b) {
    a = a + b;
    return a;
}

inline v2 operator-=(v2 &a, v2 b) {
    a = a - b;
    return a;
}

inline v3 operator-=(v3 &a, v3 b) {
    a = a - b;
    return a;
}

inline v4 operator-=(v4 &a, v4 b) {
    a = a - b;
    return a;
}

inline v2 operator*(v2 v, float32 s) {
    v2 result = mul_v2_s(v, s);
    return result; 
}

inline v3 operator*(v3 v, float32 s) {
    v3 result = mul_v3_s(v, s);
    return result; 
}

inline v4 operator*(v4 v, float32 s) {
    v4 result = mul_v4_s(v, s);
    return result; 
}

inline v2 operator/(v2 v, float32 s) {
    v2 result = div_v2_s(v, s);
    return result;
}

inline v3 operator/(v3 v, float32 s) {
    v3 result = div_v3_s(v, s);
    return result;
}

inline v4 operator/(v4 v, float32 s) {
    v4 result = div_v4_s(v, s);
    return result;
}

inline v2 operator*=(v2 &v, float32 s) {
    v = v * s;
    return v;
}

inline v3 operator*=(v3 &v, float32 s) {
    v = v * s;
    return v;
}

inline v4 operator*=(v4 &v, float32 s) {
    v = v * s;
    return v;
}

inline v2 operator/=(v2 &v, float32 s) {
    v = v / s;
    return v;
}

inline v3 operator/=(v3 &v, float32 s) {
    v = v / s;
    return v;
}

inline v4 operator/=(v4 &v, float32 s) {
    v = v / s;
    return v;
}

inline bool operator==(v2 a, v2 b) {
    bool result = a.x == b.x && a.y == b.y;
    return result;
}

inline bool operator==(v3 a, v3 b) {
    bool result = a.x == b.x && a.y == b.y && a.z == b.z;
    return result;
}

inline bool operator==(v4 a, v4 b) {
    bool result = a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
    return result;
}

inline m4 operator*(m4 a, m4 b) {
    m4 result = mul_m4(a, b);
    return result;
}

#endif // __cplusplus



#endif // SIMPLE_MATH_H
