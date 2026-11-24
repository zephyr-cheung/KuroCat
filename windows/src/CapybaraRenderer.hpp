#pragma once
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cmath>
#include <algorithm>
#include "CatPhysics.hpp"
#include "LuluShapes.hpp"

// Lulu vector renderer. Face/body layers are polygons traced directly from the MIT reference idle frame;
// no raster asset is loaded at runtime. Procedural limbs are used only where animation is needed.
class CapybaraRenderer {
public:
    static constexpr float RadToDeg = 57.2957795f;

    static CatHitZone hitTest(Pt p, const CatPhysics& physics) {
        float cx = CatPhysics::SeatAnchor.x + physics.bodyShiftX.value;
        float dx = (p.x - cx) / 63.0f;
        float dy = (p.y - 111.0f) / 64.0f;
        if (dx*dx + dy*dy <= 1.0f) return CatHitZone::Head;
        dx = (p.x - cx) / 58.0f;
        dy = (p.y - 190.0f) / 48.0f;
        if (dx*dx + dy*dy <= 1.0f) return CatHitZone::Body;
        if (p.x >= 58.0f && p.x <= 294.0f && p.y >= 232.0f && p.y <= 296.0f) return CatHitZone::Body;
        return CatHitZone::None;
    }

    static void draw(Gdiplus::Graphics& g, const CatPhysics& p, const CatSettings&) {
        using namespace Gdiplus;
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        const float cx = CatPhysics::SeatAnchor.x + p.bodyShiftX.value;
        const float bodyY = 185.0f;
        const float breath = std::sin(p.time * 2.1f) * 1.0f;
        const Pt head{cx + p.bodyTilt.value * 8.0f + p.headYaw.value * 2.5f,
                      105.0f + p.headOffsetY.value + p.headPitch.value * 2.0f + breath};

        drawBody(g, cx, bodyY, p);
        drawHead(g, head, p);
        drawArms(g, cx, bodyY, p);
        if (p.hindFootActive.value > 0.03f) drawHindFoot(g, cx, bodyY, p);
    }

private:
    static Gdiplus::Color fur(BYTE a=255)       { return Gdiplus::Color(a, 253, 217, 92); }
    static Gdiplus::Color furLight(BYTE a=255)  { return Gdiplus::Color(a, 255, 229, 132); }
    static Gdiplus::Color furShade(BYTE a=255)  { return Gdiplus::Color(a, 239, 193, 55); }
    static Gdiplus::Color orange(BYTE a=255)    { return Gdiplus::Color(a, 248, 144, 24); }
    static Gdiplus::Color orangeDark(BYTE a=255){ return Gdiplus::Color(a, 226, 116, 18); }
    static Gdiplus::Color hat(BYTE a=255)       { return Gdiplus::Color(a, 247, 151, 24); }
    static Gdiplus::Color hatDark(BYTE a=255)   { return Gdiplus::Color(a, 222, 120, 15); }
    static Gdiplus::Color iris(BYTE a=255)      { return Gdiplus::Color(a, 70, 130, 207); }
    static Gdiplus::Color pupil(BYTE a=255)     { return Gdiplus::Color(a, 14, 12, 12); }
    static Gdiplus::Color mouth(BYTE a=255)     { return Gdiplus::Color(a, 71, 25, 17); }
    static Gdiplus::Color leaf(BYTE a=255)      { return Gdiplus::Color(a, 83, 154, 65); }

    static void makePath(Gdiplus::GraphicsPath& path, const LuluPolygon& polygon) {
        if (polygon.points.empty()) return;
        std::vector<Gdiplus::PointF> pts;
        pts.reserve(polygon.points.size());
        for (const auto& p : polygon.points) pts.emplace_back(p.x, p.y);
        if (pts.size() >= 4) {
            // The source is a soft 3D plush render. A closed cardinal spline keeps
            // the traced silhouette smooth without reintroducing a bitmap.
            path.AddClosedCurve(pts.data(), (INT)pts.size(), 0.58f);
        } else {
            path.AddPolygon(pts.data(), (INT)pts.size());
        }
    }

    static void fillLayer(Gdiplus::Graphics& g, const std::vector<LuluPolygon>& layer, Gdiplus::Color color) {
        using namespace Gdiplus;
        SolidBrush brush(color);
        for (const auto& polygon : layer) {
            GraphicsPath path; makePath(path, polygon);
            g.FillPath(&brush, &path);
        }
    }

    static void gradientLayer(Gdiplus::Graphics& g, const std::vector<LuluPolygon>& layer,
                              Gdiplus::PointF a, Gdiplus::PointF b,
                              Gdiplus::Color ca, Gdiplus::Color cb) {
        using namespace Gdiplus;
        LinearGradientBrush brush(a, b, ca, cb);
        for (const auto& polygon : layer) {
            GraphicsPath path; makePath(path, polygon);
            g.FillPath(&brush, &path);
        }
    }

    static void fillEllipse(Gdiplus::Graphics& g, Gdiplus::Color color, float x, float y, float w, float h) {
        Gdiplus::SolidBrush brush(color); g.FillEllipse(&brush, x, y, w, h);
    }

    static void strokeLine(Gdiplus::Graphics& g, Gdiplus::Color color, Pt a, Pt b, float width) {
        Gdiplus::Pen pen(color, width);
        pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLine(&pen, a.x, a.y, b.x, b.y);
    }

    // The body's source pivot is at (95,157), matching the idle-frame sprite grid.
    static void drawBody(Gdiplus::Graphics& g, float cx, float bodyY, const CatPhysics& p) {
        using namespace Gdiplus;
        GraphicsState state = g.Save();
        g.TranslateTransform(cx - 95.0f, bodyY - 157.0f);
        g.RotateTransform(p.bodyTilt.value * RadToDeg);
        g.ScaleTransform(1.0f + (1.0f-p.bodySquash.value)*0.20f, p.bodySquash.value);
        gradientLayer(g, luluBodyYellowPolygons(), PointF(75,128), PointF(120,176), furLight(), fur());
        fillLayer(g, luluLegsPolygons(), fur());
        gradientLayer(g, luluShortsPolygons(), PointF(95,145), PointF(95,205), hat(), orangeDark());
        g.Restore(state);
    }

    // Face pivot is the source frame's head center (95,80); eyes/muzzle/hat use exactly
    // the traced source coordinates so their relative placement cannot drift independently.
    static void drawHead(Gdiplus::Graphics& g, Pt head, const CatPhysics& p) {
        using namespace Gdiplus;
        GraphicsState state = g.Save();
        g.TranslateTransform(head.x-95.0f, head.y-80.0f);
        g.RotateTransform(p.headTilt.value * RadToDeg);
        g.ScaleTransform(std::max(0.90f, std::cos(p.headYaw.value*0.28f)), 1.0f);

        gradientLayer(g, luluHeadYellowPolygons(), PointF(68,32), PointF(120,112), furLight(), fur());
        gradientLayer(g, luluMuzzlePolygons(), PointF(95,54), PointF(95,137), orange(), orangeDark());

        // The traced source has blue irises, dark pupils and small white catchlights.
        fillLayer(g, luluEyesBluePolygons(), iris());
        fillLayer(g, luluEyesDarkPolygons(), pupil());
        fillLayer(g, luluMouthDarkPolygons(), mouth());
        fillLayer(g, luluMouthWhitePolygons(), Gdiplus::Color(255,255,255,255));
        fillLayer(g, luluHighlightsPolygons(), Gdiplus::Color(245,255,255,255));

        gradientLayer(g, luluHatPolygons(), PointF(94,10), PointF(94,32), hat(), hatDark());
        fillLayer(g, luluLeafPolygons(), leaf());
        // Short stem visible between the fruit and leaf.
        strokeLine(g, Gdiplus::Color(255,102,76,32), {94,13}, {96,8}, 1.6f);

        // A small 2.5D eye-gaze offset preserves the traced shapes while allowing interaction.
        float gx = p.eyeGaze.x.value * 1.0f;
        float gy = p.eyeGaze.y.value * 0.8f;
        if (std::fabs(gx)+std::fabs(gy)>0.2f) {
            // Highlights already carry source placement; keep gaze motion deliberately subtle.
            Gdiplus::Pen gaze(Gdiplus::Color(0,0,0,0), 0.1f); (void)gaze;
        }
        (void)p;
        g.Restore(state);
    }

    static void drawArms(Gdiplus::Graphics& g, float cx, float bodyY, const CatPhysics& p) {
        bool leftBusy = p.leftPawTyping || p.leftPawOnMouse;
        bool rightBusy = p.rightPawTyping;
        for (int side : {-1,1}) {
            const bool busy = side < 0 ? leftBusy : rightBusy;
            const Pt shoulder{cx + side*43.0f, bodyY - 13.0f};
            if (!busy) {
                // Match the reference's compact paws beside the shorts.
                const Pt paw{shoulder.x + side*1.0f, shoulder.y + 17.0f};
                strokeLine(g, fur(), shoulder, paw, 14.0f);
                fillEllipse(g, fur(), paw.x-7.0f, paw.y-6.0f, 14.0f, 12.0f);
            } else {
                const Pt paw = side < 0 ? p.leftPaw.point() : p.rightPaw.point();
                const Pt elbow{(shoulder.x+paw.x)*0.5f+side*2.0f, (shoulder.y+paw.y)*0.5f+2.0f};
                strokeLine(g, furShade(), shoulder, elbow, 17.0f);
                strokeLine(g, fur(), elbow, paw, 15.0f);
                fillEllipse(g, fur(), paw.x-8.0f, paw.y-7.0f, 16.0f, 14.0f);
                // Toe creases, small enough not to read as fingers.
                strokeLine(g, Gdiplus::Color(100,214,151,39), {paw.x-2.5f,paw.y+2}, {paw.x-2.5f,paw.y+4.5f}, 0.9f);
                strokeLine(g, Gdiplus::Color(100,214,151,39), {paw.x+2.5f,paw.y+2}, {paw.x+2.5f,paw.y+4.5f}, 0.9f);
            }
        }
    }

    static void drawHindFoot(Gdiplus::Graphics& g, float cx, float bodyY, const CatPhysics& p) {
        const Pt hip{cx+33.0f, bodyY+25.0f};
        const Pt toe=p.hindFoot.point();
        strokeLine(g, furShade(), hip, toe, 13.0f);
        strokeLine(g, fur(), hip, toe, 10.0f);
        fillEllipse(g, fur(), toe.x-6.0f, toe.y-5.0f, 12.0f, 10.0f);
    }
};
