#version 330 core
out vec4 FragColor;

in float vWorldY;

uniform vec3  uColor;

// 从下往上的生长动画：只画 y 低于这条"水平线"的部分
uniform float uMinY;       // 物体底部世界高度
uniform float uMaxY;       // 物体顶部世界高度
uniform float uProgress;   // 0 = 完全不显示，1 = 全部显示

void main()
{
    // 把世界高度归一化到 [0,1]，超过当前进度就丢掉这个片元
    float t = (vWorldY - uMinY) / max(uMaxY - uMinY, 1e-4);
    if (t > uProgress) discard;

    // alpha 是 bloom 的自发光遮罩：描边不该发光，写 0
    FragColor = vec4(uColor, 0.0);
}
