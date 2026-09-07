#pragma once

#include "app/AppState.h"
#include "app/Strings.h"
#include "ui/controls/Button.h"
#include "ui/controls/Element.h"
#include "ui/controls/ListView.h"
#include "ui/controls/ProgressBar.h"
#include "ui/controls/ScrollView.h"
#include "ui/pages/PageActions.h"

#include <string>
#include <vector>

namespace citron::ui {

class PlayPage : public Element {
public:
    explicit PlayPage(PageActions actions);

    void update(const AppState& state, const Strings& strings);

    Size measure(const Size& available) override;
    void arrange(const Rect& bounds) override;
    void render(RenderContext& ctx) override;

private:
    struct DownloadItem {
        std::wstring version;
        std::wstring detail;
        ProgressBar* bar = nullptr;
        bool error = false;
    };

    void rebuildRows(const AppState& state, const Strings& strings);
    void rebuildDownloads(const AppState& state, const Strings& strings);

    PageActions actions_;
    const Strings* strings_ = nullptr;
    std::wstring gameName_;
    std::wstring versionText_;
    std::wstring statusText_;
    std::wstring envText_;
    bool hasVersion_ = false;
    Button* launch_ = nullptr;
    Button* manage_ = nullptr;
    Button* store_ = nullptr;
    ScrollView* list_ = nullptr;
    ListView* rows_ = nullptr;
    Element* downloadsHost_ = nullptr;
    std::vector<DownloadItem> downloads_;
    std::vector<std::string> rowKeys_;
    Rect sidebar_;
    Rect downloadsRect_;
    float downloadsHeight_ = 0.0f;
};

}
