#include "ui/Wordmark.h"

#include "ui/WordmarkData.h"

namespace citron::ui {

float Wordmark::widthForHeight(float height) {
    return height * wordmark::kViewWidth / wordmark::kViewHeight;
}

void Wordmark::draw(Renderer& renderer, Point origin, float height, Color color) {
    ID2D1Factory* factory = renderer.factory();
    if (!geometry_ || factory_ != factory) {
        geometry_ = nullptr;
        factory_ = factory;
        winrt::com_ptr<ID2D1PathGeometry> geometry;
        if (FAILED(factory->CreatePathGeometry(geometry.put()))) {
            return;
        }
        winrt::com_ptr<ID2D1GeometrySink> sink;
        if (FAILED(geometry->Open(sink.put()))) {
            return;
        }
        sink->SetFillMode(D2D1_FILL_MODE_WINDING);
        bool open = false;
        for (const auto& command : wordmark::kCommands) {
            switch (command.op) {
            case wordmark::Op::Move:
                if (open) {
                    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                }
                sink->BeginFigure(D2D1::Point2F(command.v[0], command.v[1]), D2D1_FIGURE_BEGIN_FILLED);
                open = true;
                break;
            case wordmark::Op::Curve:
                sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(command.v[0], command.v[1]), D2D1::Point2F(command.v[2], command.v[3]), D2D1::Point2F(command.v[4], command.v[5])));
                break;
            case wordmark::Op::Close:
            case wordmark::Op::End:
                if (open) {
                    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    open = false;
                }
                break;
            }
        }
        if (open) {
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        }
        sink->Close();
        geometry_ = geometry;
    }
    const float scale = height / wordmark::kViewHeight;
    const auto transform = D2D1::Matrix3x2F::Translation(-wordmark::kViewX, -wordmark::kViewY) * D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(origin.x, origin.y);
    renderer.fillGeometry(geometry_.get(), color, transform);
}

}
