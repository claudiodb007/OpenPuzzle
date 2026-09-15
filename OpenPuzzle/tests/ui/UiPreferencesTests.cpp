#include "UiLanguage.hpp"
#include "UiTheme.hpp"

#include <cassert>

int main() {
  using namespace openpuzzle::ui;

  assert(languageFromCode("unknown") == UiLanguage::English);
  assert(languageCode(UiLanguage::English) == "en");
  assert(languageCode(UiLanguage::Portuguese) == "pt");
  assert(languageCode(UiLanguage::French) == "fr");
  assert(languageCode(UiLanguage::Spanish) == "es");

  assert(translated(UiLanguage::English, "start") == "Start");
  assert(translated(UiLanguage::Portuguese, "start") == "Iniciar");
  assert(translated(UiLanguage::French, "start") == "Démarrer");
  assert(translated(UiLanguage::Spanish, "stop_now") ==
         "Detener ahora");
  assert(translated(UiLanguage::English, "execution_mode") ==
         "Execution mode");
  assert(translated(UiLanguage::Portuguese, "cpu_threads") ==
         "Threads de CPU");
  assert(translated(UiLanguage::French, "opencl_device") ==
         "Périphérique OpenCL");
  assert(translated(UiLanguage::Spanish, "rusticl_radeonsi") ==
         "GPU AMD mediante Rusticl (radeonsi)");
  assert(translated(UiLanguage::Portuguese, "assignment") ==
         "Atribuição");
  assert(translated(UiLanguage::French, "status_details") ==
         "État détaillé");

  assert(themeFromCode("unknown") == UiTheme::Light);
  assert(themeFromCode("dark") == UiTheme::Dark);
  assert(themeCode(UiTheme::Light) == "light");
  assert(themeCode(UiTheme::Dark) == "dark");

  const QString light = themeStyleSheet(UiTheme::Light);
  const QString dark = themeStyleSheet(UiTheme::Dark);
  assert(light.contains("#f4f6fa"));
  assert(dark.contains("#080b10"));
  assert(dark.contains("#e3b23c"));
  assert(dark.contains("background: transparent"));
  assert(!dark.contains("QMainWindow, QWidget"));
  assert(dark.contains(
      "QFrame#Card {\n      background: #10151d;"));
  assert(dark.contains(
      "QFrame#Metric {\n      background: #171d27;"));
  assert(dark.contains(
      "QLabel#StatusBadge {\n      background: #b91c1c;"));
  assert(dark.contains(
      "QLabel#StatusBadge[active=\"true\"] {\n"
      "      background: #15803d;"));
  assert(!dark.contains(
      "QFrame#Card {\n      background: #27351f;"));

  return 0;
}
