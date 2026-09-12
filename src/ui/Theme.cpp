#include "ui/Theme.h"

namespace citron::ui {

Theme Theme::darkTheme() {
    Theme t;
    t.dark = true;
    t.bg = Color::rgb(0x0b0c0a);
    t.raised = Color::rgb(0x141610);
    t.textHi = Color::rgb(0xf3f6e9);
    t.textBody = Color::rgb(0xc3c9b4);
    t.textMute = Color::rgb(0x9aa288);
    t.textDim = Color::rgb(0x7f8870);
    t.textFaint = Color::rgb(0x565c4b);
    t.textLabel = Color::rgb(0x5c634f);
    t.inset = Color::white(0.04f);
    t.hover = Color::white(0.04f);
    t.sel = Color::white(0.07f);
    t.hairline = Color::white(0.07f);
    t.border = Color::white(0.09f);
    t.border2 = Color::white(0.14f);
    t.border3 = Color::white(0.32f);
    t.toggleOff = Color::white(0.10f);
    t.knobOff = Color::rgb(0xcfd4c2);
    t.scrim = Color::black(0.6f);
    t.accentText = Color::rgb(0xc8f135);
    t.danger = Color::rgb(0xff8a8a);
    t.warn = Color::rgb(0xe5c15a);
    return t;
}

Theme Theme::lightTheme() {
    Theme t;
    t.dark = false;
    t.bg = Color::rgb(0xf6f7f2);
    t.raised = Color::rgb(0xffffff);
    t.textHi = Color::rgb(0x161a10);
    t.textBody = Color::rgb(0x3b4232);
    t.textMute = Color::rgb(0x5f6752);
    t.textDim = Color::rgb(0x767e68);
    t.textFaint = Color::rgb(0xa3a996);
    t.textLabel = Color::rgb(0x8a917c);
    t.inset = Color::black(0.035f);
    t.hover = Color::black(0.04f);
    t.sel = Color::black(0.07f);
    t.hairline = Color::black(0.08f);
    t.border = Color::black(0.10f);
    t.border2 = Color::black(0.16f);
    t.border3 = Color::black(0.36f);
    t.toggleOff = Color::black(0.14f);
    t.knobOff = Color::rgb(0xffffff);
    t.scrim = Color::rgb(0x14180c, 0.35f);
    t.accentText = Color::rgb(0x6f8a00);
    t.danger = Color::rgb(0xc93b3b);
    t.warn = Color::rgb(0xa3780f);
    t.dangerTint = Color::rgb(0xc93b3b, 0.10f);
    return t;
}

Theme mixTheme(const Theme& a, const Theme& b, float k) {
    Theme t = b;
    t.bg = mix(a.bg, b.bg, k);
    t.raised = mix(a.raised, b.raised, k);
    t.textHi = mix(a.textHi, b.textHi, k);
    t.textBody = mix(a.textBody, b.textBody, k);
    t.textMute = mix(a.textMute, b.textMute, k);
    t.textDim = mix(a.textDim, b.textDim, k);
    t.textFaint = mix(a.textFaint, b.textFaint, k);
    t.textLabel = mix(a.textLabel, b.textLabel, k);
    t.inset = mix(a.inset, b.inset, k);
    t.hover = mix(a.hover, b.hover, k);
    t.sel = mix(a.sel, b.sel, k);
    t.hairline = mix(a.hairline, b.hairline, k);
    t.border = mix(a.border, b.border, k);
    t.border2 = mix(a.border2, b.border2, k);
    t.border3 = mix(a.border3, b.border3, k);
    t.toggleOff = mix(a.toggleOff, b.toggleOff, k);
    t.knobOff = mix(a.knobOff, b.knobOff, k);
    t.scrim = mix(a.scrim, b.scrim, k);
    t.accentText = mix(a.accentText, b.accentText, k);
    t.danger = mix(a.danger, b.danger, k);
    t.warn = mix(a.warn, b.warn, k);
    t.accent = mix(a.accent, b.accent, k);
    t.accentHover = mix(a.accentHover, b.accentHover, k);
    t.accentPressed = mix(a.accentPressed, b.accentPressed, k);
    t.accentInk = mix(a.accentInk, b.accentInk, k);
    t.selectedTint = mix(a.selectedTint, b.selectedTint, k);
    t.dangerTint = mix(a.dangerTint, b.dangerTint, k);
    t.closeHover = mix(a.closeHover, b.closeHover, k);
    return t;
}

}
