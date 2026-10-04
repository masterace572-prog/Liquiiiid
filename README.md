# LiquidGlass-Cpp

本项目（LiquidGlass-Cpp）是基于 Kyant 项目的 C++ 独立移植与二次开发。

## 声明
- 本文件由 SilentEaves (默檐) 于 2026 年独立完成 C++ 移植与二次修改。
- 具体修改包括：将原项目（Kotlin）逻辑重写为 C++。
- 本项目同样遵守并应用 Apache License, Version 2.0 协议。

## 第三方开源项目声明
本项目的核心代码逻辑移植自 AndroidLiquidGlass 项目，来源 Kyant 的液态玻璃项目。
- 原项目版权：Copyright 2025 Kyant
- 原项目地址：https://github.com/Kyant0/AndroidLiquidGlass
- 原项目许可证：Apache License, Version 2.0

## 免责声明
本项目（LiquidGlass-Cpp）是由 SilentEaves 独立开发的第三方非官方 C++ 移植版本，仅基于开源协议对原项目进行学习与移植。

本项目与原作者 Kyant 及其官方项目没有直接的隶属关系。原作者不对本项目的代码质量及维护负责。本项目仅供学习交流。


## Dear ImGui / Android GLES3 接入

仓库附带 `ImGuiCanvasBackend.h`，可把 LiquidGlass-Cpp 的画布绘制、指针输入和面板回调桥接到 Dear ImGui 的 `ImDrawList`。先确保工程包含 Dear ImGui，并把仓库中的所有 LiquidGlass `.cpp` 编译进 Android target；本库需要 C++17 与 GLES3。

```cpp
#include "imgui.h"
#include "ImGuiCanvasBackend.h"

static lgx::ImGuiCanvasBackend g_glass;

// EGL/GLES context current, and after ImGui_ImplOpenGL3_Init():
g_glass.initialize(1.0f); // 1.0 when ImGui uses framebuffer-pixel coordinates

// Each frame, before ImGui::NewFrame():
g_glass.beginFrame(ImGui::GetIO().DeltaTime);
ImGui_ImplOpenGL3_NewFrame();
ImGui_ImplAndroid_NewFrame(width, height);
ImGui::NewFrame();
g_glass.syncFrame();

// Inside an active ImGui::Begin(), before that window's contents:
const ImVec2 p = ImGui::GetWindowPos();
const ImVec2 s = ImGui::GetWindowSize();
g_glass.drawGlassPanel(lgx::Box(p.x, p.y, p.x + s.x, p.y + s.y),
                       20.0f, 9.0f,
                       lgx::Rgba(0.08f, 0.12f, 0.18f, 0.34f), true);

// After ImGui::Render():
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
g_glass.finishFrame();
```

The adapter queues capture/paint callbacks in draw order and resets GLES state afterward. Keep glass windows/child backgrounds transparent so the shader can sample the scene behind them. See `使用说明.md` for frame ordering and details.

## 效果展示

![效果图1](Screenshot_2026-10-04-10-50-30-248_com.demo.imguifloat.jpg)
![效果图2](Screenshot_2026-10-04-10-50-24-968_com.demo.imguifloat.jpg)
![效果图3](Screenshot_2026-10-04-10-50-17-953_com.demo.imguifloat.jpg)
![效果图4](Screenshot_2026-10-04-10-50-20-875_com.demo.imguifloat.jpg)
![效果图5](Screenshot_2026-10-04-10-50-11-374_com.demo.imguifloat.jpg)
![效果图6](Screenshot_2026-10-04-10-50-03-631_com.demo.imguifloat.jpg)
