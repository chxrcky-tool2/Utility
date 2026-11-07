// themes.hpp — the ONE place every menu theme is defined.
// The Cocos menu (panel.cpp), the ImGui menu (imgui_ui.cpp) and the in-level HUD (hud.cpp)
// all read from kThemeDefs, so adding a theme = adding one row here. Nothing else to touch.
#pragma once

namespace cky {
    struct ThemeDef {
        char const*   name;          // the design's name, shown in the Theme row and in the macro folder name
        // The macro file extension for this theme: dot-first, filename-safe (".sea", ".Au", ...).
        // Macros are saved as <level>_<id>_slot<N><ext> inside "<name> theme macros". Changing name or
        // ext makes macros saved under the old one stop being found.
        char const*   ext;
        // Cocos menu colours, RGB 0-255
        unsigned char card[3];       // main card background
        unsigned char side[3];       // tab sidebar background
        unsigned char tab[3];        // tab button
        unsigned char off[3];        // toggle OFF
        unsigned char on[3];         // toggle ON
        unsigned char accent[3];     // +/- buttons, highlights
        unsigned char btn[3];        // regular buttons
        unsigned char title[3];      // title / headings text
        unsigned char top[3];        // border gradient, top
        unsigned char bottom[3];     // border gradient, bottom
        // ImGui menu + HUD colours, RGB 0-1
        float hi[3];                 // bright accent
        float lo[3];                 // deep accent
    };

    inline constexpr ThemeDef kThemeDefs[] = {
        // ---- original 10 (values unchanged) ----
        { "Neon", ".ne",
          {22,24,40}, {14,15,28}, {30,34,60}, {62,66,90}, {60,200,120}, {60,130,255}, {45,52,88}, {120,210,255},
          {255,120,190}, {225,25,50},
          {0.93f,0.30f,0.50f}, {0.62f,0.14f,0.30f} },
        { "Ocean", ".sea",
          {9,34,50}, {5,24,38}, {15,58,80}, {46,76,94}, {64,226,190}, {0,170,220}, {18,82,108}, {150,244,242},
          {72,226,210}, {6,66,130},
          {0.28f,0.88f,0.82f}, {0.03f,0.34f,0.56f} },
        { "Forest", ".leaf",
          {18,32,24}, {12,24,18}, {28,50,36}, {58,76,64}, {110,210,90}, {60,170,110}, {34,62,44}, {170,240,150},
          {120,220,120}, {20,110,70},
          {0.30f,0.76f,0.40f}, {0.10f,0.42f,0.26f} },
        { "Sunset", ".dusk",
          {38,22,30}, {28,14,22}, {60,34,44}, {88,62,70}, {255,170,60}, {255,100,90}, {74,40,52}, {255,200,140},
          {255,190,70}, {220,50,90},
          {1.00f,0.62f,0.22f}, {0.72f,0.20f,0.34f} },
        { "Mono", ".null",
          {24,24,26}, {16,16,18}, {40,40,44}, {70,70,76}, {200,200,206}, {130,130,140}, {52,52,58}, {230,230,235},
          {200,200,206}, {70,70,76},
          {0.72f,0.72f,0.76f}, {0.30f,0.30f,0.33f} },
        { "Cat", ".meow",       // cute light cat: near-white pink + peach pink paws (see fx.hpp for the paws)
          {255,244,247}, {255,231,237}, {255,208,198}, {236,210,216}, {255,160,148}, {255,176,164}, {255,200,188}, {226,110,134},
          {255,198,182}, {255,156,174},
          {1.00f,0.66f,0.58f}, {0.98f,0.76f,0.72f} },
        { "Vintage", ".vinyl",   // sepia / old paper
          {46,38,30}, {34,28,22}, {70,58,44}, {104,92,76}, {154,184,104}, {200,154,84}, {84,68,48}, {236,218,174},
          {218,188,134}, {122,80,50},
          {0.85f,0.70f,0.40f}, {0.48f,0.32f,0.20f} },
        { "Sky", ".cloud",
          {24,56,94}, {16,42,74}, {36,82,128}, {84,114,146}, {120,226,255}, {96,184,255}, {42,94,148}, {206,240,255},
          {156,218,255}, {72,152,232},
          {0.45f,0.78f,1.00f}, {0.20f,0.45f,0.78f} },
        { "Lavender", ".lilac",  // lavender purple
          {46,36,78}, {34,26,62}, {68,54,108}, {104,94,134}, {204,174,255}, {174,134,255}, {80,64,124}, {228,210,255},
          {218,194,255}, {132,92,222},
          {0.75f,0.62f,1.00f}, {0.42f,0.30f,0.72f} },
        { "Rose", ".thorn",      // light pink
          {78,42,60}, {62,32,48}, {108,60,84}, {140,104,120}, {255,176,206}, {255,134,174}, {124,64,94}, {255,228,238},
          {255,210,228}, {240,142,182},
          {1.00f,0.68f,0.80f}, {0.82f,0.38f,0.58f} },

        // ---- new 10 ----
        { "Cyber", ".tech",     // neon cyan on deep blue-black (grid, scanning laser, hex switches: see fx.hpp)
          {4,9,20}, {2,6,14}, {8,24,46}, {44,50,62}, {0,240,255}, {0,200,255}, {6,30,56}, {140,250,255},
          {0,235,255}, {0,70,150},
          {0.00f,0.94f,1.00f}, {0.00f,0.30f,0.52f} },
        { "Crimson", ".red",   // blood red on near-black
          {34,14,18}, {24,8,12}, {58,22,28}, {92,60,64}, {255,90,80}, {220,40,60}, {92,24,34}, {255,170,160},
          {255,70,70}, {120,10,30},
          {0.95f,0.26f,0.28f}, {0.50f,0.05f,0.14f} },
        { "Matrix", ".glitch",    // terminal green on pure black (digital rain, glitch text, binary toggles: see fx.hpp)
          {0,0,0}, {0,5,1}, {0,26,10}, {120,20,20}, {0,255,80}, {0,200,60}, {0,34,14}, {200,255,215},
          {0,255,70}, {0,70,24},
          {0.20f,1.00f,0.45f}, {0.00f,0.30f,0.12f} },
        { "Gold", ".Au",      // black & polished gold
          {26,22,14}, {16,13,8}, {48,40,22}, {86,76,56}, {255,208,60}, {230,170,30}, {72,58,24}, {255,230,150},
          {255,220,100}, {150,100,20},
          {1.00f,0.84f,0.30f}, {0.58f,0.38f,0.06f} },
        { "Aurora", ".earth",    // northern lights: aqua-green fading into violet
          {12,28,44}, {8,20,34}, {20,52,72}, {56,84,100}, {90,255,200}, {130,110,255}, {42,52,116}, {190,255,235},
          {90,255,200}, {120,70,230},
          {0.36f,1.00f,0.78f}, {0.46f,0.28f,0.90f} },
        { "Toxic", ".U",     // acid lime over poison purple
          {22,18,30}, {14,10,22}, {38,28,52}, {76,68,92}, {178,255,40}, {170,60,255}, {62,36,90}, {214,255,130},
          {190,255,60}, {110,30,170},
          {0.74f,1.00f,0.16f}, {0.43f,0.12f,0.66f} },
        { "Dracula", ".vamp",   // the classic purple / pink / green editor palette
          {40,42,54}, {30,31,41}, {58,60,80}, {98,104,128}, {80,250,123}, {189,147,249}, {68,71,90}, {248,248,242},
          {255,121,198}, {98,114,164},
          {1.00f,0.47f,0.78f}, {0.55f,0.40f,0.85f} },
        { "Molten rocks", ".lava",      // glowing magma on charcoal
          {28,16,12}, {18,9,7}, {52,26,18}, {92,64,56}, {255,140,30}, {255,70,20}, {96,34,18}, {255,190,120},
          {255,160,40}, {190,30,10},
          {1.00f,0.50f,0.12f}, {0.70f,0.12f,0.04f} },
        { "Mint", ".mint",      // cool pastel mint
          {30,62,58}, {22,48,46}, {44,90,84}, {104,138,130}, {170,255,215}, {110,230,190}, {50,110,100}, {220,255,240},
          {190,255,225}, {70,190,160},
          {0.62f,1.00f,0.84f}, {0.22f,0.65f,0.55f} },
        { "Arctic", ".ice",    // frosted slate-blue
          {34,44,60}, {26,34,48}, {52,66,88}, {104,118,140}, {136,192,208}, {129,161,193}, {67,86,116}, {236,239,244},
          {216,232,248}, {94,129,172},
          {0.78f,0.88f,0.96f}, {0.35f,0.48f,0.70f} },
    };

    inline constexpr int ThemeCount = static_cast<int>(sizeof(kThemeDefs) / sizeof(kThemeDefs[0]));
}
