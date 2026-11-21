#version 330 core

in vec2 vUV;
out vec4 FragColor;

// FXAA：在已经色调映射、gamma 编码之后的 LDR 图像上做抗锯齿
// 只读颜色，不需要深度/法线，属于纯后处理
uniform sampler2D uSource;
uniform float uTexelX;
uniform float uTexelY;
uniform bool  uEnabled;    // false = 直接复制，用来对比开关效果

const vec3 LUMA = vec3(0.299, 0.587, 0.114);

void main()
{
    vec3 rgbM = texture(uSource, vUV).rgb;

    if (!uEnabled)
    {
        FragColor = vec4(rgbM, 1.0);
        return;
    }

    vec2 texel = vec2(uTexelX, uTexelY);

    // 中心 + 3x3 邻域
    vec3 rgbNW = texture(uSource, vUV + vec2(-1.0, -1.0) * texel).rgb;
    vec3 rgbNE = texture(uSource, vUV + vec2( 1.0, -1.0) * texel).rgb;
    vec3 rgbSW = texture(uSource, vUV + vec2(-1.0,  1.0) * texel).rgb;
    vec3 rgbSE = texture(uSource, vUV + vec2( 1.0,  1.0) * texel).rgb;

    // 亮度：人眼对绿色最敏感，用感知权重而不是简单平均
    float lumaM  = dot(rgbM,  LUMA);
    float lumaNW = dot(rgbNW, LUMA);
    float lumaNE = dot(rgbNE, LUMA);
    float lumaSW = dot(rgbSW, LUMA);
    float lumaSE = dot(rgbSE, LUMA);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    // 对比度低于阈值：平坦区域，直接输出，省采样也避免把细节糊掉
    if ((lumaMax - lumaMin) < max(0.0312, lumaMax * 0.125))
    {
        FragColor = vec4(rgbM, 1.0);
        return;
    }

    // 边缘方向：对角的亮度差决定往哪个方向搜
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    // 方向归一化，最多搜索 8 个像素，避免远处乱采
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 / 8.0), 1.0 / 128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * texel;

    // 沿边缘方向一边采一半，相当于求边缘两侧的平均色
    vec3 rgbA = 0.5 * (
        texture(uSource, vUV + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(uSource, vUV + dir * (2.0 / 3.0 - 0.5)).rgb);

    // 再往外扩一点，用 3 个样本的结果压掉残余抖动
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(uSource, vUV + dir * -0.5).rgb +
        texture(uSource, vUV + dir *  0.5).rgb);

    float lumaB = dot(rgbB, LUMA);

    // B 的亮度跑出邻域范围，说明抹过头了，退回 A
    FragColor = vec4(((lumaB < lumaMin) || (lumaB > lumaMax)) ? rgbA : rgbB, 1.0);
}
