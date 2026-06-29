#include "openpuzzle/adapters/bitcrack/BitCrackOutputParser.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

namespace openpuzzle::bitcrack {

std::string BitCrackOutputParser::trim(std::string value) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());

    return value;
}

ParsedLine BitCrackOutputParser::parse(const std::string& line) const {
    ParsedLine result;

    {
        std::regex speedRegex(R"(([0-9]+(?:\.[0-9]+)?)\s*MKey/s)", std::regex::icase);
        std::smatch match;
        if (std::regex_search(line, match, speedRegex)) {
            result.type = ParsedLineType::Speed;
            result.value = match[1].str();
            result.speedMKeys = std::stod(result.value);
            return result;
        }
    }

    {
        std::regex startingRegex(R"(Starting at:\s*([0-9A-Fa-f]+))", std::regex::icase);
        std::smatch match;
        if (std::regex_search(line, match, startingRegex)) {
            result.type = ParsedLineType::StartingKey;
            result.value = trim(match[1].str());
            return result;
        }
    }

    {
        std::regex endingRegex(R"(Ending at:\s*([0-9A-Fa-f]+))", std::regex::icase);
        std::smatch match;
        if (std::regex_search(line, match, endingRegex)) {
            result.type = ParsedLineType::EndingKey;
            result.value = trim(match[1].str());
            return result;
        }
    }

    {
        std::regex countingRegex(R"(Counting by:\s*([0-9A-Fa-f]+))", std::regex::icase);
        std::smatch match;
        if (std::regex_search(line, match, countingRegex)) {
            result.type = ParsedLineType::CountingBy;
            result.value = trim(match[1].str());
            return result;
        }
    }

    if (line.find("Error:") != std::string::npos || line.find("[Error]") != std::string::npos) {
        result.type = ParsedLineType::Error;
        result.value = trim(line);
        return result;
    }

    if (line.find("Private key") != std::string::npos ||
        line.find("FOUND") != std::string::npos ||
        line.find("Found") != std::string::npos) {
        result.type = ParsedLineType::Found;
        result.value = trim(line);
        return result;
    }

    if (line.find("Finished") != std::string::npos || line.find("finished") != std::string::npos) {
        result.type = ParsedLineType::Finished;
        result.value = trim(line);
        return result;
    }

    return result;
}

} // namespace openpuzzle::bitcrack
