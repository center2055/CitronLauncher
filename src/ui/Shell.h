#pragma once

#include "app/Strings.h"
#include "ui/Wordmark.h"
#include "ui/controls/Dialog.h"
#include "ui/controls/Element.h"
#include "ui/controls/IconButton.h"
#include "ui/controls/Tabs.h"
#include "ui/controls/Toast.h"

#include <functional>

namespace citron::ui {

struct ShellActions {
    std::function<void(int)> navigate;
    std::function<void()> toggleTheme;
    std::function<void()> minimize;
    std::function<void()> toggleMaximize;
    std::function<void()> close;
};

class Shell : public Element {
public:
    explicit Shell(ShellActions actions);

    void setStrings(const Strings& strings);
    void setPage(int index);
    void setMaximized(bool maximized);
    void setDarkTheme(bool dark);
    Element* pageHost() { return pageHost_; }
    Toast& toast() { return *toast_; }
    Dialog& dialog() { return *dialog_; }
    int hitTestCaption(Point point) const;

    Size measure(const Size& available) override;
    void arrange(const Rect& bounds) override;
    void render(RenderContext& ctx) override;

private:
    ShellActions actions_;
    Wordmark wordmark_;
    std::wstring launcherLabel_;
    Tabs* tabs_ = nullptr;
    IconButton* themeButton_ = nullptr;
    IconButton* minButton_ = nullptr;
    IconButton* maxButton_ = nullptr;
    IconButton* closeButton_ = nullptr;
    Element* pageHost_ = nullptr;
    Toast* toast_ = nullptr;
    Dialog* dialog_ = nullptr;
    Rect titleBar_;
};

}
