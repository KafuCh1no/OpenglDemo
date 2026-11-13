#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec3 ObjectColor;
in vec2 TexCoords;      // 现在还没贴图，等 CTexture 做完再用
in vec4 vLightSpacePos;

#define MAX_LIGHTS 8

// ---- 多光源（数组形式，CPU 端一次填满） ----
uniform int   lightCount;
uniform vec3  lightPos[MAX_LIGHTS];
uniform vec3  lightDirArr[MAX_LIGHTS];
uniform vec3  lightColorArr[MAX_LIGHTS];
uniform float lightIntensity[MAX_LIGHTS];
uniform float lightCutoff[MAX_LIGHTS];       // 内锥 cos 值
uniform float lightOuterCutoff[MAX_LIGHTS];  // 外锥 cos 值
uniform int   lightType[MAX_LIGHTS];         // 0 = 点光, 1 = 聚光

uniform sampler2D shadowMap;   // 只对手电筒（0 号灯）生效
uniform vec3 viewPos;

float ShadowCalculation(vec4 fragPosLightSpace);   // 前向声明：定义在文件末尾

void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - WorldPos);         // 片元 -> 眼睛

    // 阴影只压 0 号灯（手电筒），其他灯照常照亮
    float shadow = ShadowCalculation(vLightSpacePos);

    vec3 result = 0.03 * ObjectColor;               // 极弱环境光，营造黑暗

    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        if (i >= lightCount) break;                 // 部分老驱动要求循环上界是常量

        vec3  toLight = lightPos[i] - WorldPos;
        float dist    = length(toLight);
        vec3  L       = toLight / max(dist, 1e-4);

        // 距离衰减（二次）
        float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

        // 聚光灯锥形衰减；点光直接为 1
        float spot = 1.0;
        if (lightType[i] == 1)
        {
            float theta = dot(L, normalize(-lightDirArr[i]));
            float eps   = max(lightCutoff[i] - lightOuterCutoff[i], 1e-4);
            spot = clamp((theta - lightOuterCutoff[i]) / eps, 0.0, 1.0);
        }

        // Blinn-Phong
        float diff = max(dot(N, L), 0.0);
        vec3  H    = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), 32.0);

        vec3 radiance = lightColorArr[i] * lightIntensity[i] * attenuation * spot;

        // 只有手电筒会被遮挡
        float occl = (i == 0) ? (1.0 - shadow) : 1.0;

        result += occl * (diff * ObjectColor + 0.3 * spec) * radiance;
    }

    // 距离雾：按到眼睛的距离，远处融进黑暗
    float fog = 1.0 - exp(-0.045 * length(viewPos - WorldPos));
    result = mix(result, vec3(0.0), fog);

    FragColor = vec4(result, 1.0);
}

float ShadowCalculation(vec4 fragPosLightSpace)
{
    // 透视除法
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // 映射到纹理坐标 [0, 1]
    proj = proj * 0.5 + 0.5;

    // 光源外当作不在阴影
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
    {
        return 0.0;
    }

    // 贴图记录的「这个方向最近的遮挡距离」 vs 当前片元自己的深度
    float closestDepth = texture(shadowMap, proj.xy).r;
    float currentDepth = proj.z;

    // 比记录的远 = 前面有东西挡着 = 在阴影里
    return currentDepth - 0.005 > closestDepth ? 1.0 : 0.0;
}
