#pragma once

#include "ui/Animation.h"
#include "ui/controls/Element.h"

#include <functional>
#include <string>
#include <vector>

namespace citron::ui {

enum class TabsKind {
    Navigation,
    Filter,
};

class Tabs : public Element {
public:
    Tabs(TabsKind kind, std::vector<std::wstring> labels, std::function<void(int)> onSelect);

    void setSelected(int index);
    int selected() const { return selected_; }
    void setLabels(std::vector<std::wstring> labels);

    Size measure(const Size& available) override;
    void arrange(const Rect& bounds) override;
    void render(RenderContext& ctx) override;
    Element* hitTest(Point point) override;
    bool interactive() const override { return true; }
    bool focusable() const override { return true; }
    void onMouseMove(Point point) override;
    void onMouseLeave() override;
    void onMouseUp(Point point, int button) override;
    bool onKeyDown(unsigned vk, bool ctrl, bool shift) override;

private:
    TextStyle style() const;
    int indexAt(Point point) const;

    TabsKind kind_;
    std::vector<std::wstring> labels_;
    std::vector<Rect> rects_;
    std::vector<Animated> active_;
    std::function<void(int)> onSelect_;
    int selected_ = 0;
    int hover_ = -1;
    int focusIndex_ = 0;
};

}
