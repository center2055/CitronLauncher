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

}
