#version 330 core
out vec4 FragColor;

in vec2 vUV;

// HDR 缓冲：线性空间的亮度，允许 > 1.0
uniform sampler2D hdrBuffer;

uniform float exposure;
// false = 直通（画面与改造前一致）；true = Reinhard 压缩
uniform bool  uToneMapping;

void main()
{
    vec3 hdr = texture(hdrBuffer, vUV).rgb;

    vec3 color = hdr;
    if (uToneMapping)
    {
        // Reinhard：x / (1 + x)，把 [0, inf) 单调压进 [0, 1)
        // 单调很关键 —— 亮度顺序不变，只是把超出 1.0 的部分拉开层次
        vec3 x = hdr * exposure;
        color = x / (x + vec3(1.0));
    }

    // 线性 -> sRGB 编码，gamma矫正
    FragColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
