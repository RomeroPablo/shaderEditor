#version 450

layout(location = 0) out vec4 outColor;
void main(){
    vec2 c = gl_FragCoord.xy / vec2(640.0, 480.0);
    outColor = vec4(0.0, c, 1.0);
}
