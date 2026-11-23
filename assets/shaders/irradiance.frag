#version 330 core
// 漫反射辐照度卷积：对每个方向，把半球上所有方向的环境光按 cos 加权积分。
out vec4 FragColor;

in vec3 vLocalPos;

uniform samplerCube uEnvMap;

const float PI = 3.14159265359;

void main()
{
    vec3 N = normalize(vLocalPos);

    // 以 N 为轴建一组切空间基
    vec3 up    = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = normalize(cross(N, right));

    vec3  irradiance   = vec3(0.0);
    float sampleDelta  = 0.05;      // 角度步长（弧度），越小越准越慢
    int   nrSamples    = 0;

    // 球面坐标积分：phi 绕一圈，theta 从法线到地表
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        // 乘0.5是因为只需要求上半球
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // 球面空间 -> 世界空间
            // 单位向量
            vec3 tangentSample = vec3(sin(theta) * cos(phi),
                                      sin(theta) * sin(phi),
                                      cos(theta));
            vec3 sampleVec = tangentSample.x * right
                           + tangentSample.y * up
                           + tangentSample.z * N;

            // cos(theta)*sin(theta) 是立体角加权（球面积分要乘 sin）
            irradiance += texture(uEnvMap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }

    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    FragColor  = vec4(irradiance, 1.0);
}
