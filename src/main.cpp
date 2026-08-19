// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#include "capabilities.h"
#include "coloring.h"
#include "engine.h"
#include "font.h"
#include "options.h"
#include "os.h"
#include "record.h"
#include "screen.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

bool check_compatibility(const capabilities& caps, const options& options)
{
    if (!caps.has_soft_fonts && !options.yolo) {
        std::cout << "VT Chess requires a VT220-compatible terminal or better.\n";
        std::cout << "Try 'vtchess --yolo' to bypass the compatibility checks.\n";
        return false;
    }
    if (caps.height < 24) {
        std::cout << "VT Chess requires a minimum screen height of 24.\n";
        return false;
    }
    if (caps.width < 80) {
        std::cout << "VT Chess requires a minimum screen width of 80.\n";
        return false;
    }
    return true;
}

static auto title_banner(const capabilities& caps)
{
    constexpr auto title = "VT CHESS";
    const auto y = caps.height / 2;
    const auto x = caps.width / 4 - 3;
    std::cout << "\033[" << y << ';' << x << "H\033#3" << title;
    std::cout << "\033[" << (y + 1) << ';' << x << "H\033#4" << title;
    std::cout.flush();
    return [=]() {
        const auto start_time = std::chrono::system_clock::now();
        // Wait for the terminal to finish loading fonts.
        caps.query_cursor_position();
        // But if that is quick, still leave the title up for a second.
        std::this_thread::sleep_until(start_time + 1s);
        // Erase the title lines.
        std::cout << "\033[" << y << "H\033[K";
        std::cout << "\033[" << (y + 1) << "H\033[K";
    };
}

int main(const int argc, const char* argv[])
{
    os os;

    options options(argc, argv);
    if (options.exit)
        return 1;

    capabilities caps;
    if (!check_compatibility(caps, options))
        return 1;

    // Set default attributes.
    std::cout << "\033[m";
    // Set the window title.
    std::cout << "\033]21;VT Chess\033\\";
    // Clear the screen.
    std::cout << "\033[2J";
    // Hide the cursor.
    std::cout << "\033[?25l";
    // Save the original state of the status display.
    const auto original_decssdt = caps.query_setting("$~");
    // Hide the status line.
    std::cout << "\033[0$~";
    // If we have less than 25 rows, try and get the terminal to use the
    // additional row just freed up by the hidden status line.
    auto original_decslpp = std::string{};
    if (caps.height < 25) {
        original_decslpp = caps.query_setting("t");
        std::cout << "\033[25t";
        // Reset the margins, else they'll still be limited to 24.
        std::cout << "\033[r";
        // Recalculate the window size to see if we have more space now.
        caps.query_window_size();
    }
    // Display title banner
    const auto clear_banner = title_banner(caps);
    // Setup the color assignment and palette.
    const auto colors = coloring{caps, options};
    // Load the soft font.
    const auto font = soft_font{caps};
    // Clear the title banner once that's done.
    clear_banner();

    screen screen{caps, options};
    record record{screen};
    engine engine{screen, record, caps, options};
    while (engine.run())
        record.reset();

    // Reset attributes.
    std::cout << "\033[m";
    // Clear the window title.
    std::cout << "\033]21;\033\\";
    // Clear the screen.
    std::cout << "\033[H\033[J";
    // Restore the original status display type.
    if (!original_decssdt.empty())
        std::cout << "\033[" << original_decssdt;
    // Restore the original lines per page.
    if (!original_decslpp.empty())
        std::cout << "\033[" << original_decslpp;
    // Show the cursor.
    std::cout << "\033[?25h";

    return 0;
}
