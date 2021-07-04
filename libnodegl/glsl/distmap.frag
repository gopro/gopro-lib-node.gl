/*
 * Copyright 2021 GoPro Inc.
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

#define cplx_mul(a, b) vec2(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x)
#define cplx_div(a, b) vec2((a.x*b.x + a.y*b.y) / (b.x*b.x + b.y*b.y), (a.y*b.x - a.x*b.y) / (b.x*b.x + b.y*b.y))
#define cplx_inv(z)    vec2(z.x / (z.x*z.x + z.y*z.y), -z.y / (z.x*z.x + z.y*z.y))
#define cplx_abs(z)    (z.x*z.x)

#define PI 3.14159265358979323846
#define EPS 1e-8
#define CHECK_ZERO(x) (abs(x) < 1e-6) /* XXX too high glitches; why? */
#define CBRT(x) (sign(x)*pow(abs(x), 1.0/3.0)) /* cube root */

#define FAST 0

/* Linear: f(x)=ax+b */
int root_find1(inout float root, float a, float b)
{
    if (CHECK_ZERO(a))
        return 0;
    root = -b / a;
    return 1;
}

/* Quadratic monic: f(x)=x²+ax+b */
int root_find2_monic(inout vec2 roots, float a, float b)
{
    /* depressed: t²+p with x=t-a/2 */
    float off = -a / 2.0;
    float p = -a*a/4.0 + b;
    float delta = -4.0 * p;

    if (CHECK_ZERO(delta)) {
        roots.x = off;
        return 1;
    }

    if (delta < 0.0)
        return 0;

    float z = sqrt(delta) / 2.0;
    roots = vec2(-z, z) + off;
    return 2;
}

/* Quadratic: f(x)=ax²+bx+c */
int root_find2(inout vec2 roots, float a, float b, float c)
{
    return CHECK_ZERO(a) ? root_find1(roots.x, b, c)
                         : root_find2_monic(roots, b / a, c / a);
}

/* Cubic monic: f(x)=x³+ax²+bx+c */
int root_find3_monic(inout vec3 roots, float a, float b, float c)
{
    /* depressed: t³+pt+q with x=t-a/3 */
    float off = -a / 3.0;
    float p = b - a*a / 3.0;
    float q = a*a*a*2.0/27.0 - a*b/3.0 + c;
    float q2 = q / 2.0;
    float p3 = p / 3.0;
    float delta = q2*q2 + p3*p3*p3; // simplified discriminant

    if (CHECK_ZERO(p) && CHECK_ZERO(q)) {
        roots.x = off;
        return 1;
    }

    if (CHECK_ZERO(delta)) {
        float u = CBRT(q2);
        roots.xy = off + vec2(2.0 * u, -u);
        return 2;
    }

    if (delta > 0.0) {
        float z = sqrt(delta);
        float u = CBRT(-q2 + z);
        float v = CBRT(-q2 - z);
        roots.x = off + u + v;
        return 1;
    }

    /* see https://en.wikipedia.org/wiki/Cubic_equation#Trigonometric_and_hyperbolic_solutions */
    float u = 2.0 * sqrt(-p3);
    float v = acos(3.0*q / (2.0*p) * sqrt(-1.0 / p3)) / 3.0;
    roots = off + u * cos(v + vec3(0.0, 2.0*PI/3.0, 4.0*PI/3.0));
    return 3;
}

/* Cubic: f(x)=ax³+bx²+cx+d */
int root_find3(inout vec3 roots, float a, float b, float c, float d)
{
    return CHECK_ZERO(a) ? root_find2(roots.xy, b, c, d)
                         : root_find3_monic(roots, b / a, c / a, d / a);
}

/* Quartic monic: f(x)=x⁴+ax³+bx²+c */
int root_find4_monic(inout vec4 roots, float a, float b, float c, float d)
{
    /* depressed: t⁴+pt²+qt+r with x=t-a/4 */
    float off = -a / 4.0;
    float p = -3.0*a*a/8.0 + b;
    float q = a*a*a/8.0 - a*b/2.0 + c;
    float r = -3.0*a*a*a*a/256.0 + a*a*b/16.0 - a*c/4.0 + d;

    int nroot;

    if (CHECK_ZERO(r)) {
        roots.x = 0.0;
        nroot = root_find3_monic(roots.yzw, 0.0, p, q);
        roots += off;
        return nroot + 1;
    }

    root_find3_monic(roots.xyz, -p/2.0, -r, p*r/2.0 - q*q/8.0);

    /* a cubic monic will always cross the x axis at some point, so there
     * is always at least one root */
    float z = roots.x;
    float s = z*z - r;
    float t = 2.0*z - p;

    /* s and t are the same sign (because st=q²/4), so technically only one
     * if is necessary; both are kept for consistency. */
    if (s < 0.0 || t < 0.0)
        return 0;

    float u = sqrt(s);
    float v = sqrt(t);
    float sv = q < 0.0 ? -v : v;

    nroot = root_find2_monic(roots.xy, sv, z - u);
    if      (nroot == 0) nroot += root_find2_monic(roots.xy, -sv, z + u);
    else if (nroot == 1) nroot += root_find2_monic(roots.yz, -sv, z + u);
    else if (nroot == 2) nroot += root_find2_monic(roots.zw, -sv, z + u);

    roots += off;
    return nroot;
}

/* Quartic: f(x)=ax⁴+bx³+cx²+d */
int root_find4(inout vec4 roots, float a, float b, float c, float d, float e)
{
    return CHECK_ZERO(a) ? root_find3(roots.xyz, b, c, d, e)
                         : root_find4_monic(roots, b / a, c / a, d / a, e / a);
}

/*
 * Generated with:
 *     import math
 *     n = 5
 *     for k in range(n):
 *         angle = 2*math.pi/n
 *         off = math.pi/(2*n)
 *         z = angle * k + off
 *         c, s = math.cos(z), math.sin(z)
 *         print(f'#define K{k} vec2({c:18.15f}, {s:18.15f})')
 */

#define K0 vec2( 0.951056516295154,  0.309016994374947)
#define K1 vec2( 0.000000000000000,  1.000000000000000)
#define K2 vec2(-0.951056516295154,  0.309016994374948)
#define K3 vec2(-0.587785252292473, -0.809016994374947)
#define K4 vec2( 0.587785252292473, -0.809016994374948)

#define MAX_ITERATION 16

vec2 poly1(float a, float b, vec2 x)
{
    return a * x + vec2(b, 0.0);
}

vec2 poly2(float a, float b, float c, vec2 x)
{
    vec2 z = poly1(a, b, x);
    return cplx_mul(z, x) + vec2(c, 0.0);
}

vec2 poly3(float a, float b, float c, float d, vec2 x)
{
    vec2 z = poly2(a, b, c, x);
    return cplx_mul(z, x) + vec2(d, 0.0);
}

vec2 poly4(float a, float b, float c, float d, float e, vec2 x)
{
    vec2 z = poly3(a, b, c, d, x);
    return cplx_mul(z, x) + vec2(e, 0.0);
}

vec2 poly5(float a, float b, float c, float d, float e, float f, vec2 x)
{
    vec2 z = poly4(a, b, c, d, e, x);
    return cplx_mul(z, x) + vec2(f, 0.0);
}

float get_err_sq(vec2 a, vec2 b)
{
    vec2 z = a - b;
    return z.x*z.x + z.y*z.y;
}

vec2 update_value(vec2 z0, vec2 z1, vec2 z2, vec2 z3, vec2 z4,
                  float a, float b, float c, float d, float e, float f)
{
    vec2 d0 = z0 - z1;
    vec2 d1 = z0 - z2;
    vec2 d2 = z0 - z3;
    vec2 d3 = z0 - z4;
    vec2 inv_d0 = cplx_inv(d0);
    vec2 inv_d1 = cplx_inv(d1);
    vec2 inv_d2 = cplx_inv(d2);
    vec2 inv_d3 = cplx_inv(d3);
    vec2 sum = inv_d0 + inv_d1 + inv_d2 + inv_d3;
    vec2 p5 = poly5(a, b, c, d, e, f, z0);
    vec2 p4 = poly4(5.0 * a, 4.0 * b, 3.0 * c, 2.0 * d, e, z0);
    vec2 pod = cplx_div(p5, p4);
    vec2 den = vec2(1.0, 0.0) - cplx_mul(pod, sum);
    return z0 - cplx_div(pod, den);
}

/* https://en.wikipedia.org/wiki/Aberth_method */
int alberth_ehrlich_p5(inout float roots[5], float a, float b, float c, float d, float e, float f)
{
    float r = pow(abs(f / a), 0.2);
    vec2 prv0 = r * K0;
    vec2 prv1 = r * K1;
    vec2 prv2 = r * K2;
    vec2 prv3 = r * K3;
    vec2 prv4 = r * K4;

    for (int m = 1; m <= MAX_ITERATION; m++) {
        vec2 cur0 = update_value(prv0, prv1, prv2, prv3, prv4, a, b, c, d, e, f);
        vec2 cur1 = update_value(prv1, prv0, prv2, prv3, prv4, a, b, c, d, e, f);
        vec2 cur2 = update_value(prv2, prv0, prv1, prv3, prv4, a, b, c, d, e, f);
        vec2 cur3 = update_value(prv3, prv0, prv1, prv2, prv4, a, b, c, d, e, f);
        vec2 cur4 = update_value(prv4, prv0, prv1, prv2, prv3, a, b, c, d, e, f);
        float err = get_err_sq(cur0, prv0) \
                        + get_err_sq(cur1, prv1) \
                        + get_err_sq(cur2, prv2) \
                        + get_err_sq(cur3, prv3) \
                        + get_err_sq(cur4, prv4);
        if (err < 5.0*EPS*EPS)
            break;
        prv0 = cur0;
        prv1 = cur1;
        prv2 = cur2;
        prv3 = cur3;
        prv4 = cur4;
    }

    int nroot = 0;
    if (abs(prv0.y) <= EPS) roots[nroot++] = prv0.x;
    if (abs(prv1.y) <= EPS) roots[nroot++] = prv1.x;
    if (abs(prv2.y) <= EPS) roots[nroot++] = prv2.x;
    if (abs(prv3.y) <= EPS) roots[nroot++] = prv3.x;
    if (abs(prv4.y) <= EPS) roots[nroot++] = prv4.x;
    return nroot;
}

/* Quintic monic: f(x)=x⁵+ax⁴+bx³+cx²+dx+e */
int root_find5_monic(inout float roots[5], float a, float b, float c, float d, float e)
{
    return alberth_ehrlich_p5(roots, 1.0, a, b, c, d, e);
}

/* Quintic: f(x)=ax⁵+bx⁴+cx³+dx²+ex+f */
int root_find5(inout float roots[5], float a, float b, float c, float d, float e, float f)
{
    if (!CHECK_ZERO(a))
        return root_find5_monic(roots, b / a, c / a, d / a, e / a, f / a);

    vec4 roots4 = vec4(0.0);
    int nroot = root_find4(roots4, b, c, d, e, f);
    roots = float[5](roots4.x, roots4.y, roots4.z, roots4.w, 0.0);
    return nroot;
}

float poly4(float a, float b, float c, float d, float e, float t)
{
    return (((a * t + b) * t + c) * t + d) * t + e;
}

float poly3(float a, float b, float c, float d, float t)
{
    return ((a * t + b) * t + c) * t + d;
}

#if FAST
float normal_iter(vec2 a, vec2 b, vec2 c, vec2 dmp, float t)
{
    vec2 at = a * t;
    vec2 p1 = at + b;
    vec2 v = (p1 * t + c) * t + dmp;    // poly3(t)-p: ((at + b)t + c) + d - p
    vec2 tang = (p1 + p1 + at) * t + c; // tangent/derivative: (3at + 2b)t + c
    float l2_tang = dot(tang, tang);    // length² of the tangent
    return t - dot(tang, v) / l2_tang;
}

float get_dist(vec2 a, vec2 b, vec2 c, vec2 dmp, float t)
{
    t = normal_iter(a, b, c, dmp, t);
    t = normal_iter(a, b, c, dmp, t);
    t = normal_iter(a, b, c, dmp, t);
    t = clamp(t, 0.0, 1.0);
    vec2 v = ((a * t + b) * t + c) * t + dmp; // vector from poly(t) to p
    return dot(v, v);
}

float get_distance_sq_poly5(vec2 p, vec2 a, vec2 b, vec2 c, vec2 d)
{
    vec2 dmp = d - p;
    float min_dist = 1e38;
    min_dist = min(min_dist, get_dist(a, b, c, dmp, 0.0));
    min_dist = min(min_dist, get_dist(a, b, c, dmp, 0.5));
    min_dist = min(min_dist, get_dist(a, b, c, dmp, 1.0));
    return min_dist;
}

#else
// XXX: doesn't actually look needed for some reason: worse, it gliches some
// outbounds
#define DERIVATE4 0

float get_distance_sq_poly5(vec2 p, vec2 a, vec2 b, vec2 c, vec2 d)
{
    /*
     * Calculate coefficients for the derivate d'(t) (degree 5) of d(t)
     * where d(t) is the distance squared
     * See https://stackoverflow.com/questions/2742610/closest-point-on-a-cubic-bezier-curve/57315396#57315396
     */
    float dt_a =  6.0*(a.x*a.x + a.y*a.y);
    float dt_b = 10.0*(a.x*b.x + a.y*b.y);
    float dt_c =  4.0*(2.0*(a.x*c.x + a.y*c.y) + b.x*b.x + b.y*b.y);
    float dt_d =  6.0*(a.x*(d.x-p.x) + b.x*c.x + a.y*(d.y-p.y) + b.y*c.y);
    float dt_e =  2.0*(2.0*(b.x*d.x - b.x*p.x + b.y*d.y - b.y*p.y) + c.x*c.x + c.y*c.y);
    float dt_f =  2.0*(c.x*d.x - c.x*p.x + c.y*d.y - c.y*p.y);

    /*
     * Calculate the derivate d''(t) (degree 4)
     */
#if DERIVATE4
    float ddt_a = 5.0*dt_a;
    float ddt_b = 4.0*dt_b;
    float ddt_c = 3.0*dt_c;
    float ddt_d = 2.0*dt_d;
    float ddt_e =     dt_e;
#endif

    float roots5[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    int nb_roots = root_find5(roots5, dt_a, dt_b, dt_c, dt_d, dt_e, dt_f);

    float min_dist = 1e38;

    /* also include start and end points */
    float roots[7] = {0.0, 1.0, roots5[0], roots5[1], roots5[2], roots5[3], roots5[4]};
    for (int r = 0; r < nb_roots + 2; r++) {
        float t = roots[r];
        if (t < 0.0 || t > 1.0) /* ignore out of bounds roots */
            continue;

#if DERIVATE4
        // Check for d''(t)≥0
        if (poly4(ddt_a, ddt_b, ddt_c, ddt_d, ddt_e, t) < 0.0)
            continue;
#endif

        float xmp = p.x - poly3(a.x, b.x, c.x, d.x, t);
        float ymp = p.y - poly3(a.y, b.y, c.y, d.y, t);
        float dist = xmp*xmp + ymp*ymp;

        min_dist = min(min_dist, dist);
    }

    return min_dist;
}
#endif

int get_nb_intersect_poly5(vec2 p, vec2 a, vec2 b, vec2 c, vec2 d)
{
    /*
     * On the y dimension, find the intersection of the cubic polynomial and
     * the horizontal line passing by current point p. This is equivalent to
     * solving at³+bt²+ct+d=y.
     *
     * The we check these intersection on the 2nd dimension (x). Note that x
     * and y dimension could be swapped here.
     */
    vec3 roots = vec3(0.0);
    int n_roots = root_find3(roots, a.y, b.y, c.y, d.y - p.y);
    int nb_intersect = 0;
    for (int i = 0; i < n_roots; i++) {
        float t = roots[i];
        if (t >= 0.0 && t <= 1.0 && (((a.x * t + b.x) * t) + c.x) * t + d.x > p.x)
            nb_intersect++;
    }
    return nb_intersect;
}

#define linear_interp(a, b, x) (((x) - (a)) / ((b) - (a)))

/* pos is in [0;1] for both axis, with corner in the bottom-left */
vec4 draw_shape(vec2 pos, int shape_id)
{
    /* spread position on both size of both axis */
    pos = linear_interp(vec2(spread), vec2(1.0 - spread), pos);

    float min_dist = 1e38;

    // XXX: can we reduce the number of polynomial to evaluate for a given pixel?
    int start = poly_start[shape_id];
    int end   = poly_start[shape_id + 1];

    int nb_intersect = 0;
    for (int i = start; i < end; i++) {
        vec4 poly_x = poly_x_buf[i];
        vec4 poly_y = poly_y_buf[i];
        vec2 a = vec2(poly_x.x, poly_y.x);
        vec2 b = vec2(poly_x.y, poly_y.y);
        vec2 c = vec2(poly_x.z, poly_y.z);
        vec2 d = vec2(poly_x.w, poly_y.w);

        nb_intersect += get_nb_intersect_poly5(pos, a, b, c, d);
        float dist = get_distance_sq_poly5(pos, a, b, c, d);
        min_dist = min(min_dist, dist);
    }

    float dsign = nb_intersect % 2 == 0 ? 1.0 : -1.0;
    return vec4(vec3(dsign * sqrt(min_dist)), 1.0);
}

void main()
{
    vec2 grid_pos = var_uvcoord * vec2(grid);
    vec2 floor_pos = floor(grid_pos);
    ivec2 ipos = ivec2(floor_pos);
    int shape_id = ipos.y * grid.x + ipos.x;
    if (shape_id < nb_shapes)
        ngl_out_color = draw_shape(grid_pos - floor_pos, shape_id);
    else
        ngl_out_color = vec4(0.0);
}
