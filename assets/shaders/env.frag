#version 330 core
// 程序化生成的"环境"：一间接了月光和壁炉余光的暗房间。
// 不依赖外部 HDR 资源，纯按方向算颜色，直接渲染进 cubemap。
out vec4 FragColor;

in vec3 vLocalPos;

void main()
{
    vec3 dir = normalize(vLocalPos);

    // 上冷下暖的三段渐变：地板反光偏暖，天花偏冷
    vec3 ground = vec3(0.022, 0.017, 0.013);
    vec3 wall   = vec3(0.028, 0.026, 0.024);
    vec3 sky    = vec3(0.020, 0.024, 0.032);

    vec3 col = mix(ground, wall, smoothstep(-0.85, -0.05, dir.y));
    col      = mix(col,    sky,  smoothstep( 0.15,  0.90, dir.y));

    // 月光：和场景里那盏平行光大致同向，给金属一个冷色高光
    vec3  moonDir = normalize(vec3(-0.35, 0.55, -0.75));
    float moon    = pow(max(dot(dir, moonDir), 0.0), 32.0);
    col += vec3(0.35, 0.45, 0.75) * moon * 1.6;

    // 壁炉方向：很弱的暖色团块，让小金属件不至于全黑还能看出暖调
    vec3  fireDir = normalize(vec3(0.55, -0.10, 0.40));
    float fire    = pow(max(dot(dir, fireDir), 0.0), 8.0);
    col += vec3(1.00, 0.45, 0.15) * fire * 0.45;

    FragColor = vec4(col, 1.0);
}
