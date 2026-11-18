#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec3 ObjectColor;
in vec2 TexCoords;
in vec2 vWorldUV;        // 世界空间 UV（单位：米）
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

uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;
uniform sampler2D shadowMap2;
uniform sampler2D shadowMap3;       // 平行光
uniform mat4 lightSpaceMatrices[MAX_LIGHTS];

// 点光源阴影：cubemap 里存的是「到光源的线性距离 / farPlane」
uniform samplerCube shadowCube0;
uniform samplerCube shadowCube1;
uniform samplerCube shadowCube2;
uniform samplerCube shadowCube3;
uniform float pointFarPlane[MAX_LIGHTS];

uniform vec3 viewPos;

// 材质
uniform sampler2D albedoMap;
uniform bool      uHasTexture;
uniform float     uScale;      // UV 平铺密度
uniform vec3      tint;        // 颜色倾向
uniform sampler2D normalMap;
uniform bool uHasNormalMap;
uniform float uNormalStrength;

// true = 世界空间 UV（墙面），false = 模型自带 UV
uniform bool uUseWorldUV;

float ShadowCalculation(vec4 fragPosLightSpace, sampler2D shadowMap);   // 前向声明：定义在文件末尾
float PointShadowCalculation(vec3 fragPos, vec3 lightPos, samplerCube cube, float farPlane);

// 创建把“表面的局部坐标系”转成“世界坐标系”的矩阵
mat3 BuildTBN(vec3 N, vec3 worldPos, vec2 uv)
{
    vec3 dp1 = dFdx(worldPos);
    vec3 dp2 = dFdy(worldPos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);

    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    return mat3(T * invmax, B * invmax, N);
}

void main()
{
    // 墙面：世界 UV（uScale = 每米重复几次）；模型：自带 UV（uScale 一般设 1）
    vec2 uv = uUseWorldUV ? (vWorldUV * uScale) : (TexCoords * uScale);

    // 法线
    vec3 N = normalize(Normal);
    // 片元 -> 眼睛
    vec3 V = normalize(viewPos - WorldPos);

    // 有法向贴图就换掉N
    if (uHasNormalMap)
    {
        // 把（0， 1）向量解码成方向向量（-1， 1）
        vec3 tn = texture(normalMap, uv).rgb * 2.0 - 1.0;
        // 调节强度
        tn.xy *= uNormalStrength;
        // 切向方向转换到世界方向
        N = normalize(BuildTBN(N, WorldPos, uv) * tn);
    }

    // 材质 albedo：顶点色 × tint，有贴图再乘贴图
    vec3 albedo = ObjectColor * tint;
    if (uHasTexture)
        albedo *= texture(albedoMap, uv).rgb;

    // 极弱环境光，营造黑暗
    vec3 result = 0.03 * albedo;

    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        if (i >= lightCount) break;

        vec3  L;
        float attenuation = 1.0;
        float spot        = 1.0;

        if (lightType[i] == 2)
        {
            // 平行光：只有方向，不使用 position，也不随距离衰减
            L = normalize(-lightDirArr[i]);
        }
        else
        {
            vec3  toLight = lightPos[i] - WorldPos;
            float dist    = length(toLight);
            // 片元 -> 光源
            L = toLight / max(dist, 1e-4);

            // 距离衰减（二次）
            attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

            // 聚光灯锥形衰减；点光直接为 1
            if (lightType[i] == 1)
            {
                float theta = dot(L, normalize(-lightDirArr[i]));
                float eps   = max(lightCutoff[i] - lightOuterCutoff[i], 1e-4);
                spot = clamp((theta - lightOuterCutoff[i]) / eps, 0.0, 1.0);
            }
        }

        // Blinn-Phong
        // 漫反射
        float diff = max(dot(N, L), 0.0);
        // 半程向量
        vec3  H    = normalize(L + V);
        // 镜面反射 即高光
        float spec = pow(max(dot(N, H), 0.0), 32.0);

        vec3 radiance = lightColorArr[i] * lightIntensity[i] * attenuation * spot;

        // 遮挡系数
        float occl = 1.0;
        if (lightType[i] == 1 || lightType[i] == 2)
        {
            // 聚光灯：2D 深度图
            vec4 lp = lightSpaceMatrices[i] * vec4(WorldPos, 1.0);
            float shadow = 0.0;
            if      (i == 0) shadow = ShadowCalculation(lp, shadowMap0);
            else if (i == 1) shadow = ShadowCalculation(lp, shadowMap1);
            else if (i == 2) shadow = ShadowCalculation(lp, shadowMap2);
            else if (i == 3) shadow = ShadowCalculation(lp, shadowMap3);
            occl = 1.0 - shadow;
        }
        else if (lightType[i] == 0)
        {
            // 点光源：cubemap，按到光源的距离比较
            float shadow = 0.0;
            if      (i == 0) shadow = PointShadowCalculation(WorldPos, lightPos[0], shadowCube0, pointFarPlane[0]);
            else if (i == 1) shadow = PointShadowCalculation(WorldPos, lightPos[1], shadowCube1, pointFarPlane[1]);
            else if (i == 2) shadow = PointShadowCalculation(WorldPos, lightPos[2], shadowCube2, pointFarPlane[2]);
            occl = 1.0 - shadow;
        }

        result += occl * (diff * albedo + 0.3 * spec) * radiance;
    }

    // 距离雾：按到眼睛的距离，远处融进黑暗
    float fog = 1.0 - exp(-0.045 * length(viewPos - WorldPos));
    result = mix(result, vec3(0.0), fog);

    // 线性 -> sRGB 编码（配合纹理的 GL_SRGB8_ALPHA8 解码）
    FragColor = vec4(pow(result, vec3(1.0 / 2.2)), 1.0);
}

// 阴影计算
float ShadowCalculation(vec4 fragPosLightSpace, sampler2D shadowMap)
{
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;

    // outside the light frustum -> treat as lit
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
    {
        return 0.0;
    }

    float closestDepth = texture(shadowMap, proj.xy).r;
    float currentDepth = proj.z;

    // bias用来区分"真的被挡住"和"深度比较的误差",目前暂时用不到
    float bias = 0.0002;   // 5 cm
    return currentDepth > closestDepth ? 1.0 : 0.0;
}

// 点光源阴影：比较到光源的线性距离
// cubemap 里存的是 length(fragPos - lightPos) / farPlane（由 gl_FragDepth 写入）
float PointShadowCalculation(vec3 fragPos, vec3 lightPos, samplerCube cube, float farPlane)
{
    vec3  dir     = fragPos - lightPos;
    float current = length(dir) / farPlane;

    // dir 既是采样方向也是距离向量，GPU 按主轴自动选面
    float closest = texture(cube, dir).r;

    // 6 个面朝向不同，acne 程度不一样，bias 取比聚光大的折中值
    // 注意这里是「归一化到 farPlane 的距离」，0.015 * 30m = 0.45m
    float bias = 0.015;
    return current - bias > closest ? 1.0 : 0.0;
}
