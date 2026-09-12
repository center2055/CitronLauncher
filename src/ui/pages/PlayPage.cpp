#include "ui/pages/PlayPage.h"

#include "core/Format.h"
#include "core/Text.h"

#include <format>

namespace citron::ui {

namespace {

const TextStyle kMonoLabel{Font::Mono, 10.0f, 500, 2.5f};

class VersionRowElement : public ListRow {
public:
    VersionRowElement(std::wstring version, std::wstring tag, bool preview, std::wstring size) : version_(std::move(version)), tag_(std::move(tag)), preview_(preview), size_(std::move(size)) {
        setRowHeight(38.0f);
        setRowPadding(10.0f, 8.0f);
    }

protected:
    void renderContent(RenderContext& ctx) override {
        const Theme& t = ctx.theme;
        const Rect c = contentRect();
        const TextStyle versionStyle{Font::Mono, 15.0f, 600, 0.0f};
        const TextStyle tagStyle{Font::Body, 11.0f, 700, 0.44f};
        const TextStyle sizeStyle{Font::Body, 12.0f, 400, 0.0f};
        const float sizeWidth = std::max(58.0f, host_->measureText(size_, sizeStyle).w);
        const float tagWidth = host_->measureText(tag_, tagStyle).w;
        float x = c.x + 3.0f + 12.0f;
        const float right = c.right();
        ctx.r.drawText(size_, sizeStyle, {right - sizeWidth, c.y, sizeWidth, c.h}, t.textDim, {Align::End, Align::Center, true, false});
        const float tagX = right - sizeWidth - 12.0f - tagWidth;
        ctx.r.drawText(tag_, tagStyle, {tagX, c.y, tagWidth + 2.0f, c.h}, preview_ ? t.warn : t.textBody);
        ctx.r.drawText(version_, versionStyle, {x, c.y, std::max(0.0f, tagX - 12.0f - x), c.h}, t.textHi);
    }

private:
    std::wstring version_;
    std::wstring tag_;
    bool preview_;
    std::wstring size_;
};

class DownloadsHost : public Element {
public:
    Size measure(const Size& available) override { return available; }
};

}

PlayPage::PlayPage(PageActions actions) : actions_(std::move(actions)) {
    launch_ = add(std::make_unique<Button>(L"Launch", ButtonKind::Primary, [this] {
        if (actions_.launch) {
            actions_.launch();
        }
    }));
    launch_->setTrailingIcon(Icon::ArrowRight, 20.0f);
    manage_ = add(std::make_unique<Button>(L"Manage versions", ButtonKind::Secondary, [this] {
        if (actions_.navigate) {
            actions_.navigate(Page::Versions);
        }
    }));
    store_ = add(std::make_unique<Button>(L"Open Store", ButtonKind::Row, [this] {
        if (actions_.openStore) {
            actions_.openStore();
        }
    }));
    store_->setVisible(false);
    list_ = add(std::make_unique<ScrollView>());
    list_->setPadding(12.0f, 12.0f, 12.0f, 12.0f);
    auto rows = std::make_unique<ListView>(0.0f);
    rows_ = rows.get();
    list_->setContent(std::move(rows));
    downloadsHost_ = add(std::make_unique<DownloadsHost>());
    downloadsHost_->setVisible(false);
}

void PlayPage::rebuildRows(const AppState& state, const Strings& strings) {
    std::vector<std::string> keys;
    const auto installed = state.installedRows();
    for (const auto* row : installed) {
        keys.push_back(row->id.key());
    }
    if (keys == rowKeys_) {
        size_t i = 0;
        for (const auto& child : rows_->children()) {
            if (i < installed.size()) {
                static_cast<ListRow*>(child.get())->setSelected(installed[i]->selected);
            }
            ++i;
        }
        return;
    }
    rowKeys_ = keys;
    rows_->clearChildren();
    for (const auto* row : installed) {
        const bool preview = row->id.channel == VersionChannel::Preview;
        auto element = std::make_unique<VersionRowElement>(text::toWide(row->id.number.toString()), preview ? strings.preview : strings.release, preview,
                                                           row->size > 0 ? text::toWide(format::bytes(row->size)) : L"");
        element->setSelected(row->selected);
        const VersionId id = row->id;
        element->setOnActivate([this, id] {
            if (actions_.select) {
                actions_.select(id);
            }
        });
        rows_->addChild(std::move(element));
    }
}

void PlayPage::rebuildDownloads(const AppState& state, const Strings& strings) {
    const auto active = state.activeOperations();
    std::vector<DownloadItem> items;
    for (const auto* row : active) {
        DownloadItem item;
        item.version = text::toWide(row->id.number.toString());
        const auto& p = row->progress;
        std::wstring label;
        switch (p.stage) {
        case InstallStage::Downloading:
            label = std::format(L"{} · {}", text::toWide(format::percent(p.fraction())), text::toWide(format::speed(p.bytesPerSecond)));
            break;
        case InstallStage::Verifying:
            label = std::format(L"{} · {}", strings.statusVerifying, text::toWide(format::percent(p.fraction())));
            break;
        case InstallStage::Extracting:
            label = std::format(L"{} · {}", strings.statusExtracting, text::toWide(format::percent(p.fraction())));
            break;
        case InstallStage::Removing:
            label = strings.statusRemoving;
            break;
        default:
            label = strings.statusDownloading;
            break;
        }
        item.detail = label;
        items.push_back(std::move(item));
    }
    bool same = items.size() == downloads_.size();
    if (same) {
        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i].version != downloads_[i].version) {
                same = false;
                break;
            }
        }
    }
    if (!same) {
        downloadsHost_->clearChildren();
        downloads_.clear();
        for (auto& item : items) {
            item.bar = downloadsHost_->add(std::make_unique<ProgressBar>());
            downloads_.push_back(std::move(item));
        }
    } else {
        for (size_t i = 0; i < items.size(); ++i) {
            downloads_[i].detail = items[i].detail;
        }
    }
    for (size_t i = 0; i < downloads_.size() && i < active.size(); ++i) {
        downloads_[i].bar->setFraction(static_cast<float>(active[i]->progress.fraction()));
    }
    downloadsHost_->setVisible(!downloads_.empty());
    downloadsHeight_ = downloads_.empty() ? 0.0f : 14.0f + 12.0f + 12.0f + static_cast<float>(downloads_.size()) * (22.0f + 8.0f + 4.0f) + static_cast<float>(downloads_.size() - 1) * 12.0f + 14.0f;
}

void PlayPage::update(const AppState& state, const Strings& strings) {
    strings_ = &strings;
    gameName_ = strings.gameName;
    const VersionRow* selected = state.selectedRow();
    hasVersion_ = selected != nullptr;
    versionText_ = selected != nullptr ? text::toWide(selected->id.number.toString()) : strings.noVersion;
    launch_->setLabel(state.launching ? strings.launching : strings.launch);
    launch_->setBusy(state.launching);
    launch_->setEnabled(selected != nullptr && selected->installed && !state.launching);
    manage_->setLabel(strings.manageVersions);
    statusText_.clear();
    if (selected == nullptr) {
        statusText_ = strings.noVersionHint;
    }
    envText_.clear();
    bool showStore = false;
    if (state.environment && !state.environment->gamingServices) {
        envText_ = strings.envGamingServices;
        showStore = true;
    }
    store_->setLabel(strings.envHint);
    store_->setVisible(showStore);
    rebuildRows(state, strings);
    rebuildDownloads(state, strings);
    invalidate();
}

Size PlayPage::measure(const Size& available) {
    return available;
}

void PlayPage::arrange(const Rect& bounds) {
    bounds_ = bounds;
    const bool narrow = bounds.w < 800.0f;
    const bool wide = bounds.w >= 1400.0f;
    const float paneWidth = narrow ? 280.0f : wide ? 360.0f : 320.0f;
    const float padX = narrow ? 26.0f : 32.0f;
    const float padY = narrow ? 22.0f : 28.0f;
    sidebar_ = {bounds.right() - paneWidth, bounds.y, paneWidth, bounds.h};
    const Rect main{bounds.x, bounds.y, bounds.w - paneWidth, bounds.h};

    float y = main.y + padY;
    y += 22.0f;
    y += 6.0f + 44.0f;
    y += 22.0f;
    const Size ls = launch_->measure({});
    launch_->arrange({main.x + padX, y, ls.w, ls.h});
    const Size ms = manage_->measure({});
    manage_->arrange({main.x + padX + ls.w + 12.0f, y, ms.w, ms.h});
    y += ls.h + 16.0f;
    if (store_->visible()) {
        const Size ss = store_->measure({});
        store_->arrange({main.x + padX, y + 22.0f + 8.0f, ss.w, ss.h});
    }

    downloadsRect_ = {sidebar_.x, sidebar_.bottom() - downloadsHeight_, sidebar_.w, downloadsHeight_};
    list_->arrange({sidebar_.x, sidebar_.y, sidebar_.w, std::max(0.0f, sidebar_.h - downloadsHeight_)});
    if (downloadsHost_->visible()) {
        downloadsHost_->setBounds(downloadsRect_);
        float dy = downloadsRect_.y + 14.0f + 12.0f + 12.0f;
        for (auto& item : downloads_) {
            item.bar->arrange({downloadsRect_.x + 20.0f, dy + 22.0f + 8.0f, downloadsRect_.w - 40.0f, 4.0f});
            dy += 22.0f + 8.0f + 4.0f + 12.0f;
        }
    }
}

void PlayPage::render(RenderContext& ctx) {
    const Theme& t = ctx.theme;
    const bool narrow = bounds_.w < 800.0f;
    const float padX = narrow ? 26.0f : 32.0f;
    const float padY = narrow ? 22.0f : 28.0f;
    const float mainWidth = sidebar_.x - bounds_.x - padX * 2.0f;
    float y = bounds_.y + padY;
    ctx.r.drawText(gameName_, {Font::Title, 17.0f, 600, 0.0f}, {bounds_.x + padX, y, mainWidth, 22.0f}, t.textBody);
    y += 22.0f + 6.0f;
    ctx.r.drawText(versionText_, {Font::Mono, 40.0f, 500, -1.2f}, {bounds_.x + padX, y, mainWidth, 44.0f}, hasVersion_ ? t.textHi : t.textDim);
    y += 44.0f + 22.0f + launch_->bounds().h + 16.0f;
    if (!statusText_.empty()) {
        ctx.r.drawText(statusText_, {Font::Body, 13.0f, 400, 0.0f}, {bounds_.x + padX, y, mainWidth, 20.0f}, t.textMute);
        y += 22.0f;
    }
    if (!envText_.empty()) {
        ctx.r.drawText(envText_, {Font::Body, 13.0f, 400, 0.0f}, {bounds_.x + padX, y, mainWidth, 40.0f}, t.warn, {Align::Start, Align::Start, true, true});
    }
    ctx.r.fillRect({sidebar_.x, sidebar_.y, 1.0f, sidebar_.h}, t.hairline);
    Element::render(ctx);
    if (downloadsHost_->visible()) {
        const Rect d = downloadsRect_;
        ctx.r.fillRect({d.x, d.y, d.w, 1.0f}, t.hairline);
        float dy = d.y + 14.0f;
        ctx.r.drawText(strings_ != nullptr ? strings_->downloadingUpper : L"DOWNLOADING", kMonoLabel, {d.x + 20.0f, dy, d.w - 40.0f, 12.0f}, t.textLabel);
        dy += 12.0f + 12.0f;
        for (const auto& item : downloads_) {
            const Rect line{d.x + 20.0f, dy, d.w - 40.0f, 22.0f};
            ctx.r.drawText(item.version, {Font::Mono, 15.0f, 600, 0.0f}, line, t.textHi);
            ctx.r.drawText(item.detail, {Font::Body, 12.0f, 400, 0.0f}, line, t.textMute, {Align::End, Align::Center, true, false});
            dy += 22.0f + 8.0f + 4.0f + 12.0f;
        }
    }
}

}
