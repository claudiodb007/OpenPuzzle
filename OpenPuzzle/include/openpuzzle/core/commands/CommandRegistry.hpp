#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace openpuzzle {

using CommandHandler = std::function<int(const std::vector<std::string>&)>;

class CommandRegistry {
public:
    void registerCommand(const std::string& name, CommandHandler handler);

    bool has(const std::string& name) const;

    int execute(const std::string& name,
                const std::vector<std::string>& args) const;

private:
    std::unordered_map<std::string, CommandHandler> handlers_;
};

} // namespace openpuzzle
