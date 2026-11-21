#version 330 core
out vec4 FragColor;

in vec2 vUV;

uniform sampler2D uSource;
uniform vec2      uTexel;   // 1.0 / 纹理尺寸
uniform vec2      uDir;     // (1,0) 水平，(0,1) 垂直

// 9 抽样高斯（5 权重对称展开），rgb 和 alpha（透射率）一起模糊
void main()
{
    const float w[5] = float[5](0.227027, 0.1945946, 0.1216216,
                                0.054054, 0.016216);

    vec2 offs = uTexel * uDir;
    vec4 sum  = texture(uSource, vUV) * w[0];

    for (int i = 1; i < 5; ++i)
    {
        vec2 o = offs * float(i);
        sum += texture(uSource, vUV + o) * w[i];
        sum += texture(uSource, vUV - o) * w[i];
    }

    FragColor = sum;
}
