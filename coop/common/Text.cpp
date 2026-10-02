#include "Text.h"

#include "Protocol.h"

#include <algorithm>
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

std::string SanitizeText(const std::string& text, size_t maxChars) {
    std::string valid = ToValidUtf8(text);
    std::string cleaned;
    cleaned.reserve(valid.size());
    for (size_t i = 0; i < valid.size(); i++) {
        unsigned char c = (unsigned char)valid[i];
        if (c == '\r') {
            if (i + 1 >= valid.size() || valid[i + 1] != '\n') {
                cleaned.push_back('\n');
            }
        } else if (c == '\n') {
            cleaned.push_back('\n');
        } else if (c == '\t') {
            cleaned.push_back(' ');
        } else if (c >= 0x20 && c != 0x7F) {
            cleaned.push_back((char)c);
        }
    }
    size_t begin = cleaned.find_first_not_of(" \n");
    if (begin == std::string::npos) {
        return "";
    }
    size_t end = cleaned.find_last_not_of(" \n");
    cleaned = cleaned.substr(begin, end - begin + 1);

    size_t pos = 0;
    size_t chars = 0;
    while (pos < cleaned.size() && chars < maxChars) {
        pos += Utf8SequenceLength((unsigned char)cleaned[pos]); // well formed: ToValidUtf8 ran first
        chars++;
    }
    cleaned.resize(std::min(pos, cleaned.size()));
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

// Length of the well-formed UTF-8 sequence that starts at text[i], or 0 if there is none.
static size_t ValidUtf8Sequence(const std::string& text, size_t i) {
    unsigned char lead = (unsigned char)text[i];
    if (lead < 0x80) {
        return 1;
    }
    size_t len = 0;
    uint32_t codePoint = 0;
    uint32_t smallest = 0; // a smaller code point in this many bytes is an overlong form
    if (lead >= 0xC2 && lead <= 0xDF) {
        len = 2;
        codePoint = lead & 0x1F;
        smallest = 0x80;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        len = 3;
        codePoint = lead & 0x0F;
        smallest = 0x800;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        len = 4;
        codePoint = lead & 0x07;
        smallest = 0x10000;
    } else {
        return 0;
    }
    if (i + len > text.size()) {
        return 0;
    }
    for (size_t k = 1; k < len; k++) {
        unsigned char next = (unsigned char)text[i + k];
        if ((next & 0xC0) != 0x80) {
            return 0;
        }
        codePoint = (codePoint << 6) | (next & 0x3F);
    }
    if (codePoint < smallest || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
        return 0;
    }
    return len;
}

bool IsValidUtf8(const std::string& text) {
    for (size_t i = 0; i < text.size();) {
        size_t len = ValidUtf8Sequence(text, i);
        if (len == 0) {
            return false;
        }
        i += len;
    }
    return true;
}

std::string ToValidUtf8(std::string text) {
    for (size_t i = 0; i < text.size();) {
        size_t len = ValidUtf8Sequence(text, i);
        if (len == 0) {
            text[i] = '?';
            len = 1;
        }
        i += len;
    }
    return text;
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

// The code point of the well-formed sequence of len bytes at text[i].
static uint32_t DecodeUtf8(const std::string& text, size_t i, size_t len) {
    static const unsigned char kLeadBits[] = { 0, 0x7F, 0x1F, 0x0F, 0x07 };
    uint32_t codePoint = (unsigned char)text[i] & kLeadBits[len];
    for (size_t k = 1; k < len; k++) {
        codePoint = (codePoint << 6) | ((unsigned char)text[i + k] & 0x3F);
    }
    return codePoint;
}

// U+00C0..U+00FF (the accented letters of Latin-1) in plain ASCII.
static const char* const kLatin1Letters[64] = {
    "A", "A", "A", "A", "A", "A", "AE", "C", "E", "E", "E", "E", "I", "I", "I", "I",  // U+00C0
    "D", "N", "O", "O", "O", "O", "O",  "x", "O", "U", "U", "U", "U", "Y", "Th", "ss", // U+00D0
    "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",  // U+00E0
    "d", "n", "o", "o", "o", "o", "o",  "/", "o", "u", "u", "u", "u", "y", "th", "y",  // U+00F0
};

std::string ToGameFontText(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        size_t len = ValidUtf8Sequence(text, i);
        if (len == 0) {
            out.push_back('?');
            i++;
            continue;
        }
        uint32_t c = DecodeUtf8(text, i, len);
        i += len;
        if (c == '\n' || (c >= 0x20 && c < 0x7F)) {
            out.push_back((char)c);
        } else if (c == '\t' || c == 0xA0) { // a tab, a no-break space
            out.push_back(' ');
        } else if (c < 0x20 || c == 0x7F || c == 0xA1 || c == 0xBF) {
            continue; // control characters (the font's own codes), ¡ and ¿
        } else if (c >= 0xC0 && c <= 0xFF) {
            out += kLatin1Letters[c - 0xC0];
        } else if (c == 0x2018 || c == 0x2019) {
            out.push_back('\'');
        } else if (c == 0x201C || c == 0x201D || c == 0xAB || c == 0xBB) {
            out.push_back('"');
        } else if (c == 0x2013 || c == 0x2014) {
            out.push_back('-');
        } else if (c == 0x2026) {
            out += "...";
        } else {
            out.push_back('?');
        }
    }
    return out;
}

void SplitTextBoxes(std::string& text, char lineBreak, char boxBreak, int linesPerBox) {
    int line = 1; // the line of its box the text is on
    for (char& c : text) {
        if (c == boxBreak) {
            line = 1;
        } else if (c == lineBreak) {
            if (line >= linesPerBox) {
                c = boxBreak;
                line = 1;
            } else {
                line++;
            }
        }
    }
}

} // namespace coop
