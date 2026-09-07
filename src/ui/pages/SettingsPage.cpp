#include "ui/pages/SettingsPage.h"

#include "core/Text.h"
#include "ui/controls/Text.h"

#include <format>

namespace citron::ui {

namespace {

const TextStyle kLabel{Font::Mono, 10.0f, 500, 2.5f};
const TextStyle kTitle{Font::Title, 15.0f, 600, 0.0f};
const TextStyle kSub{Font::Body, 13.0f, 400, 0.0f};
const TextStyle kMono14{Font::Mono, 14.0f, 400, 0.0f};
const TextStyle kMono13{Font::Mono, 13.0f, 400, 0.0f};
const TextStyle kBody13{Font::Body, 13.0f, 400, 0.0f};

}

class SettingsContent : public Element {
public:
    SettingsContent(PageActions actions, AboutInfo about, std::span<const std::uint8_t> iconPng) : actions_(std::move(actions)), about_(std::move(about)), iconPng_(iconPng) {
        english_ = add(std::make_unique<Button>(L"English", ButtonKind::Segment, [this] { actions_.setLanguage("en"); }));
        german_ = add(std::make_unique<Button>(L"Deutsch", ButtonKind::Segment, [this] { actions_.setLanguage("de"); }));
        closeToggle_ = add(std::make_unique<Toggle>(true, [this](bool on) { actions_.setCloseOnLaunch(on); }));
        keepToggle_ = add(std::make_unique<Toggle>(true, [this](bool on) { actions_.setKeepInstallers(on); }));
        updateToggle_ = add(std::make_unique<Toggle>(true, [this](bool on) { actions_.setCheckUpdates(on); }));
        browse_ = add(std::make_unique<Button>(L"", ButtonKind::Secondary, [this] { actions_.browseRoot(); }));
        browse_->setHeight(36.0f);
        browse_->setPadding(14.0f, 14.0f);
        browse_->setFontSize(13.0f);
        apply_ = add(std::make_unique<Button>(L"", ButtonKind::Primary, [this] { actions_.applyRoot(); }));
        apply_->setHeight(38.0f);
        apply_->setPadding(18.0f, 18.0f);
        apply_->setFontSize(14.0f);
        reset_ = add(std::make_unique<Button>(L"", ButtonKind::Ghost, [this] { actions_.resetRoot(); }));
        check_ = add(std::make_unique<Button>(L"", ButtonKind::Secondary, [this] { actions_.checkUpdates(); }));
        check_->setHeight(32.0f);
        check_->setPadding(12.0f, 12.0f);
        check_->setFontSize(12.0f);
        check_->setRadius(6.0f);
        check_->setLeadingIcon(Icon::Refresh, 13.0f);
        github_ = add(std::make_unique<Button>(L"GitHub ↗", ButtonKind::Link, [this] { actions_.openGithub(); }));
        website_ = add(std::make_unique<Button>(L"", ButtonKind::Link, [this] { actions_.openWebsite(); }));
        licenses_ = add(std::make_unique<Button>(L"", ButtonKind::Link, [this] { actions_.openLicenses(); }));
    }

    void update(const AppState& state, const Strings& strings, const std::wstring& rootPath, const std::wstring& installersPath, const std::wstring& logsPath) {
        s_ = &strings;
        english_->setSelected(state.settings.language == "en");
        german_->setSelected(state.settings.language == "de");
        closeToggle_->setOn(state.settings.closeOnLaunch, false);
        keepToggle_->setOn(state.settings.keepInstallers, false);
        updateToggle_->setOn(state.settings.checkUpdates, false);
        rootPath_ = state.pendingRoot.empty() ? rootPath : state.pendingRoot;
        installersPath_ = installersPath;
        logsPath_ = logsPath;
        browse_->setLabel(strings.browse);
        apply_->setLabel(strings.apply);
        apply_->setEnabled(!state.pendingRoot.empty());
        reset_->setLabel(strings.reset);
        check_->setLabel(state.updateStatus == UpdateStatus::Checking ? strings.checking : strings.checkNow);
        check_->setBusy(state.updateStatus == UpdateStatus::Checking);
        website_->setLabel(strings.website + L" ↗");
        licenses_->setLabel(strings.licenses + L" ↗");
        switch (state.updateStatus) {
        case UpdateStatus::UpToDate:
            updateText_ = strings.upToDate;
            updateRole_ = TextRole::Accent;
            break;
        case UpdateStatus::Available:
            updateText_ = strings.updateAvailable + L": " + state.updateVersion;
            updateRole_ = TextRole::Warn;
            break;
        case UpdateStatus::Failed:
            updateText_ = strings.updateFailed;
            updateRole_ = TextRole::Dim;
            break;
        case UpdateStatus::Checking:
            updateText_ = strings.checking;
            updateRole_ = TextRole::Dim;
            break;
        case UpdateStatus::Unknown:
            updateText_ = strings.notChecked;
            updateRole_ = TextRole::Dim;
            break;
        }
        checkedText_.clear();
        if (state.updateCheckedAt > 0) {
            const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            const auto delta = std::max<std::int64_t>(0, now - state.updateCheckedAt);
            std::wstring ago;
            if (delta < 60) {
                ago = strings.justNow;
            } else if (delta < 3600) {
                const auto minutes = delta / 60;
                ago = std::vformat(strings.minutesAgo, std::make_wformat_args(minutes));
            } else {
                const auto hours = delta / 3600;
                ago = std::vformat(strings.hoursAgo, std::make_wformat_args(hours));
            }
            checkedText_ = std::vformat(strings.checkedAgo, std::make_wformat_args(ago));
        }
        invalidate();
    }

    Size measure(const Size& available) override {
        narrow_ = available.w < 800.0f;
        const float padX = narrow_ ? 26.0f : 32.0f;
        const float padY = narrow_ ? 22.0f : 28.0f;
        width_ = std::min(780.0f, available.w - padX * 2.0f);
        float y = padY;
        y += 12.0f;
        y += 22.0f + 38.0f + 16.0f + 1.0f;
        y += 3.0f * (16.0f + 38.0f + 16.0f + 1.0f);
        y += 30.0f + 12.0f;
        y += 18.0f + 40.0f + 12.0f;
        y += 2.0f * 20.0f + 8.0f + 16.0f + 1.0f;
        y += 16.0f + 38.0f;
        y += 34.0f + 12.0f;
        y += 16.0f + 40.0f + 14.0f + 1.0f;
        y += 14.0f + 20.0f + 8.0f + 20.0f + 8.0f;
        disclaimerHeight_ = host_ != nullptr && s_ != nullptr ? host_->measureText(s_->disclaimer, kBody13, std::max(50.0f, width_ - legalColumn_ - 24.0f), true).h : 20.0f;
        y += disclaimerHeight_ + 24.0f;
        return {available.w, y + padY};
    }

    void arrange(const Rect& bounds) override {
        bounds_ = bounds;
        const float padX = narrow_ ? 26.0f : 32.0f;
        const float padY = narrow_ ? 22.0f : 28.0f;
        const float left = bounds.x + padX;
        const float right = left + width_;
        float y = bounds.y + padY;
        labelY_ = y;
        y += 12.0f;
        y += 22.0f;
        languageRow_ = {left, y, width_, 38.0f};
        {
            const Size es = english_->measure({});
            const Size gs = german_->measure({});
            const float segWidth = es.w + gs.w + 6.0f;
            segment_ = {right - segWidth - 2.0f, y + 1.0f, segWidth + 2.0f, 36.0f};
            english_->arrange({segment_.x + 3.0f, segment_.y + 3.0f, es.w, 30.0f});
            german_->arrange({segment_.x + 3.0f + es.w, segment_.y + 3.0f, gs.w, 30.0f});
        }
        y += 38.0f + 16.0f;
        hairlines_.clear();
        hairlines_.push_back(y);
        y += 1.0f;
        Toggle* toggles[] = {closeToggle_, keepToggle_, updateToggle_};
        for (int i = 0; i < 3; ++i) {
            y += 16.0f;
            toggleRows_[i] = {left, y, width_, 38.0f};
            toggles[i]->arrange({right - 40.0f, y + 8.0f, 40.0f, 22.0f});
            y += 38.0f + 16.0f;
            hairlines_.push_back(y);
            y += 1.0f;
        }
        y += 30.0f;
        pathsLabelY_ = y;
        y += 12.0f;
        y += 18.0f;
        rootRow_ = {left, y, width_, 40.0f};
        {
            const Size bs = browse_->measure({});
            browse_->arrange({right - bs.w, y + 2.0f, bs.w, 36.0f});
        }
        y += 40.0f + 12.0f;
        pathGridY_ = y;
        y += 2.0f * 20.0f + 8.0f + 16.0f;
        hairlines_.push_back(y);
        y += 1.0f + 16.0f;
        {
            const Size as = apply_->measure({});
            apply_->arrange({left, y, as.w, 38.0f});
            const Size rs = reset_->measure({});
            reset_->arrange({left + as.w + 10.0f, y, rs.w, 38.0f});
        }
        y += 38.0f + 34.0f;
        aboutLabelY_ = y;
        y += 12.0f + 16.0f;
        aboutRow_ = {left, y, width_, 40.0f};
        {
            const Size cs = check_->measure({});
            check_->arrange({right - cs.w, y + 4.0f, cs.w, 32.0f});
        }
        y += 40.0f + 14.0f;
        hairlines_.push_back(y);
        y += 1.0f + 14.0f;
        infoGridY_ = y;
        {
            const float linksY = y + 20.0f + 8.0f;
            float lx = left + legalColumn_;
            for (Button* b : {github_, website_, licenses_}) {
                const Size s = b->measure({});
                b->arrange({lx, linksY, s.w, 20.0f});
                lx += s.w + 18.0f;
            }
        }
    }

    void render(RenderContext& ctx) override {
        if (s_ == nullptr) {
            return;
        }
        const Theme& t = ctx.theme;
        const Strings& s = *s_;
        const float left = languageRow_.x;
        const float width = width_;
        ctx.r.drawText(s.settingsUpper, kLabel, {left, labelY_, width, 12.0f}, t.textLabel);
        auto titleSub = [&](const Rect& row, const std::wstring& title, const std::wstring& sub, float textWidth) {
            ctx.r.drawText(title, kTitle, {row.x, row.y, textWidth, 20.0f}, t.textHi);
            ctx.r.drawText(sub, kSub, {row.x, row.y + 20.0f, textWidth, 18.0f}, t.textMute);
        };
        titleSub(languageRow_, s.language, s.languageSub, segment_.x - left - 20.0f);
        ctx.r.fillRect(segment_, t.inset, 8.0f);
        ctx.r.strokeRect(segment_, t.border, 1.0f, 8.0f);
        titleSub(toggleRows_[0], s.closeOnLaunch, s.closeOnLaunchSub, width - 60.0f);
        titleSub(toggleRows_[1], s.keepInstallers, s.keepInstallersSub, width - 60.0f);
        titleSub(toggleRows_[2], s.checkUpdates, s.checkUpdatesSub, width - 60.0f);
        for (const float y : hairlines_) {
            ctx.r.fillRect({left, y, width, 1.0f}, t.hairline);
        }
        ctx.r.drawText(s.pathsUpper, kLabel, {left, pathsLabelY_, width, 12.0f}, t.textLabel);
        const float rootTextWidth = browse_->bounds().x - left - 14.0f;
        ctx.r.drawText(s.rootDir, kSub, {rootRow_.x, rootRow_.y, rootTextWidth, 18.0f}, t.textMute);
        ctx.r.drawText(rootPath_, kMono14, {rootRow_.x, rootRow_.y + 18.0f + 4.0f, rootTextWidth, 20.0f}, t.textHi);
        const float col = std::max(host_->measureText(s.installers, kBody13).w, host_->measureText(s.logs, kBody13).w) + 24.0f;
        ctx.r.drawText(s.installers, kBody13, {left, pathGridY_, col, 20.0f}, t.textDim);
        ctx.r.drawText(installersPath_, kMono13, {left + col, pathGridY_, width - col, 20.0f}, t.textBody);
        ctx.r.drawText(s.logs, kBody13, {left, pathGridY_ + 28.0f, col, 20.0f}, t.textDim);
        ctx.r.drawText(logsPath_, kMono13, {left + col, pathGridY_ + 28.0f, width - col, 20.0f}, t.textBody);
        ctx.r.drawText(s.aboutUpper, kLabel, {left, aboutLabelY_, width, 12.0f}, t.textLabel);
        if (!icon_ && !iconPng_.empty()) {
            icon_ = ctx.r.loadPng(iconPng_);
            iconGeneration_ = ctx.r.generation();
        } else if (icon_ && iconGeneration_ != ctx.r.generation()) {
            icon_ = ctx.r.loadPng(iconPng_);
            iconGeneration_ = ctx.r.generation();
        }
        ctx.r.drawBitmap(icon_.get(), {aboutRow_.x, aboutRow_.y + 3.0f, 34.0f, 34.0f}, 1.0f, 9.0f);
        const float textX = aboutRow_.x + 34.0f + 14.0f;
        const TextStyle nameStyle{Font::Title, 15.0f, 700, 0.0f};
        const float nameWidth = host_->measureText(L"Citron Launcher", nameStyle).w;
        ctx.r.drawText(L"Citron Launcher", nameStyle, {textX, aboutRow_.y, nameWidth + 2.0f, 20.0f}, t.textHi);
        ctx.r.drawText(about_.version, {Font::Body, 15.0f, 500, 0.0f}, {textX + nameWidth + 6.0f, aboutRow_.y, 120.0f, 20.0f}, t.textMute);
        ctx.r.drawText(s.aboutSub, kSub, {textX, aboutRow_.y + 20.0f, check_->bounds().x - textX - 160.0f, 18.0f}, t.textMute);
        const float statusRight = check_->bounds().x - 14.0f;
        ctx.r.drawText(updateText_, {Font::Body, 13.0f, 600, 0.0f}, {statusRight - 200.0f, aboutRow_.y + 1.0f, 200.0f, 18.0f}, Text::roleColor(t, updateRole_), {Align::End, Align::Center, true, false});
        ctx.r.drawText(checkedText_, {Font::Body, 12.0f, 400, 0.0f}, {statusRight - 200.0f, aboutRow_.y + 21.0f, 200.0f, 16.0f}, t.textDim, {Align::End, Align::Center, true, false});
        const float infoCol = legalColumn_;
        ctx.r.drawText(s.build, kBody13, {left, infoGridY_, infoCol, 20.0f}, t.textDim);
        ctx.r.drawText(about_.build, kMono13, {left + infoCol, infoGridY_, width - infoCol, 20.0f}, t.textBody);
        ctx.r.drawText(s.links, kBody13, {left, infoGridY_ + 28.0f, infoCol, 20.0f}, t.textDim);
        ctx.r.drawText(s.legal, kBody13, {left, infoGridY_ + 56.0f, infoCol, 20.0f}, t.textDim);
        ctx.r.drawText(s.disclaimer, kBody13, {left + infoCol, infoGridY_ + 56.0f, width - infoCol, disclaimerHeight_ + 4.0f}, t.textDim, {Align::Start, Align::Start, false, true});
        Element::render(ctx);
    }

private:
    PageActions actions_;
    AboutInfo about_;
    std::span<const std::uint8_t> iconPng_;
    const Strings* s_ = nullptr;
    winrt::com_ptr<ID2D1Bitmap1> icon_;
    unsigned iconGeneration_ = 0;
    Button* english_ = nullptr;
    Button* german_ = nullptr;
    Toggle* closeToggle_ = nullptr;
    Toggle* keepToggle_ = nullptr;
    Toggle* updateToggle_ = nullptr;
    Button* browse_ = nullptr;
    Button* apply_ = nullptr;
    Button* reset_ = nullptr;
    Button* check_ = nullptr;
    Button* github_ = nullptr;
    Button* website_ = nullptr;
    Button* licenses_ = nullptr;
    std::wstring rootPath_;
    std::wstring installersPath_;
    std::wstring logsPath_;
    std::wstring updateText_;
    std::wstring checkedText_;
    TextRole updateRole_ = TextRole::Dim;
    bool narrow_ = false;
    float width_ = 0.0f;
    float labelY_ = 0.0f;
    float pathsLabelY_ = 0.0f;
    float pathGridY_ = 0.0f;
    float aboutLabelY_ = 0.0f;
    float infoGridY_ = 0.0f;
    float disclaimerHeight_ = 20.0f;
    float legalColumn_ = 70.0f;
    Rect languageRow_;
    Rect segment_;
    Rect toggleRows_[3];
    Rect rootRow_;
    Rect aboutRow_;
    std::vector<float> hairlines_;
};

SettingsPage::SettingsPage(PageActions actions, AboutInfo about, std::span<const std::uint8_t> iconPng) {
    scroll_ = add(std::make_unique<ScrollView>());
    auto content = std::make_unique<SettingsContent>(std::move(actions), std::move(about), iconPng);
    content_ = content.get();
    scroll_->setContent(std::move(content));
}

void SettingsPage::update(const AppState& state, const Strings& strings, const std::wstring& rootPath, const std::wstring& installersPath, const std::wstring& logsPath) {
    content_->update(state, strings, rootPath, installersPath, logsPath);
    if (host_ != nullptr) {
        arrange(bounds_);
    }
}

Size SettingsPage::measure(const Size& available) {
    return available;
}

void SettingsPage::arrange(const Rect& bounds) {
    bounds_ = bounds;
    scroll_->arrange(bounds);
}

}
