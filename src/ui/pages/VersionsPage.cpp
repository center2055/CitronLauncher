#include "ui/pages/VersionsPage.h"

#include "core/Format.h"
#include "core/Text.h"
#include "ui/Icons.h"
#include "ui/controls/Button.h"
#include "ui/controls/ProgressBar.h"

#include <format>

namespace citron::ui {

class VersionRowView : public ListRow {
public:
    VersionRowView(PageActions& actions, const VersionId& id) : actions_(actions), id_(id) {
        setRowHeight(42.0f);
        setRowPadding(12.0f, 6.0f);
        setSelectable(true);
        setOnActivate([this] {
            if (installed_ && actions_.select) {
                actions_.select(id_);
            }
        });
        download_ = add(std::make_unique<Button>(L"", ButtonKind::Row, [this] {
            if (actions_.install) {
                actions_.install(id_);
            }
        }));
        cancel_ = add(std::make_unique<Button>(L"", ButtonKind::RowMuted, [this] {
            if (actions_.cancel) {
                actions_.cancel(id_);
            }
        }));
        cancel_->setDangerHover(true);
        remove_ = add(std::make_unique<Button>(L"", ButtonKind::RowMuted, [this] {
            if (actions_.remove) {
                actions_.remove(id_);
            }
        }));
        remove_->setDangerHover(true);
        retry_ = add(std::make_unique<Button>(L"", ButtonKind::Row, [this] {
            if (actions_.install) {
                actions_.install(id_);
            }
        }));
        dismiss_ = add(std::make_unique<Button>(L"", ButtonKind::Ghost, [this] {
            if (actions_.dismiss) {
                actions_.dismiss(id_);
            }
        }));
        dismiss_->setHeight(30.0f);
        dismiss_->setPadding(10.0f, 10.0f);
        dismiss_->setFontSize(12.0f);
        bar_ = add(std::make_unique<ProgressBar>());
        bar_->setWidth(90.0f);
        bar_->setVisible(false);
    }

    void apply(const VersionRow& row, const Strings& strings, bool armed, bool narrow) {
        version_ = text::toWide(row.id.number.toString());
        preview_ = row.id.channel == VersionChannel::Preview;
        tag_ = preview_ ? strings.preview : strings.release;
        size_ = row.size > 0 ? text::toWide(format::bytes(row.size)) : L"";
        installed_ = row.installed;
        narrow_ = narrow;
        setSelected(row.selected);
        const auto& p = row.progress;
        const bool busy = isBusy(p.stage);
        const bool failed = p.stage == InstallStage::Failed;
        statusColor_ = StatusColor::Dim;
        bar_->setVisible(false);
        // deleting walks the whole install and reports no measurable progress,
        // so the row shows a label without a bar and offers nothing to cancel
        const bool removing = p.stage == InstallStage::Removing;
        if (removing) {
            statusColor_ = StatusColor::Warn;
            status_ = strings.statusRemoving;
        } else if (busy) {
            bar_->setVisible(true);
            bar_->setFraction(static_cast<float>(p.fraction()));
            bar_->setError(false);
            statusColor_ = StatusColor::Warn;
            if (p.stage == InstallStage::Downloading) {
                status_ = std::format(L"{} {}", strings.statusDownloading, text::toWide(format::percent(p.fraction())));
            } else if (p.stage == InstallStage::Verifying) {
                status_ = strings.statusVerifying;
            } else if (p.stage == InstallStage::Extracting) {
                status_ = std::format(L"{} {}", strings.statusExtracting, text::toWide(format::percent(p.fraction())));
            } else {
                status_ = strings.statusDownloading;
            }
        } else if (failed) {
            bar_->setVisible(true);
            bar_->setFraction(static_cast<float>(p.fraction()), false);
            bar_->setError(true);
            statusColor_ = StatusColor::Danger;
            status_ = strings.statusFailed;
            if (p.error) {
                status_ = std::format(L"{} · {}", strings.statusFailed, text::toWide(p.error->message));
            }
        } else if (row.selected && row.installed) {
            status_ = strings.statusActive;
            statusColor_ = StatusColor::Accent;
        } else if (row.installed) {
            status_ = strings.statusInstalled;
            statusColor_ = StatusColor::Accent;
        } else if (row.downloaded) {
            status_ = strings.statusDownloaded;
        } else if (!row.inCatalog) {
            status_ = strings.statusUnavailable;
        } else {
            status_ = strings.statusNotDownloaded;
        }
        download_->setLabel(row.downloaded ? strings.install : strings.download);
        cancel_->setLabel(strings.cancel);
        remove_->setLabel(armed ? strings.confirmRemove : strings.remove);
        remove_->setArmed(armed);
        retry_->setLabel(strings.retry);
        dismiss_->setLabel(strings.dismiss);
        download_->setVisible(!busy && !failed && !row.installed && row.inCatalog);
        cancel_->setVisible(busy && !removing);
        remove_->setVisible(!busy && !failed && (row.installed || row.downloaded));
        retry_->setVisible(failed && row.inCatalog);
        dismiss_->setVisible(failed);
        setDimmed(!row.inCatalog && !row.installed && !row.downloaded);
        invalidate();
    }

    void arrange(const Rect& bounds) override {
        bounds_ = bounds;
        const Rect c = contentRect();
        float x = c.right();
        for (Button* b : {dismiss_, retry_, remove_, cancel_, download_}) {
            if (!b->visible()) {
                continue;
            }
            const Size s = b->measure({});
            x -= s.w;
            b->arrange({x, c.y + (c.h - s.h) / 2.0f, s.w, s.h});
            x -= 6.0f;
        }
        actionsLeft_ = std::min(x + 6.0f, c.right() - 150.0f);
        const float statusWidth = narrow_ ? 110.0f : 200.0f;
        statusRight_ = actionsLeft_ - 14.0f;
        statusLeft_ = statusRight_ - statusWidth;
        if (bar_->visible()) {
            const TextStyle statusStyle{Font::Body, 12.0f, 400, 0.0f};
            const float textWidth = host_->measureText(status_, statusStyle).w;
            const float barX = std::max(statusLeft_, statusRight_ - textWidth - 10.0f - 90.0f);
            bar_->arrange({barX, c.y + (c.h - 4.0f) / 2.0f, 90.0f, 4.0f});
        }
    }

protected:
    void renderContent(RenderContext& ctx) override {
        const Theme& t = ctx.theme;
        const Rect c = contentRect();
        const TextStyle versionStyle{Font::Mono, 14.0f, 600, 0.0f};
        const TextStyle tagStyle{Font::Body, 11.0f, 700, 0.44f};
        const TextStyle sizeStyle{Font::Body, 12.0f, 400, 0.0f};
        const TextStyle statusStyle{Font::Body, 12.0f, 400, 0.0f};
        float x = c.x + 3.0f + 14.0f;
        const float versionWidth = host_->measureText(version_, versionStyle).w;
        ctx.r.drawText(version_, versionStyle, {x, c.y, versionWidth + 2.0f, c.h}, t.textHi);
        x += versionWidth + 10.0f;
        const float tagWidth = host_->measureText(tag_, tagStyle).w;
        ctx.r.drawText(tag_, tagStyle, {x, c.y, tagWidth + 2.0f, c.h}, preview_ ? t.warn : t.textBody);
        x += tagWidth + 14.0f;
        const float sizeRight = std::max(x, statusLeft_ - 14.0f);
        ctx.r.drawText(size_, sizeStyle, {x, c.y, std::max(0.0f, sizeRight - x), c.h}, t.textDim);
        Color statusColor = t.textDim;
        switch (statusColor_) {
        case StatusColor::Accent: statusColor = t.accentText; break;
        case StatusColor::Warn: statusColor = t.warn; break;
        case StatusColor::Danger: statusColor = t.danger; break;
        case StatusColor::Dim: break;
        }
        float statusLeft = statusLeft_;
        if (bar_->visible()) {
            statusLeft = bar_->bounds().right() + 10.0f;
        }
        ctx.r.drawText(status_, statusStyle, {statusLeft, c.y, std::max(0.0f, statusRight_ - statusLeft), c.h}, statusColor, {Align::End, Align::Center, true, false});
    }

private:
    enum class StatusColor {
        Dim,
        Accent,
        Warn,
        Danger,
    };

    PageActions& actions_;
    VersionId id_;
    std::wstring version_;
    std::wstring tag_;
    std::wstring size_;
    std::wstring status_;
    StatusColor statusColor_ = StatusColor::Dim;
    bool preview_ = false;
    bool installed_ = false;
    bool narrow_ = false;
    float actionsLeft_ = 0.0f;
    float statusLeft_ = 0.0f;
    float statusRight_ = 0.0f;
    Button* download_ = nullptr;
    Button* cancel_ = nullptr;
    Button* remove_ = nullptr;
    Button* retry_ = nullptr;
    Button* dismiss_ = nullptr;
    ProgressBar* bar_ = nullptr;
};

VersionsPage::VersionsPage(PageActions actions) : actions_(std::move(actions)) {
    search_ = add(std::make_unique<TextInput>(L"", [this](const std::wstring& text) {
        if (actions_.search) {
            actions_.search(text);
        }
    }));
    refresh_ = add(std::make_unique<IconButton>(Icon::Refresh, IconButtonKind::Plain, [this] {
        if (actions_.refreshCatalog) {
            actions_.refreshCatalog();
        }
    }));
    filters_ = add(std::make_unique<Tabs>(TabsKind::Filter, std::vector<std::wstring>{L"All", L"Release", L"Preview", L"Installed"}, [this](int index) {
        if (actions_.filter) {
            actions_.filter(static_cast<VersionFilter>(index));
        }
    }));
    scroll_ = add(std::make_unique<ScrollView>());
    scroll_->setPadding(12.0f, 0.0f, 12.0f, 12.0f);
    auto list = std::make_unique<ListView>(2.0f);
    list_ = list.get();
    scroll_->setContent(std::move(list));
}

void VersionsPage::focusSearch() {
    if (host_ != nullptr) {
        host_->requestFocus(search_);
    }
}

void VersionsPage::update(const AppState& state, const Strings& strings) {
    strings_ = &strings;
    search_->setPlaceholder(strings.searchVersion);
    if (search_->text() != state.search) {
        search_->setText(state.search);
    }
    refresh_->setSpinning(state.catalogLoading);
    refresh_->setTooltip(strings.refresh);
    filters_->setLabels({strings.filterAll, strings.filterRelease, strings.filterPreview, strings.filterInstalled});
    filters_->setSelected(static_cast<int>(state.filter));
    if (state.filter != lastFilter_) {
        // slide in from the side the selection moved toward, so the switch reads
        // as motion rather than a plain fade
        slideFrom_ = static_cast<int>(state.filter) > static_cast<int>(lastFilter_) ? 24.0f : -24.0f;
        lastFilter_ = state.filter;
        listFade_.jump(0.0f);
        listFade_.set(1.0f);
        if (host_ != nullptr) {
            host_->requestFrame();
        }
    }

    const auto visible = state.visibleRows();
    const bool narrow = bounds_.w < 800.0f;
    std::vector<std::string> keys;
    for (const auto* row : visible) {
        keys.push_back(row->id.key());
    }
    if (keys != rowKeys_) {
        rowKeys_ = keys;
        list_->clearChildren();
        rows_.clear();
        for (const auto* row : visible) {
            auto element = std::make_unique<VersionRowView>(actions_, row->id);
            rows_.push_back(element.get());
            list_->addChild(std::move(element));
        }
        scroll_->scrollTo(0.0f);
    }
    for (size_t i = 0; i < rows_.size() && i < visible.size(); ++i) {
        rows_[i]->apply(*visible[i], strings, state.armedRemove && *state.armedRemove == visible[i]->id, narrow);
    }
    empty_ = visible.empty();
    emptyText_ = state.versions.empty() ? strings.versionsEmpty : strings.noResults;
    if (host_ != nullptr) {
        arrange(bounds_);
    }
    invalidate();
}

Size VersionsPage::measure(const Size& available) {
    return available;
}

void VersionsPage::arrange(const Rect& bounds) {
    bounds_ = bounds;
    const float width = std::min(820.0f, bounds.w);
    column_ = {bounds.x + (bounds.w - width) / 2.0f, bounds.y, width, bounds.h};
    float y = column_.y + 14.0f;
    searchBox_ = {column_.x + 16.0f, y, column_.w - 32.0f, 38.0f};
    search_->arrange({searchBox_.x + 12.0f + 16.0f + 10.0f, searchBox_.y + 8.0f, searchBox_.w - 12.0f - 16.0f - 10.0f - 10.0f - 24.0f - 12.0f, 22.0f});
    refresh_->arrange({searchBox_.right() - 12.0f - 24.0f, searchBox_.y + 7.0f, 24.0f, 24.0f});
    y += 38.0f + 10.0f;
    const Size fs = filters_->measure({});
    filters_->arrange({column_.x + 16.0f + 4.0f, y, fs.w, 34.0f});
    y += 34.0f + 8.0f;
    scroll_->arrange({column_.x, y, column_.w, std::max(0.0f, column_.bottom() - y)});
}

void VersionsPage::render(RenderContext& ctx) {
    const Theme& t = ctx.theme;
    ctx.r.fillRect(searchBox_, t.inset, 8.0f);
    ctx.r.strokeRect(searchBox_, search_->focused() ? t.border3 : t.border, 1.0f, 8.0f);
    drawIcon(ctx.r, Icon::Search, {searchBox_.x + 12.0f, searchBox_.y + (38.0f - 16.0f) / 2.0f, 16.0f, 16.0f}, t.textMute, 2.0f);
    const Rect fb = filters_->bounds();
    ctx.r.fillRect({column_.x + 16.0f, fb.bottom() - 1.0f, column_.w - 32.0f, 1.0f}, t.hairline);
    if (listFade_.step(ctx.now) && host_ != nullptr) {
        host_->requestFrame();
    }
    for (const auto& child : children()) {
        if (!child->visible() || child.get() == static_cast<Element*>(scroll_)) {
            continue;
        }
        child->render(ctx);
    }
    if (scroll_->visible()) {
        const float listOpacity = listFade_.value();
        if (listOpacity >= 0.999f) {
            scroll_->render(ctx);
        } else {
            // the opacity layer is pushed untransformed so it clips the sliding
            // content to the list viewport
            ctx.r.pushOpacity(scroll_->bounds(), listOpacity);
            ctx.r.pushTransform(slideFrom_ * (1.0f - listOpacity), 0.0f);
            scroll_->render(ctx);
            ctx.r.popTransform();
            ctx.r.popOpacity();
        }
    }
    if (empty_) {
        const Rect area = scroll_->bounds();
        ctx.r.drawText(emptyText_, {Font::Body, 14.0f, 400, 0.0f}, {area.x, area.y + 40.0f, area.w, 20.0f}, t.textDim, {Align::Center, Align::Center, true, false});
    }
}

}
