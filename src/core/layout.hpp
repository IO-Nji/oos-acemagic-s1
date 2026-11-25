#pragma once

// Fixed geometry of the 320x170 panel: outer frame, header, footer and the page area.
namespace Layout {
    constexpr int SCREEN_WIDTH = 320;
    constexpr int SCREEN_HEIGHT = 170;
    
    constexpr int OUTER_PADDING = 3;    // screen edge to frame
    constexpr int BORDER_WIDTH = 1;
    constexpr int BORDER_RADIUS = 4;
    constexpr int INNER_PADDING = 6;    // frame to content
    constexpr int INSET = OUTER_PADDING + BORDER_WIDTH + INNER_PADDING;
    
    constexpr int BORDER_X = OUTER_PADDING;
    constexpr int BORDER_Y = OUTER_PADDING;
    constexpr int BORDER_W = SCREEN_WIDTH - 2 * OUTER_PADDING;
    constexpr int BORDER_H = SCREEN_HEIGHT - 2 * OUTER_PADDING;
    
    constexpr int CONTENT_X = INSET;
    constexpr int CONTENT_Y = INSET;
    constexpr int CONTENT_W = SCREEN_WIDTH - 2 * INSET;
    constexpr int CONTENT_H = SCREEN_HEIGHT - 2 * INSET;
    
    constexpr int HEADER_HEIGHT = 14;
    constexpr int HEADER_Y = CONTENT_Y;
    constexpr int HEADER_DIVIDER_Y = HEADER_Y + HEADER_HEIGHT;
    
    constexpr int FOOTER_HEIGHT = 14;
    constexpr int FOOTER_Y = CONTENT_Y + CONTENT_H - FOOTER_HEIGHT;
    constexpr int FOOTER_DIVIDER_Y = FOOTER_Y - 1;
    
    // Page area between header and footer, with a 2px gap on each side
    constexpr int MAIN_Y = HEADER_DIVIDER_Y + 2;
    constexpr int MAIN_H = FOOTER_Y - MAIN_Y - 2;
}

struct Rect {
    int x, y, w, h;
    
    constexpr Rect(int x_, int y_, int w_, int h_) : x(x_), y(y_), w(w_), h(h_) {}
    
    constexpr int right() const { return x + w; }
    constexpr int bottom() const { return y + h; }
    
    // The col-th of totalCols equal columns
    constexpr Rect column(int col, int totalCols, int gap) const {
        int colW = (w - (totalCols - 1) * gap) / totalCols;
        return Rect(x + col * (colW + gap), y, colW, h);
    }
};

namespace Layout {
    constexpr Rect MAIN(CONTENT_X, MAIN_Y, CONTENT_W, MAIN_H);
    constexpr int CARD_GAP = 3;         // space between cards on the S1
}

// How a device arranges the dashboard on its display: where the header, the page
// and the footer go, and how much space separates things. The defaults are the S1.
struct Chrome {
    int width = Layout::SCREEN_WIDTH;     // area the dashboard occupies, from the top-left
    int height = Layout::SCREEN_HEIGHT;
    bool framed = true;                   // rounded frame around everything, bars as flat strips;
                                          // otherwise no frame and the bars are drawn as cards
    int margin = Layout::INSET;           // display edge to content
    int barGap = 2;                       // header and footer to the page area
    int cardGap = Layout::CARD_GAP;       // between cards on a page
    
    constexpr Rect header() const {
        return Rect(margin, margin, width - 2 * margin, Layout::HEADER_HEIGHT);
    }
    constexpr Rect footer() const {
        return Rect(margin, height - margin - Layout::FOOTER_HEIGHT, width - 2 * margin, Layout::FOOTER_HEIGHT);
    }
    constexpr Rect page() const {
        int top = header().bottom() + barGap;
        return Rect(margin, top, width - 2 * margin, footer().y - barGap - top);
    }
};
