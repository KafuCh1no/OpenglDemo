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

// 平行光（月光）：只有方向，没有位置和距离衰减
uniform vec3  lightDir;
uniform vec3  lightColor;
uniform float lightIntensity;

// 雾参数
uniform float densityBase;     // 基础密度
uniform float heightFalloff;   // 高度衰减速度
uniform float fogBaseHeight;   // 从这个高度开始衰减
uniform float maxDistance;     // 最远步进距离
uniform float phaseG;          // 相函数 g

const int   STEPS = 24;
const float PI    = 3.14159265;

// Henyey-Greenstein 相函数：描述光朝观察方向散射多少
// g越小扩散越严重
float HG(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

// 4x4 Bayer 抖动，打散步进产生的同心圆条带
float bayer4(vec2 fragCoord)
{
    int x = int(mod(fragCoord.x, 4.0));
    int y = int(mod(fragCoord.y, 4.0));
    const float m[16] = float[16](
         0.0,  8.0,  2.0, 10.0,
        12.0,  4.0, 14.0,  6.0,
         3.0, 11.0,  1.0,  9.0,
        15.0,  7.0, 13.0,  5.0);
    return (m[y * 4 + x] + 0.5) / 16.0;
}

void main()
{
    // 用深度还原世界坐标：这就是"终点"，不求交
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

    float jitter = bayer4(gl_FragCoord.xy);

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
            float closest = texture(shadowMap, proj.xy).r;
            lit = (proj.z - 0.0005 > closest) ? 0.0 : 1.0;
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
