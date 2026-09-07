#include "ui/Icons.h"

#include <cmath>
#include <numbers>

namespace citron::ui {

namespace {

struct PathBuilder {
    winrt::com_ptr<ID2D1PathGeometry> geometry;
    winrt::com_ptr<ID2D1GeometrySink> sink;
    bool open = false;

    explicit PathBuilder(ID2D1Factory* factory) {
        factory->CreatePathGeometry(geometry.put());
        if (geometry) {
            geometry->Open(sink.put());
        }
    }

    void move(float x, float y) {
        if (open) {
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
        }
        sink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_HOLLOW);
        open = true;
    }

    void line(float x, float y) { sink->AddLine(D2D1::Point2F(x, y)); }

    void arc(float x, float y, float radius, bool large, bool clockwise) {
        D2D1_ARC_SEGMENT segment{};
        segment.point = D2D1::Point2F(x, y);
        segment.size = D2D1::SizeF(radius, radius);
        segment.rotationAngle = 0.0f;
        segment.sweepDirection = clockwise ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
        segment.arcSize = large ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;
        sink->AddArc(segment);
    }

    void close() {
        if (open) {
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            open = false;
        }
    }

    ID2D1PathGeometry* finish() {
        if (open) {
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            open = false;
        }
        sink->Close();
        return geometry.get();
    }
};

}

void drawIcon(Renderer& renderer, Icon icon, const Rect& box, Color color, float strokeWidth, float rotationDegrees) {
    ID2D1Factory* factory = renderer.factory();
    const float unit = icon == Icon::Minimize || icon == Icon::Maximize || icon == Icon::Restore || icon == Icon::Close ? 12.0f : 24.0f;
    const float scale = std::min(box.w, box.h) / unit;
    const float ox = box.x + (box.w - unit * scale) / 2.0f;
    const float oy = box.y + (box.h - unit * scale) / 2.0f;
    D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(ox, oy);
    if (rotationDegrees != 0.0f) {
        const auto center = box.center();
        transform = transform * D2D1::Matrix3x2F::Rotation(rotationDegrees, D2D1::Point2F(center.x, center.y));
    }
    const float width = strokeWidth / scale;
    PathBuilder path(factory);
    if (!path.sink) {
        return;
    }
    switch (icon) {
    case Icon::Sun: {
        path.move(16.0f, 12.0f);
        path.arc(8.0f, 12.0f, 4.0f, true, true);
        path.arc(16.0f, 12.0f, 4.0f, true, true);
        path.close();
        const float pairs[][4] = {{12, 2, 12, 4}, {12, 20, 12, 22}, {4.9f, 4.9f, 6.3f, 6.3f}, {17.7f, 17.7f, 19.1f, 19.1f}, {2, 12, 4, 12}, {20, 12, 22, 12}, {4.9f, 19.1f, 6.3f, 17.7f}, {17.7f, 6.3f, 19.1f, 4.9f}};
        for (const auto& p : pairs) {
            path.move(p[0], p[1]);
            path.line(p[2], p[3]);
        }
        break;
    }
    case Icon::Moon: {
        path.move(21.0f, 12.8f);
        path.arc(11.2f, 3.0f, 9.0f, true, true);
        path.arc(21.0f, 12.8f, 7.0f, false, false);
        path.close();
        break;
    }
    case Icon::Minimize:
        path.move(1.0f, 6.0f);
        path.line(11.0f, 6.0f);
        break;
    case Icon::Maximize:
        path.move(2.5f, 1.5f);
        path.line(9.5f, 1.5f);
        path.arc(10.5f, 2.5f, 1.0f, false, true);
        path.line(10.5f, 9.5f);
        path.arc(9.5f, 10.5f, 1.0f, false, true);
        path.line(2.5f, 10.5f);
        path.arc(1.5f, 9.5f, 1.0f, false, true);
        path.line(1.5f, 2.5f);
        path.arc(2.5f, 1.5f, 1.0f, false, true);
        path.close();
        break;
    case Icon::Restore:
        path.move(3.5f, 3.5f);
        path.line(3.5f, 2.0f);
        path.line(10.0f, 2.0f);
        path.line(10.0f, 8.5f);
        path.line(8.5f, 8.5f);
        path.move(2.0f, 3.5f);
        path.line(8.5f, 3.5f);
        path.line(8.5f, 10.0f);
        path.line(2.0f, 10.0f);
        path.close();
        break;
    case Icon::Close:
        path.move(2.0f, 2.0f);
        path.line(10.0f, 10.0f);
        path.move(10.0f, 2.0f);
        path.line(2.0f, 10.0f);
        break;
    case Icon::Search:
        path.move(18.0f, 11.0f);
        path.arc(4.0f, 11.0f, 7.0f, true, true);
        path.arc(18.0f, 11.0f, 7.0f, true, true);
        path.close();
        path.move(20.0f, 20.0f);
        path.line(16.5f, 16.5f);
        break;
    case Icon::Refresh:
        path.move(21.0f, 12.0f);
        path.arc(18.4f, 5.6f, 9.0f, true, true);
        path.move(21.0f, 3.0f);
        path.line(21.0f, 9.0f);
        path.line(15.0f, 9.0f);
        break;
    case Icon::ArrowRight:
        path.move(5.0f, 12.0f);
        path.line(19.0f, 12.0f);
        path.move(13.0f, 6.0f);
        path.line(19.0f, 12.0f);
        path.line(13.0f, 18.0f);
        break;
    case Icon::Spinner:
        path.move(21.0f, 12.0f);
        path.arc(18.4f, 5.6f, 9.0f, true, true);
        break;
    }
    renderer.strokeGeometry(path.finish(), color, width, transform, true);
}

}
