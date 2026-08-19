// VT Chess
// Copyright (c) 2026 James Holderness
// Distributed under the MIT License

#include "coloring.h"

#include "capabilities.h"
#include "options.h"

#include <iostream>

coloring::coloring(capabilities& caps, const options& options)
    : _using_colors{options.color && caps.has_color}
{
    if (_using_colors) {
        // Save the current text color assignment.
        _color_assignment = caps.query_setting("1,|");
        // Make sure the text color assignment is white on black.
        if (!_color_assignment.empty())
            std::cout << "\033[1;7;0,|";
        // Save the current color table.
        _color_table = caps.query_color_table();
        // If the color table can be queried, it's safe to assume we can set it
        // to the palette we need. If not, we won't use color at all.
        if (!_color_table.empty())
            std::cout << "\033P2$p0;2;0;0;0/1;2;56;37;26/4;2;24;49;73/7;2;95;95;95/8;2;50;50;50/9;2;80;70;57/12;2;30;55;80/15;2;80;80;80\033\\";
        else
            caps.has_color = false;
    } else if (caps.has_sixel && caps.terminal_id == 2) {  // VT240
        // We set the first 4 colors to grayscale to override the VT240 defaults,
        // but for this to work correctly, it must be configured for Color Display.
        std::cout << "\033P0;1q#1;2;70;70;70#2;2;100;100;100#3;2;70;70;70#4;2;0;0;0\033\\";
    } else if (caps.terminal_id == 18 || caps.terminal_id == 19) {  // VT330/VT340
        // Color table 1 will hopefully prevent the VT330 and VT340 using a dimmer
        // background for reverse video, but the default palette should be fine.
        std::cout << "\033[1){";
    }
}

coloring::~coloring()
{
    if (_using_colors) {
        // Restore the original color assignment.
        if (!_color_assignment.empty())
            std::cout << "\033[" << _color_assignment;
        // Restore the original color table.
        if (!_color_table.empty())
            std::cout << "\033P2$p" << _color_table << "\033\\";
    }
    // We don't bother restoring the VT240 color table here because that will
    // be reset by the DECSTR sequence in the soft_font destructor.
}
