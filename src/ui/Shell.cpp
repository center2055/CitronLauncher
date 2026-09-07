#include "ui/Shell.h"

#include <windows.h>

namespace citron::ui {

namespace {

constexpr float kTitleHeight = 52.0f;
constexpr float kWordmarkHeight = 18.0f;

class PageHost : public Element {
public:
    Size measure(const Size& available) override { return available; }
};

}

Shell::Shell(ShellActions actions) : actions_(std::move(actions)) {
    tabs_ = add(std::make_unique<Tabs>(TabsKind::Navigation, std::vector<std::wstring>{L"Play", L"Versions", L"Settings"}, [this](int index) {
        if (actions_.navigate) {
            actions_.navigate(index);
        }
    }));
    themeButton_ = add(std::make_unique<IconButton>(Icon::Sun, IconButtonKind::Caption, [this] {
        if (actions_.toggleTheme) {
            actions_.toggleTheme();
        }
    }));
    themeButton_->setIconSize(14.0f);
    themeButton_->setStroke(2.0f);
    minButton_ = add(std::make_unique<IconButton>(Icon::Minimize, IconButtonKind::Caption, [this] {
        if (actions_.minimize) {
            actions_.minimize();
        }
    }));
    maxButton_ = add(std::make_unique<IconButton>(Icon::Maximize, IconButtonKind::Caption, [this] {
        if (actions_.toggleMaximize) {
            actions_.toggleMaximize();
        }
    }));
    closeButton_ = add(std::make_unique<IconButton>(Icon::Close, IconButtonKind::CaptionClose, [this] {
        if (actions_.close) {
            actions_.close();
        }
    }));
    pageHost_ = add(std::make_unique<PageHost>());
    toast_ = add(std::make_unique<Toast>());
    dialog_ = add(std::make_unique<Dialog>());
}

void Shell::setStrings(const Strings& strings) {
    launcherLabel_ = strings.launcher;
    tabs_->setLabels({strings.tabPlay, strings.tabVersions, strings.tabSettings});
    themeButton_->setTooltip(strings.toggleTheme);
    minButton_->setTooltip(strings.minimize);
    maxButton_->setTooltip(strings.maximize);
    closeButton_->setTooltip(strings.close);
    invalidate();
}

void Shell::setPage(int index) {
    tabs_->setSelected(index);
}

void Shell::setMaximized(bool maximized) {
    maxButton_->setIcon(maximized ? Icon::Restore : Icon::Maximize);
}

void Shell::setDarkTheme(bool dark) {
    themeButton_->setIcon(dark ? Icon::Sun : Icon::Moon);
}

int Shell::hitTestCaption(Point point) const {
    if (dialog_->visible()) {
        return HTCLIENT;
    }
    if (!titleBar_.contains(point)) {
        return HTCLIENT;
    }
    if (maxButton_->bounds().contains(point)) {
        return HTMAXBUTTON;
    }
    for (const Element* e : {static_cast<const Element*>(tabs_), static_cast<const Element*>(themeButton_), static_cast<const Element*>(minButton_), static_cast<const Element*>(closeButton_)}) {
        if (e->bounds().contains(point)) {
            return HTCLIENT;
        }
    }
    return HTCAPTION;
}

Size Shell::measure(const Size& available) {
    return available;
}

void Shell::arrange(const Rect& bounds) {
    bounds_ = bounds;
    titleBar_ = {bounds.x, bounds.y, bounds.w, kTitleHeight};
    const Size tabSize = tabs_->measure(bounds.w > 0 ? Size{bounds.w, kTitleHeight} : Size{});
    tabs_->arrange({bounds.x + (bounds.w - tabSize.w) / 2.0f, bounds.y, tabSize.w, kTitleHeight});
    float x = bounds.right() - 12.0f;
    const float buttonY = bounds.y + (kTitleHeight - 30.0f) / 2.0f;
    x -= 36.0f;
    closeButton_->arrange({x, buttonY, 36.0f, 30.0f});
    x -= 38.0f;
    maxButton_->arrange({x, buttonY, 36.0f, 30.0f});
    x -= 38.0f;
    minButton_->arrange({x, buttonY, 36.0f, 30.0f});
    x -= 36.0f + 8.0f;
    themeButton_->arrange({x, buttonY, 36.0f, 30.0f});
    pageHost_->arrange({bounds.x, bounds.y + kTitleHeight, bounds.w, std::max(0.0f, bounds.h - kTitleHeight)});
    if (toast_->visible()) {
        const Size ts = toast_->measure(bounds.w > 0 ? Size{bounds.w - 40.0f, 60.0f} : Size{});
        toast_->arrange({bounds.x + (bounds.w - ts.w) / 2.0f, bounds.bottom() - 20.0f - ts.h, ts.w, ts.h});
    }
    dialog_->arrange(bounds);
}

void Shell::render(RenderContext& ctx) {
    const Theme& t = ctx.theme;
    ctx.r.fillRect(bounds_, t.bg);
    const float markWidth = Wordmark::widthForHeight(kWordmarkHeight);
    wordmark_.draw(ctx.r, {titleBar_.x + 20.0f, titleBar_.y + (kTitleHeight - kWordmarkHeight) / 2.0f}, kWordmarkHeight, t.textHi);
    const TextStyle labelStyle{Font::Body, 13.0f, 400, 0.0f};
    ctx.r.drawText(launcherLabel_, labelStyle, {titleBar_.x + 20.0f + markWidth + 12.0f, titleBar_.y, 120.0f, kTitleHeight}, t.textDim);
    ctx.r.fillRect({titleBar_.x, titleBar_.bottom() - 1.0f, titleBar_.w, 1.0f}, t.hairline);
    Element::render(ctx);
}

}
