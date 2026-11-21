#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D gDepth;      // 场景深度（近平面 0.1 / 远平面 100）
uniform sampler2D gNormal;     // 世界空间法线（MRT 附件 1）
uniform sampler2D uNoise;      // 4x4 随机向量，用来旋转采样核

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uInvProjection;

uniform vec3  uKernel[32];
uniform float uRadius;
uniform float uBias;
uniform float uStrength;
uniform float uPower;
uniform float uNoiseScaleX;
uniform float uNoiseScaleY;

// 用深度反推视空间坐标
vec3 ViewPosFromDepth(vec2 uv, float depth)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 vp   = uInvProjection * clip;
    return vp.xyz / vp.w;
}

void main()
{
    float depth = texture(gDepth, vUV).r;

    // 背景（天空 / 未写深度）不参与遮蔽
    if (depth >= 1.0)
    {
        FragColor = vec4(1.0);
        return;
    }

    vec3 fragPos = ViewPosFromDepth(vUV, depth);

    // 世界空间法线 -> 视空间
    vec3 normal = normalize(mat3(uView) * texture(gNormal, vUV).rgb);

    // 用噪声构造 TBN：让 32 个样本绕法线随机旋转
    vec3 randomVec = normalize(texture(uNoise, vUV * vec2(uNoiseScaleX, uNoiseScaleY)).xyz);
    vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;

    for (int i = 0; i < 32; ++i)
    {
        // 半球内的采样点
        vec3 samplePos = fragPos + TBN * uKernel[i] * uRadius;

        // 投回屏幕，看那个位置的实际深度
        vec4 offset = uProjection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz  = offset.xyz * 0.5 + 0.5;

        if (offset.x < 0.0 || offset.x > 1.0 ||
            offset.y < 0.0 || offset.y > 1.0)
            continue;

        float sampleDepth = ViewPosFromDepth(offset.xy, texture(gDepth, offset.xy).r).z;

        // 距离衰减：远处的采样点不该影响近处的遮蔽
        float rangeCheck = smoothstep(0.0, 1.0,
            uRadius / max(abs(fragPos.z - sampleDepth), 1e-4));

        // samplePos 比实际表面更远 -> 被挡住
        occlusion += (sampleDepth >= samplePos.z + uBias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / 32.0) * uStrength;

    // 对比度曲线：线性算出来的值大多在 0.8~1.0 之间，直接乘几乎看不出来，
    // 幂函数把接近 1 的部分压下去，接触处的暗角才会明显
    occlusion = pow(clamp(occlusion, 0.0, 1.0), uPower);

    FragColor = vec4(vec3(clamp(occlusion, 0.0, 1.0)), 1.0);
}
