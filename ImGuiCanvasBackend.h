/*
 * SPDX-License-Identifier: Apache-2.0
 * Dear ImGui bridge for LiquidGlass-Cpp.
 *
 * Include Dear ImGui before this header. Compile the LiquidGlass-Cpp sources
 * (including Context.cpp, GlassPanel.cpp, and Shaders.cpp) into the same target.
 */
#pragma once

#include "GlassUI.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef IMGUI_VERSION
#error "Include imgui.h before ImGuiCanvasBackend.h"
#endif

namespace lgx {

class ImGuiCanvasBackend : public CanvasBackend {
public:
    ImGuiCanvasBackend() = default;
    ~ImGuiCanvasBackend() override = default;

    ImGuiCanvasBackend(const ImGuiCanvasBackend&) = delete;
    ImGuiCanvasBackend& operator=(const ImGuiCanvasBackend&) = delete;

    // Call once from the render thread after ImGui and its GLES3 backend have
    // been initialized and the EGL context is current. The sample menu uses
    // ImGui coordinates that are already in framebuffer pixels, so its scale
    // is 1.0f; pass a different scale only when your ImGui coordinates are dp.
    bool initialize(float uiScale = 1.0f) {
        if (initialized_) return ready_;
        initialized_ = true;

        attachCanvasBackend(this);
        setScale(uiScale);
        setBackdropEnabled(true);
        setTextMeasurer(&ImGuiCanvasBackend::measureText);

        ready_ = startup();
        if (ready_) {
            GlassColors colors;
            colors.primary = Rgba(0.075f, 0.085f, 0.115f, 1.0f);
            colors.secondary = Rgba(0.12f, 0.15f, 0.21f, 1.0f);
            setGlassColors(colors, false);
        }
        return ready_;
    }

    // Call before ImGui::NewFrame(). This keeps queued ImDrawList callback
    // payloads alive through the following RenderDrawData call.
    void beginFrame(float deltaSeconds) {
        callbacks_.clear();
        pointerPressedClaimed_ = false;
        lgx::beginFrame(deltaSeconds);
    }

    // Call immediately after ImGui::NewFrame(), once ImGui has updated IO.
    void syncFrame() {
        if (ImGui::GetCurrentContext() == nullptr) return;

        ImGuiIO& io = ImGui::GetIO();
        Surface surface;
        surface.width = io.DisplaySize.x;
        surface.height = io.DisplaySize.y;
        surface.pixelScaleX = io.DisplayFramebufferScale.x > 0.0f
            ? io.DisplayFramebufferScale.x : 1.0f;
        surface.pixelScaleY = io.DisplayFramebufferScale.y > 0.0f
            ? io.DisplayFramebufferScale.y : 1.0f;
        setSurface(surface);

        pointer_.x = io.MousePos.x;
        pointer_.y = io.MousePos.y;
        pointer_.down = io.MouseDown[0];
        pointer_.pressed = ImGui::IsMouseClicked(0);
        pointer_.released = ImGui::IsMouseReleased(0);
        setPointer(pointer_);
    }

    // Call after ImGui_ImplOpenGL3_RenderDrawData().
    void finishFrame() {
        if (pointer_.released) activeOwner_ = nullptr;
    }

    bool ready() const { return ready_; }

    // Draw a glass card at absolute ImGui screen coordinates. Call after Begin()
    // and before drawing the card's contents. A translucent ImGui fallback is
    // used if shader startup failed, so the menu remains visible.
    void drawGlassPanel(const Box& box, float radius, float blur,
                        const Rgba& tint, bool withShadow = false) {
        if (ImGui::GetCurrentContext() == nullptr || box.width() <= 0.0f
                || box.height() <= 0.0f) {
            return;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float clipPadding = withShadow ? 56.0f : 1.0f;
        drawList->PushClipRect(
            ImVec2(box.left - clipPadding, box.top - clipPadding),
            ImVec2(box.right + clipPadding, box.bottom + clipPadding), false);

        if (ready_) {
            PanelStyle style;
            style.corners = Corners::rounded(radius);
            style.optics.blurPx = blur;
            style.optics.lensHeightPx = 5.0f;
            style.optics.lensAmountPx = 12.0f;
            style.optics.depthAmount = 0.0f;
            style.optics.darkGuard = 1.0f;
            style.optics.spectral = 0.18f;
            style.surface = tint;
            style.surfaceMode = SurfaceMode::Overlay;
            style.backdropOn = true;
            style.fallback = Rgba(0.075f, 0.09f, 0.13f, 0.92f);
            style.edge = EdgeLight::contour(-90.0f, 1.35f, 0xBFFFFFFFu);
            style.edge.alpha = 0.64f;
            style.shadowOn = withShadow;
            style.shadow = DropShadow(22.0f, 0.0f, 6.0f, 0x46000000u, 1.0f);
            lgx::drawPanel(style, box);
        }
        else {
            const ImU32 fill = toImColor(tint);
            const ImU32 edge = IM_COL32(255, 255, 255, 72);
            drawList->AddRectFilled(ImVec2(box.left, box.top),
                                     ImVec2(box.right, box.bottom), fill, radius);
            drawList->AddRect(ImVec2(box.left, box.top),
                              ImVec2(box.right, box.bottom), edge, radius, 0, 1.0f);
        }

        drawList->PopClipRect();
    }

    Box clipRect() override {
        if (ImGui::GetCurrentContext() == nullptr) return Box();
        const ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 minimum = drawList->GetClipRectMin();
        const ImVec2 maximum = drawList->GetClipRectMax();
        return Box(minimum.x, minimum.y, maximum.x, maximum.y);
    }

    void onFillRect(const Box& box, float radius, unsigned int color) override {
        ImDrawList* drawList = currentDrawList();
        if (drawList == nullptr) return;
        drawList->AddRectFilled(ImVec2(box.left, box.top), ImVec2(box.right, box.bottom),
                                toImColor(color), roundedRadius(box, radius));
    }

    void onStrokeRect(const Box& box, float radius, float thickness,
                      unsigned int color) override {
        ImDrawList* drawList = currentDrawList();
        if (drawList == nullptr || thickness <= 0.0f) return;
        drawList->AddRect(ImVec2(box.left, box.top), ImVec2(box.right, box.bottom),
                          toImColor(color), roundedRadius(box, radius), 0, thickness);
    }

    void onFillPoly(const Point* points, int count, unsigned int color) override {
        if (points == nullptr || count < 3) return;
        ImDrawList* drawList = currentDrawList();
        if (drawList == nullptr) return;
        std::vector<ImVec2> converted;
        converted.reserve((size_t)count);
        for (int i = 0; i < count; ++i) converted.push_back(ImVec2(points[i].x, points[i].y));
        drawList->AddConvexPolyFilled(converted.data(), count, toImColor(color));
    }

    void onFillConcave(const Point* points, int count, unsigned int color) override {
        if (points == nullptr || count < 3) return;
        ImDrawList* drawList = currentDrawList();
        if (drawList == nullptr) return;
        triangulateAndFill(drawList, points, count, toImColor(color));
    }

    void onStrokePoly(const Point* points, int count, float thickness,
                      unsigned int color) override {
        if (points == nullptr || count < 2 || thickness <= 0.0f) return;
        ImDrawList* drawList = currentDrawList();
        if (drawList == nullptr) return;
        const ImU32 packed = toImColor(color);
        for (int i = 0; i < count; ++i) {
            const Point& from = points[i];
            const Point& to = points[(i + 1) % count];
            drawList->AddLine(ImVec2(from.x, from.y), ImVec2(to.x, to.y), packed, thickness);
        }
    }

    void onLine(const Point& from, const Point& to, unsigned int color,
                float thickness) override {
        ImDrawList* drawList = currentDrawList();
        if (drawList == nullptr || thickness <= 0.0f) return;
        drawList->AddLine(ImVec2(from.x, from.y), ImVec2(to.x, to.y),
                          toImColor(color), thickness);
    }

    void onFillCircle(const Point& center, float radius, unsigned int color,
                      int segments) override {
        ImDrawList* drawList = currentDrawList();
        if (drawList == nullptr || radius <= 0.0f) return;
        drawList->AddCircleFilled(ImVec2(center.x, center.y), radius, toImColor(color),
                                  segments > 2 ? segments : 0);
    }

    void onFillTriangle(const Point& a, const Point& b, const Point& c,
                        unsigned int color) override {
        ImDrawList* drawList = currentDrawList();
        if (drawList == nullptr) return;
        drawList->AddTriangleFilled(ImVec2(a.x, a.y), ImVec2(b.x, b.y), ImVec2(c.x, c.y),
                                    toImColor(color));
    }

    void onFillArc(const Point& center, float radius, float fromRadians,
                   float toRadians, unsigned int color, int segments) override {
        ImDrawList* drawList = currentDrawList();
        if (drawList == nullptr || radius <= 0.0f) return;

        const float sweep = toRadians - fromRadians;
        if (std::fabs(sweep) >= 6.2831852f - 0.001f) {
            drawList->AddCircleFilled(ImVec2(center.x, center.y), radius, toImColor(color),
                                      segments > 2 ? segments : 0);
            return;
        }

        int arcSegments = segments;
        if (arcSegments < 2) arcSegments = 2;
        if (arcSegments > 256) arcSegments = 256;
        std::vector<Point> wedge;
        wedge.reserve((size_t)arcSegments + 2);
        wedge.push_back(center);
        for (int i = 0; i <= arcSegments; ++i) {
            const float t = (float)i / (float)arcSegments;
            const float angle = fromRadians + sweep * t;
            wedge.push_back(Point(center.x + std::cos(angle) * radius,
                                  center.y + std::sin(angle) * radius));
        }
        triangulateAndFill(drawList, wedge.data(), (int)wedge.size(), toImColor(color));
    }

    void onText(const Point& position, float size, unsigned int color,
                const char* content, float wrapWidth) override {
        if (content == nullptr || size <= 0.0f) return;
        ImDrawList* drawList = currentDrawList();
        ImFont* font = ImGui::GetFont();
        if (drawList == nullptr || font == nullptr) return;
        drawList->AddText(font, size, ImVec2(position.x, position.y), toImColor(color),
                          content, nullptr, wrapWidth > 0.0f ? wrapWidth : 0.0f);
    }

    void onImage(unsigned int texture, const Box& box, const Point& uvMin,
                 const Point& uvMax, unsigned int color) override {
        ImDrawList* drawList = currentDrawList();
        if (drawList == nullptr || texture == 0u) return;
        drawList->AddImage(toImTextureId<ImTextureID>(texture),
                           ImVec2(box.left, box.top), ImVec2(box.right, box.bottom),
                           ImVec2(uvMin.x, uvMin.y), ImVec2(uvMax.x, uvMax.y),
                           toImColor(color));
    }

    void onPushClip(const Box& box) override {
        ImDrawList* drawList = currentDrawList();
        if (drawList != nullptr) {
            drawList->PushClipRect(ImVec2(box.left, box.top),
                                   ImVec2(box.right, box.bottom), true);
        }
    }

    void onPopClip() override {
        ImDrawList* drawList = currentDrawList();
        if (drawList != nullptr) drawList->PopClipRect();
    }

    void onPanelCapture(int index) override {
        addPanelCallback(CallbackKind::Capture, index);
    }

    void onPanelPaint(int index) override {
        addPanelCallback(CallbackKind::Paint, index);
    }

    PointerProbe onProbe(const Box& box, const void* owner, bool interactive) override {
        PointerProbe result;
        if (!interactive || owner == nullptr || pointerIsBlocked()) return result;

        const bool hovered = box.holds(pointer_.x, pointer_.y);
        if (pointer_.pressed && !pointerPressedClaimed_) {
            activeOwner_ = hovered ? owner : nullptr;
            pointerPressedClaimed_ = true;
        }
        result.activated = pointer_.pressed && activeOwner_ == owner;
        result.held = pointer_.down && activeOwner_ == owner;
        result.clicked = pointer_.released && activeOwner_ == owner && hovered;
        if (pointer_.released && activeOwner_ == owner) activeOwner_ = nullptr;
        return result;
    }

    void onBlockPointer(const Box& box) override {
        pointerBlocks_.push_back(box);
    }

    void onClearPointerBlock() override {
        pointerBlocks_.clear();
    }

private:
    enum class CallbackKind { Capture, Paint };

    struct CallbackData {
        CallbackKind kind = CallbackKind::Capture;
        int panelIndex = -1;
        Box clip;
    };

    static TextSize measureText(const char* content, float size) {
        TextSize measured;
        if (content == nullptr || size <= 0.0f || ImGui::GetCurrentContext() == nullptr) {
            return measured;
        }
        ImFont* font = ImGui::GetFont();
        if (font == nullptr) return measured;
        const ImVec2 span = font->CalcTextSizeA(size, 1000000.0f, 0.0f, content);
        measured.width = span.x;
        measured.height = span.y;
        return measured;
    }

    static ImU32 toImColor(unsigned int argb) {
        const int alpha = (int)((argb >> 24) & 0xFFu);
        const int red = (int)((argb >> 16) & 0xFFu);
        const int green = (int)((argb >> 8) & 0xFFu);
        const int blue = (int)(argb & 0xFFu);
        return IM_COL32(red, green, blue, alpha);
    }

    static ImU32 toImColor(const Rgba& color) {
        const int red = (int)(saturate(color.r) * 255.0f + 0.5f);
        const int green = (int)(saturate(color.g) * 255.0f + 0.5f);
        const int blue = (int)(saturate(color.b) * 255.0f + 0.5f);
        const int alpha = (int)(saturate(color.a) * 255.0f + 0.5f);
        return IM_COL32(red, green, blue, alpha);
    }

    template <typename T>
    static typename std::enable_if<std::is_pointer<T>::value, T>::type
    toImTextureId(unsigned int texture) {
        return reinterpret_cast<T>(static_cast<intptr_t>(texture));
    }

    template <typename T>
    static typename std::enable_if<!std::is_pointer<T>::value, T>::type
    toImTextureId(unsigned int texture) {
        return static_cast<T>(texture);
    }

    static float roundedRadius(const Box& box, float radius) {
        const float width = std::fabs(box.width());
        const float height = std::fabs(box.height());
        const float limit = 0.5f * (width < height ? width : height);
        if (!std::isfinite(radius) || radius > limit) return limit;
        return radius > 0.0f ? radius : 0.0f;
    }

    ImDrawList* currentDrawList() const {
        if (ImGui::GetCurrentContext() == nullptr) return nullptr;
        return ImGui::GetWindowDrawList();
    }

    void addPanelCallback(CallbackKind kind, int index) {
        ImDrawList* drawList = currentDrawList();
        if (drawList == nullptr || index < 0) return;

        std::unique_ptr<CallbackData> item(new CallbackData());
        item->kind = kind;
        item->panelIndex = index;
        item->clip = clipRect();
        CallbackData* stableData = item.get();
        callbacks_.push_back(std::move(item));

        drawList->AddCallback(&ImGuiCanvasBackend::dispatchPanelCallback, stableData);
        // The glass shader changes GLES state; ask the ImGui GLES backend to
        // restore its own program, buffers, viewport, blending and scissor.
        drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    }

    static void dispatchPanelCallback(const ImDrawList*, const ImDrawCmd* command) {
        if (command == nullptr || command->UserCallbackData == nullptr) return;
        const CallbackData* data = static_cast<const CallbackData*>(command->UserCallbackData);
        if (data->kind == CallbackKind::Capture) {
            runPanelCapture(data->panelIndex, data->clip);
        }
        else {
            runPanelPaint(data->panelIndex, data->clip);
        }
    }

    bool pointerIsBlocked() const {
        for (size_t i = 0; i < pointerBlocks_.size(); ++i) {
            if (pointerBlocks_[i].holds(pointer_.x, pointer_.y)) return true;
        }
        return false;
    }

    static float cross(const Point& a, const Point& b, const Point& c) {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    }

    static bool pointInTriangle(const Point& point, const Point& a, const Point& b,
                                const Point& c, bool counterClockwise) {
        const float epsilon = 0.00001f;
        const float ab = cross(a, b, point);
        const float bc = cross(b, c, point);
        const float ca = cross(c, a, point);
        if (counterClockwise) return ab >= -epsilon && bc >= -epsilon && ca >= -epsilon;
        return ab <= epsilon && bc <= epsilon && ca <= epsilon;
    }

    static void triangulateAndFill(ImDrawList* drawList, const Point* points, int count,
                                   ImU32 color) {
        if (drawList == nullptr || points == nullptr || count < 3) return;

        float twiceArea = 0.0f;
        for (int i = 0; i < count; ++i) {
            const Point& a = points[i];
            const Point& b = points[(i + 1) % count];
            twiceArea += a.x * b.y - b.x * a.y;
        }
        if (std::fabs(twiceArea) < 0.00001f) return;
        const bool ccw = twiceArea > 0.0f;

        std::vector<int> remaining;
        remaining.reserve((size_t)count);
        for (int i = 0; i < count; ++i) remaining.push_back(i);

        int guard = count * count;
        while (remaining.size() > 3 && guard-- > 0) {
            bool clippedEar = false;
            for (size_t i = 0; i < remaining.size(); ++i) {
                const int previous = remaining[(i + remaining.size() - 1) % remaining.size()];
                const int current = remaining[i];
                const int next = remaining[(i + 1) % remaining.size()];
                const float corner = cross(points[previous], points[current], points[next]);
                if ((ccw && corner <= 0.00001f) || (!ccw && corner >= -0.00001f)) continue;

                bool containsVertex = false;
                for (size_t j = 0; j < remaining.size(); ++j) {
                    const int candidate = remaining[j];
                    if (candidate == previous || candidate == current || candidate == next) continue;
                    if (pointInTriangle(points[candidate], points[previous], points[current],
                                        points[next], ccw)) {
                        containsVertex = true;
                        break;
                    }
                }
                if (containsVertex) continue;

                drawList->AddTriangleFilled(
                    ImVec2(points[previous].x, points[previous].y),
                    ImVec2(points[current].x, points[current].y),
                    ImVec2(points[next].x, points[next].y), color);
                remaining.erase(remaining.begin() + (std::ptrdiff_t)i);
                clippedEar = true;
                break;
            }
            if (!clippedEar) return; // Degenerate/self-intersecting input: draw nothing.
        }

        if (remaining.size() == 3) {
            drawList->AddTriangleFilled(
                ImVec2(points[remaining[0]].x, points[remaining[0]].y),
                ImVec2(points[remaining[1]].x, points[remaining[1]].y),
                ImVec2(points[remaining[2]].x, points[remaining[2]].y), color);
        }
    }

    bool initialized_ = false;
    bool ready_ = false;
    bool pointerPressedClaimed_ = false;
    const void* activeOwner_ = nullptr;
    PointerState pointer_;
    std::vector<Box> pointerBlocks_;
    std::vector<std::unique_ptr<CallbackData> > callbacks_;
};

} // namespace lgx
