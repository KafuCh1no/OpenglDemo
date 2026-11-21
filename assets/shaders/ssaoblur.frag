#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D gAO;
uniform float uTexelX;
uniform float uTexelY;

void main()
{
    // 5x5 盒式模糊：SSAO 的噪点很细，模糊一次就足够平滑
    vec2 texel = vec2(uTexelX, uTexelY);

    float sum = 0.0;
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            sum += texture(gAO, vUV + vec2(x, y) * texel).r;
        }
    }

    FragColor = vec4(vec3(sum / 25.0), 1.0);
}
