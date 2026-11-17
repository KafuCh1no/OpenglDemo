# 基于OpenGL的恐怖游戏 Demo

第一人称手电筒探索 Demo，用 OpenGL 3.3 Core 从零实现渲染管线：
阴影贴图、多光源、PBR 风格材质、OBJ 模型加载、AABB 碰撞。

<!-- TODO: 放一张实际画面 -->

## 特性

- **多光源**：手电筒（聚光，跟随相机）+ 壁灯（点光），数组化 uniform 循环
- **阴影贴图**：每盏聚光灯一张深度图，PCF 边缘软化
- **材质系统**：albedo 贴图 + 法线贴图 + tint 调色 + UV 平铺
- **模型加载**：tinyobjloader 解析 OBJ，索引去重后转交错顶点缓冲
- **碰撞检测**：AABB 逐轴滑动，可沿墙滑行
- **调试面板**：ImGui 实时调光源强度、材质参数、阴影开关
- **正确的伽马管线**：sRGB 纹理解码 → linear 空间计算 → sRGB 编码输出

## 构建

依赖全部随工程携带（`third_party/`），无需额外安装。

### Windows + Visual Studio

1. VS 打开 `Demo` 文件夹
2. 等待 CMake 自动配置
3. F5 运行

### 命令行

```powershell
cmake -S . -B out/build/x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/x64-Debug
out/build/x64-Debug/Demo.exe
