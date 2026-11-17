# 问题整理

- [阴影](#阴影)

## 阴影

- 问题：导入建模后阴影出现在奇怪的位置

![问题截图](./images/shadow1.png)

- 原因分析：

  1. 主 Pass（`assets/shaders/horror.vert`）用 `model * vec4(aPos, 1.0)` 把顶点从模型空间变换到世界空间，模型的位移 / 旋转 / 缩放全靠这个 `model` 矩阵：

     ```glsl
     vec4 world4 = model * vec4(aPos, 1.0);
     WorldPos    = world4.xyz;
     gl_Position = projection * view * world4;
     ```

  2. 阴影 Pass（`assets/shaders/shadowdepth.vert`）里 **没有 `model`**，顶点保持模型空间原始坐标：

     ```glsl
     gl_Position = lightSpaceMatrix * vec4(aPos, 1.0);
     ```

  3. 根因在接口层：`CShadow::Render` 只接收 `std::vector<const CMesh*>`，而 `CMesh` 只持有 VAO / VBO / EBO，**不携带任何变换信息**。深度 Pass 即使想应用 model 也没有渠道。

  4. 结果：深度图里模型停留在原点、原始大小。

  5. 视觉表现：原点附近的地面 / 墙面出现模型的轮廓阴影，轮廓和实体一致、边缘硬，看起来就像场景里**凭空多了一把椅子**。

  6. 为什么之前只有盒子时没暴露：`AppendBoxMesh` 生成的顶点**本身就是世界坐标**，Group 的 model 是单位矩阵，两条路径恰好一致，问题被掩盖了。

- 解决方案：

  1. `assets/shaders/shadowdepth.vert` 增加 model uniform：

     ```glsl
     uniform mat4 lightSpaceMatrix;
     uniform mat4 model;

     void main()
     {
         gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
     }
     ```

  2. `CShadow` 改为按「mesh + model」成对绘制：

     ```cpp
     struct DrawItem
     {
         const CMesh* mesh  = nullptr;
         glm::mat4    model{ 1.0f };
     };

     void Render(const std::vector<DrawItem>& items, ...);
     ```

     绘制循环里逐个设置 model：

     ```cpp
     for (const DrawItem& it : items)
     {
         if (!it.mesh) continue;
         m_depthShader.setMat4("model", it.model);
         it.mesh->Draw();
     }
     ```

  3. `CScene::Render` 构造 items 时带上每个 Group 的变换：

     ```cpp
     items.push_back({
         g->mesh.get(),
         ComposeTransform(g->modelPos, g->modelRotDeg, g->modelScale)
     });
     ```

  4. 验证：改完后「第二把椅子」消失，桌椅的阴影落在正确位置。

- 经验总结：

  - **多 Pass 渲染时，所有 Pass 必须共用同一套变换**。主 Pass 用了哪些矩阵，阴影 Pass 一个都不能少，否则几何位置直接错位。
  - 接口设计上，只传 mesh 不传 model 就是埋坑。`CMesh` 是纯 GPU 资源封装，本来就不该知道变换；变换应该由调用方显式传进来。