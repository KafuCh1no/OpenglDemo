#version 330 core
out vec4 FragColor;

in vec2 vUV;
in vec3 vProj;

uniform sampler2D uReflection;
uniform float     uTint;       // 反射亮度（老镜子偏暗）
uniform float     uEdgeFade;   // 镜框内侧的暗边宽度

void main()
{
    // 镜面片的世界位置投影到「镜面相机」的裁剪空间，再转成纹理坐标：
    // 这样每个镜面像素取到的正是反射相机在同一个三维点上看到的内容
    vec2 screenUV = (vProj.xy / max(vProj.z, 1e-4)) * 0.5 + 0.5;

    vec3 refl = texture(uReflection, screenUV).rgb;

    // 边框内侧压暗一点，像旧镜子边缘氧化
    float edge  = min(min(vUV.x, 1.0 - vUV.x), min(vUV.y, 1.0 - vUV.y));
    float shade = mix(0.55, 1.0, smoothstep(0.0, max(uEdgeFade, 1e-4), edge));

    FragColor = vec4(refl * uTint * shade, 1.0);
}
