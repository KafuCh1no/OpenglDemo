# 素材来源 / Credits

本项目使用的第三方素材及其许可证。

---

## 模型

### Poly Haven

| 文件 | 资产 ID | 作者 | 修改说明 |
|------|---------|------|---------|
| `barrel/Barrel_01.obj` | [Barrel_01](https://polyhaven.com/a/Barrel_01) | Jorge Camacho | 未修改，运行时缩放与摆放 |
| `chandelier/Chandelier_02.obj` | [Chandelier_02](https://polyhaven.com/a/Chandelier_02) | Kirill Sannikov | 未修改，运行时缩放与摆放 |
| `fancy_picture_frame/frame.obj`、`canvas.obj` | [fancy_picture_frame_01](https://polyhaven.com/a/fancy_picture_frame_01) | Rob Tuytel、Rico Cilliers | 从 glTF 拆出 frame / canvas 两个网格，运行时缩放 |
| `gothic-cabinet/GothicCabinet_01.obj` | [GothicCabinet_01](https://polyhaven.com/a/GothicCabinet_01) | Kirill Sannikov | 未修改，运行时缩放与摆放 |
| `lantern/Lantern_01.obj` | [Lantern_01](https://polyhaven.com/a/Lantern_01) | Rajil Jose Macatangay | 未修改，运行时缩放与摆放 |


### Sketchfab

| 文件 | 资产 | 作者 | 来源 | 修改说明 |
|------|------|------|------|---------|
| `the_bathroom_free/` | The Bathroom (Free) | Evan | [Sketchfab](https://sketchfab.com/3d-models/the-bathroom-free-d5e5035dda434b8d9beaa7271f1c85fc) | 拆分为独立部件使用（洗手台 / 浴缸 / 镜子 / 蜡烛 / 地砖 / 门 / 窗 / 墙裙），运行时缩放与重新摆放；`Wall_Trim` 图集另被裁切成无缝墙纸 |
| `door/door.obj` | Door | hoschu | [Sketchfab](https://sketchfab.com/3d-models/door-adf292f437f24151918a3b16ecef52d2) | 未修改，运行时缩放与摆放 |
| `wooden_window/wooden_window.obj` | Wooden Window | adventurer | [Sketchfab](https://sketchfab.com/3d-models/wooden-window-df1861ed87184c34acba78530bd2152f) | 未修改，运行时缩放与摆放 |
| `fireplace/fireplace.obj` | Fireplace Low-poly | MaX3Dd | [Sketchfab](https://sketchfab.com/3d-models/fireplace-low-poly-d1473f5f384a411f979b99214de9f2a9) | 未修改 |


### Fab（CC BY 4.0）

| 文件 | 资产 | 作者 | 来源 | 修改说明 |
|------|------|------|------|---------|
| `table-damaged/` | Table-Damaged | Aidin Salsabili | [Fab](https://www.fab.com/listings/99c1cefd-e3b4-4643-9bb3-7732b4116335) | 未修改，运行时缩放 |
| `chair/SM_Chair.obj` | Antique Chair | Krag Studio | [Fab](https://www.fab.com/listings/50020a0c-fc0b-4368-a414-aeb8f75f30fd) | 未修改，运行时缩放与摆放（场景内 4 把） |

---

## 贴图

| 文件 | 来源 | 许可证 | 修改说明 |
|------|------|--------|---------|
| `textures/Tiles010_2K-JPG_*` | [ambientCG](https://ambientcg.com/view?id=Tiles010) | CC0 | 未修改 |
| `textures/WoodFloor007_2K-JPG_*` | [ambientCG](https://ambientcg.com/view?id=WoodFloor007) | CC0 | 未修改 |
| `textures/WallpaperOld_*` | The Bathroom (Free) 资产包 | CC BY 4.0 | 从包内 `Wall_Trim` 图集裁出无缝花纹（205px 一个周期），署名见上方 |
> 其余贴图（`Bath_*`、`Bathtube_*`、`Candle_*`、`Floor_*`、`Frames_*`、`Door_and_windows_*`、`Wall_Trim_*`）
> 均随 The Bathroom (Free) 资产包提供，许可同其模型。

---

## 代码库

| 库 | 用途 | 许可证 |
|----|------|--------|
| [GLFW](https://www.glfw.org/) | 窗口与输入 | zlib/libpng |
| [GLAD](https://github.com/Dav1dde/glad) | OpenGL 函数加载 | MIT / Public Domain |
| [GLM](https://github.com/g-truc/glm) | 数学库 | MIT |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | OBJ 解析 | MIT |
| [stb_image](https://github.com/nothings/stb) | 图像解码 | Public Domain / MIT |
| [Dear ImGui](https://github.com/ocornut/imgui) | 调试界面 | MIT |

---

## 说明

- 本项目为个人学习作品，不用于商业用途。