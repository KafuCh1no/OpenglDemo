#version 330 core
out vec4 FragColor;

in vec2 vUV;
in vec3 vProj;
in vec3 vWorldPos;

#define MAX_LIGHTS 8

uniform sampler2D uReflection;
uniform float     uTint;       // 反射亮度（老镜子偏暗）
uniform float     uEdgeFade;   // 镜框内侧的暗边宽度

// ---- 镜面高光 ----
// 镜子除了反射，表面还有一层玻璃。强光直射时这层玻璃会反光，
// 只在反射图上乘系数是出不来这个亮斑的。
uniform vec3  uNormal;         // 镜面法线（世界空间）
uniform vec3  viewPos;
uniform float uSpecStrength;   // 高光强度（0 关闭）

uniform int   lightCount;
uniform vec3  lightPos[MAX_LIGHTS];
uniform vec3  lightDirArr[MAX_LIGHTS];
uniform vec3  lightColorArr[MAX_LIGHTS];
uniform float lightIntensity[MAX_LIGHTS];
uniform float lightCutoff[MAX_LIGHTS];
uniform float lightOuterCutoff[MAX_LIGHTS];
uniform int   lightType[MAX_LIGHTS];

void main()
{
    // 镜面片的世界位置投影到「镜面相机」的裁剪空间，再转成纹理坐标：
    // 这样每个镜面像素取到的正是反射相机在同一个三维点上看到的内容
    vec2 screenUV = (vProj.xy / max(vProj.z, 1e-4)) * 0.5 + 0.5;

    vec3 refl = texture(uReflection, screenUV).rgb;

    // 边框内侧压暗一点，像旧镜子边缘氧化
    float edge  = min(min(vUV.x, 1.0 - vUV.x), min(vUV.y, 1.0 - vUV.y));
    float shade = mix(0.55, 1.0, smoothstep(0.0, max(uEdgeFade, 1e-4), edge));

    // ---- 被光照到时的镜面反光 ----
    vec3 spec = vec3(0.0);
    if (uSpecStrength > 0.0)
    {
        vec3 N = normalize(uNormal);
        vec3 V = normalize(viewPos - vWorldPos);

        for (int i = 0; i < MAX_LIGHTS; ++i)
        {
            if (i >= lightCount) break;

            vec3  L;
            float atten = 1.0;
            float spot  = 1.0;

            if (lightType[i] == 2)
            {
                L = normalize(-lightDirArr[i]);
            }
            else
            {
                vec3  toLight = lightPos[i] - vWorldPos;
                float dist    = length(toLight);
                L = toLight / max(dist, 1e-4);
                atten = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

                if (lightType[i] == 1)
                {
                    float theta = dot(L, normalize(-lightDirArr[i]));
                    float eps   = max(lightCutoff[i] - lightOuterCutoff[i], 1e-4);
                    float x     = clamp((theta - lightOuterCutoff[i]) / eps, 0.0, 1.0);
                    spot = pow(x, 5.0);
                }
            }

            float NdotL = max(dot(N, L), 0.0);
            if (NdotL <= 0.0) continue;

            vec3  H     = normalize(L + V);
            float NdotH = max(dot(N, H), 0.0);

            // 玻璃非常光滑：用很高的指数，只留下一个集中的亮斑
            float s = pow(NdotH, 800.0);

            spec += lightColorArr[i] * lightIntensity[i] * atten * spot * s * NdotL;
        }
    }

    // alpha 是 bloom 的自发光遮罩：镜子不属于自发光，
    // 写 1 会让整块镜面被当成光源，整面镜子糊上一层光晕
    FragColor = vec4(refl * uTint * shade + spec * uSpecStrength, 0.0);
}
