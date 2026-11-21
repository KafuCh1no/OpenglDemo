#version 330 core
out vec4 FragColor;

in vec2 vUV;

// 主场景深度
uniform sampler2D gDepth;
// 平行光的阴影图 + light space 矩阵
uniform sampler2D shadowMap;
uniform mat4      lightSpace;

uniform mat4 invViewProj;
uniform vec3 cameraPos;

// 平行光
uniform vec3  lightDir;
uniform vec3  lightColor;
uniform float lightIntensity;

// 雾参数
uniform float densityBase;     // 基础密度
uniform float heightFalloff;   // 高度衰减速度
uniform float fogBaseHeight;   // 从这个高度开始衰减
uniform float maxDistance;     // 最远步进距离
uniform float phaseG;          // 相函数 g

const int   STEPS = 32;
const float PI    = 3.14159265;

// Henyey-Greenstein 相函数：描述光朝观察方向散射多少
// g越小扩散越严重
float HG(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

// 交错梯度噪声（IGN）：逐像素抖动，配合模糊 Pass 消除条带
float ign(vec2 fragCoord)
{
    return fract(52.9829189 * fract(dot(fragCoord, vec2(0.06711056, 0.00583715))));
}

// 4 抽样 PCF：单点采样会让步进跨越阴影边界时出现椒盐噪点
float shadowFactor(vec3 proj, vec2 texel)
{
    const float bias = 0.0008;
    vec2 o = texel * 0.75;
    float lit = 0.0;
    lit += (proj.z - bias > texture(shadowMap, proj.xy + vec2(-o.x, -o.y)).r) ? 0.0 : 1.0;
    lit += (proj.z - bias > texture(shadowMap, proj.xy + vec2( o.x, -o.y)).r) ? 0.0 : 1.0;
    lit += (proj.z - bias > texture(shadowMap, proj.xy + vec2(-o.x,  o.y)).r) ? 0.0 : 1.0;
    lit += (proj.z - bias > texture(shadowMap, proj.xy + vec2( o.x,  o.y)).r) ? 0.0 : 1.0;
    return lit * 0.25;
}

void main()
{
    // 用深度还原世界坐标
    float depth = texture(gDepth, vUV).r;
    vec4  ndc   = vec4(vUV * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4  wp    = invViewProj * ndc;
    vec3  world = wp.xyz / wp.w;

    // 起点是相机
    vec3  rayDir = normalize(world - cameraPos);
    float rayLen = min(length(world - cameraPos), maxDistance);

    // 平行光：无衰减，光照范围完全由阴影图决定
    vec3  L        = normalize(-lightDir);
    vec3  radiance = lightColor * lightIntensity;
    float phase    = HG(dot(rayDir, L), phaseG);

    // 逐像素抖动把步进条带打散成颗粒，之后由 CVolumetricFog 的模糊 Pass 磨平
    float jitter = ign(gl_FragCoord.xy);
    vec2  texel  = 1.0 / vec2(textureSize(shadowMap, 0));

    vec3  scattered = vec3(0.0);
    // 沿路径累积的“密度 × 距离”
    float optical   = 0.0;

    for (int i = 0; i < STEPS; ++i)
    {
        // 近处采样密、远处稀疏
        float u  = (float(i) + jitter) / float(STEPS);
        float t  = rayLen * u * u;
        float dt = rayLen * 2.0 * u / float(STEPS);

        vec3 p = cameraPos + rayDir * t;

        // 密度 = 基础值 × 高度衰减（贴地更浓）
        float density = densityBase * exp(-max(p.y - fogBaseHeight, 0.0) * heightFalloff);

        // 查询遮挡
        vec4 lp   = lightSpace * vec4(p, 1.0);
        vec3 proj = lp.xyz / lp.w * 0.5 + 0.5;

        float lit = 1.0;
        if (proj.x >= 0.0 && proj.x <= 1.0 &&
            proj.y >= 0.0 && proj.y <= 1.0 && proj.z <= 1.0)
        {
            lit = shadowFactor(proj, texel);
        }
        // 无论有没有光，雾都在吸收
        float sampleOptical = density * dt;
        float tr            = exp(-optical);      // 走到这里之前还剩多少光
        optical += sampleOptical;

        // 只有被照亮的雾才散射出可见的光
        if (lit > 0.0)
            scattered += radiance * phase * tr * sampleOptical;
    }

    // rgb = 散射光，a = 透射率（雾不吃光时是 1）
    FragColor = vec4(scattered, exp(-optical));
}
