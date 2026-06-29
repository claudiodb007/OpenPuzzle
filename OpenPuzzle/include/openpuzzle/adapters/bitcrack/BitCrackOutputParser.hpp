#pragma once

#include <optional>
#include <string>

namespace openpuzzle::bitcrack {

enum class ParsedLineType {
    Unknown,
    Speed,
    StartingKey,
    EndingKey,
    CountingBy,
    Found,
    Error,
    Finished
};

struct ParsedLine {
    ParsedLineType type = ParsedLineType::Unknown;
    std::string value;
    double speedMKeys = 0.0;
};

class BitCrackOutputParser {
public:
    ParsedLine parse(const std::string& line) const;

private:
    static std::string trim(std::string value);
};

} // namespace openpuzzle::bitcrack
