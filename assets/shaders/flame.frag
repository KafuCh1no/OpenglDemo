#version 330 core

in vec2  vUV;
in float vH;

out vec4 FragColor;

uniform float uIntensity;

void main()
{
    // 颜色
    vec3 hot  = vec3(1.00, 0.95, 0.80);
    vec3 warm = vec3(1.00, 0.45, 0.10);
    vec3 tip  = vec3(0.45, 0.05, 0.01);

    vec3 col = mix(hot,  warm, smoothstep(0.00, 0.45, vH));
    col      = mix(col,  tip,  smoothstep(0.55, 1.00, vH));

    // 顶部淡出，形成火苗的尖端
    float fade = 1.0 - smoothstep(0.55, 1.00, vH);

    // 根部收缩
    fade *= smoothstep(0.0, 0.06, vH);

    float a = fade * uIntensity;

    // 加法混合：rgb * alpha 会被加到画面上
    FragColor = vec4(col, a);
}
