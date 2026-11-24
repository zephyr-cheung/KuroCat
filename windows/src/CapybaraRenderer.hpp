#pragma once
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cmath>
#include <algorithm>
#include "CatPhysics.hpp"
#include "LuluShapes.hpp"

// 水豚噜噜（Lulu）矢量渲染器。
// 轮廓数据来自 scripts/trace-lulu-shapes.py：从 MIT 授权的参考图描出的多边形（无位图）。
// 复用 CatPhysics 的头部 yaw/pitch/tilt、爪子弹簧、打字吸附与摸头/睡觉状态。
class CapybaraRenderer {
public:
    static constexpr float RadToDeg = 57.2957795f;

    static CatHitZone hitTest(Pt p, const CatPhysics& physics) {
        float cx = CatPhysics::SeatAnchor.x + physics.bodyShiftX.value;
        float hdx = (p.x - cx) / 64.0f;
        float hdy = (p.y - 112.0f) / 60.0f;
        if (hdx * hdx + hdy * hdy <= 1.0f) return CatHitZone::Head;

        float bdx = (p.x - cx) / 58.0f;
        float bdy = (p.y - 196.0f) / 44.0f;
        if (bdx * bdx + bdy * bdy <= 1.0f) return CatHitZone::Body;

        if (p.x >= 58.0f && p.x <= 294.0f && p.y >= 232.0f && p.y <= 296.0f) return CatHitZone::Body;
        return CatHitZone::None;
    }

    static void draw(Gdiplus::Graphics& g, const CatPhysics& physics, const CatSettings& settings) {
        using namespace Gdiplus;
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

        float breath = std::sin(physics.time * 2.1f) * 1.6f;
        float cx = CatPhysics::SeatAnchor.x + physics.bodyShiftX.value;
        float bodyY = 217.0f;
        Pt head = { cx + physics.bodyTilt.value * 26.0f + physics.headYaw.value * 6.0f,
                    107.0f + physics.headOffsetY.value + physics.headPitch.value * 4.0f + breath * 1.2f };

        drawBody(g, cx, bodyY, physics);
        drawHead(g, head, physics);
        drawArms(g, cx, bodyY, physics);
        if (physics.hindFootActive.value > 0.03f) drawHindFoot(g, cx, bodyY, physics);
    }

private:
    // ---------- palette (sampled from the reference art) ----------
    static Gdiplus::Color fur(BYTE a = 255)        { return Gdiplus::Color(a, 251, 213, 74); }
    static Gdiplus::Color furLight(BYTE a = 255)   { return Gdiplus::Color(a, 255, 232, 133); }
    static Gdiplus::Color furShade(BYTE a = 255)   { return Gdiplus::Color(a, 236, 183, 44); }
    static Gdiplus::Color muzzle(BYTE a = 255)     { return Gdiplus::Color(a, 248, 144, 24); }
    static Gdiplus::Color muzzleDark(BYTE a = 255) { return Gdiplus::Color(a, 228, 117, 16); }
    static Gdiplus::Color shorts(BYTE a = 255)     { return Gdiplus::Color(a, 238, 138, 26); }
    static Gdiplus::Color shortsDark(BYTE a = 255) { return Gdiplus::Color(a, 206, 108, 12); }
    static Gdiplus::Color hat(BYTE a = 255)        { return Gdiplus::Color(a, 247, 161, 28); }
    static Gdiplus::Color hatDark(BYTE a = 255)    { return Gdiplus::Color(a, 220, 126, 12); }
    static Gdiplus::Color iris(BYTE a = 255)       { return Gdiplus::Color(a, 74, 122, 186); }
    static Gdiplus::Color pupil(BYTE a = 255)      { return Gdiplus::Color(a, 24, 18, 13); }
    static Gdiplus::Color mouthCol(BYTE a = 255)   { return Gdiplus::Color(a, 84, 30, 17); }
    static Gdiplus::Color leaf(BYTE a = 255)       { return Gdiplus::Color(a, 90, 162, 74); }

    static void fillEllipse(Gdiplus::Graphics& g, Gdiplus::Color c, float x, float y, float w, float h) {
        using namespace Gdiplus;
        SolidBrush b(c);
        g.FillEllipse(&b, x, y, w, h);
    }

    static void addRoundedRect(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r) {
        float d = std::min(r * 2.0f, std::min(w, h));
        if (d <= 0.5f) { p.AddRectangle(Gdiplus::RectF(x, y, w, h)); return; }
        p.AddArc(x, y, d, d, 180.0f, 90.0f);
        p.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
        p.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
        p.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
        p.CloseFigure();
    }

    static void drawCapsule(Gdiplus::Graphics& g, Gdiplus::Color c, Pt a, Pt b, float width) {
        using namespace Gdiplus;
        Pen p(c, width);
        p.SetStartCap(LineCapRound);
        p.SetEndCap(LineCapRound);
        g.DrawLine(&p, PointF(a.x, a.y), PointF(b.x, b.y));
    }

    static void makePath(Gdiplus::GraphicsPath& path, const std::vector<LuluPoint>& pts) {
        using namespace Gdiplus;
        if (pts.empty()) return;
        std::vector<PointF> fp;
        fp.reserve(pts.size());
        for (const auto& p : pts) fp.push_back(PointF(p.x, p.y));
        path.AddPolygon(fp.data(), (INT)fp.size());
    }

    static void drawShape(Gdiplus::Graphics& g, const std::vector<LuluPoint>& pts, Gdiplus::Color c) {
        if (pts.empty()) return;
        Gdiplus::GraphicsPath p;
        makePath(p, pts);
        Gdiplus::SolidBrush b(c);
        g.FillPath(&b, &p);
    }

    static void drawShapeGradient(Gdiplus::Graphics& g, const std::vector<LuluPoint>& pts,
                                  Gdiplus::PointF from, Gdiplus::PointF to,
                                  Gdiplus::Color c0, Gdiplus::Color c1) {
        if (pts.empty()) return;
        using namespace Gdiplus;
        GraphicsPath p;
        makePath(p, pts);
        LinearGradientBrush b(from, to, c0, c1);
        g.FillPath(&b, &p);
    }

    // ---------- body ----------

    static void drawBody(Gdiplus::Graphics& g, float cx, float bodyY, const CatPhysics& physics) {
        using namespace Gdiplus;
        float sy = physics.bodySquash.value, sx = 1.0f + (1.0f - sy) * 0.55f;

        GraphicsState st = g.Save();
        g.TranslateTransform(cx, bodyY);
        g.RotateTransform(physics.bodyTilt.value * RadToDeg);
        g.ScaleTransform(sx, sy);

        // Chubby plush body, nearly as wide as the head.
        GraphicsPath body;
        body.StartFigure();
        body.AddBezier(PointF(-48, -74), PointF(-58, -60), PointF(-58, -24), PointF(-57, -6));
        body.AddBezier(PointF(-57, 8), PointF(-56, 18), PointF(-50, 22), PointF(-42, 23));
        body.AddLine(PointF(-42, 23), PointF(42, 23));
        body.AddBezier(PointF(50, 22), PointF(56, 18), PointF(57, 8), PointF(57, -6));
        body.AddBezier(PointF(58, -24), PointF(58, -60), PointF(48, -74), PointF(0, -78));
        body.AddBezier(PointF(-20, -76), PointF(-40, -76), PointF(-48, -74), PointF(-48, -74));
        body.CloseFigure();

        LinearGradientBrush bg(PointF(-36, -78), PointF(34, 24), furLight(), fur());
        g.FillPath(&bg, &body);
        {
            GraphicsState cs = g.Save();
            g.SetClip(&body, CombineModeIntersect);
            fillEllipse(g, Color(44, 255, 242, 168), -22.0f, -40.0f, 44.0f, 46.0f);
            g.Restore(cs);
        }

        // Short legs + soft feet.
        GraphicsPath legL;
        addRoundedRect(legL, -36.0f, 14.0f, 22.0f, 20.0f, 10.0f);
        GraphicsPath legR;
        addRoundedRect(legR, 14.0f, 14.0f, 22.0f, 20.0f, 10.0f);
        SolidBrush legBrush(fur());
        g.FillPath(&legBrush, &legL);
        g.FillPath(&legBrush, &legR);

        // Orange shorts.
        GraphicsPath sh;
        addRoundedRect(sh, -57.0f, -18.0f, 114.0f, 35.0f, 13.0f);
        LinearGradientBrush shb(PointF(0, -18), PointF(0, 17), shorts(), shortsDark());
        g.FillPath(&shb, &sh);
        Pen waist(Color(115, 255, 224, 150), 1.8f);
        g.DrawLine(&waist, PointF(-50.0f, -12.0f), PointF(50.0f, -12.0f));
        Pen seam(Color(80, 150, 70, 8), 1.4f);
        g.DrawLine(&seam, PointF(0.0f, -2.0f), PointF(0.0f, 15.0f));

        g.Restore(st);
    }

    // ---------- head ----------

    static void drawHead(Gdiplus::Graphics& g, Pt head, const CatPhysics& physics) {
        using namespace Gdiplus;
        float yaw = physics.headYaw.value;
        float scaleX = 1.0f - std::fabs(yaw) * 0.030f;

        GraphicsState st = g.Save();
        g.TranslateTransform(head.x, head.y);
        g.RotateTransform(physics.headTilt.value * RadToDeg);
        g.ScaleTransform(scaleX, 1.0f);

        // Head silhouette (ears are part of the traced outline).
        GraphicsPath hp;
        makePath(hp, luluHeadShape());
        {
            LinearGradientBrush hb(PointF(-24, -46), PointF(28, 58), furLight(), fur());
            g.FillPath(&hb, &hp);
            GraphicsState cs = g.Save();
            g.SetClip(&hp, CombineModeIntersect);
            fillEllipse(g, Color(44, 255, 246, 196), -46.0f, -46.0f, 64.0f, 30.0f);
            fillEllipse(g, Color(30, 208, 146, 18), -26.0f, 30.0f, 96.0f, 40.0f);
            g.Restore(cs);
        }

        // Inner ears (warm orange, tucked toward the face).
        for (int side = -1; side <= 1; side += 2) {
            GraphicsState ies = g.Save();
            g.TranslateTransform(side * 45.0f, -36.0f - physics.earFold.value * 4.0f);
            g.RotateTransform(side * 18.0f);
            fillEllipse(g, Color(205, 228, 118, 18), -4.5f, -6.0f, 9.0f, 12.0f);
            g.Restore(ies);
        }

        // Muzzle, with a touch of 2.5D parallax.
        GraphicsState ms = g.Save();
        g.TranslateTransform(yaw * 3.0f, physics.headPitch.value * 2.0f);
        g.ScaleTransform(std::max(0.90f, std::cos(yaw * 0.42f)), 1.0f);
        GraphicsPath mp;
        makePath(mp, luluMuzzleShape());
        {
            LinearGradientBrush mb(PointF(0, -18), PointF(0, 56), muzzle(), muzzleDark());
            g.FillPath(&mb, &mp);
            GraphicsState cs = g.Save();
            g.SetClip(&mp, CombineModeIntersect);
            fillEllipse(g, Color(44, 255, 206, 124), -30.0f, -16.0f, 60.0f, 16.0f);
            fillEllipse(g, Color(40, 190, 92, 10), -44.0f, 30.0f, 88.0f, 32.0f);
            g.Restore(cs);
        }
        g.Restore(ms);

        drawEyes(g, physics);
        drawMouth(g, physics);

        // Mandarin hat, sitting on the crown.
        GraphicsState hs = g.Save();
        g.TranslateTransform(yaw * 4.0f, -54.0f + physics.headPitch.value * 2.0f);
        g.RotateTransform(physics.headTilt.value * RadToDeg * 0.25f);
        {
            Pen stem(Color(255, 118, 84, 36), 2.0f);
            stem.SetStartCap(LineCapRound);
            g.DrawLine(&stem, PointF(0.0f, -15.0f), PointF(1.5f, -22.0f));
            GraphicsState ls = g.Save();
            g.TranslateTransform(8.0f, -21.0f);
            g.RotateTransform(-32.0f);
            fillEllipse(g, leaf(), -7.0f, -4.0f, 14.0f, 8.0f);
            g.Restore(ls);
            LinearGradientBrush hb(PointF(0, -18), PointF(0, 18), hat(), hatDark());
            g.FillEllipse(&hb, -21.0f, -19.0f, 42.0f, 38.0f);
            fillEllipse(g, Color(95, 255, 222, 150), -11.0f, -12.0f, 12.0f, 9.0f);
        }
        g.Restore(hs);

        g.Restore(st);
    }

    static void drawEyes(Gdiplus::Graphics& g, const CatPhysics& physics) {
        using namespace Gdiplus;
        float closure = physics.effectiveEyeClosure();
        float hunt = physics.huntIntensity.value;
        float gazeX = physics.eyeGaze.x.value * 2.0f;
        float gazeY = physics.eyeGaze.y.value * 1.4f;

        for (int side = -1; side <= 1; side += 2) {
            float ex = side * 23.0f + physics.headYaw.value * 2.5f;
            float ey = -25.0f + physics.headPitch.value * 1.5f;

            if (physics.isHappyClosedEyes() || closure > 0.72f) {
                GraphicsPath arc;
                arc.AddBezier(PointF(ex - 7.0f, ey + 2.0f), PointF(ex - 2.5f, ey - 4.5f),
                              PointF(ex + 2.5f, ey - 4.5f), PointF(ex + 7.0f, ey + 2.0f));
                Pen pp(mouthCol(), 2.2f);
                pp.SetStartCap(LineCapRound);
                pp.SetEndCap(LineCapRound);
                g.DrawPath(&pp, &arc);
                continue;
            }
            if (physics.isBlissfulSquint()) {
                Pen pp(mouthCol(), 2.2f);
                pp.SetStartCap(LineCapRound);
                pp.SetEndCap(LineCapRound);
                g.DrawLine(&pp, PointF(ex - 6.5f, ey), PointF(ex + 6.5f, ey));
                continue;
            }

            float sy = std::clamp(1.0f - closure * 0.95f, 0.10f, 1.0f);
            GraphicsState st = g.Save();
            g.TranslateTransform(ex, ey);
            g.ScaleTransform(1.0f, sy);
            fillEllipse(g, Color(58, 205, 143, 30), -8.4f, -8.6f, 16.8f, 17.2f);
            fillEllipse(g, iris(), -7.6f, -7.8f, 15.2f, 15.6f);
            float pr = 6.9f + hunt * 0.7f;
            fillEllipse(g, pupil(), -pr + gazeX, -pr + gazeY, pr * 2.0f, pr * 2.06f);
            fillEllipse(g, Color(245, 255, 255, 255), -4.3f + gazeX, -5.4f + gazeY, 4.2f, 4.2f);
            fillEllipse(g, Color(150, 255, 255, 255), 1.9f + gazeX, 3.2f + gazeY, 1.9f, 1.9f);
            g.Restore(st);
        }
    }

    static void drawMouth(Gdiplus::Graphics& g, const CatPhysics& physics) {
        using namespace Gdiplus;
        float open = physics.huntIntensity.value;
        float w = 17.0f + open * 2.0f;
        float h = 12.5f + open * 3.0f;
        float y = 17.0f;

        GraphicsPath mp;
        mp.StartFigure();
        mp.AddBezier(PointF(-w, y + 1.0f), PointF(-w * 0.55f, y + h), PointF(w * 0.55f, y + h), PointF(w, y + 1.0f));
        mp.AddBezier(PointF(w, y + 1.0f), PointF(w * 0.55f, y - 2.2f), PointF(-w * 0.55f, y - 2.2f), PointF(-w, y + 1.0f));
        mp.CloseFigure();
        SolidBrush mb(mouthCol());
        g.FillPath(&mb, &mp);

        GraphicsPath teeth;
        teeth.StartFigure();
        teeth.AddBezier(PointF(-w * 0.80f, y + 1.2f), PointF(-w * 0.30f, y - 0.2f),
                        PointF(w * 0.30f, y - 0.2f), PointF(w * 0.80f, y + 1.2f));
        teeth.AddBezier(PointF(w * 0.80f, y + 1.2f), PointF(w * 0.40f, y + 2.6f),
                        PointF(-w * 0.40f, y + 2.6f), PointF(-w * 0.80f, y + 1.2f));
        teeth.CloseFigure();
        SolidBrush tb(Color(255, 255, 255, 255));
        g.FillPath(&tb, &teeth);
    }

    // ---------- arms (idle stubs / typing on the keyboard) ----------

    static void drawArms(Gdiplus::Graphics& g, float cx, float bodyY, const CatPhysics& physics) {
        using namespace Gdiplus;
        bool leftBusy = physics.leftPawTyping || physics.leftPawOnMouse;
        bool rightBusy = physics.rightPawTyping;

        for (int side = -1; side <= 1; side += 2) {
            bool busy = side < 0 ? leftBusy : rightBusy;
            Pt shoulder = { cx + side * 43.0f, bodyY - 26.0f };

            if (!busy) {
                Pt end = { shoulder.x + side * 2.0f, shoulder.y + 26.0f };
                drawCapsule(g, fur(), shoulder, end, 18.0f);
                fillEllipse(g, fur(), end.x - 9.0f, end.y - 7.0f, 18.0f, 16.0f);
                Pen crease(Color(70, 214, 160, 40), 1.2f);
                crease.SetStartCap(LineCapRound);
                g.DrawLine(&crease, PointF(end.x - 4.0f, end.y + 2.0f), PointF(end.x - 4.0f, end.y + 5.0f));
                g.DrawLine(&crease, PointF(end.x + 4.0f, end.y + 2.0f), PointF(end.x + 4.0f, end.y + 5.0f));
                continue;
            }

            Pt paw = side < 0 ? physics.leftPaw.point() : physics.rightPaw.point();
            Pt mid = { (shoulder.x + paw.x) * 0.5f + side * 2.0f, (shoulder.y + paw.y) * 0.5f + 4.0f };
            drawCapsule(g, furShade(), shoulder, mid, 20.0f);
            drawCapsule(g, fur(), shoulder, paw, 18.0f);
            fillEllipse(g, furShade(), paw.x - 10.0f, paw.y - 9.0f, 20.0f, 18.0f);
            fillEllipse(g, fur(), paw.x - 9.0f, paw.y - 9.0f, 18.0f, 16.0f);
            Pen toe(Color(120, 208, 150, 34), 1.2f);
            toe.SetStartCap(LineCapRound);
            g.DrawLine(&toe, PointF(paw.x - 3.0f, paw.y + 1.0f), PointF(paw.x - 3.0f, paw.y + 4.8f));
            g.DrawLine(&toe, PointF(paw.x + 3.0f, paw.y + 1.0f), PointF(paw.x + 3.0f, paw.y + 4.8f));
        }
    }

    static void drawHindFoot(Gdiplus::Graphics& g, float cx, float bodyY, const CatPhysics& physics) {
        using namespace Gdiplus;
        Pt hip = { cx + 34.0f, bodyY + 40.0f };
        Pt t = physics.hindFoot.point();
        drawCapsule(g, furShade(), hip, t, 15.0f);
        drawCapsule(g, fur(), hip, t, 13.0f);
        fillEllipse(g, fur(), t.x - 8.0f, t.y - 7.0f, 16.0f, 14.0f);
    }
};
