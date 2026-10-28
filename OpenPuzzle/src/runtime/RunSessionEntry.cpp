#include "openpuzzle/runtime/RunSession.hpp"
#include "openpuzzle/engines/VanityRunProfile.hpp"
#ifdef OPENPUZZLE_ENABLE_VANITY_PREVIEW
#include "VanityRunRoute.hpp"
#endif
#include <algorithm>
#include <cctype>
#include <iostream>

namespace openpuzzle {
int RunSession::run(const std::vector<std::string>& args) const {
  bool vanity = false;
  bool explicitLocal = false;
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--vanity-session" || args[i] == "--vanity-native-local" || args[i] == "--receipt-protocol") explicitLocal = vanity = true;
    if (args[i] != "--engine" || i + 1 == args.size()) continue;
    auto value = args[i + 1];
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {return std::tolower(c);});
    vanity = vanity || (value == "vanity" && !VanityRunProfile::configured()) || value == "vanity-preview";
  }
  if (vanity || explicitLocal) {
#ifdef OPENPUZZLE_ENABLE_VANITY_PREVIEW
    return vanityPreview::runLocalSessionRoute(args);
#else
    std::cerr << "Vanity is experimental: production assignments are not enabled.\nNo assignment requested.\n";
    return 1;
#endif
  }
  return runOrdinary(args);
}
}
