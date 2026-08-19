// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#include "capabilities.h"

#include "os.h"

#include <cstring>
#include <iostream>

using namespace std::string_literals;

capabilities::capabilities()
{
    // Save the cursor position.
    std::cout << "\0337";
    // Request 7-bit C1 controls from the terminal.
    std::cout << "\033 F";
    // Determine the addressable page size.
    std::cout << "\033[999;999H";
    const auto size = query_cursor_position();
    if (size) std::tie(height, width) = size.value();
    // But on terminals with windowing, what we really care about is the visible
    // window size. This will replace the height and width calculated above.
    query_window_size();
    // Check if 8-bit controls are supported.
    std::cout << "\0338\2335n\033[1K";
    has_8bit = !_query(R"(\x1B\[\d*n)", true).empty();
    // Retrieve the device attributes report for feature detection.
    _query_device_attributes();
    // Retrieve the terminal id so we can guess the font size.
    _query_terminal_id();
    // Hack to detect KoalaTerm which has SGR issues we need to deal with.
    has_broken_sgr = (terminal_id == 41 && query_setting("m").empty());
    // Disable scrollback so we can use pages.
    _original_decrpl = query_mode(112);
    std::cout << "\033[?112l";
    // Query DECPPM to determine if pages are supported.
    has_pages = query_mode(64).has_value();
    // Restore the cursor position.
    std::cout << "\0338";
}

capabilities::~capabilities()
{
    // Restore the original scrollback state.
    if (_original_decrpl == true)
        std::cout << "\033[?112h";
}

void capabilities::query_window_size()
{
    std::cout << "\033[\"v";
    const auto report = _query(R"(\x1B\[(\d+);(\d+)[;\d]+"w)", true);
    if (!report.empty()) {
        height = std::stoi(report[1]);
        width = std::stoi(report[2]);
    }
}

std::optional<std::pair<int, int>> capabilities::query_cursor_position() const
{
    std::cout << "\033[6n";
    const auto pos = _query(R"(\x1B\[(\d+);(\d+)R)", false);
    if (pos.empty()) return {};
    return std::make_pair(std::stoi(pos[1]), std::stoi(pos[2]));
}

std::optional<bool> capabilities::query_mode(const int mode) const
{
    std::cout << "\033[?" << mode << "$p";
    const auto report = _query(R"(\x1B\[\?(\d+);(\d+)\$y)", true);
    if (!report.empty()) {
        const auto returned_mode = std::stoi(report[1]);
        const auto status = std::stoi(report[2]);
        if (returned_mode == mode) {
            if (status == 1) return true;
            if (status == 2) return false;
        }
    }
    return {};
}

std::string capabilities::query_setting(const std::string_view setting) const
{
    std::cout << "\033P$q" << setting << "\033\\";
    const auto report = _query(R"(\x1BP1\$r(.*)\x1B\\)", true);
    if (!report.empty())
        return report[1];
    else
        return {};
}

std::string capabilities::query_color_table() const
{
    std::cout << "\033[2;2$u";
    const auto report = _query(R"(\x1BP2\$s(.*)\x1B\\)", true);
    if (!report.empty())
        return report[1];
    else
        return {};
}

void capabilities::_query_device_attributes()
{
    std::cout << "\033[c";
    // The Reflection Desktop terminal sometimes uses comma separators
    // instead of semicolons in their DA report, so we allow for either.
    const auto report = _query(R"(\x1B\[\?(\d+)([;,\d]*)c)", false);
    if (!report.empty()) {
        // The first parameter indicates the terminal conformance level.
        level = std::stoi(report[1]);
        // Level 4+ conformance implies support for rectangle ops.
        if (level >= 64)
            has_rectangle_ops = true;
        // The remaining parameters indicate additional feature extensions.
        const auto features = report[2].str();
        const auto digits = std::regex(R"(\d+)");
        auto it = std::sregex_iterator(features.begin(), features.end(), digits);
        while (it != std::sregex_iterator()) {
            const auto feature = std::stoi(it->str());
            switch (feature) {
                case 4: has_sixel = true; break;
                case 7: has_soft_fonts = true; break;
                case 22: has_color = true; break;
                case 28: has_rectangle_ops = true; break;
            }
            it++;
        }
    }
}

void capabilities::_query_terminal_id()
{
    std::cout << "\033[>c";
    const auto report = _query(R"(\x1B\[>(\d+)[;\d]*c)", true);
    if (!report.empty()) {
        terminal_id = std::stoi(report[1]);
    }
}

std::smatch capabilities::_query(const char* pattern, const bool may_not_work)
{
    auto final_char = pattern[strlen(pattern) - 1];
    if (may_not_work) {
        // If we're uncertain this query is supported, we'll send an extra DA
        // or DSR-CPR query to make sure that we get some kind of response.
        if (final_char == 'R') {
            final_char = 'c';
            std::cout << "\033[c";
        } else {
            final_char = 'R';
            std::cout << "\033[6n";
        }
    }
    std::cout.flush();
    // This needs to be static so the returned smatch can reference it.
    static auto response = std::string{};
    response.clear();
    auto last_escape = 0;
    for (;;) {
        const auto ch = os::getch();
        // Ignore XON, XOFF
        if (ch == '\021' || ch == '\023')
            continue;
        // If we've sent an extra query, the last escape should be the
        // start of that response, which we'll ultimately drop.
        if (may_not_work && ch == '\033')
            last_escape = response.length();
        response += ch;
        if (ch == final_char) break;
    }
    // Drop the extra response if one was requested.
    if (may_not_work)
        response = response.substr(0, last_escape);
    auto match = std::smatch{};
    std::regex_match(response, match, std::regex(pattern));
    return match;
}
