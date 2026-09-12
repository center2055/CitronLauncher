#pragma once

#include "app/AppState.h"
#include "app/Strings.h"
#include "ui/Animation.h"
#include "ui/controls/Element.h"
#include "ui/controls/IconButton.h"
#include "ui/controls/ListView.h"
#include "ui/controls/ScrollView.h"
#include "ui/controls/Tabs.h"
#include "ui/controls/TextInput.h"
#include "ui/pages/PageActions.h"

#include <string>
#include <vector>

namespace citron::ui {

class VersionRowView;

class VersionsPage : public Element {
public:
    explicit VersionsPage(PageActions actions);

    void update(const AppState& state, const Strings& strings);
    void focusSearch();

    Size measure(const Size& available) override;
    void arrange(const Rect& bounds) override;
    void render(RenderContext& ctx) override;

private:
    PageActions actions_;
    const Strings* strings_ = nullptr;
    TextInput* search_ = nullptr;
    IconButton* refresh_ = nullptr;
    Tabs* filters_ = nullptr;
    ScrollView* scroll_ = nullptr;
    ListView* list_ = nullptr;
    std::vector<std::string> rowKeys_;
    std::vector<VersionRowView*> rows_;
    Animated listFade_{1.0f, 210.0};
    VersionFilter lastFilter_ = VersionFilter::All;
    float slideFrom_ = 0.0f;
    std::wstring emptyText_;
    bool empty_ = false;
    Rect searchBox_;
    Rect column_;
};

}
