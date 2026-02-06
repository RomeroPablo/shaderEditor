#version 450

layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PushConstants {
    vec2  resolution;
    float time;
    float _pad;
} pc;

void main() {
    vec2 c = gl_FragCoord.xy / pc.resolution;
    fragColor = vec4(c, abs(sin(pc.time)),1.0);
}
