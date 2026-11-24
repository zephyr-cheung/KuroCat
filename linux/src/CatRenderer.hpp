#pragma once
#include <cairo.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include "CatPhysics.hpp"
#include "../../shared/CoatPatterns.hpp"

enum class CatHitZone {
    None,
    Head,
    Body
};

struct RGBA {
    double r, g, b, a;
};

class CatRenderer {
public:
    static CatHitZone hitTest(Pt p, const CatPhysics& physics, CatCoatTheme coat = CatCoatTheme::ClassicBlack) {
        float cx = CatPhysics::SeatAnchor.x + physics.bodyShiftX.value;
        float breath = std::sin(physics.time * 2.1f) * 1.6f;
        float siameseLift = (coat == CatCoatTheme::Siamese) ? -3.0f : 0.0f;
        Pt headCenter = { cx + physics.bodyTilt.value * 36.0f + physics.headYaw.value * 7.5f,
                          126.0f + siameseLift + physics.headOffsetY.value + physics.headPitch.value * 4.8f + breath * 1.1f };

        float hdx = (p.x - headCenter.x) / 44.0f;
        float hdy = (p.y - headCenter.y) / 38.0f;
        if (hdx * hdx + hdy * hdy <= 1.0f) return CatHitZone::Head;

        float bdx = (p.x - cx) / 58.0f;
        float bdy = (p.y - 196.0f) / 56.0f;
        if (bdx * bdx + bdy * bdy <= 1.0f) return CatHitZone::Body;

        if (p.x >= 58.0f && p.x <= 294.0f && p.y >= 232.0f && p.y <= 296.0f) return CatHitZone::Body;
        return CatHitZone::None;
    }

    static void draw(cairo_t* cr, const CatPhysics& physics, const CatSettings& settings) {
        cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

        float breath = std::sin(physics.time * 2.1f) * 1.6f;
        float seatX = CatPhysics::SeatAnchor.x + physics.bodyShiftX.value;
        float seatY = CatPhysics::SeatAnchor.y;

        float siameseLift = (settings.coatTheme == CatCoatTheme::Siamese) ? -3.0f : 0.0f;
        Pt headCenter = {
            seatX + physics.bodyTilt.value * 36.0f + physics.headYaw.value * 7.5f,
            126.0f + siameseLift + physics.headOffsetY.value + physics.headPitch.value * 4.8f + breath * 1.1f
        };

        drawMouse(cr, physics, settings.keyboardTheme);
        drawKeyboard(cr, physics, settings.keyboardTheme, settings.eyeTheme, settings.showKeyLegends);
        drawTail(cr, seatX, seatY, physics, settings.coatTheme);
        drawEarsLayer(cr, headCenter, physics, settings.coatTheme);
        drawUnifiedSilhouette(cr, seatX, seatY, headCenter, breath, physics, settings.coatTheme);

        if (physics.hindFootActive.value > 0.03f) {
            drawHindFoot(cr, seatX, seatY, physics, settings.coatTheme);
        }

        drawHeadFeatures(cr, headCenter, physics, settings.eyeTheme, settings.coatTheme);
        drawFrontPaws(cr, seatX, seatY, breath, physics, settings.coatTheme);

        if (!physics.floatingKeycaps.empty()) {
            drawFloatingKeycaps(cr, physics);
        }
        drawSparkles(cr, physics, settings.eyeTheme);
    }

private:
    static constexpr double PI = 3.14159265358979323846;

    static constexpr RGBA catBlack      = { 0.085, 0.085, 0.095, 1.0 };
    static constexpr RGBA contourMint   = { 0.82,  0.92,  0.89,  0.76 };
    static constexpr RGBA whiskerWhite  = { 0.88,  0.90,  0.92,  0.68 };
    static constexpr RGBA siameseCream    = { 0.965, 0.937, 0.894, 1.0 };
    static constexpr RGBA siameseLatte    = { 0.918, 0.859, 0.784, 1.0 };
    static constexpr RGBA siameseMocha    = { 0.416, 0.314, 0.255, 1.0 };
    static constexpr RGBA siamesePoint    = { 0.208, 0.149, 0.122, 1.0 };
    static constexpr RGBA siameseInnerEar = { 0.365, 0.275, 0.228, 1.0 };
    static constexpr RGBA siameseOut      = { 0.68,  0.59,  0.50,  0.42 };
    static constexpr RGBA siameseCrease   = { 0.553, 0.471, 0.404, 0.54 };
    static constexpr RGBA siameseWhisk    = { 0.478, 0.388, 0.325, 0.62 };

    static void setCol(cairo_t* cr, RGBA c, double alphaMul = 1.0) {
        cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a * alphaMul);
    }

    static bool patterned(CatCoatTheme c) { return c == CatCoatTheme::Shorthair || c == CatCoatTheme::OrangeWhite || c == CatCoatTheme::Tuxedo || c == CatCoatTheme::Calico || c == CatCoatTheme::GingerTabby; }
    static bool hasTailRings(CatCoatTheme c) { return c == CatCoatTheme::Shorthair || c == CatCoatTheme::OrangeWhite || c == CatCoatTheme::GingerTabby; }
    static RGBA rgb(unsigned int hex, double a=1.0) { return {((hex>>16)&255)/255.0,((hex>>8)&255)/255.0,(hex&255)/255.0,a}; }
    static RGBA bodyColor(CatCoatTheme c) {
        switch (c) {
            case CatCoatTheme::Shorthair:  return rgb(0xC2C5C1);
            case CatCoatTheme::OrangeWhite:return rgb(0xF2EEE4);
            case CatCoatTheme::Tuxedo:     return rgb(0x1A1A1C);
            case CatCoatTheme::Calico:     return rgb(0xF7F3EC);
            case CatCoatTheme::GingerTabby:return rgb(0xE2A45E);
            case CatCoatTheme::BritishBlue:return rgb(0x8A97A5);
            default: return catBlack;
        }
    }
    static RGBA limbColor(CatCoatTheme c) {
        switch (c) {
            case CatCoatTheme::OrangeWhite:return rgb(0xD8AA73);
            case CatCoatTheme::BritishBlue:return rgb(0x8492A0);
            default: return bodyColor(c);
        }
    }
    static RGBA pawColor(CatCoatTheme c) {
        switch (c) {
            case CatCoatTheme::Shorthair:
            case CatCoatTheme::OrangeWhite:
            case CatCoatTheme::Calico:     return rgb(0xF2EEE4);
            case CatCoatTheme::Tuxedo:     return rgb(0xF4F1EA);
            case CatCoatTheme::GingerTabby:return rgb(0xF7E8D2);
            case CatCoatTheme::BritishBlue:return rgb(0x99A6B3);
            case CatCoatTheme::Siamese:    return siamesePoint;
            default: return catBlack;
        }
    }
    static RGBA earTailColor(CatCoatTheme c) {
        switch (c) {
            case CatCoatTheme::Siamese:    return siamesePoint;
            case CatCoatTheme::Calico:     return rgb(0xD99A5B);
            case CatCoatTheme::BritishBlue:return rgb(0x7D8A98);
            default: return limbColor(c);
        }
    }
    static RGBA earColor(CatCoatTheme c, bool isLeft) {
        if (c == CatCoatTheme::Calico) return isLeft ? rgb(0xD99A5B) : rgb(0x413A3A);
        return earTailColor(c);
    }
    static RGBA creaseColor(CatCoatTheme c) {
        switch (c) {
            case CatCoatTheme::Tuxedo:     return rgb(0xC9CDD3,0.42);
            case CatCoatTheme::BritishBlue:return rgb(0x53606C,0.55);
            case CatCoatTheme::GingerTabby:return rgb(0x9C6432,0.58);
            default:
                if (patterned(c)) return rgb(0x8B8171,0.60);
                return contourMint;
        }
    }
    static RGBA whiskerColor(CatCoatTheme c) {
        switch (c) {
            case CatCoatTheme::Siamese:    return siameseWhisk;
            case CatCoatTheme::Tuxedo:     return rgb(0xD9DCE1,0.67);
            case CatCoatTheme::BritishBlue:return rgb(0xE6ECF2,0.59);
            default:
                if (patterned(c)) return rgb(0x8D8170,0.68);
                return whiskerWhite;
        }
    }
    static RGBA tailRingColor(CatCoatTheme c) {
        switch (c) {
            case CatCoatTheme::Shorthair:  return rgb(0x55595A,0.50);
            case CatCoatTheme::GingerTabby:return rgb(0xA9723C,0.59);
            default: return rgb(0xA57543,0.50);
        }
    }
    static const std::vector<CoatMark>* marksFor(CatCoatTheme c) {
        switch (c) {
            case CatCoatTheme::Shorthair:  return &shorthairMarks();
            case CatCoatTheme::OrangeWhite:return &orangeWhiteMarks();
            case CatCoatTheme::Tuxedo:     return &tuxedoMarks();
            case CatCoatTheme::Calico:     return &calicoMarks();
            case CatCoatTheme::GingerTabby:return &gingerTabbyMarks();
            case CatCoatTheme::BritishBlue:return &britishBlueMarks();
            default: return nullptr;
        }
    }
    static Pt coatMarkPoint(CoatPoint p, bool headSurface, bool skull, CatCoatTheme coat, float seatX, float seatY, Pt head, const CatPhysics& physics) {
        if (headSurface) {
            bool wrapped = coat != CatCoatTheme::Shorthair;
            float depth = wrapped ? 29.6f*std::sqrt(std::max(0.0f,1-p.x*p.x/(46*46)-p.y*p.y/(40*40))) : 27.5f;
            Pt q=skull ? Pt{p.x,p.y} : projectHead3D(p.x,p.y,depth,physics.headYaw.value*0.50f,physics.headPitch.value*0.36f);
            float c=std::cos(physics.headTilt.value),s=std::sin(physics.headTilt.value);
            return {head.x+q.x*c-q.y*s,head.y+q.x*s+q.y*c};
        }
        float sy=physics.bodySquash.value,sx=1.0f+(1.0f-sy)*0.55f;
        float c=std::cos(physics.bodyTilt.value),s=std::sin(physics.bodyTilt.value);
        return {seatX+p.x*sx*c-p.y*sy*s,seatY+p.x*sx*s+p.y*sy*c};
    }
    static void drawCoatMarks(cairo_t* cr,float seatX,float seatY,Pt head,const CatPhysics& physics,CatCoatTheme coat) {
        const auto* marks=marksFor(coat);
        if (!marks) return;
        for (const auto& mark: *marks) {
            cairo_new_path(cr);
            bool first=true;
            for (const auto& point: mark.points) {
                Pt p=coatMarkPoint(point,mark.head,mark.skull,coat,seatX,seatY,head,physics);
                if (first) { cairo_move_to(cr,p.x,p.y); first=false; } else cairo_line_to(cr,p.x,p.y);
            }
            setCol(cr,rgb(mark.rgb,mark.alpha));
            if (mark.width==0) { cairo_close_path(cr);cairo_fill(cr); }
            else { cairo_set_line_width(cr,mark.width);cairo_set_line_cap(cr,CAIRO_LINE_CAP_ROUND);cairo_set_line_join(cr,CAIRO_LINE_JOIN_ROUND);cairo_stroke(cr); }
        }
    }

    static RGBA eyePrimary(EyeColorTheme t) {
        switch (t) {
            case EyeColorTheme::NeonChartreuse: return { 0.79, 0.95, 0.25, 1.0 };
            case EyeColorTheme::SiameseBlue:    return { 0.69, 0.855, 0.95, 1.0 };
            case EyeColorTheme::WarmAmber:      return { 0.98, 0.76, 0.22, 1.0 };
            case EyeColorTheme::IceCyan:        return { 0.45, 0.92, 0.96, 1.0 };
            case EyeColorTheme::ReferenceHazel: return {218.0/255,178.0/255,87.0/255,1};
            case EyeColorTheme::SoftAmber: return {217.0/255,173.0/255,85.0/255,1};
        }
        return { 0.79, 0.95, 0.25, 1.0 };
    }

    static RGBA blend(RGBA a, RGBA b, double t) {
        t = std::clamp(t, 0.0, 1.0);
        return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
    }

    static void quadCurveTo(cairo_t* cr, double x0, double y0, double cx, double cy, double x2, double y2) {
        double c1x = x0 + (2.0 / 3.0) * (cx - x0);
        double c1y = y0 + (2.0 / 3.0) * (cy - y0);
        double c2x = x2 + (2.0 / 3.0) * (cx - x2);
        double c2y = y2 + (2.0 / 3.0) * (cy - y2);
        cairo_curve_to(cr, c1x, c1y, c2x, c2y, x2, y2);
    }

    static void roundedRect(cairo_t* cr, double x, double y, double w, double h, double r) {
        r = std::min(r, std::min(w * 0.5, h * 0.5));
        cairo_new_sub_path(cr);
        cairo_arc(cr, x + w - r, y + r, r, -PI * 0.5, 0);
        cairo_arc(cr, x + w - r, y + h - r, r, 0, PI * 0.5);
        cairo_arc(cr, x + r, y + h - r, r, PI * 0.5, PI);
        cairo_arc(cr, x + r, y + r, r, PI, PI * 1.5);
        cairo_close_path(cr);
    }

    static void fillEllipse(cairo_t* cr, double cx, double cy, double rx, double ry) {
        cairo_save(cr);
        cairo_translate(cr, cx, cy);
        cairo_scale(cr, rx, ry);
        cairo_arc(cr, 0, 0, 1.0, 0, PI * 2.0);
        cairo_restore(cr);
    }

    static Pt projectHead3D(float x, float y, float z, float yawRad, float pitchRad) {
        float cy = std::cos(yawRad), sy = std::sin(yawRad);
        float x1 = x * cy + z * sy;
        float z1 = -x * sy + z * cy;
        float cp = std::cos(pitchRad), sp = std::sin(pitchRad);
        float y2 = y * cp + z1 * sp;
        return { x1, y2 };
    }

    static Pt quadPoint(Pt p0, Pt p1, Pt p2, float t) {
        float mt = 1.0f - t;
        return { mt * mt * p0.x + 2.0f * mt * t * p1.x + t * t * p2.x,
                 mt * mt * p0.y + 2.0f * mt * t * p1.y + t * t * p2.y };
    }

    static Pt quadTangent(Pt p0, Pt p1, Pt p2, float t) {
        float mt = 1.0f - t;
        return { 2.0f * mt * (p1.x - p0.x) + 2.0f * t * (p2.x - p1.x),
                 2.0f * mt * (p1.y - p0.y) + 2.0f * t * (p2.y - p1.y) };
    }

    // MARK: - Mini White Mouse (180° facing the cat)

    static void drawMouse(cairo_t* cr, const CatPhysics& physics, KeyboardTheme theme) {
        Pt center = { CatPhysics::MouseBaseCenter.x + physics.mouseOffset.point().x,
                      CatPhysics::MouseBaseCenter.y + physics.mouseOffset.point().y };
        float pressShift = std::max(physics.mousePressLeft.value, physics.mousePressRight.value) * 1.6f;

        cairo_save(cr);
        cairo_translate(cr, center.x, center.y + pressShift);
        cairo_rotate(cr, 0.22);

        // Shadow
        cairo_new_path(cr);
        fillEllipse(cr, 0, 4.0, 13.5, 15.5);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.24);
        cairo_fill(cr);

        RGBA bodyCol, borderCol, wheelCol;
        if (theme == KeyboardTheme::MidnightHacker) {
            bodyCol = {0.20, 0.21, 0.24, 1.0}; borderCol = {0.08, 0.08, 0.10, 1.0}; wheelCol = {0.78, 0.95, 0.25, 1.0};
        } else if (theme == KeyboardTheme::RetroCream) {
            bodyCol = {0.89, 0.87, 0.82, 1.0}; borderCol = {0.20, 0.19, 0.18, 1.0}; wheelCol = {0.88, 0.42, 0.24, 1.0};
        } else {
            bodyCol = {0.955, 0.950, 0.925, 1.0}; borderCol = {0.14, 0.14, 0.16, 1.0}; wheelCol = {0.96, 0.69, 0.16, 1.0};
        }

        cairo_new_path(cr);
        fillEllipse(cr, 0, 0, 13.5, 16.5);
        setCol(cr, bodyCol);
        cairo_fill_preserve(cr);

        if (physics.mousePressLeft.value > 0.05f || physics.mousePressRight.value > 0.05f) {
            cairo_save(cr);
            cairo_clip(cr);
            if (physics.mousePressLeft.value > 0.05f) {
                cairo_set_source_rgba(cr, 0.79, 0.95, 0.25, 0.38 * physics.mousePressLeft.value);
                cairo_rectangle(cr, 0, 0, 14, 17);
                cairo_fill(cr);
            }
            if (physics.mousePressRight.value > 0.05f) {
                cairo_set_source_rgba(cr, 0.79, 0.95, 0.25, 0.38 * physics.mousePressRight.value);
                cairo_rectangle(cr, -14, 0, 14, 17);
                cairo_fill(cr);
            }
            cairo_restore(cr);
        } else {
            cairo_new_path(cr);
        }

        // Front button split seam pointing away from cat (bottom)
        setCol(cr, borderCol, 0.32);
        cairo_set_line_width(cr, 1.05);
        cairo_move_to(cr, 0, 16.0);
        cairo_line_to(cr, 0, 1.5);
        cairo_stroke(cr);

        // Amber scroll wheel
        roundedRect(cr, -1.9, 2.5, 3.8, 8.5, 1.9);
        setCol(cr, wheelCol);
        cairo_fill(cr);

        // Outer border
        cairo_new_path(cr);
        fillEllipse(cr, 0, 0, 13.5, 16.5);
        setCol(cr, borderCol);
        cairo_set_line_width(cr, 1.7);
        cairo_stroke(cr);

        cairo_restore(cr);
    }

    // MARK: - Mini 65% Mechanical Keyboard (180° facing the cat)

    static void drawQuadPath(cairo_t* cr, Pt tl, Pt tr, Pt br, Pt bl) {
        cairo_new_path(cr);
        cairo_move_to(cr, tl.x, tl.y);
        cairo_line_to(cr, tr.x, tr.y);
        cairo_line_to(cr, br.x, br.y);
        cairo_line_to(cr, bl.x, bl.y);
        cairo_close_path(cr);
    }

    static std::string keycapLegend(PhysicalKey k) {
        switch (k) {
            case PhysicalKey::Escape: return "ESC";
            case PhysicalKey::Tab: return "⇥";
            case PhysicalKey::CapsLock: return "⇪";
            case PhysicalKey::LeftShift:
            case PhysicalKey::RightShift: return "⇧";
            case PhysicalKey::LeftControl:
            case PhysicalKey::RightControl: return "Ctrl";
            case PhysicalKey::LeftOption:
            case PhysicalKey::RightOption: return "Alt";
            case PhysicalKey::LeftCommand:
            case PhysicalKey::RightCommand: return "Super";
            case PhysicalKey::Backspace: return "⌫";
            case PhysicalKey::Enter: return "↵";
            case PhysicalKey::Space: return "";
            default: return keyBadgeLabel(k);
        }
    }

    static void drawKeyboard(cairo_t* cr, const CatPhysics& physics, KeyboardTheme theme, EyeColorTheme eyeTheme, bool showLegends) {
        Pt tl = KeyboardLayout::screenQuadPoint(-0.012f, -0.04f);
        Pt tr = KeyboardLayout::screenQuadPoint(1.012f, -0.04f);
        Pt bl = KeyboardLayout::screenQuadPoint(-0.016f, 1.04f);
        Pt br = KeyboardLayout::screenQuadPoint(1.016f, 1.04f);
        constexpr float depth = 4.5f;

        drawQuadPath(cr, {tl.x, tl.y + depth + 2}, {tr.x, tr.y + depth + 2},
                         {br.x, br.y + depth + 3}, {bl.x, bl.y + depth + 3});
        cairo_set_source_rgba(cr, 0, 0, 0, 0.22);
        cairo_fill(cr);

        RGBA baseCol, deckCol, keyCol, borderCol, accentCol;
        if (theme == KeyboardTheme::MidnightHacker) {
            baseCol = {0.06, 0.06, 0.08, 1}; deckCol = {0.10, 0.11, 0.13, 1};
            keyCol = {0.21, 0.22, 0.25, 1}; borderCol = {0.11, 0.11, 0.13, 0.95}; accentCol = {0.77, 0.94, 0.25, 1};
        } else if (theme == KeyboardTheme::RetroCream) {
            baseCol = {0.25, 0.24, 0.22, 1}; deckCol = {0.38, 0.36, 0.33, 1};
            keyCol = {0.90, 0.88, 0.82, 1}; borderCol = {0.68, 0.65, 0.60, 0.85}; accentCol = {0.88, 0.42, 0.24, 1};
        } else {
            baseCol = {0.10, 0.10, 0.12, 1}; deckCol = {0.14, 0.14, 0.16, 1};
            keyCol = {0.968, 0.964, 0.942, 1}; borderCol = {0.78, 0.77, 0.74, 0.75}; accentCol = {0.97, 0.70, 0.16, 1};
        }

        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        drawQuadPath(cr, {tl.x, tl.y + depth}, {tr.x, tr.y + depth}, {br.x, br.y + depth}, {bl.x, bl.y + depth});
        setCol(cr, baseCol);
        cairo_fill_preserve(cr);
        cairo_set_line_width(cr, 2.5);
        cairo_stroke(cr);

        drawQuadPath(cr, tl, tr, br, bl);
        setCol(cr, deckCol);
        cairo_fill_preserve(cr);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);

        constexpr float padU = 0.0028f;
        constexpr float padV = 0.014f;
        RGBA glowCol = eyePrimary(eyeTheme);

        for (const auto& cap : KeyboardLayout::keycaps()) {
            float glow = 0.0f;
            auto it = physics.keyGlow.find(cap.key);
            if (it != physics.keyGlow.end()) glow = it->second;

            float su0 = (1.0f - (cap.xUnits + cap.widthUnits)) + padU;
            float su1 = (1.0f - cap.xUnits) - padU;
            float sv0 = (1.0f - (cap.row + 1) / 5.0f) + padV;
            float sv1 = (1.0f - cap.row / 5.0f) - padV;

            float dy = glow * 1.9f;
            Pt kTL = KeyboardLayout::screenQuadPoint(su0, sv0); kTL.y += dy;
            Pt kTR = KeyboardLayout::screenQuadPoint(su1, sv0); kTR.y += dy;
            Pt kBR = KeyboardLayout::screenQuadPoint(su1, sv1); kBR.y += dy;
            Pt kBL = KeyboardLayout::screenQuadPoint(su0, sv1); kBL.y += dy;

            drawQuadPath(cr, kTL, kTR, kBR, kBL);
            RGBA fill = cap.isAmberAccent ? accentCol : keyCol;
            if (glow > 0.02f) fill = blend(fill, glowCol, glow * 0.55);

            setCol(cr, fill);
            cairo_fill_preserve(cr);
            setCol(cr, borderCol);
            cairo_set_line_width(cr, 0.65);
            cairo_stroke(cr);

            if (showLegends) {
                bool isSpace = (cap.key == PhysicalKey::Space);
                std::string legend = isSpace ? std::string("space") : keycapLegend(cap.key);
                if (!legend.empty()) {
                    double cx = (kTL.x + kTR.x + kBR.x + kBL.x) * 0.25;
                    double cy = (kTL.y + kTR.y + kBR.y + kBL.y) * 0.25;
                    if (isSpace) {
                        // The spacebar sits right under the cat: its legs hide the middle, and an
                        // unlabeled visible end looked like an odd blank patch beside the cat.
                        // Label the end that stays visible instead.
                        Pt sp = KeyboardLayout::screenQuadPoint(su0 + (su1 - su0) * 0.82f, (sv0 + sv1) * 0.5f);
                        cx = sp.x;
                        cy = sp.y + dy;
                    }
                    if (cap.isAmberAccent || theme != KeyboardTheme::MidnightHacker) {
                        cairo_set_source_rgba(cr, 0.14, 0.14, 0.16, 0.78);
                    } else {
                        cairo_set_source_rgba(cr, 0.88, 0.90, 0.92, 0.82);
                    }
                    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                    cairo_set_font_size(cr, legend.size() <= 1 ? 5.2 : 3.8);
                    cairo_text_extents_t ext;
                    cairo_text_extents(cr, legend.c_str(), &ext);
                    cairo_save(cr);
                    cairo_translate(cr, cx, cy);
                    // Keyboard faces the cat (180°), so legends read upright from the cat's seat.
                    cairo_rotate(cr, 3.14159265358979 + 0.052);
                    cairo_move_to(cr, -(ext.width * 0.5 + ext.x_bearing), -(ext.height * 0.5 + ext.y_bearing));
                    cairo_show_text(cr, legend.c_str());
                    cairo_restore(cr);
                }
            }
        }

        // Backlit keycap glow halos
        for (const auto& kv : physics.keyGlow) {
            if (kv.second <= 0.05f) continue;
            Pt c = KeyboardLayout::canvasPointForKey(kv.first);
            double r = 17.0 + kv.second * 6.0;
            cairo_pattern_t* pat = cairo_pattern_create_radial(c.x, c.y, 0, c.x, c.y, r);
            cairo_pattern_add_color_stop_rgba(pat, 0.0, glowCol.r, glowCol.g, glowCol.b, 0.36 * kv.second);
            cairo_pattern_add_color_stop_rgba(pat, 1.0, glowCol.r, glowCol.g, glowCol.b, 0.0);
            cairo_set_source(cr, pat);
            cairo_arc(cr, c.x, c.y, r, 0, PI * 2.0);
            cairo_fill(cr);
            cairo_pattern_destroy(pat);
        }
    }

    // MARK: - Tail

    static void drawTail(cairo_t* cr, float seatX, float seatY, const CatPhysics& physics, CatCoatTheme coat) {
        float p = physics.tailPhase;
        float hunt = std::clamp(physics.huntIntensity.value, 0.0f, 1.0f);
        float wag1 = std::sin(p) * (8.0f + hunt * 5.5f);
        float wag2 = std::cos(p * 1.3f) * (6.0f + hunt * 4.5f);
        float huntDrop = hunt * 14.0f;

        cairo_save(cr);
        if (hasTailRings(coat)) cairo_push_group(cr);
        setCol(cr, earTailColor(coat));
        cairo_set_line_width(cr, coat == CatCoatTheme::Siamese ? 9.5 : 11.0);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_move_to(cr, seatX + 34.0, seatY - 6.0);
        cairo_curve_to(cr,
            seatX + 64.0, seatY + 2.0 + wag1 * 0.35 + huntDrop * 0.4,
            seatX + 66.0 + wag2 * 0.4, seatY - 26.0 + wag1 * 0.7 + huntDrop,
            seatX + 91.0 + wag2 * 0.5, seatY - 30.0 + wag1 * 0.85 + huntDrop);
        cairo_stroke(cr);
        if (hasTailRings(coat)) {
            cairo_set_operator(cr,CAIRO_OPERATOR_ATOP);
            setCol(cr,tailRingColor(coat));
            cairo_set_line_width(cr,3.5);cairo_set_line_cap(cr,CAIRO_LINE_CAP_BUTT);
            double x0=seatX+34,y0=seatY-6;
            double x1=seatX+64,y1=seatY+2+wag1*0.35+huntDrop*0.4;
            double x2=seatX+66+wag2*0.4,y2=seatY-26+wag1*0.7+huntDrop;
            double x3=seatX+91+wag2*0.5,y3=seatY-30+wag1*0.85+huntDrop;
            for (double t: {0.50,0.78}) {
                double u=1-t,x=u*u*u*x0+3*u*u*t*x1+3*u*t*t*x2+t*t*t*x3;
                double y=u*u*u*y0+3*u*u*t*y1+3*u*t*t*y2+t*t*t*y3;
                double dx=3*u*u*(x1-x0)+6*u*t*(x2-x1)+3*t*t*(x3-x2);
                double dy=3*u*u*(y1-y0)+6*u*t*(y2-y1)+3*t*t*(y3-y2);
                double len=std::max(0.001,std::hypot(dx,dy)),nx=-dy/len*8,ny=dx/len*8;
                cairo_move_to(cr,x+nx,y+ny);cairo_line_to(cr,x-nx,y-ny);cairo_stroke(cr);
            }
            cairo_pop_group_to_source(cr);cairo_set_operator(cr,CAIRO_OPERATOR_OVER);cairo_paint(cr);
        }
        cairo_restore(cr);
    }

    // MARK: - 3D-Projected Ears

    static void drawEarsLayer(cairo_t* cr, Pt center, const CatPhysics& physics, CatCoatTheme coat) {
        cairo_save(cr);
        cairo_translate(cr, center.x, center.y);
        cairo_rotate(cr, physics.headTilt.value);

        float yawRad = physics.headYaw.value * 0.50f;
        float pitchRad = physics.headPitch.value * 0.36f;
        float fold = physics.earFold.value;
        float twitch = physics.earTwitch.value * 0.045f;

        drawEar3D(cr, true, yawRad, pitchRad, fold, twitch, coat);
        drawEar3D(cr, false, yawRad, pitchRad, fold, -twitch, coat);
        cairo_restore(cr);
    }

    static void drawEar3D(cairo_t* cr, bool isLeft, float yawRad, float pitchRad, float fold, float twitch, CatCoatTheme coat) {
        float sign = isLeft ? -1.0f : 1.0f;
        bool isSiamese = (coat == CatCoatTheme::Siamese);
        float foldAngle = sign * ((isSiamese ? 0.14f : 0.06f) + fold * 0.34f) + twitch;
        float earLen = isSiamese ? 29.5f : 32.0f;
        float tipDx = std::sin(foldAngle) * earLen;
        float tipDy = -std::cos(foldAngle) * earLen;

        Pt inB  = projectHead3D(sign * (isSiamese ? 10.0f : 9.0f), isSiamese ? -16.5f : -16.0f, 0.0f, yawRad, pitchRad);
        Pt outB = projectHead3D(sign * (isSiamese ? 35.5f : 35.0f), isSiamese ? -4.5f : -5.0f, -2.0f, yawRad, pitchRad);
        Pt tip  = projectHead3D(sign * (isSiamese ? 25.0f : 24.5f) + tipDx, (isSiamese ? -18.5f : -20.0f) + tipDy, -3.0f, yawRad, pitchRad);
        float tipSpread = isSiamese ? 4.2f : 3.8f;
        Pt tipL = { tip.x - sign * tipSpread, tip.y + 2.8f };
        Pt tipR = { tip.x + sign * tipSpread, tip.y + 3.2f };

        cairo_new_path(cr);
        cairo_move_to(cr, inB.x, inB.y);
        quadCurveTo(cr, inB.x, inB.y, (inB.x + tipL.x) * 0.5 - sign * 1.5, (inB.y + tipL.y) * 0.5 - 2.0, tipL.x, tipL.y);
        quadCurveTo(cr, tipL.x, tipL.y, tip.x, tip.y - 2.2, tipR.x, tipR.y);
        quadCurveTo(cr, tipR.x, tipR.y, (outB.x + tipR.x) * 0.5 + sign * 2.5, (outB.y + tipR.y) * 0.5 - 2.0, outB.x, outB.y);
        cairo_close_path(cr);

        setCol(cr, earColor(coat, isLeft));
        cairo_fill(cr);

        if (isSiamese) {
            Pt inI  = { inB.x + (tip.x - inB.x) * 0.24f + sign * 3.0f, inB.y + (tip.y - inB.y) * 0.16f };
            Pt outI = { outB.x + (tip.x - outB.x) * 0.20f - sign * 3.8f, outB.y + (tip.y - outB.y) * 0.18f };
            Pt tipI = { (inB.x + outB.x) * 0.16f + tip.x * 0.68f, (inB.y + outB.y) * 0.14f + tip.y * 0.72f + 2.5f };
            cairo_new_path(cr);
            cairo_move_to(cr, inI.x, inI.y);
            quadCurveTo(cr, inI.x, inI.y, (inI.x + tipI.x) * 0.5 - sign * 0.8, (inI.y + tipI.y) * 0.5, tipI.x, tipI.y);
            quadCurveTo(cr, tipI.x, tipI.y, (outI.x + tipI.x) * 0.5 + sign * 1.2, (outI.y + tipI.y) * 0.5, outI.x, outI.y);
            cairo_close_path(cr);
            setCol(cr, siameseInnerEar);
            cairo_fill(cr);
        }
    }

    // MARK: - Unified C1-Continuous Cat Silhouette (+ Siamese 2.5D Face Mask)

    static void drawUnifiedSilhouette(cairo_t* cr, float seatX, float seatY, Pt headCenter, float breath, const CatPhysics& physics, CatCoatTheme coat) {
        float yaw = physics.headYaw.value;
        float pitch = physics.headPitch.value;
        float hTilt = physics.headTilt.value;
        float bTilt = physics.bodyTilt.value;

        float cH = std::cos(hTilt), sH = std::sin(hTilt);
        auto H = [&](float lx, float ly) -> Pt {
            return { headCenter.x + lx * cH - ly * sH, headCenter.y + lx * sH + ly * cH };
        };

        float cB = std::cos(bTilt), sB = std::sin(bTilt);
        float sy = physics.bodySquash.value;
        float sx = 1.0f + (1.0f - sy) * 0.55f;
        auto B = [&](float lx, float ly) -> Pt {
            float rx = lx * sx, ry = ly * sy;
            return { seatX + rx * cB - ry * sB, seatY + rx * sB + ry * cB };
        };

        bool isSiamese = (coat == CatCoatTheme::Siamese);
        float cheekShift = yaw * 3.0f;
        float crownDrop = pitch * 2.0f;
        float cheekHalfW = 42.5f;
        float crownCtrlW = isSiamese ? 25.0f : 26.0f;
        float crownBaseY = isSiamese ? -29.5f : -30.0f;
        float cheekY = isSiamese ? 6.5f : 5.0f;
        float cheekCtrlTopY = isSiamese ? -13.0f : -14.0f;
        float neckPinchX = isSiamese ? 6.5f : 2.5f;
        float neckCtrlY = isSiamese ? 27.5f : 24.0f;
        float hipHalfW = isSiamese ? 44.5f : 49.0f;
        float waistCtrlW = isSiamese ? 41.0f : 51.5f;
        float waistCtrlY = isSiamese ? -50.0f : -48.0f;
        float botOuterW = isSiamese ? 42.0f : 47.0f;
        float botInnerW = isSiamese ? 32.0f : 36.0f;
        float botHalfW = isSiamese ? 22.0f : 24.0f;
        float lCX = -cheekHalfW + cheekShift * 0.4f;
        float rCX =  cheekHalfW + cheekShift * 0.4f;

        Pt crown  = H(cheekShift * 0.2f, crownBaseY + crownDrop);
        Pt rCheek = H(rCX, cheekY);
        Pt rHip   = B(hipHalfW, -14.0f);
        Pt rBot   = B(botHalfW, 12.0f);
        Pt lBot   = B(-botHalfW, 12.0f);
        Pt lHip   = B(-hipHalfW, -14.0f);
        Pt lCheek = H(lCX, cheekY);

        auto buildPath = [&]() {
            Pt c1, c2;
            cairo_new_path(cr);
            cairo_move_to(cr, crown.x, crown.y);
            c1 = H(crownCtrlW + cheekShift * 0.3f, crownBaseY + crownDrop); c2 = H(rCX + 1.5f, cheekCtrlTopY);
            cairo_curve_to(cr, c1.x, c1.y, c2.x, c2.y, rCheek.x, rCheek.y);
            c1 = H(rCX - neckPinchX, neckCtrlY); c2 = B(waistCtrlW, waistCtrlY + breath * 0.5f);
            cairo_curve_to(cr, c1.x, c1.y, c2.x, c2.y, rHip.x, rHip.y);
            c1 = B(botOuterW, 5.0f); c2 = B(botInnerW, 12.0f);
            cairo_curve_to(cr, c1.x, c1.y, c2.x, c2.y, rBot.x, rBot.y);
            cairo_line_to(cr, lBot.x, lBot.y);
            c1 = B(-botInnerW, 12.0f); c2 = B(-botOuterW, 5.0f);
            cairo_curve_to(cr, c1.x, c1.y, c2.x, c2.y, lHip.x, lHip.y);
            c1 = B(-waistCtrlW, waistCtrlY + breath * 0.5f); c2 = H(lCX + neckPinchX, neckCtrlY);
            cairo_curve_to(cr, c1.x, c1.y, c2.x, c2.y, lCheek.x, lCheek.y);
            c1 = H(lCX - 1.5f, cheekCtrlTopY); c2 = H(-crownCtrlW + cheekShift * 0.3f, crownBaseY + crownDrop);
            cairo_curve_to(cr, c1.x, c1.y, c2.x, c2.y, crown.x, crown.y);
            cairo_close_path(cr);
        };

        if (coat != CatCoatTheme::Siamese) {
            buildPath();
            setCol(cr, bodyColor(coat));
            cairo_fill(cr);
            if (marksFor(coat)) {
                cairo_save(cr); buildPath(); cairo_clip(cr);
                drawCoatMarks(cr,seatX,seatY,headCenter,physics,coat); cairo_restore(cr);
            }
        } else {
            cairo_save(cr);
            buildPath();
            cairo_clip(cr);

            cairo_pattern_t* bg = cairo_pattern_create_linear(seatX, headCenter.y - 34.0, seatX, seatY + 18.0);
            cairo_pattern_add_color_stop_rgba(bg, 0.15, siameseCream.r, siameseCream.g, siameseCream.b, 1.0);
            cairo_pattern_add_color_stop_rgba(bg, 1.0,  siameseLatte.r, siameseLatte.g, siameseLatte.b, 1.0);
            cairo_set_source(cr, bg);
            cairo_paint(cr);
            cairo_pattern_destroy(bg);

            // 2.5D Rotating Seal-Point "挖煤" Face Mask (no center hole!)
            float yawRad = yaw * 0.50f;
            float pitchRad = pitch * 0.36f;
            Pt maskC = projectHead3D(0.0f, 3.0f, 27.0f, yawRad, pitchRad);
            double maskSX = std::max(0.76, std::cos((double)yawRad * 0.7)) * 1.24;
            double maskSY = std::max(0.82, std::cos((double)pitchRad * 0.7)) * 0.88;

            cairo_save(cr);
            cairo_translate(cr, headCenter.x, headCenter.y);
            cairo_rotate(cr, hTilt);
            cairo_translate(cr, maskC.x, maskC.y);
            cairo_scale(cr, maskSX, maskSY);

            cairo_pattern_t* mp = cairo_pattern_create_radial(0, 0, 0.0, 0, 0, 31.5);
            cairo_pattern_add_color_stop_rgba(mp, 0.0,  siamesePoint.r, siamesePoint.g, siamesePoint.b, 1.0);
            cairo_pattern_add_color_stop_rgba(mp, 0.42, siamesePoint.r, siamesePoint.g, siamesePoint.b, 0.92);
            cairo_pattern_add_color_stop_rgba(mp, 0.74, siameseMocha.r, siameseMocha.g, siameseMocha.b, 0.62);
            cairo_pattern_add_color_stop_rgba(mp, 1.0,  siameseMocha.r, siameseMocha.g, siameseMocha.b, 0.0);
            cairo_set_source(cr, mp);
            cairo_arc(cr, 0, 0, 31.5, 0, PI * 2.0);
            cairo_fill(cr);
            cairo_pattern_destroy(mp);
            cairo_restore(cr);

            Pt muzC = projectHead3D(0.0f, 7.5f, 28.0f, yawRad, pitchRad);
            cairo_save(cr);
            cairo_translate(cr, headCenter.x, headCenter.y);
            cairo_rotate(cr, hTilt);
            cairo_translate(cr, muzC.x, muzC.y);
            cairo_scale(cr, std::max(0.76, std::cos((double)yawRad * 0.7)) * 1.04, std::max(0.82, std::cos((double)pitchRad * 0.7)) * 0.76);
            cairo_pattern_t* mp2 = cairo_pattern_create_radial(0, 0, 0.0, 0, 0, 19.5);
            cairo_pattern_add_color_stop_rgba(mp2, 0.0,  siamesePoint.r, siamesePoint.g, siamesePoint.b, 1.0);
            cairo_pattern_add_color_stop_rgba(mp2, 0.50, siamesePoint.r, siamesePoint.g, siamesePoint.b, 0.85);
            cairo_pattern_add_color_stop_rgba(mp2, 1.0,  siamesePoint.r, siamesePoint.g, siamesePoint.b, 0.0);
            cairo_set_source(cr, mp2);
            cairo_arc(cr, 0, 0, 19.5, 0, PI * 2.0);
            cairo_fill(cr);
            cairo_pattern_destroy(mp2);
            cairo_restore(cr);

            cairo_restore(cr);

            buildPath();
            setCol(cr, siameseOut);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
        }

        // Tucked hind paws at bottom
        cairo_save(cr);
        cairo_translate(cr, seatX, seatY);
        cairo_rotate(cr, bTilt);
        cairo_scale(cr, sx, sy);

        double leftHindCX = isSiamese ? -23.25 : -26.5;
        double rightHindCX = isSiamese ? 23.25 : 26.5;
        double hindRX = isSiamese ? 7.75 : 8.5;
        double leftHindCY = isSiamese ? 9.5 : 8.5;
        double leftHindRY = isSiamese ? 6.0 : 5.5;
        double rightHindCY = (coat == CatCoatTheme::ClassicBlack || coat == CatCoatTheme::Tuxedo || isSiamese) ? 10.75 : 8.5;
        double rightHindRY = (coat == CatCoatTheme::ClassicBlack || coat == CatCoatTheme::Tuxedo || isSiamese) ? 6.25 : 5.5;

        setCol(cr, pawColor(coat));
        cairo_new_path(cr);
        fillEllipse(cr, leftHindCX, leftHindCY, hindRX, leftHindRY);
        cairo_fill(cr);
        if (physics.hindFootActive.value < 0.35f) {
            cairo_new_path(cr);
            fillEllipse(cr, rightHindCX, rightHindCY, hindRX, rightHindRY);
            cairo_fill(cr);
        }

        setCol(cr, coat == CatCoatTheme::Siamese ? siameseCream : creaseColor(coat), 0.56);
        cairo_set_line_width(cr, 1.3);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_new_path(cr);
        cairo_arc(cr, leftHindCX, leftHindCY, 7.5, 0.25, PI - 0.25);
        cairo_stroke(cr);
        if (physics.hindFootActive.value < 0.35f) {
            cairo_new_path(cr);
            cairo_arc(cr, rightHindCX, rightHindCY, 7.5, 0.25, PI - 0.25);
            cairo_stroke(cr);
        }
        cairo_restore(cr);
    }

    // MARK: - 3rd Key Hind Foot

    static void drawHindFoot(cairo_t* cr, float seatX, float seatY, const CatPhysics& physics, CatCoatTheme coat) {
        Pt hip = { seatX + 26.0f, seatY + 2.0f };
        Pt target = physics.hindFoot.point();
        Pt mid = { (hip.x + target.x) * 0.5f + 10.0f, (hip.y + target.y) * 0.5f - 8.0f };
        drawLimb(cr, hip, mid, target, 15.0f, 12.0f, true, 0.85f * physics.hindFootActive.value, coat);
    }

    // MARK: - 2.5D Eyes & Whiskers

    static void drawHeadFeatures(cairo_t* cr, Pt center, const CatPhysics& physics, EyeColorTheme eyeTheme, CatCoatTheme coat) {
        cairo_save(cr);
        cairo_translate(cr, center.x, center.y);
        cairo_rotate(cr, physics.headTilt.value);

        float yawRad = physics.headYaw.value * 0.50f;
        float pitchRad = physics.headPitch.value * 0.36f;

        drawEye3D(cr, true, yawRad, pitchRad, physics, eyeTheme, coat);
        drawEye3D(cr, false, yawRad, pitchRad, physics, eyeTheme, coat);
        if (coat == CatCoatTheme::Siamese) {
            Pt nose = projectHead3D(0.0f, 8.2f, 29.0f, yawRad, pitchRad);
            Pt mL   = projectHead3D(-4.2f, 12.2f, 28.4f, yawRad, pitchRad);
            Pt mM   = projectHead3D(0.0f, 11.2f, 28.8f, yawRad, pitchRad);
            Pt mR   = projectHead3D(4.2f, 12.2f, 28.4f, yawRad, pitchRad);
            cairo_new_path(cr);
            cairo_move_to(cr, nose.x - 2.3, nose.y - 1.1);
            quadCurveTo(cr, nose.x - 2.3, nose.y - 1.1, nose.x, nose.y - 1.6, nose.x + 2.3, nose.y - 1.1);
            quadCurveTo(cr, nose.x + 2.3, nose.y - 1.1, nose.x + 1.8, nose.y + 0.6, nose.x, nose.y + 1.5);
            quadCurveTo(cr, nose.x, nose.y + 1.5, nose.x - 1.8, nose.y + 0.6, nose.x - 2.3, nose.y - 1.1);
            cairo_close_path(cr);
            cairo_set_source_rgba(cr, 0.14, 0.09, 0.08, 0.95);
            cairo_fill(cr);
            cairo_set_source_rgba(cr, 0.78, 0.66, 0.56, 0.55);
            cairo_set_line_width(cr, 1.05);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_new_path(cr);
            cairo_move_to(cr, mL.x, mL.y);
            quadCurveTo(cr, mL.x, mL.y, (mL.x + mM.x) * 0.5, mL.y + 1.6, mM.x, mM.y);
            quadCurveTo(cr, mM.x, mM.y, (mM.x + mR.x) * 0.5, mR.y + 1.6, mR.x, mR.y);
            cairo_stroke(cr);
        }
        drawWhiskers3D(cr, yawRad, pitchRad, physics, coat);
        cairo_restore(cr);
    }

    static void drawEye3D(cairo_t* cr, bool isLeft, float yawRad, float pitchRad, const CatPhysics& physics, EyeColorTheme eyeTheme, CatCoatTheme coat) {
        float sign = isLeft ? -1.0f : 1.0f;
        RGBA eyeCol = eyePrimary(eyeTheme);
        Pt proj = projectHead3D(sign * 17.2f, -2.0f, 27.5f, yawRad, pitchRad);

        float eyeNormal = yawRad + sign * 0.34f;
        double foreX = std::clamp((double)std::cos(eyeNormal) / std::cos(0.34), 0.64, 1.06);
        double foreY = std::clamp((double)std::cos(pitchRad * 0.85f), 0.78, 1.03);

        cairo_save(cr);
        cairo_translate(cr, proj.x, proj.y);
        cairo_scale(cr, foreX, foreY);

        if (physics.isHappyClosedEyes()) {
            setCol(cr, eyeCol);
            cairo_set_line_width(cr, 2.6);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_move_to(cr, -11.5, 1.0);
            quadCurveTo(cr, -11.5, 1.0, sign * 1.5, 3.4, 11.5, 1.0);
            cairo_stroke(cr);
            cairo_restore(cr);
            return;
        }

        if (physics.isBlissfulSquint()) {
            setCol(cr, eyeCol);
            cairo_new_path(cr);
            cairo_move_to(cr, -11.5, -0.5);
            quadCurveTo(cr, -11.5, -0.5, 0.0, 0.6, 11.5, -0.5);
            quadCurveTo(cr, 11.5, -0.5, 0.0, 4.8, -11.5, -0.5);
            cairo_close_path(cr);
            cairo_fill(cr);
            cairo_restore(cr);
            return;
        }

        float hunt = std::clamp(physics.huntIntensity.value, 0.0f, 1.0f);
        cairo_rotate(cr, sign * (0.06f * (1.0f - hunt * 0.6f)));

        bool isSiameseEye = (coat == CatCoatTheme::Siamese && eyeTheme == EyeColorTheme::SiameseBlue);
        double rx = isSiameseEye ? (11.4 + hunt * 1.3) : (12.0 + hunt * 1.1);
        double ry = isSiameseEye ? (9.8 + hunt * 2.0) : (10.8 + hunt * 1.8);

        cairo_save(cr);
        cairo_new_path(cr);
        fillEllipse(cr, 0, 0, rx, ry);
        cairo_clip(cr);

        // Clip the opening before drawing; never repaint the face through the oval.
        float closure = std::clamp(physics.effectiveEyeClosure(), 0.0f, 1.0f);
        if (closure > 0.0f) {
            cairo_rectangle(cr, -rx - 2, -ry + 2 * ry * closure,
                            2 * rx + 4, 2 * ry * (1 - closure));
            cairo_clip(cr);
        }

        setCol(cr, eyeCol);
        cairo_rectangle(cr, -rx - 1, -ry - 1, rx * 2 + 2, ry * 2 + 2);
        cairo_fill(cr);
        if (eyeTheme == EyeColorTheme::SiameseBlue) {
            cairo_pattern_t* ig = cairo_pattern_create_linear(0.0, -ry, 0.0, ry);
            cairo_pattern_add_color_stop_rgba(ig, 0.00, 0.56, 0.77, 0.90, 1.0);
            cairo_pattern_add_color_stop_rgba(ig, 0.55, 0.71, 0.87, 0.96, 1.0);
            cairo_pattern_add_color_stop_rgba(ig, 1.00, 0.86, 0.945, 0.99, 1.0);
            cairo_set_source(cr, ig);
            cairo_rectangle(cr, -rx - 1, -ry - 1, rx * 2 + 2, ry * 2 + 2);
            cairo_fill(cr);
            cairo_pattern_destroy(ig);
        }

        Pt gaze = physics.eyeGaze.point();
        double px = gaze.x * (4.2 - hunt * 1.2);
        double py = gaze.y * (3.6 - hunt * 1.0);
        double pScale = physics.pupilScale.value;
        double normalW = 5.0 * pScale;
        double normalH = 13.5 * std::min(1.12, pScale);
        double pupilW = normalW + (18.8 - normalW) * hunt;
        double pupilH = normalH + (18.8 - normalH) * hunt;

        // Reference iris colors, blended only in a narrow ring around the moving pupil.
        if (eyeTheme == EyeColorTheme::ReferenceHazel) {
            for (int i = 0; i <= 24; ++i) {
                double t = i / 24.0, mix = t*t*(3-2*t), rim = (1-t)*2.2;
                cairo_set_source_rgb(cr, (218-77*mix)/255, (178-33*mix)/255, (87+41*mix)/255);
                cairo_new_path(cr);
                fillEllipse(cr, px, py, pupilW*0.5+rim, pupilH*0.5+rim);
                cairo_fill(cr);
            }
        }
        setCol(cr, catBlack);
        cairo_new_path(cr);
        fillEllipse(cr, px, py, pupilW * 0.5, pupilH * 0.5);
        cairo_fill(cr);

        if (hunt > 0.25f) {
            double ga = std::min(1.0, (hunt - 0.25) / 0.55) * 0.92;
            double gr = 2.1 + hunt * 0.9;
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, ga);
            cairo_arc(cr, px - pupilW * 0.22, py - pupilH * 0.22, gr, 0, PI * 2.0);
            cairo_fill(cr);
        } else if (eyeTheme == EyeColorTheme::SiameseBlue) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.86);
            cairo_arc(cr, px - 2.9, py - 3.6, 1.65, 0, PI * 2.0);
            cairo_fill(cr);
        }

        cairo_restore(cr);
        cairo_restore(cr);
    }

    static void drawWhiskers3D(cairo_t* cr, float yawRad, float pitchRad, const CatPhysics& physics, CatCoatTheme coat) {
        float wiggle = std::sin(physics.time * 4.5f) * (std::max(physics.petHeadIntensity, physics.petBodyIntensity) * 1.2f);
        setCol(cr, whiskerColor(coat));
        cairo_set_line_width(cr, 1.05);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

        const float rows[3][3] = {
            { 10.5f, 36.5f, 8.8f + wiggle },
            { 14.2f, 37.8f, 14.2f + wiggle * 0.5f },
            { 18.0f, 35.5f, 19.6f - wiggle * 0.4f }
        };

        for (float sign : {-1.0f, 1.0f}) {
            for (int i = 0; i < 3; ++i) {
                Pt p0 = projectHead3D(sign * 24.0f, rows[i][0], 23.0f, yawRad, pitchRad);
                Pt p2 = projectHead3D(sign * rows[i][1], rows[i][2], 14.0f, yawRad, pitchRad);
                cairo_move_to(cr, p0.x, p0.y);
                quadCurveTo(cr, p0.x, p0.y, (p0.x + p2.x) * 0.5, (p0.y + p2.y) * 0.5 - 0.8, p2.x, p2.y);
            }
        }
        cairo_stroke(cr);
    }

    // MARK: - Front Paws

    static void drawFrontPaws(cairo_t* cr, float seatX, float seatY, float breath, const CatPhysics& physics, CatCoatTheme coat) {
        Pt lPos = physics.leftPaw.point();
        Pt rPos = physics.rightPaw.point();
        float lDist = std::hypot(lPos.x - CatPhysics::LeftPawRest.x, lPos.y - CatPhysics::LeftPawRest.y);
        float rDist = std::hypot(rPos.x - CatPhysics::RightPawRest.x, rPos.y - CatPhysics::RightPawRest.y);

        bool isSiamese = (coat == CatCoatTheme::Siamese);
        float siameseLift = isSiamese ? -3.0f : 0.0f;
        float idleTopY = isSiamese ? (seatY - 31.0f) : (seatY - 30.0f);
        float idleTopX = isSiamese ? 16.5f : 18.0f;
        float idleBotX = isSiamese ? 15.0f : 16.0f;
        float shX = isSiamese ? 18.5f : 20.0f;
        float limbStartW = isSiamese ? 17.0f : 19.0f;
        float limbEndW = isSiamese ? 12.5f : 13.5f;

        auto shoulder = [&](float x) -> Pt {
            float sy=physics.bodySquash.value, sx=1+(1-sy)*0.55f;
            float c=std::cos(physics.bodyTilt.value), s=std::sin(physics.bodyTilt.value);
            float floor=126+siameseLift+physics.headOffsetY.value+physics.headPitch.value*4.8f+breath*1.1f+52;
            return {seatX+x*sx*c+43*sy*s, std::max(seatY+x*sx*s-43*sy*c,floor)};
        };
        Pt lSh=shoulder(-shX), rSh=shoulder(shX);

        if (lDist < 6.0f && !physics.leftPawOnMouse && !physics.leftPawTyping) {
            drawIdleFrontLeg(cr, { seatX - idleTopX, idleTopY }, { seatX - idleBotX, seatY + 12.0f }, true, coat);
        } else {
            Pt mid = { (lSh.x + lPos.x) * 0.5f - (physics.leftPawOnMouse ? 8.0f : 4.0f), (lSh.y + lPos.y) * 0.5f - 6.0f };
            drawLimb(cr, lSh, mid, lPos, limbStartW, limbEndW, false, 0.78f, coat);
        }

        if (rDist < 6.0f && !physics.rightPawTyping) {
            drawIdleFrontLeg(cr, { seatX + idleTopX, idleTopY }, { seatX + idleBotX, seatY + 12.0f }, false, coat);
        } else {
            Pt mid = { (rSh.x + rPos.x) * 0.5f + 4.0f, (rSh.y + rPos.y) * 0.5f - 6.0f };
            drawLimb(cr, rSh, mid, rPos, limbStartW, limbEndW, true, 0.78f, coat);
        }
    }

    static void drawIdleFrontLeg(cairo_t* cr, Pt top, Pt bottom, bool isLeft, CatCoatTheme coat) {
        float sign = isLeft ? -1.0f : 1.0f;
        if (coat == CatCoatTheme::Siamese) {
            cairo_save(cr);
            roundedRect(cr, bottom.x - 7.5, top.y, 15.0, bottom.y - top.y + 5.0, 6.5);
            cairo_pattern_t* grad = cairo_pattern_create_linear(top.x, top.y, bottom.x, bottom.y + 5.0);
            cairo_pattern_add_color_stop_rgba(grad, 0.05, siameseCream.r, siameseCream.g, siameseCream.b, 0.0);
            cairo_pattern_add_color_stop_rgba(grad, 0.50, siameseMocha.r, siameseMocha.g, siameseMocha.b, 0.75);
            cairo_pattern_add_color_stop_rgba(grad, 0.82, siamesePoint.r, siamesePoint.g, siamesePoint.b, 1.0);
            cairo_set_source(cr, grad);
            cairo_fill(cr);
            cairo_pattern_destroy(grad);
            cairo_restore(cr);
        }

        if (patterned(coat) || coat == CatCoatTheme::ClassicBlack) {
            setCol(cr,pawColor(coat));
            roundedRect(cr,bottom.x-7.5,bottom.y-13,15,18,6.5);cairo_fill(cr);
        }
        setCol(cr, coat == CatCoatTheme::Siamese ? siameseCrease : creaseColor(coat));
        cairo_set_line_width(cr, 1.45);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

        cairo_move_to(cr, top.x + sign * 8.5, top.y);
        quadCurveTo(cr, top.x + sign * 8.5, top.y, top.x + sign * 9.5, (top.y + bottom.y) * 0.5, bottom.x + sign * 7.5, bottom.y - 1.0);
        cairo_move_to(cr, top.x - sign * 7.0, top.y + 4.0);
        quadCurveTo(cr, top.x - sign * 7.0, top.y + 4.0, top.x - sign * 6.5, (top.y + bottom.y) * 0.5, bottom.x - sign * 6.0, bottom.y - 1.0);
        cairo_stroke(cr);
    }

    static void drawLimb(cairo_t* cr, Pt start, Pt ctrl, Pt end, float startW, float endW, bool outlineOnLeft, float outlineAlpha, CatCoatTheme coat) {
        constexpr int steps = 14;
        std::vector<Pt> leftEdge, rightEdge;
        leftEdge.reserve(steps + 1); rightEdge.reserve(steps + 1);

        for (int i = 0; i <= steps; ++i) {
            float t = (float)i / (float)steps;
            Pt p = quadPoint(start, ctrl, end, t);
            Pt tan = quadTangent(start, ctrl, end, t);
            float len = std::max(0.001f, std::hypot(tan.x, tan.y));
            float nx = -tan.y / len, ny = tan.x / len;
            float hw = (startW + (endW - startW) * t) * 0.5f;
            leftEdge.push_back({ p.x + nx * hw, p.y + ny * hw });
            rightEdge.push_back({ p.x - nx * hw, p.y - ny * hw });
        }

        auto traceLimb = [&]() {
            cairo_new_path(cr);
            cairo_move_to(cr, leftEdge[0].x, leftEdge[0].y);
            for (int i = 1; i <= steps; ++i) cairo_line_to(cr, leftEdge[i].x, leftEdge[i].y);
            for (int i = steps; i >= 0; --i) cairo_line_to(cr, rightEdge[i].x, rightEdge[i].y);
            Pt tangent=quadTangent(start,ctrl,end,0);
            float length=std::max(0.001f,std::hypot(tangent.x,tangent.y));
            quadCurveTo(cr,rightEdge[0].x,rightEdge[0].y,
                start.x-tangent.x/length*startW*0.5f,start.y-tangent.y/length*startW*0.5f,
                leftEdge[0].x,leftEdge[0].y);
            cairo_close_path(cr);
        };

        double pawR = endW * 0.58;
        if (coat != CatCoatTheme::Siamese) {
            traceLimb();
            setCol(cr, limbColor(coat));
            if (patterned(coat)) {
                RGBA milk=pawColor(coat),base=limbColor(coat);
                cairo_pattern_t* gradient=cairo_pattern_create_linear(start.x,start.y,end.x,end.y);
                cairo_pattern_add_color_stop_rgb(gradient,0,milk.r,milk.g,milk.b);
                cairo_pattern_add_color_stop_rgb(gradient,0.30,base.r,base.g,base.b);
                cairo_pattern_set_extend(gradient,CAIRO_EXTEND_PAD);
                cairo_set_source(cr,gradient);cairo_fill(cr);cairo_pattern_destroy(gradient);
            } else cairo_fill(cr);
            if (patterned(coat)) {
                int first=(int)(steps*0.72f);
                cairo_new_path(cr);cairo_move_to(cr,leftEdge[first].x,leftEdge[first].y);
                for (int i=first+1;i<=steps;++i)cairo_line_to(cr,leftEdge[i].x,leftEdge[i].y);
                for (int i=steps;i>=first;--i)cairo_line_to(cr,rightEdge[i].x,rightEdge[i].y);
                cairo_close_path(cr);setCol(cr,pawColor(coat));cairo_fill(cr);
            }
            cairo_new_path(cr);
            fillEllipse(cr, end.x, end.y, pawR, pawR * 0.85);
            cairo_fill(cr);
        } else {
            cairo_save(cr);
            traceLimb();
            cairo_pattern_t* grad = cairo_pattern_create_linear(start.x, start.y, end.x, end.y);
            cairo_pattern_add_color_stop_rgba(grad, 0.0,  siameseCream.r, siameseCream.g, siameseCream.b, 0.0);
            cairo_pattern_add_color_stop_rgba(grad, 0.16, siameseCream.r, siameseCream.g, siameseCream.b, 1.0);
            cairo_pattern_add_color_stop_rgba(grad, 0.48, siameseMocha.r, siameseMocha.g, siameseMocha.b, 1.0);
            cairo_pattern_add_color_stop_rgba(grad, 0.78, siamesePoint.r, siamesePoint.g, siamesePoint.b, 1.0);
            cairo_set_source(cr, grad);
            cairo_fill(cr);
            cairo_pattern_destroy(grad);
            cairo_restore(cr);

            setCol(cr, siamesePoint);
            cairo_new_path(cr);
            fillEllipse(cr, end.x, end.y, pawR, pawR * 0.85);
            cairo_fill(cr);

            const auto& outerEdge = outlineOnLeft ? rightEdge : leftEdge;
            setCol(cr, siameseOut, 0.78);
            cairo_set_line_width(cr, 1.25);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
            cairo_move_to(cr, outerEdge[3].x, outerEdge[3].y);
            for (int i = 4; i < steps; ++i) cairo_line_to(cr, outerEdge[i].x, outerEdge[i].y);
            cairo_stroke(cr);
        }

        const auto& hiEdge = outlineOnLeft ? leftEdge : rightEdge;
        int sIdx = 2, eIdx = (int)(steps * 0.68f);
        setCol(cr, coat == CatCoatTheme::Siamese ? siameseOut : creaseColor(coat), outlineAlpha * 0.9);
        cairo_set_line_width(cr, 1.35);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_move_to(cr, hiEdge[sIdx].x, hiEdge[sIdx].y);
        for (int i = sIdx + 1; i <= eIdx; ++i) cairo_line_to(cr, hiEdge[i].x, hiEdge[i].y);
        cairo_stroke(cr);
    }

    // MARK: - Floating Keycaps

    static void drawFloatingKeycaps(cairo_t* cr, const CatPhysics& physics) {
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

        for (const auto& b : physics.floatingKeycaps) {
            float prog = b.age / b.lifetime;
            double scale = 1.0;
            if (b.age < 0.09f) scale = 0.72 + 0.38 * std::sin((b.age / 0.09) * PI * 0.65);
            else if (b.age < 0.18f) scale = 1.10 - 0.10 * ((b.age - 0.09) / 0.09);

            double alpha = prog < 0.62f ? 1.0 : std::max(0.0, 1.0 - (prog - 0.62) / 0.38);
            if (alpha < 0.02) continue;

            // Estimate display glyph count (UTF-8 multi-byte symbols count as 1 char)
            size_t glyphLen = 0;
            bool hasNonAscii = false;
            for (unsigned char c : b.label) {
                if ((c & 0xC0) != 0x80) glyphLen++;
                if (c >= 0x80) hasNonAscii = true;
            }
            double w = glyphLen <= 1 ? 23.0 : (glyphLen == 2 ? (hasNonAscii ? 34.0 : 30.0) : 38.0);
            double h = 22.0;

            cairo_save(cr);
            cairo_translate(cr, b.position.x, b.position.y);
            cairo_rotate(cr, b.rotation);
            cairo_scale(cr, scale, scale);

            roundedRect(cr, -w * 0.5, -h * 0.5 + 2.5, w, h, 5.5);
            cairo_set_source_rgba(cr, 0.15, 0.15, 0.17, 0.95 * alpha);
            cairo_fill(cr);

            roundedRect(cr, -w * 0.5, -h * 0.5, w, h, 5.2);
            cairo_set_source_rgba(cr, 0.975, 0.972, 0.952, alpha);
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 0.16, 0.16, 0.18, alpha);
            cairo_set_line_width(cr, 1.45);
            cairo_stroke(cr);

            cairo_set_font_size(cr, glyphLen <= 1 ? 11.5 : 9.8);
            cairo_text_extents_t ext;
            cairo_text_extents(cr, b.label.c_str(), &ext);
            cairo_set_source_rgba(cr, 0.12, 0.12, 0.14, alpha);
            cairo_move_to(cr, -(ext.width * 0.5 + ext.x_bearing), -(ext.height * 0.5 + ext.y_bearing));
            cairo_show_text(cr, b.label.c_str());

            cairo_restore(cr);
        }
    }

    // MARK: - Sparkles & Sleep Z

    static void drawSparkles(cairo_t* cr, const CatPhysics& physics, EyeColorTheme eyeTheme) {
        RGBA c = eyePrimary(eyeTheme);
        for (const auto& sp : physics.sparkles) {
            double p = std::min(1.0, (double)(sp.age / sp.lifetime));
            double alpha = std::sin(p * PI) * 0.85;
            if (alpha < 0.02) continue;

            cairo_save(cr);
            setCol(cr, c, alpha);
            if (sp.isSleepZ) {
                cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, 11.0 + p * 3.0);
                cairo_move_to(cr, sp.position.x, sp.position.y);
                cairo_show_text(cr, "z");
            } else {
                double r = 3.5 + p * 1.5;
                cairo_translate(cr, sp.position.x, sp.position.y);
                cairo_move_to(cr, 0, -r);
                quadCurveTo(cr, 0, -r, 0.8, -0.8, r, 0);
                quadCurveTo(cr, r, 0, 0.8, 0.8, 0, r);
                quadCurveTo(cr, 0, r, -0.8, 0.8, -r, 0);
                quadCurveTo(cr, -r, 0, -0.8, -0.8, 0, -r);
                cairo_close_path(cr);
                cairo_fill(cr);
            }
            cairo_restore(cr);
        }
    }
};
