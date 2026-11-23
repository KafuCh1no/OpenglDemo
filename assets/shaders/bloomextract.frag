#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uHdr;
uniform float     uThreshold;

// true  = 只从自发光物体（火焰）提取：HDR 的 alpha 由火焰用加法混合抬起
// false = 老行为，全屏按亮度阈值截取（手电筒照白墙也会有光晕）
uniform bool      uOnlyEmissive;

void main()
{
    vec4 s = texture(uHdr, vUV);
    vec3 c = s.rgb;

    float w;
    if (uOnlyEmissive)
    {
        // 自发光模式：遮罩本身就是权重。
        // 不透明几何的 alpha 是 0，只有火焰用加法混合把它抬起来，
        // 所以这里不需要再看亮度阈值——火焰在 HDR 里只有 1~2，
        // 按阈值 10 截的话什么都截不到。
        w = clamp(s.a, 0.0, 1.0);
    }
    else
    {
        // 老行为：感知亮度 + 软阈值，阈值附近平滑过渡避免光晕硬边
        float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
        w = smoothstep(uThreshold, uThreshold + 0.5, lum);
    }

    FragColor = vec4(c * w, 1.0);
}
