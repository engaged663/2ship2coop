#include "Text.h"

#include "Protocol.h"

#include <charconv>

namespace coop {

bool IsValidNick(const std::string& nick) {
    if (nick.size() < (size_t)kNickMin || nick.size() > (size_t)kNickMax) {
        return false;
    }
    for (unsigned char c : nick) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        if (!ok) {
            return false;
        }
    }
    return true;
}

// Length in bytes of the UTF-8 sequence that starts with this lead byte (1 for stray bytes).
static size_t Utf8SequenceLength(unsigned char lead) {
    if (lead >= 0xF0) {
        return 4;
    }
    if (lead >= 0xE0) {
        return 3;
    }
    if (lead >= 0xC0) {
        return 2;
    }
    return 1;
}

std::string SanitizeChat(const std::string& text, size_t maxChars) {
    std::string cleaned;
    cleaned.reserve(text.size());
    for (unsigned char c : text) {
        if (c == '\t' || c == '\n' || c == '\r') {
            cleaned.push_back(' ');
        } else if (c >= 0x20 && c != 0x7F) {
            cleaned.push_back((char)c);
        }
    }

    size_t begin = cleaned.find_first_not_of(' ');
    if (begin == std::string::npos) {
        return "";
    }
    size_t end = cleaned.find_last_not_of(' ');
    cleaned = cleaned.substr(begin, end - begin + 1);

    size_t pos = 0;
    size_t chars = 0;
    while (pos < cleaned.size() && chars < maxChars) {
        size_t len = Utf8SequenceLength((unsigned char)cleaned[pos]);
        if (pos + len > cleaned.size()) {
            break; // truncated sequence at the end: drop it
        }
        pos += len;
        chars++;
    }
    cleaned.resize(pos);
    return cleaned;
}

std::vector<std::string> SplitCommandLine(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    bool hasToken = false;
    for (char c : line) {
        if (c == '"') {
            inQuotes = !inQuotes;
            hasToken = true;
        } else if (c == ' ' && !inQuotes) {
            if (hasToken) {
                tokens.push_back(current);
                current.clear();
                hasToken = false;
            }
        } else {
            current.push_back(c);
            hasToken = true;
        }
    }
    if (hasToken) {
        tokens.push_back(current);
    }
    return tokens;
}

std::string JoinFrom(const std::vector<std::string>& tokens, size_t start) {
    std::string out;
    for (size_t i = start; i < tokens.size(); i++) {
        if (!out.empty()) {
            out.push_back(' ');
        }
        out += tokens[i];
    }
    return out;
}

std::string ToLower(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
    }
    return text;
}

bool EqualsIgnoreCase(const std::string& a, const std::string& b) {
    return a.size() == b.size() && ToLower(a) == ToLower(b);
}

bool ParseInt(const std::string& text, int& out) {
    if (text.empty()) {
        return false;
    }
    int value = 0;
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc() || result.ptr != text.data() + text.size()) {
        return false;
    }
    out = value;
    return true;
}

} // namespace coop
