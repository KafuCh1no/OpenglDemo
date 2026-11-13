#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec3 ObjectColor;
in vec2 TexCoords;      // 现在还没贴图，等 CTexture 做完再用

// ---- 手电筒 ----
uniform vec3  lightPos;      // = 相机位置（手电筒拿在手里）
uniform vec3  lightDir;      // = 相机朝向（含轻微摇摆）
uniform vec3  lightColor;
uniform float cutoff;        // 内锥边缘的 cos 值（亮度 100% 的范围）
uniform float outerCutoff;   // 外锥边缘的 cos 值（亮度衰减到 0 的范围）

uniform vec3 viewPos;

void main()
{
    vec3 N = normalize(Normal);
    vec3 L = normalize(lightPos - WorldPos);        // 片元 -> 光源
    vec3 V = normalize(viewPos - WorldPos);         // 片元 -> 眼睛
    vec3 H = normalize(L + V);                      // 半程向量（Blinn-Phong）

    // ===== ① 距离衰减：越远越暗（二次衰减）=====
    float dist = length(lightPos - WorldPos);
    float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

    // ===== ② 锥形衰减：手电筒的灵魂 =====
    // theta: 片元方向与手电筒中轴的夹角余弦（越接近 1 越在光锥中心）
    float theta     = dot(L, normalize(-lightDir));
    float epsilon   = cutoff - outerCutoff;
    // intensity: 光锥中心 1.0，从内锥到外锥平滑降到 0.0
    float intensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);

    // ===== ③ Blinn-Phong 光照（你学过的那套）=====
    vec3 ambient  = 0.03 * ObjectColor;                       // 极弱环境光，营造黑暗
    float diff    = max(dot(N, L), 0.0);
    vec3 diffuse  = diff * lightColor * ObjectColor;
    float spec    = pow(max(dot(N, H), 0.0), 32.0);
    vec3 specular = spec * lightColor * 0.3;                  // 手电筒高光别太抢

    vec3 color = (ambient + (diffuse + specular) * intensity) * attenuation;

    // ===== ④ 距离雾：远处融进黑暗 =====
    float fog = 1.0 - exp(-0.045 * dist);
    color = mix(color, vec3(0.0), fog);

    FragColor = vec4(color, 1.0);
}
