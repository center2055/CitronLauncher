#pragma once

#include "ui/Geometry.h"

namespace citron::ui {

struct Theme {
    bool dark = true;
    Color bg, raised, textHi, textBody, textMute, textDim, textFaint, textLabel;
    Color inset, hover, sel, hairline, border, border2, border3, toggleOff, knobOff, scrim;
    Color accentText, danger, warn;
    Color accent = Color::rgb(0xc8f135);
    Color accentHover = Color::rgb(0xd4f55a);
    Color accentPressed = Color::rgb(0xb3d92f);
    Color accentInk = Color::rgb(0x10140b);
    Color selectedTint = Color::rgb(0xc8f135, 0.07f);
    Color dangerTint = Color::rgb(0xff8a8a, 0.12f);
    Color closeHover = Color::rgb(0xc4322f);

    static Theme darkTheme();
    static Theme lightTheme();
};

}
