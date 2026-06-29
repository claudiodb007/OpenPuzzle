#pragma once
#include <string>
#include <boost/multiprecision/cpp_int.hpp>
namespace openpuzzle {
class KeySpace {
public:
    using Int = boost::multiprecision::cpp_int;
    KeySpace() = default;
    KeySpace(Int start, Int end);
    static KeySpace fromHexPair(const std::string& startHex, const std::string& endHex);
    static KeySpace fromColonString(const std::string& text);
    const Int& start() const { return start_; }
    const Int& end() const { return end_; }
    Int sizeInclusive() const;
    bool valid() const;
    static Int hexToInt(const std::string& hex);
    static std::string intToHex(const Int& value, int minWidth = 0);
    std::string startHex(int minWidth = 0) const;
    std::string endHex(int minWidth = 0) const;
    std::string toColonString(int minWidth = 0) const;
private:
    Int start_{0};
    Int end_{0};
};
}
