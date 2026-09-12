#pragma once

#include "app/AppState.h"
#include "app/Strings.h"
#include "ui/controls/Button.h"
#include "ui/controls/Element.h"
#include "ui/controls/ScrollView.h"
#include "ui/controls/Toggle.h"
#include "ui/pages/PageActions.h"

#include <string>

namespace citron::ui {

class SettingsContent;

struct AboutInfo {
    std::wstring version;
};

class SettingsPage : public Element {
public:
    SettingsPage(PageActions actions, AboutInfo about);

    void update(const AppState& state, const Strings& strings, const std::wstring& rootPath, const std::wstring& installersPath, const std::wstring& logsPath);

    Size measure(const Size& available) override;
    void arrange(const Rect& bounds) override;

private:
    ScrollView* scroll_ = nullptr;
    SettingsContent* content_ = nullptr;
};

}
