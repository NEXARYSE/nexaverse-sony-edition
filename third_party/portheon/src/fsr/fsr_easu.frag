#version 450
// PORTHEON 5.0: FSR 1 EASU (edge-adaptive spatial upsampling) over the game's smaller frame, as AMD FidelityFX FSR 1
// does it (MIT licence, see licenses\FSR1.txt): 12 taps around the output pixel, a direction and a length from the
// luma of the 4 nearest texels, a Lanczos-like lobe stretched along the edge, clamped to the 2x2 neighbours.
layout(set = 0, binding = 0) uniform sampler2D src;
layout(push_constant) uniform P { vec2 inSize; vec2 outSize; float sharp; float pad; } p;
layout(location = 0) out vec4 outColor;

void setF(inout vec2 dir, inout float len, vec2 pp, bool biS, bool biT, bool biU, bool biV, float lA, float lB, float lC, float lD, float lE)
{
    float w = 0.0;
    if (biS) w = (1.0 - pp.x) * (1.0 - pp.y);
    if (biT) w = pp.x * (1.0 - pp.y);
    if (biU) w = (1.0 - pp.x) * pp.y;
    if (biV) w = pp.x * pp.y;
    float dc = lD - lC, cb = lC - lB;
    float lenX = max(abs(dc), abs(cb)); lenX = 1.0 / max(lenX, 1e-8);
    float dirX = lD - lB; dir.x += dirX * w;
    lenX = clamp(abs(dirX) * lenX, 0.0, 1.0); lenX *= lenX; len += lenX * w;
    float ec = lE - lC, ca = lC - lA;
    float lenY = max(abs(ec), abs(ca)); lenY = 1.0 / max(lenY, 1e-8);
    float dirY = lE - lA; dir.y += dirY * w;
    lenY = clamp(abs(dirY) * lenY, 0.0, 1.0); lenY *= lenY; len += lenY * w;
}
void tap(inout vec3 aC, inout float aW, vec2 off, vec2 dir, vec2 len2, float lob, float clp, vec3 c)
{
    vec2 v = vec2(off.x * dir.x + off.y * dir.y, off.x * (-dir.y) + off.y * dir.x) * len2;
    float d2 = min(v.x * v.x + v.y * v.y, clp);
    float wB = 0.4 * d2 - 1.0, wA = lob * d2 - 1.0;
    wB *= wB; wA *= wA;
    wB = 1.5625 * wB - 0.5625;
    float w = wB * wA;
    aC += c * w; aW += w;
}
void main()
{
    vec2 ip = floor(gl_FragCoord.xy);
    vec2 scale = p.inSize / p.outSize, rcpIn = 1.0 / p.inSize;
    vec2 pp = ip * scale + (0.5 * scale - 0.5);
    vec2 fp = floor(pp); pp -= fp;
    vec2 p0 = fp * rcpIn + vec2(rcpIn.x, -rcpIn.y);
    vec2 p1 = p0 + vec2(-rcpIn.x, 2.0 * rcpIn.y);
    vec2 p2 = p0 + vec2(rcpIn.x, 2.0 * rcpIn.y);
    vec2 p3 = p0 + vec2(0.0, 4.0 * rcpIn.y);
    vec4 bczzR = textureGather(src, p0, 0), bczzG = textureGather(src, p0, 1), bczzB = textureGather(src, p0, 2);
    vec4 ijfeR = textureGather(src, p1, 0), ijfeG = textureGather(src, p1, 1), ijfeB = textureGather(src, p1, 2);
    vec4 klhgR = textureGather(src, p2, 0), klhgG = textureGather(src, p2, 1), klhgB = textureGather(src, p2, 2);
    vec4 zzonR = textureGather(src, p3, 0), zzonG = textureGather(src, p3, 1), zzonB = textureGather(src, p3, 2);
    vec4 bczzL = bczzB * 0.5 + (bczzR * 0.5 + bczzG);
    vec4 ijfeL = ijfeB * 0.5 + (ijfeR * 0.5 + ijfeG);
    vec4 klhgL = klhgB * 0.5 + (klhgR * 0.5 + klhgG);
    vec4 zzonL = zzonB * 0.5 + (zzonR * 0.5 + zzonG);
    float bL = bczzL.x, cL = bczzL.y, iL = ijfeL.x, jL = ijfeL.y, fL = ijfeL.z, eL = ijfeL.w;
    float kL = klhgL.x, lL = klhgL.y, hL = klhgL.z, gL = klhgL.w, oL = zzonL.z, nL = zzonL.w;
    vec2 dir = vec2(0.0); float len = 0.0;
    setF(dir, len, pp, true, false, false, false, bL, eL, fL, gL, jL);
    setF(dir, len, pp, false, true, false, false, cL, fL, gL, hL, kL);
    setF(dir, len, pp, false, false, true, false, fL, iL, jL, kL, nL);
    setF(dir, len, pp, false, false, false, true, gL, jL, kL, lL, oL);
    vec2 dir2 = dir * dir; float dirR = dir2.x + dir2.y;
    bool zro = dirR < (1.0 / 32768.0);
    dirR = inversesqrt(max(dirR, 1e-12)); dirR = zro ? 1.0 : dirR; dir.x = zro ? 1.0 : dir.x; dir *= vec2(dirR);
    len = len * 0.5; len *= len;
    float stretch = (dir.x * dir.x + dir.y * dir.y) / max(max(abs(dir.x), abs(dir.y)), 1e-8);
    vec2 len2 = vec2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);
    float lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;
    float clp = 1.0 / lob;
    vec3 cf = vec3(ijfeR.z, ijfeG.z, ijfeB.z), cg = vec3(klhgR.w, klhgG.w, klhgB.w), cj = vec3(ijfeR.y, ijfeG.y, ijfeB.y), ck = vec3(klhgR.x, klhgG.x, klhgB.x);
    vec3 mn4 = min(min(cf, cg), min(cj, ck)), mx4 = max(max(cf, cg), max(cj, ck));
    vec3 aC = vec3(0.0); float aW = 0.0;
    tap(aC, aW, vec2( 0.0, -1.0) - pp, dir, len2, lob, clp, vec3(bczzR.x, bczzG.x, bczzB.x));   // b
    tap(aC, aW, vec2( 1.0, -1.0) - pp, dir, len2, lob, clp, vec3(bczzR.y, bczzG.y, bczzB.y));   // c
    tap(aC, aW, vec2(-1.0,  1.0) - pp, dir, len2, lob, clp, vec3(ijfeR.x, ijfeG.x, ijfeB.x));   // i
    tap(aC, aW, vec2( 0.0,  1.0) - pp, dir, len2, lob, clp, cj);                                // j
    tap(aC, aW, vec2( 0.0,  0.0) - pp, dir, len2, lob, clp, cf);                                // f
    tap(aC, aW, vec2(-1.0,  0.0) - pp, dir, len2, lob, clp, vec3(ijfeR.w, ijfeG.w, ijfeB.w));   // e
    tap(aC, aW, vec2( 1.0,  1.0) - pp, dir, len2, lob, clp, ck);                                // k
    tap(aC, aW, vec2( 2.0,  1.0) - pp, dir, len2, lob, clp, vec3(klhgR.y, klhgG.y, klhgB.y));   // l
    tap(aC, aW, vec2( 2.0,  0.0) - pp, dir, len2, lob, clp, vec3(klhgR.z, klhgG.z, klhgB.z));   // h
    tap(aC, aW, vec2( 1.0,  0.0) - pp, dir, len2, lob, clp, cg);                                // g
    tap(aC, aW, vec2( 1.0,  2.0) - pp, dir, len2, lob, clp, vec3(zzonR.z, zzonG.z, zzonB.z));   // o
    tap(aC, aW, vec2( 0.0,  2.0) - pp, dir, len2, lob, clp, vec3(zzonR.w, zzonG.w, zzonB.w));   // n
    vec3 c = min(mx4, max(mn4, aC / max(aW, 1e-8)));
    outColor = vec4(c, 1.0);
}
