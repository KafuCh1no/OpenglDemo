#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D gAO;
uniform vec2 uTexelSize;   // 1 / 分辨率
uniform vec2 uDirection;   // (1,0) = 水平，(0,1) = 垂直

void main()
{
    // 9-tap 可分离高斯：跑两趟后等价于 9x9，但只需 18 次采样。
    // 单趟 5x5 盒式压不住 4x4 噪声纹理的图案，会留下网格状的麻点。
    const float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

    float sum = texture(gAO, vUV).r * w[0];
    for (int i = 1; i < 5; ++i)
    {
        vec2 off = uDirection * uTexelSize * float(i);
        sum += texture(gAO, vUV + off).r * w[i];
        sum += texture(gAO, vUV - off).r * w[i];
    }

    FragColor = vec4(vec3(sum), 1.0);
}
