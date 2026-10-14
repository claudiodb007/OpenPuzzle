#pragma once
#include "openpuzzle/ui/WindowsUiProcess.hpp"
#ifdef _WIN32
namespace openpuzzle::ui {
// Same colors as src/ui/UiTheme.cpp, without a Qt dependency in the native panel.
struct WindowsUiPalette {
 COLORREF background,panel,soft,border,text,muted,input,accent,accentText,secondary;
 static WindowsUiPalette get(bool dark){
  if(dark)return {RGB(8,11,16),RGB(16,21,29),RGB(23,29,39),RGB(41,49,61),RGB(245,241,231),RGB(166,173,186),RGB(12,17,24),RGB(227,178,60),RGB(17,19,24),RGB(34,42,54)};
  return {RGB(244,246,250),RGB(255,255,255),RGB(247,248,252),RGB(224,229,237),RGB(24,34,53),RGB(104,115,138),RGB(255,255,255),RGB(189,134,22),RGB(255,255,255),RGB(233,237,245)};
 }
};
}
#endif
