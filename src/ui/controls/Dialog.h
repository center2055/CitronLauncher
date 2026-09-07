#pragma once

#include "ui/controls/Button.h"
#include "ui/controls/Element.h"

#include <functional>
#include <string>
#include <vector>

namespace citron::ui {

class Dialog : public Element {
public:
    Dialog();

    void show(std::wstring title, std::vector<std::wstring> paragraphs, std::wstring confirmLabel, std::wstring cancelLabel, bool destructive,
              std::function<void()> onConfirm, std::function<void()> onCancel);
    void hide();
    void cancel();

    Size measure(const Size& available) override;
    void arrange(const Rect& bounds) override;
    void render(RenderContext& ctx) override;
    Element* hitTest(Point point) override;
    bool interactive() const override { return true; }
    bool onKeyDown(unsigned vk, bool ctrl, bool shift) override;
    void onMouseUp(Point point, int button) override;

private:
    std::wstring title_;
    std::vector<std::wstring> paragraphs_;
    bool destructive_ = false;
    std::function<void()> onCancel_;
    Button* confirm_ = nullptr;
    Button* cancelButton_ = nullptr;
    Rect panel_;
    std::vector<float> paragraphHeights_;
};

}
