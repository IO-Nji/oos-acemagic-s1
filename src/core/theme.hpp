#pragma once
#include "core/color.hpp"

// oOS theme. Semantic colour tokens: screens reference these roles, never raw colours,
// so the whole dashboard is restyled from this file. Palette taken from the oOS
// embedded UI (o_OS_t5): deep navy background, cyan accent, white/grey text.
// Translucent oOS styles are pre-mixed against the background because the panel has
// no alpha channel.
namespace Theme {
    // Base palette, 0xRRGGBB
    constexpr uint32_t P_BG      = 0x02141D;
    constexpr uint32_t P_ACCENT  = 0x0BF5F8;
    constexpr uint32_t P_ACCENT2 = 0x0ACCCA;
    constexpr uint32_t P_WHITE   = 0xFFFFFF;
    constexpr uint32_t P_TEXT    = 0xCFCCCC;
    constexpr uint32_t P_MUTED   = 0x808080;
    constexpr uint32_t P_OK      = 0x12F4A0;
    constexpr uint32_t P_WARN    = 0xF9B611;
    constexpr uint32_t P_CRIT    = 0xFF5A85;   // oOS 0xD31757 lightened to reach 4.5:1 as text
    constexpr uint32_t P_INFO    = 0x0795F7;

    // Surfaces
    constexpr uint16_t BLACK        = 0x0000;
    constexpr uint16_t BG           = colorHex(P_BG);
    constexpr uint32_t P_SURFACE    = mixHex(P_ACCENT, P_BG, 9);
    constexpr uint16_t SURFACE      = colorHex(P_SURFACE);                    // cards, header, footer
    constexpr uint16_t BORDER       = colorHex(mixHex(P_ACCENT, P_BG, 61));   // outer frame (oOS border opacity 155)
    constexpr uint16_t CARD_BORDER  = colorHex(mixHex(P_ACCENT, P_BG, 30));   // card outlines
    constexpr uint16_t DIVIDER      = colorHex(mixHex(P_ACCENT, P_BG, 30));   // horizontal rules
    constexpr uint16_t TRACK        = colorHex(mixHex(P_MUTED, P_BG, 38));    // unfilled part of gauges and bars
    constexpr uint16_t BAR_BORDER   = colorHex(mixHex(P_MUTED, P_BG, 70));    // outline of stacked bars

    // Text
    constexpr uint16_t TEXT         = colorHex(P_TEXT);     // names and plain text
    constexpr uint16_t TEXT_LABEL   = colorHex(0x8A9499);   // field labels, secondary text
    constexpr uint16_t TEXT_FAINT   = colorHex(0x8A9499);   // hints and placeholders
    constexpr uint16_t TITLE        = colorHex(P_ACCENT);   // page and section titles
    constexpr uint16_t VALUE        = colorHex(P_WHITE);    // primary readings
    constexpr uint16_t VALUE_DIM    = colorHex(0x9FB4BA);   // secondary readings
    constexpr uint16_t VALUE_ALT    = colorHex(P_ACCENT2);  // readings in the accent colour
    constexpr uint16_t CLOCK        = colorHex(P_WHITE);

    // State
    constexpr uint16_t OK           = colorHex(P_OK);
    constexpr uint16_t WARN         = colorHex(P_WARN);
    constexpr uint16_t CRIT         = colorHex(P_CRIT);
    constexpr uint16_t INFO         = colorHex(P_INFO);     // transitional states
    constexpr uint16_t OFF          = colorHex(0x8A9499);

    // Threshold colours for percentages and temperatures
    constexpr uint16_t LEVEL_OK     = OK;
    constexpr uint16_t LEVEL_WARN   = WARN;
    constexpr uint16_t LEVEL_CRIT   = CRIT;

    // Graph series
    constexpr uint16_t DOWN         = colorHex(P_ACCENT);                     // received traffic
    constexpr uint16_t DOWN_FILL    = colorHex(mixHex(P_ACCENT, P_BG, 28));
    constexpr uint16_t UP           = colorHex(P_WARN);                       // sent traffic
    constexpr uint16_t UP_FILL      = colorHex(mixHex(P_WARN, P_BG, 28));
    constexpr uint16_t GRAPH_LINE   = colorHex(P_ACCENT2);
    constexpr uint16_t GRAPH_FILL   = colorHex(mixHex(P_ACCENT, P_BG, 24));
    constexpr uint16_t GRAPH_BG     = colorHex(mixHex(P_ACCENT, P_BG, 4));

    // Memory breakdown bar
    constexpr uint16_t MEM_USED     = colorHex(P_ACCENT);
    constexpr uint16_t MEM_CACHED   = colorHex(P_INFO);
    constexpr uint16_t MEM_BUFFERS  = colorHex(P_OK);

    // Section title accents (one accent keeps the oOS look; change here to colour-code sections)
    constexpr uint16_t SEC_CPU      = TITLE;
    constexpr uint16_t SEC_MEM      = TITLE;
    constexpr uint16_t SEC_DISK     = TITLE;
    constexpr uint16_t SEC_POWER    = TITLE;
    constexpr uint16_t SEC_UPTIME   = TITLE;
}
