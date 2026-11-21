#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uSource;

// 步长（UV 空间）
uniform float uDirX;
uniform float uDirY;

// 9 抽头高斯权重（σ ≈ 2）
const float kWeights[5] = float[5](
    0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216
);

void main()
{
    vec3  sum = texture(uSource, vUV).rgb * kWeights[0];
    float wsum = kWeights[0];

    for (int i = 1; i < 5; ++i)
    {
        vec2 off = vec2(uDirX, uDirY) * float(i);

        sum += texture(uSource, vUV + off).rgb * kWeights[i];
        sum += texture(uSource, vUV - off).rgb * kWeights[i];
        wsum += kWeights[i] * 2.0;
    }

    FragColor = vec4(sum / wsum, 1.0);
}
