#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uHdr;
uniform float     uThreshold;

void main()
{
    vec3 c = texture(uHdr, vUV).rgb;

    // 感知亮度
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));

    // 软阈值：阈值附近平滑过渡，避免光晕出现硬边
    float w = smoothstep(uThreshold, uThreshold + 0.5, lum);

    FragColor = vec4(c * w, 1.0);
}
