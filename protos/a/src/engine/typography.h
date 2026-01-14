#pragma once

// Typography profile with power-law font size distribution
// All sizes in pixels

namespace Typography {

// Base font size (body text)
#ifdef WEB_BUILD
constexpr float BASE_FONT_SIZE = 20.0f;  // Larger for mobile readability
#else
constexpr float BASE_FONT_SIZE = 16.0f;
#endif

// Power law exponent for heading scale
// Each heading level is BASE * (SCALE_RATIO ^ (6 - level))
// This creates a smooth typographic hierarchy
constexpr float SCALE_RATIO = 1.2f;  // Minor third scale

// Heading font sizes (power law: base * ratio^n)
// H6 = base, H5 = base*1.2, H4 = base*1.44, H3 = base*1.728, H2 = base*2.074, H1 = base*2.488
constexpr float H1_SIZE = BASE_FONT_SIZE * 2.488f;  // ~40px
constexpr float H2_SIZE = BASE_FONT_SIZE * 2.074f;  // ~33px
constexpr float H3_SIZE = BASE_FONT_SIZE * 1.728f;  // ~28px
constexpr float H4_SIZE = BASE_FONT_SIZE * 1.44f;   // ~23px
constexpr float H5_SIZE = BASE_FONT_SIZE * 1.2f;    // ~19px
constexpr float H6_SIZE = BASE_FONT_SIZE;           // 16px

// Get font size for heading level (1-6)
inline float headingSize(int level) {
    switch (level) {
        case 1: return H1_SIZE;
        case 2: return H2_SIZE;
        case 3: return H3_SIZE;
        case 4: return H4_SIZE;
        case 5: return H5_SIZE;
        case 6: return H6_SIZE;
        default: return BASE_FONT_SIZE;
    }
}

// Spacing
constexpr float PARAGRAPH_MARGIN = 8.0f;       // 0.5rem equivalent
constexpr float HEADING_MARGIN_TOP = 16.0f;    // 1rem - extra space before headings
constexpr float HEADING_MARGIN_BOTTOM = 8.0f;  // 0.5rem

// Block spacing
constexpr float BLOCK_SPACING = 8.0f;  // Space between all block elements

// Document margins
constexpr float DOCUMENT_MARGIN = 32.0f;

// Blockquote
constexpr float BLOCKQUOTE_INDENT = 16.0f;
constexpr float BLOCKQUOTE_BAR_WIDTH = 3.0f;

// Code blocks
constexpr float CODE_BLOCK_PADDING = 6.0f;

// List indentation
constexpr float LIST_INDENT = 20.0f;

// Line height multiplier
constexpr float LINE_HEIGHT_RATIO = 1.0f;  // 1.0 = same as font size

}  // namespace Typography
