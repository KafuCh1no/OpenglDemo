#version 330 core
out vec4 FragColor;

in vec2 vUV;

// HDR 缓冲：线性空间的亮度，允许 > 1.0
uniform sampler2D hdrBuffer;

uniform float exposure;
uniform bool  uToneMapping;

// 体积雾：rgb = 散射光，a = 透射率
uniform sampler2D fogBuffer;
uniform bool      uHasFog;

// Bloom：亮部模糊后的纹理
uniform sampler2D bloomBuffer;
uniform bool      uHasBloom;
uniform float     uBloomStrength;

// SSAO：屏幕空间遮蔽（1 = 无遮蔽，0 = 全遮蔽）
uniform sampler2D aoBuffer;
uniform bool      uHasAO;
uniform float     uAOStrength;

vec3 ACES(vec3 x);

void main()
{
    vec3 hdr = texture(hdrBuffer, vUV).rgb;

    // 体积雾合成：先按透射率衰减，再叠加散射光
    // 必须在线性空间、色调映射之前做
    // SSAO：只压暗环境光那一部分，直接乘在场景颜色上（近似）
    if (uHasAO)
    {
        float ao = texture(aoBuffer, vUV).r;
        hdr *= mix(1.0, ao, uAOStrength);
    }

    if (uHasFog)
    {
        vec4 fog = texture(fogBuffer, vUV);
        hdr = hdr * fog.a + fog.rgb;
    }

    // Bloom：在色调映射之前叠加，超亮部分会被 ACES 一起压回去
    if (uHasBloom)
        hdr += texture(bloomBuffer, vUV).rgb * uBloomStrength;

    vec3 color = hdr;
    if (uToneMapping)
    {
        vec3 x = hdr * exposure;
        color = ACES(x);
    }

    // 线性 -> sRGB 编码，gamma矫正
    FragColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}

vec3 ACES(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
