/*
 * Copyright 2016 GoPro Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "math_utils.h"

static const float zvec[4];

float ngli_vec3_length(const float *v)
{
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

void ngli_vec3_scale(float *dst, const float *v, float s)
{
    dst[0] = v[0] * s;
    dst[1] = v[1] * s;
    dst[2] = v[2] * s;
}

void ngli_vec3_sub(float *dst, const float *v1, const float *v2)
{
    dst[0] = v1[0] - v2[0];
    dst[1] = v1[1] - v2[1];
    dst[2] = v1[2] - v2[2];
}

void ngli_vec3_norm(float *dst, const float *v)
{
    if (!memcmp(v, zvec, 3 * sizeof(*v))) {
        memcpy(dst, zvec, 3 * sizeof(*v));
        return;
    }

    const float l = 1.0f / ngli_vec3_length(v);

    dst[0] = v[0] * l;
    dst[1] = v[1] * l;
    dst[2] = v[2] * l;
}

void ngli_vec3_cross(float *dst, const float *v1, const float *v2)
{
    float v[3];

    v[0] = v1[1]*v2[2] - v1[2]*v2[1];
    v[1] = v1[2]*v2[0] - v1[0]*v2[2];
    v[2] = v1[0]*v2[1] - v1[1]*v2[0];

    memcpy(dst, v, sizeof(v));
}

float ngli_vec3_dot(const float *v1, const float *v2)
{
    return v1[0]*v2[0]
         + v1[1]*v2[1]
         + v1[2]*v2[2];
}

void ngli_vec3_normalvec(float *dst, const float *a, const float *b, const float *c)
{
    float d[3];
    float e[3];

    ngli_vec3_sub(d, b, a);
    ngli_vec3_sub(e, c, a);
    ngli_vec3_cross(dst, d, e);
    ngli_vec3_norm(dst, dst);
}

float ngli_vec4_length(const float *v)
{
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2] + v[3]*v[3]);
}

void ngli_vec4_norm(float *dst, const float *v)
{
    if (!memcmp(v, zvec, 4 * sizeof(*v))) {
        memcpy(dst, zvec, 4 * sizeof(*v));
        return;
    }

    const float l = 1.0f / ngli_vec4_length(v);

    dst[0] = v[0] * l;
    dst[1] = v[1] * l;
    dst[2] = v[2] * l;
    dst[3] = v[3] * l;
}

void ngli_vec4_add(float *dst, const float *v1, const float *v2)
{
    dst[0] = v1[0] + v2[0];
    dst[1] = v1[1] + v2[1];
    dst[2] = v1[2] + v2[2];
    dst[3] = v1[3] + v2[3];
}

void ngli_vec4_sub(float *dst, const float *v1, const float *v2)
{
    dst[0] = v1[0] - v2[0];
    dst[1] = v1[1] - v2[1];
    dst[2] = v1[2] - v2[2];
    dst[3] = v1[3] - v2[3];
}

void ngli_vec4_neg(float *dst, const float *v)
{
    dst[0] = -v[0];
    dst[1] = -v[1];
    dst[2] = -v[2];
    dst[3] = -v[3];
}

void ngli_vec4_scale(float *dst, const float *v, float s)
{
    dst[0] = v[0] * s;
    dst[1] = v[1] * s;
    dst[2] = v[2] * s;
    dst[3] = v[3] * s;
}

float ngli_vec4_dot(const float *v1, const float *v2)
{
    return v1[0]*v2[0]
         + v1[1]*v2[1]
         + v1[2]*v2[2]
         + v1[3]*v2[3];
}

void ngli_vec4_lerp(float *dst, const float *v1, const float *v2, float c)
{
    dst[0] = v1[0] + c*(v2[0] - v1[0]);
    dst[1] = v1[1] + c*(v2[1] - v1[1]);
    dst[2] = v1[2] + c*(v2[2] - v1[2]);
    dst[3] = v1[3] + c*(v2[3] - v1[3]);
}

void ngli_mat3_from_mat4(float *dst, const float *m)
{
    memcpy(dst,     m,     3 * sizeof(*m));
    memcpy(dst + 3, m + 4, 3 * sizeof(*m));
    memcpy(dst + 6, m + 8, 3 * sizeof(*m));
}

void ngli_mat3_mul_scalar(float *dst, const float *m, float s)
{
    float tmp[3*3];

    for (int i = 0; i < 9; i++) {
        tmp[i] = m[i] * s;
    }

    memcpy(dst, tmp, sizeof(tmp));
}

void ngli_mat3_transpose(float *dst, const float *m)
{
    float tmp[3*3];

    tmp[0] = m[0];
    tmp[1] = m[3];
    tmp[2] = m[6];

    tmp[3] = m[1];
    tmp[4] = m[4];
    tmp[5] = m[7];

    tmp[6] = m[2];
    tmp[7] = m[5];
    tmp[8] = m[8];

    memcpy(dst, tmp, sizeof(tmp));
}

float ngli_mat3_determinant(const float *m)
{
    float ret;

    ret  = m[0]*m[4]*m[8] + m[1]*m[5]*m[6] + m[2]*m[3]*m[7];
    ret -= m[2]*m[4]*m[6] + m[1]*m[3]*m[8] + m[0]*m[5]*m[7];

    return ret;
}

void ngli_mat3_adjugate(float *dst, const float* m)
{
    float tmp[3*3];

    tmp[0] = m[4]*m[8] - m[5]*m[7];
    tmp[1] = m[2]*m[7] - m[1]*m[8];
    tmp[2] = m[1]*m[5] - m[2]*m[4];
    tmp[3] = m[5]*m[6] - m[3]*m[8];
    tmp[4] = m[0]*m[8] - m[2]*m[6];
    tmp[5] = m[2]*m[3] - m[0]*m[5];
    tmp[6] = m[3]*m[7] - m[4]*m[6];
    tmp[7] = m[1]*m[6] - m[0]*m[7];
    tmp[8] = m[0]*m[4] - m[1]*m[3];

    memcpy(dst, tmp, sizeof(tmp));
}

void ngli_mat3_inverse(float *dst, const float *m)
{
    float a[3*3];
    float det = ngli_mat3_determinant(m);

    if (det == 0.0) {
        memcpy(dst, m, 3 * 3 * sizeof(*m));
        return;
    }

    ngli_mat3_adjugate(a, m);
    ngli_mat3_mul_scalar(dst, a, 1.0 / det);
}

void ngli_mat4_mul_c(float *dst, const float *m1, const float *m2)
{
    float m[4*4];

    m[ 0] = m1[0]*m2[ 0] + m1[4]*m2[ 1] + m1[ 8]*m2[ 2] + m1[12]*m2[ 3];
    m[ 1] = m1[1]*m2[ 0] + m1[5]*m2[ 1] + m1[ 9]*m2[ 2] + m1[13]*m2[ 3];
    m[ 2] = m1[2]*m2[ 0] + m1[6]*m2[ 1] + m1[10]*m2[ 2] + m1[14]*m2[ 3];
    m[ 3] = m1[3]*m2[ 0] + m1[7]*m2[ 1] + m1[11]*m2[ 2] + m1[15]*m2[ 3];

    m[ 4] = m1[0]*m2[ 4] + m1[4]*m2[ 5] + m1[ 8]*m2[ 6] + m1[12]*m2[ 7];
    m[ 5] = m1[1]*m2[ 4] + m1[5]*m2[ 5] + m1[ 9]*m2[ 6] + m1[13]*m2[ 7];
    m[ 6] = m1[2]*m2[ 4] + m1[6]*m2[ 5] + m1[10]*m2[ 6] + m1[14]*m2[ 7];
    m[ 7] = m1[3]*m2[ 4] + m1[7]*m2[ 5] + m1[11]*m2[ 6] + m1[15]*m2[ 7];

    m[ 8] = m1[0]*m2[ 8] + m1[4]*m2[ 9] + m1[ 8]*m2[10] + m1[12]*m2[11];
    m[ 9] = m1[1]*m2[ 8] + m1[5]*m2[ 9] + m1[ 9]*m2[10] + m1[13]*m2[11];
    m[10] = m1[2]*m2[ 8] + m1[6]*m2[ 9] + m1[10]*m2[10] + m1[14]*m2[11];
    m[11] = m1[3]*m2[ 8] + m1[7]*m2[ 9] + m1[11]*m2[10] + m1[15]*m2[11];

    m[12] = m1[0]*m2[12] + m1[4]*m2[13] + m1[ 8]*m2[14] + m1[12]*m2[15];
    m[13] = m1[1]*m2[12] + m1[5]*m2[13] + m1[ 9]*m2[14] + m1[13]*m2[15];
    m[14] = m1[2]*m2[12] + m1[6]*m2[13] + m1[10]*m2[14] + m1[14]*m2[15];
    m[15] = m1[3]*m2[12] + m1[7]*m2[13] + m1[11]*m2[14] + m1[15]*m2[15];

    memcpy(dst, m, sizeof(m));
}

void ngli_mat4_mul_vec4_c(float *dst, const float *m, const float *v)
{
    float tmp[4];

    tmp[0] = m[ 0]*v[0] + m[ 4]*v[1] + m[ 8]*v[2] + m[12]*v[3];
    tmp[1] = m[ 1]*v[0] + m[ 5]*v[1] + m[ 9]*v[2] + m[13]*v[3];
    tmp[2] = m[ 2]*v[0] + m[ 6]*v[1] + m[10]*v[2] + m[14]*v[3];
    tmp[3] = m[ 3]*v[0] + m[ 7]*v[1] + m[11]*v[2] + m[15]*v[3];

    memcpy(dst, tmp, sizeof(tmp));
}

void ngli_mat4_look_at(float *dst, float *eye, float *center, float *up)
{
    float f[3];
    float s[3];
    float u[3];

    ngli_vec3_sub(f, center, eye);
    ngli_vec3_norm(f, f);

    ngli_vec3_cross(s, f, up);
    ngli_vec3_norm(s, s);

    ngli_vec3_cross(u, s, f);

    dst[ 0] =  s[0];
    dst[ 1] =  u[0];
    dst[ 2] = -f[0];
    dst[ 3] =  0.0;

    dst[ 4] =  s[1];
    dst[ 5] =  u[1];
    dst[ 6] = -f[1];
    dst[ 7] =  0.0;

    dst[ 8] =  s[2];
    dst[ 9] =  u[2];
    dst[10] = -f[2];
    dst[11] =  0.0;

    dst[12] = -ngli_vec3_dot(s, eye);
    dst[13] = -ngli_vec3_dot(u, eye);
    dst[14] =  ngli_vec3_dot(f, eye);
    dst[15] =  1.0;
}

void ngli_mat4_identity(float *dst)
{
    static const float id[4*4] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    memcpy(dst, id, sizeof(id));
}

void ngli_mat4_orthographic(float *dst, float left, float right,
                            float bottom, float top, float near, float far)
{
    const float dx = right - left;
    const float dy = top - bottom;
    const float dz = far - near;

    ngli_mat4_identity(dst);

    if (dx == 0 || dy == 0 || dz == 0)
        return;

    const float tx = -(right + left) / dx;
    const float ty = -(top + bottom) / dy;
    const float tz = -(far + near)   / dz;

    dst[0 ] =  2 / dx;
    dst[5 ] =  2 / dy;
    dst[10] = -2 / dz;
    dst[12] = tx;
    dst[13] = ty;
    dst[14] = tz;
}

void ngli_mat4_perspective(float *dst, float fov, float aspect, float near, float far)
{
    const float r = fov / 2 * M_PI / 180.0f;
    const float s = sin(r);
    const float z = far - near;

    ngli_mat4_identity(dst);

    if (z == 0 || s == 0 || aspect == 0) {
        return;
    }

    const float c = cos(r) / s;

    dst[ 0] =  c / aspect;
    dst[ 5] =  c;
    dst[10] = -(far + near) / z;
    dst[11] = -1;
    dst[14] = -2 * near * far / z;
    dst[15] =  0;
}

void ngli_mat4_rotate(float *dst, float angle, float *axis)
{
    const float a = cos(angle);
    const float b = sin(angle);
    const float c = 1.0f - a;

    dst[ 0] = a + axis[0] * axis[0] * c;
    dst[ 1] = axis[0] * axis[1] * c + axis[2] * b;
    dst[ 2] = axis[0] * axis[2] * c - axis[1] * b;
    dst[ 3] = 0.0f;

    dst[ 4] = axis[0] * axis[1] * c - axis[2] * b;
    dst[ 5] = a + axis[1] * axis[1] * c;
    dst[ 6] = axis[1] * axis[2] * c + axis[0] * b;
    dst[ 7] = 0.0f;

    dst[ 8] = axis[0] * axis[2] * c + axis[1] * b;
    dst[ 9] = axis[1] * axis[2] * c - axis[0] * b;
    dst[10] = a + axis[2] * axis[2] * c;
    dst[11] = 0.0f;

    dst[12] = 0.0f;
    dst[13] = 0.0f;
    dst[14] = 0.0f;
    dst[15] = 1.0f;
}

void ngli_mat4_rotate_from_quat(float *dst, const float *q)
{
    float tmp[4];
    const float *tmpp = q;

    float length = ngli_vec4_length(q);
    if (length > 1.0) {
        ngli_vec4_norm(tmp, q);
        tmpp = tmp;
    }

    const float x2  = tmpp[0] + tmpp[0];
    const float y2  = tmpp[1] + tmpp[1];
    const float z2  = tmpp[2] + tmpp[2];

    const float yy2 = tmpp[1] * y2;
    const float xy2 = tmpp[0] * y2;
    const float xz2 = tmpp[0] * z2;
    const float yz2 = tmpp[1] * z2;
    const float zz2 = tmpp[2] * z2;
    const float wz2 = tmpp[3] * z2;
    const float wy2 = tmpp[3] * y2;
    const float wx2 = tmpp[3] * x2;
    const float xx2 = tmpp[0] * x2;

    dst[ 0] = -yy2 - zz2 + 1.0f;
    dst[ 1] =  xy2 + wz2;
    dst[ 2] =  xz2 - wy2;
    dst[ 3] =  0.0f;
    dst[ 4] =  xy2 - wz2;
    dst[ 5] = -xx2 - zz2 + 1.0f;
    dst[ 6] =  yz2 + wx2;
    dst[ 7] =  0.0f;
    dst[ 8] =  xz2 + wy2;
    dst[ 9] =  yz2 - wx2;
    dst[10] = -xx2 - yy2 + 1.0f;
    dst[11] =  0.0f;
    dst[12] =  0.0f;
    dst[13] =  0.0f;
    dst[14] =  0.0f;
    dst[15] =  1.0f;
}

void ngli_mat4_translate(float *dst, float x, float y, float z)
{
    memset(dst, 0, 4 * 4 * sizeof(*dst));
    dst[ 0] = 1.0f;
    dst[ 5] = 1.0f;
    dst[10] = 1.0f;
    dst[12] = x;
    dst[13] = y;
    dst[14] = z;
    dst[15] = 1.0f;
}

void ngli_mat4_scale(float *dst, float x, float y, float z)
{
    memset(dst, 0, 4 * 4 * sizeof(*dst));
    dst[ 0] =  x;
    dst[ 5] =  y;
    dst[10] =  z;
    dst[15] =  1.0f;
}

#define COS_ALPHA_THRESHOLD 0.9995f

void ngli_quat_slerp(float *dst, const float *q1, const float *q2, float t)
{
    float tmp_q1[4];
    const float *tmp_q1p = q1;

    float cos_alpha = ngli_vec4_dot(q1, q2);

    if (cos_alpha < 0.0f) {
        cos_alpha = -cos_alpha;
        ngli_vec4_neg(tmp_q1, q1);
        tmp_q1p = tmp_q1;
    }

    if (cos_alpha > COS_ALPHA_THRESHOLD) {
        ngli_vec4_lerp(dst, tmp_q1p, q2, t);
        ngli_vec4_norm(dst, dst);
        return;
    }

    if (cos_alpha < -1.0f)
        cos_alpha = -1.0f;
    else if (cos_alpha > 1.0f)
        cos_alpha = 1.0f;

    const float alpha = acosf(cos_alpha);
    const float theta = alpha * t;

    float tmp[4];
    ngli_vec4_scale(tmp, tmp_q1p, cos_alpha);
    ngli_vec4_sub(tmp, q2, tmp);
    ngli_vec4_norm(tmp, tmp);

    float tmp1[4];
    float tmp2[4];
    ngli_vec4_scale(tmp1, tmp_q1p, cos(theta));
    ngli_vec4_scale(tmp2, tmp, sin(theta));
    ngli_vec4_add(dst, tmp1, tmp2);
}
