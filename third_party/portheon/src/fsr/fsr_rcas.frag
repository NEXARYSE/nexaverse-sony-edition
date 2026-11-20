#version 450
// PORTHEON 5.0: FSR 1 RCAS (robust contrast-adaptive sharpening), as AMD FidelityFX FSR 1 does it (MIT licence, see
// licenses\FSR1.txt): a 5-tap cross, the sharpening lobe limited so no pixel goes past its neighbours' range.
// sharp: 0 = the strongest, 1 = half as strong, 2 = a quarter (stops, as FSR's own setting).
layout(set = 0, binding = 0) uniform sampler2D src;
layout(push_constant) uniform P { vec2 inSize; vec2 outSize; float sharp; float pad; } p;
layout(location = 0) out vec4 outColor;

vec3 at(ivec2 q) { return texelFetch(src, clamp(q, ivec2(0), ivec2(p.inSize) - 1), 0).rgb; }
void main()
{
    ivec2 ip = ivec2(gl_FragCoord.xy);
    vec3 b = at(ip + ivec2(0, -1)), d = at(ip + ivec2(-1, 0)), e = at(ip), f = at(ip + ivec2(1, 0)), h = at(ip + ivec2(0, 1));
    vec3 mn4 = min(min(b, d), min(f, h)), mx4 = max(max(b, d), max(f, h));
    vec2 peakC = vec2(1.0, -4.0);
    vec3 hitMin = mn4 / max(4.0 * mx4, vec3(1e-5));
    vec3 hitMax = (peakC.x - mx4) / min(4.0 * mn4 + peakC.y, vec3(-1e-5));
    vec3 lobeRGB = max(-hitMin, hitMax);
    float lobe = max(-0.1875, min(max(lobeRGB.r, max(lobeRGB.g, lobeRGB.b)), 0.0)) * exp2(-p.sharp);
    float rcpL = 1.0 / (4.0 * lobe + 1.0);
    vec3 c = (lobe * (b + d + h + f) + e) * rcpL;
    outColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
