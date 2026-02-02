#include <string.h>
#include <symbos.h>
#include "font_big.c"
#include "font_txt.c"

#define TAB_TEXTLEN 12288
#define TAB_LINKLEN 1024
#define MAX_TABS 16
#define MAX_HISTORY 16
#define PROGRESS_COLOR (COLOR_ORANGE << 6) | (COLOR_RED << 4) | (COLOR_BLACK << 2) | COLOR_BLACK

unsigned char screenmode = 0;

char textbuf[TAB_LINKLEN];
char pathbuf[256];
unsigned long addr_stack[16];
unsigned char addr_i = 0;
unsigned char tab_bank[MAX_TABS];
unsigned char tab_linkbank[MAX_TABS];
char* tab_text[MAX_TABS];
char* tab_ctrl[MAX_TABS];
char* tab_link[MAX_TABS];
unsigned short tab_len[MAX_TABS];
unsigned long tab_hist[MAX_TABS][MAX_HISTORY];
unsigned char tab_hist_on[MAX_TABS];
unsigned char tab_hist_max[MAX_TABS];
unsigned char tab_i = 0;
_data char tab_titles[MAX_TABS][16] =
   {{'+', 0}, {'+', 0}, {'+', 0}, {'+', 0}, {'+', 0}, {'+', 0}, {'+', 0}, {'+', 0},
    {'+', 0}, {'+', 0}, {'+', 0}, {'+', 0}, {'+', 0}, {'+', 0}, {'+', 0}, {'+', 0}};

typedef struct {
    unsigned short start;
    unsigned short end;
    unsigned long addr;
} Link;

// page content
unsigned char gfx_splash_bank = 0;
char* gfx_splash_addr = 0;
unsigned short gfx_splash_len = 0;
unsigned char gfx_page1_bank = 0;
char* gfx_page1_addr = 0;
unsigned char gfx_page2_bank = 0;
char* gfx_page2_addr = 0;
unsigned char gfx_hover_bank = 0;
char* gfx_hover_addr = 0;
unsigned char gfx_header_bank = 0;
char* gfx_header_addr = 0;
unsigned short max_page_size = 4539;
unsigned short max_hover_size = 3537;
unsigned short max_header_size = 1059;
unsigned char on_page = 0;
unsigned char hover_coords[3][5][4] =
  {{{16, 12, 65, 138}, {132, 78, 223, 120}, {150, 45, 190, 77}, {73, 49, 93, 135}, {0, 0, 0, 0}},
   {{0, 18, 36, 97}, {46, 47, 86, 133}, {141, 62, 165, 129}, {192, 0, 224, 110}, {94, 61, 125, 123}},
   {{0, 36, 48, 112}, {53, 85, 84, 143}, {120, 78, 192, 146}, {162, 24, 224, 106}, {78, 38, 125, 125}}};
unsigned char hover_xy[3][5][4] =
  {{{20, 12, 64, 126}, {132, 78, 92, 33}, {150, 45, 40, 32}, {73, 49, 20, 86}, {0, 0, 0, 0}},
   {{0, 18, 36, 79}, {46, 47, 40, 86}, {141, 62, 24, 67}, {192, 0, 32, 111}, {94, 61, 32, 63}},
   {{1, 46, 36, 61}, {53, 85, 32, 59}, {108, 77, 88, 69}, {168, 31, 48, 20}, {90, 65, 24, 8}}};
unsigned short hover_offsets[2][3][5] =
  {{{0, 2019, 2781, 3104, 0},
    {0, 714, 1577, 1982, 2873},
    {0, 552, 1027, 2548, 2791}},
   {{0, 4042, 5570, 6220, 0},
    {0, 1432, 3162, 3976, 5762},
    {0, 1108, 2062, 5108, 5598}}};
unsigned long about_addr = 621455460;
unsigned long hover_dest[3][5] =
   {{176, 404022, 319187, 0, 0},
    {501467, 617200, 589564, 667538, 0},
    {754928, 859410, 960537, 1044078, 0}};
unsigned long chunks_start[16] = {0, 32665841, 73775262, 114825337, 156097402, 202761217, 258438717, 302835235, 345086420, 392591423, 429604444, 477468671, 504784656, 536425067, 560578616, 588879823};

// window 8x8 icon
_transfer char icon[19] = {0x02, 0x08, 0x08, 0xFF, 0xFF, 0xCD, 0x33, 0x88, 0x19, 0x89, 0x11, 0x88, 0x19, 0x89, 0x11, 0xB8, 0xD1, 0xFF, 0xFF};

// form1 controls
_transfer Ctrl c_area = {0, C_AREA, -1, COLOR_RED, 0, 0, 10000, 10000};
_transfer Ctrl c_erase = {1, C_AREA, -1, COLOR_RED, 28, 50, 172, 10};
_transfer Ctrl c_splash0 = {2, C_IMAGE, -1, 0, 28, 60, 172, 40};
_transfer Ctrl c_splash1 = {3, C_IMAGE, -1, 0, 8, 8, 68, 98};
_transfer Ctrl c_splash2 = {4, C_IMAGE, -1, 0, 78, 8, 68, 98};
_transfer Ctrl c_splash3 = {5, C_IMAGE, -1, 0, 148, 8, 68, 98};
_transfer Ctrl c_splash4 = {6, C_IMAGE, -1, 0, 194, 144, 24, 11};
_transfer Ctrl c_splash5 = {7, C_IMAGE, -1, 0, 5, 144, 28, 11};
_transfer Ctrl c_cursor = {8, C_FRAME, -1, (COLOR_RED << 4) | (COLOR_YELLOW << 2) | COLOR_YELLOW, 9, 9, 66, 84};
_transfer Ctrl_Group ctrls = {3, 0, &c_area};

// form1 alternate page controls
_transfer Ctrl c1_back1 = {0, C_IMAGE, -1, 0, 0, 0, 224, 81};
_transfer Ctrl c1_back2 = {1, C_IMAGE, -1, 0, 0, 81, 224, 81};
_transfer Ctrl c1_hover = {2, C_IMAGE, -1, 0, 0, 0, 0, 0};
_transfer Ctrl_Group ctrls1 = {2, 0, &c1_back1};

// form1 window data record
_transfer Window form1 = {
    WIN_NORMAL,
    WIN_CLOSE | WIN_TITLE | WIN_ICON,
    0,          // PID
    10, 10,     // x, y
    224, 162,   // w, h
    0, 0,       // xscroll, yscroll
    224, 162,   // wfull, hfull
    224, 162,   // wmin, hmin
    224, 162,   // wmax, hmax
    icon,       // icon
    "Encontra '26", // title text
    0,          // no status text
    0,          // no menu
    &ctrls};    // controls

// tabs
_transfer Ctrl_Tabs cd2_tabs = {1, (COLOR_BLACK << 6) | (COLOR_RED << 4) | (COLOR_BLACK << 2) | COLOR_ORANGE, 0};
_transfer Ctrl_Tab tabs[MAX_TABS] =
   {{&tab_titles[0][0], -1},
    {&tab_titles[1][0], -1},
    {&tab_titles[2][0], -1},
    {&tab_titles[3][0], -1},
    {&tab_titles[4][0], -1},
    {&tab_titles[5][0], -1},
    {&tab_titles[6][0], -1},
    {&tab_titles[7][0], -1},
    {&tab_titles[8][0], -1},
    {&tab_titles[9][0], -1},
    {&tab_titles[10][0], -1},
    {&tab_titles[11][0], -1},
    {&tab_titles[12][0], -1},
    {&tab_titles[13][0], -1},
    {&tab_titles[14][0], -1},
    {&tab_titles[15][0], -1}};

// list
_data char articles[4096];
_transfer Ctrl_Text_Font cd2_titles[240] =
   {{articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
	{articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text},
    {articles, (COLOR_BLACK << 4) | COLOR_WHITE, ALIGN_LEFT | TEXT_16COLOR, font_text}};

_transfer Ctrl cc_area = {0, C_AREA, -1, COLOR_YELLOW, 0, 0, 256, 2160};
_transfer Ctrl cc_titles[240] =
   {{10, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[0], 1, 0, 256, 9},
    {11, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[1], 1, 9, 256, 9},
    {12, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[2], 1, 18, 256, 9},
    {13, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[3], 1, 27, 256, 9},
    {14, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[4], 1, 36, 256, 9},
    {15, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[5], 1, 45, 256, 9},
    {16, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[6], 1, 54, 256, 9},
    {17, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[7], 1, 63, 256, 9},
    {18, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[8], 1, 72, 256, 9},
    {19, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[9], 1, 81, 256, 9},
    {20, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[10], 1, 90, 256, 9},
    {21, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[11], 1, 99, 256, 9},
    {22, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[12], 1, 108, 256, 9},
    {23, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[13], 1, 117, 256, 9},
    {24, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[14], 1, 126, 256, 9},
    {25, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[15], 1, 135, 256, 9},
    {26, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[16], 1, 144, 256, 9},
    {27, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[17], 1, 153, 256, 9},
    {28, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[18], 1, 162, 256, 9},
    {29, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[19], 1, 171, 256, 9},
    {30, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[20], 1, 180, 256, 9},
    {31, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[21], 1, 189, 256, 9},
    {32, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[22], 1, 198, 256, 9},
    {33, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[23], 1, 207, 256, 9},
    {34, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[24], 1, 216, 256, 9},
    {35, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[25], 1, 225, 256, 9},
    {36, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[26], 1, 234, 256, 9},
    {37, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[27], 1, 243, 256, 9},
    {38, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[28], 1, 252, 256, 9},
    {39, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[29], 1, 261, 256, 9},
    {40, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[30], 1, 270, 256, 9},
    {41, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[31], 1, 279, 256, 9},
    {42, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[32], 1, 288, 256, 9},
    {43, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[33], 1, 297, 256, 9},
    {44, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[34], 1, 306, 256, 9},
    {45, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[35], 1, 315, 256, 9},
    {46, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[36], 1, 324, 256, 9},
    {47, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[37], 1, 333, 256, 9},
    {48, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[38], 1, 342, 256, 9},
    {49, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[39], 1, 351, 256, 9},
    {50, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[40], 1, 360, 256, 9},
    {51, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[41], 1, 369, 256, 9},
    {52, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[42], 1, 378, 256, 9},
    {53, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[43], 1, 387, 256, 9},
    {54, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[44], 1, 396, 256, 9},
    {55, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[45], 1, 405, 256, 9},
    {56, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[46], 1, 414, 256, 9},
    {57, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[47], 1, 423, 256, 9},
    {58, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[48], 1, 432, 256, 9},
    {59, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[49], 1, 441, 256, 9},
    {60, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[50], 1, 450, 256, 9},
    {61, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[51], 1, 459, 256, 9},
    {62, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[52], 1, 468, 256, 9},
    {63, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[53], 1, 477, 256, 9},
    {64, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[54], 1, 486, 256, 9},
    {65, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[55], 1, 495, 256, 9},
    {66, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[56], 1, 504, 256, 9},
    {67, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[57], 1, 513, 256, 9},
    {68, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[58], 1, 522, 256, 9},
    {69, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[59], 1, 531, 256, 9},
    {70, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[60], 1, 540, 256, 9},
    {71, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[61], 1, 549, 256, 9},
    {72, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[62], 1, 558, 256, 9},
    {73, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[63], 1, 567, 256, 9},
    {74, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[64], 1, 576, 256, 9},
    {75, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[65], 1, 585, 256, 9},
    {76, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[66], 1, 594, 256, 9},
    {77, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[67], 1, 603, 256, 9},
    {78, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[68], 1, 612, 256, 9},
    {79, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[69], 1, 621, 256, 9},
    {80, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[70], 1, 630, 256, 9},
    {81, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[71], 1, 639, 256, 9},
    {82, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[72], 1, 648, 256, 9},
    {83, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[73], 1, 657, 256, 9},
    {84, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[74], 1, 666, 256, 9},
    {85, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[75], 1, 675, 256, 9},
    {86, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[76], 1, 684, 256, 9},
    {87, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[77], 1, 693, 256, 9},
    {88, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[78], 1, 702, 256, 9},
    {89, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[79], 1, 711, 256, 9},
    {90, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[80], 1, 720, 256, 9},
    {91, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[81], 1, 729, 256, 9},
    {92, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[82], 1, 738, 256, 9},
    {93, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[83], 1, 747, 256, 9},
    {94, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[84], 1, 756, 256, 9},
    {95, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[85], 1, 765, 256, 9},
    {96, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[86], 1, 774, 256, 9},
    {97, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[87], 1, 783, 256, 9},
    {98, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[88], 1, 792, 256, 9},
    {99, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[89], 1, 801, 256, 9},
    {100, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[90], 1, 810, 256, 9},
    {101, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[91], 1, 819, 256, 9},
    {102, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[92], 1, 828, 256, 9},
    {103, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[93], 1, 837, 256, 9},
    {104, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[94], 1, 846, 256, 9},
    {105, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[95], 1, 855, 256, 9},
    {106, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[96], 1, 864, 256, 9},
    {107, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[97], 1, 873, 256, 9},
    {108, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[98], 1, 882, 256, 9},
    {109, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[99], 1, 891, 256, 9},
    {110, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[100], 1, 900, 256, 9},
    {111, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[101], 1, 909, 256, 9},
    {112, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[102], 1, 918, 256, 9},
    {113, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[103], 1, 927, 256, 9},
    {114, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[104], 1, 936, 256, 9},
    {115, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[105], 1, 945, 256, 9},
    {116, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[106], 1, 954, 256, 9},
    {117, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[107], 1, 963, 256, 9},
    {118, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[108], 1, 972, 256, 9},
    {119, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[109], 1, 981, 256, 9},
    {120, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[110], 1, 990, 256, 9},
    {121, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[111], 1, 999, 256, 9},
    {122, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[112], 1, 1008, 256, 9},
    {123, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[113], 1, 1017, 256, 9},
    {124, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[114], 1, 1026, 256, 9},
    {125, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[115], 1, 1035, 256, 9},
    {126, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[116], 1, 1044, 256, 9},
    {127, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[117], 1, 1053, 256, 9},
    {128, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[118], 1, 1062, 256, 9},
    {129, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[119], 1, 1071, 256, 9},
    {130, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[120], 1, 1080, 256, 9},
    {131, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[121], 1, 1089, 256, 9},
    {132, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[122], 1, 1098, 256, 9},
    {133, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[123], 1, 1107, 256, 9},
    {134, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[124], 1, 1116, 256, 9},
    {135, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[125], 1, 1125, 256, 9},
    {136, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[126], 1, 1134, 256, 9},
    {137, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[127], 1, 1143, 256, 9},
    {138, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[128], 1, 1152, 256, 9},
    {139, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[129], 1, 1161, 256, 9},
    {140, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[130], 1, 1170, 256, 9},
    {141, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[131], 1, 1179, 256, 9},
    {142, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[132], 1, 1188, 256, 9},
    {143, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[133], 1, 1197, 256, 9},
    {144, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[134], 1, 1206, 256, 9},
    {145, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[135], 1, 1215, 256, 9},
    {146, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[136], 1, 1224, 256, 9},
    {147, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[137], 1, 1233, 256, 9},
    {148, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[138], 1, 1242, 256, 9},
    {149, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[139], 1, 1251, 256, 9},
    {150, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[140], 1, 1260, 256, 9},
    {151, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[141], 1, 1269, 256, 9},
    {152, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[142], 1, 1278, 256, 9},
    {153, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[143], 1, 1287, 256, 9},
    {154, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[144], 1, 1296, 256, 9},
    {155, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[145], 1, 1305, 256, 9},
    {156, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[146], 1, 1314, 256, 9},
    {157, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[147], 1, 1323, 256, 9},
    {158, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[148], 1, 1332, 256, 9},
    {159, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[149], 1, 1341, 256, 9},
    {160, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[150], 1, 1350, 256, 9},
    {161, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[151], 1, 1359, 256, 9},
    {162, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[152], 1, 1368, 256, 9},
    {163, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[153], 1, 1377, 256, 9},
    {164, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[154], 1, 1386, 256, 9},
    {165, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[155], 1, 1395, 256, 9},
    {166, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[156], 1, 1404, 256, 9},
    {167, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[157], 1, 1413, 256, 9},
    {168, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[158], 1, 1422, 256, 9},
    {169, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[159], 1, 1431, 256, 9},
    {170, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[160], 1, 1440, 256, 9},
    {171, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[161], 1, 1449, 256, 9},
    {172, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[162], 1, 1458, 256, 9},
    {173, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[163], 1, 1467, 256, 9},
    {174, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[164], 1, 1476, 256, 9},
    {175, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[165], 1, 1485, 256, 9},
    {176, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[166], 1, 1494, 256, 9},
    {177, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[167], 1, 1503, 256, 9},
    {178, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[168], 1, 1512, 256, 9},
    {179, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[169], 1, 1521, 256, 9},
    {180, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[170], 1, 1530, 256, 9},
    {181, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[171], 1, 1539, 256, 9},
    {182, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[172], 1, 1548, 256, 9},
    {183, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[173], 1, 1557, 256, 9},
    {184, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[174], 1, 1566, 256, 9},
    {185, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[175], 1, 1575, 256, 9},
    {186, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[176], 1, 1584, 256, 9},
    {187, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[177], 1, 1593, 256, 9},
    {188, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[178], 1, 1602, 256, 9},
    {189, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[179], 1, 1611, 256, 9},
    {190, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[180], 1, 1620, 256, 9},
    {191, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[181], 1, 1629, 256, 9},
    {192, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[182], 1, 1638, 256, 9},
    {193, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[183], 1, 1647, 256, 9},
    {194, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[184], 1, 1656, 256, 9},
    {195, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[185], 1, 1665, 256, 9},
    {196, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[186], 1, 1674, 256, 9},
    {197, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[187], 1, 1683, 256, 9},
    {198, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[188], 1, 1692, 256, 9},
    {199, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[189], 1, 1701, 256, 9},
    {200, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[190], 1, 1710, 256, 9},
    {201, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[191], 1, 1719, 256, 9},
    {202, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[192], 1, 1728, 256, 9},
    {203, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[193], 1, 1737, 256, 9},
    {204, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[194], 1, 1746, 256, 9},
    {205, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[195], 1, 1755, 256, 9},
    {206, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[196], 1, 1764, 256, 9},
    {207, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[197], 1, 1773, 256, 9},
    {208, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[198], 1, 1782, 256, 9},
    {209, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[199], 1, 1791, 256, 9},
    {210, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[200], 1, 1800, 256, 9},
    {211, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[201], 1, 1809, 256, 9},
    {212, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[202], 1, 1818, 256, 9},
    {213, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[203], 1, 1827, 256, 9},
    {214, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[204], 1, 1836, 256, 9},
    {215, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[205], 1, 1845, 256, 9},
    {216, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[206], 1, 1854, 256, 9},
    {217, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[207], 1, 1863, 256, 9},
    {218, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[208], 1, 1872, 256, 9},
    {219, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[209], 1, 1881, 256, 9},
    {220, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[210], 1, 1890, 256, 9},
    {221, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[211], 1, 1899, 256, 9},
    {222, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[212], 1, 1908, 256, 9},
    {223, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[213], 1, 1917, 256, 9},
    {224, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[214], 1, 1926, 256, 9},
    {225, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[215], 1, 1935, 256, 9},
    {226, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[216], 1, 1944, 256, 9},
    {227, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[217], 1, 1953, 256, 9},
    {228, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[218], 1, 1962, 256, 9},
    {229, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[219], 1, 1971, 256, 9},
    {230, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[220], 1, 1980, 256, 9},
    {231, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[221], 1, 1989, 256, 9},
    {232, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[222], 1, 1998, 256, 9},
    {233, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[223], 1, 2007, 256, 9},
    {234, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[224], 1, 2016, 256, 9},
    {235, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[225], 1, 2025, 256, 9},
    {236, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[226], 1, 2034, 256, 9},
    {237, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[227], 1, 2043, 256, 9},
    {238, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[228], 1, 2052, 256, 9},
    {239, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[229], 1, 2061, 256, 9},
    {240, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[230], 1, 2070, 256, 9},
    {241, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[231], 1, 2079, 256, 9},
    {242, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[232], 1, 2088, 256, 9},
    {243, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[233], 1, 2097, 256, 9},
    {244, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[234], 1, 2106, 256, 9},
    {245, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[235], 1, 2115, 256, 9},
    {246, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[236], 1, 2124, 256, 9},
    {247, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[237], 1, 2133, 256, 9},
    {248, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[238], 1, 2142, 256, 9},
    {249, C_TEXT_FONT, -1, (unsigned short)&cd2_titles[239], 1, 2151, 256, 9}};
_transfer Ctrl_Group cg_list = {241, 0, &cc_area};
_transfer Ctrl_Collection cd2_list = {&cg_list, 256, 1024, 0, 0, CSCROLL_BOTH};

// form2 control data
_data char title[100];
_transfer Ctrl_Text_Font cd2_title = {title, (COLOR_BLACK << 4) | COLOR_LBLUE, ALIGN_LEFT | TEXT_16COLOR, font_big};

// form2 calculation rules
_transfer Calc_Rule cr2_area1 = {0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1};
_transfer Calc_Rule cr2_line1 = {88, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1};
_transfer Calc_Rule cr2_image = {0, 0, 1, 0, 0, 1, 88, 0, 1, 48, 0, 1};
_transfer Calc_Rule cr2_button1 = {2, 0, 1, 50, 0, 1, 14, 0, 1, 12, 0, 1};
_transfer Calc_Rule cr2_button2 = {18, 0, 1, 50, 0, 1, 52, 0, 1, 12, 0, 1};
_transfer Calc_Rule cr2_button3 = {72, 0, 1, 50, 0, 1, 14, 0, 1, 12, 0, 1};
_transfer Calc_Rule cr2_line2 = {0, 0, 1, 64, 0, 1, 88, 0, 1, 1, 0, 1};
_transfer Calc_Rule cr2_list = {0, 0, 1, 65, 0, 1, 88, 0, 1, -65, 1, 1};
_transfer Calc_Rule cr2_area2 = {89, 0, 1, 2, 0, 1, -89, 1, 1, 27, 0, 1};
_transfer Calc_Rule cr2_tabs = {89, 0, 1, 2, 0, 1, -89, 1, 1, 11, 0, 1};
_transfer Calc_Rule cr2_title = {92, 0, 1, 13, 0, 1, -92, 1, 1, 15, 0, 1};
_transfer Calc_Rule cr2_textbox1 = {89, 0, 1, 29, 0, 1, -89, 1, 1, -29, 1, 1};

// form2 controls
_transfer Ctrl c2_area1 = {0, C_AREA, -1, COLOR_ORANGE, 0, 0, 88, 10000};
_transfer Ctrl c2_line1 = {0, C_AREA, -1, COLOR_BLACK, 88, 0, 1, 10000};
_transfer Ctrl c2_image = {0, C_IMAGE, -1, 0, 0, 0, 88, 48};
_transfer Ctrl c2_button1 = {3, C_BUTTON, -1, (unsigned short)"<", 2, 50, 14, 12};
_transfer Ctrl c2_button2 = {4, C_BUTTON, -1, (unsigned short)"Close Tab", 18, 50, 52, 12};
_transfer Ctrl c2_button3 = {5, C_BUTTON, -1, (unsigned short)">", 72, 50, 14, 12};
_transfer Ctrl c2_line2 = {0, C_AREA, -1, COLOR_BLACK, 0, 64, 88, 1};
_transfer Ctrl c2_list = {6, C_COLLECTION, -1, (unsigned short)&cd2_list, 0, 65, 88, 89};
_transfer Ctrl c2_area2 = {0, C_AREA, -1, COLOR_LBLUE | AREA_16COLOR, 89, 0, 10000, 27};
_transfer Ctrl c2_tabs = {7, C_TABS, -1, (unsigned short)&cd2_tabs, 89, 2, 10000, 11};
_transfer Ctrl c2_title = {0, C_TEXT_FONT, -1, (unsigned short)&cd2_title, 92, 13, 10000, 15};
_transfer Ctrl c2_textbox1 = {8, C_TEXTBOX, -1, 0, 89, 29, 127, 125};
_transfer Ctrl_Group ctrls2 = {12, 0, &c2_area1, &cr2_area1};

// form2 window data record
_transfer Window form2 = {
    WIN_MAXIMIZED,
    WIN_CLOSE | WIN_TITLE | WIN_RESIZABLE | WIN_ICON | WIN_ADJUSTX | WIN_ADJUSTY,
    0,          // PID
    10, 10,     // x, y
    216, 154,   // w, h
    0, 0,       // xscroll, yscroll
    216, 154,   // wfull, hfull
    150, 100,   // wmin, hmin
    10000, 10000, // wmax, hmax
    icon,       // icon
    "Encontra '26", // title text
    0,          // no status text
    0,          // no menu
    &ctrls2};    // controls

// form3 control data
_data char cd3_input1_buf[33];
_transfer Ctrl_Text cd3_text1 = {"Find:", (COLOR_BLACK << 2) | COLOR_ORANGE, ALIGN_LEFT};
_transfer Ctrl_Input cd3_input1 = {cd3_input1_buf, 0, 0, 0, 0, 32};

// form3 controls
_transfer Ctrl c3_area = {0, C_AREA, -1, COLOR_ORANGE, 0, 0, 10000, 10000};
_transfer Ctrl c3_text1 = {1, C_TEXT, -1, (unsigned short)&cd3_text1, 4, 4, 24, 8};
_transfer Ctrl c3_input1 = {2, C_INPUT, -1, (unsigned short)&cd3_input1, 25, 2, 74, 12};
_transfer Ctrl c3_button1 = {3, C_BUTTON, -1, (unsigned short)"Find", 102, 2, 36, 12};
_transfer Ctrl c3_button2 = {4, C_BUTTON, -1, (unsigned short)"Close", 102, 16, 36, 12};
_transfer Ctrl c3_progress1 = {5, C_PROGRESS, -1, (0 << 8) | PROGRESS_COLOR, 3, 18, 96, 8};
_transfer Ctrl_Group ctrls3 = {6, 0, &c3_area};

// form3 window data record
_transfer Window form3 = {
    WIN_NORMAL,
    WIN_CLOSE | WIN_TITLE | WIN_STATUS | WIN_NOTTASKBAR | WIN_MODAL,
    0,          // PID
    10, 10,     // x, y
    140, 30,    // w, h
    0, 0,       // xscroll, yscroll
    140, 30,    // wfull, hfull
    140, 30,    // wmin, hmin
    140, 30,    // wmax, hmax
    0,          // no icon
    "Search",   // title text
    "Ready",    // status text
    0,          // no menu
    &ctrls3};    // controls

char winID, artID, findID;

int list_sel = -1;
int old_list_sel;

void oom(void) {
    MsgBox("Out of memory", 0, 0, COLOR_BLACK, BUTTON_OK, 0, 0, 0);
}

void ferr(void) {
    MsgBox("File access error:", pathbuf, 0, COLOR_BLACK, BUTTON_OK, 0, 0, 0);
}

void release_tab(unsigned char tab) {
    Mem_Release(tab_bank[tab], tab_text[tab], TAB_TEXTLEN + sizeof(font_text));
    Mem_Release(tab_bank[tab], tab_ctrl[tab], sizeof(Ctrl_TextBox) + 768*2);
    Mem_Release(tab_linkbank[tab], tab_link[tab], TAB_LINKLEN);
}

void quit(void) {
    while (tab_i)
        release_tab(--tab_i);
    if (gfx_splash_bank)
        Mem_Release(gfx_splash_bank, gfx_splash_addr, gfx_splash_len);
    if (gfx_page1_bank)
        Mem_Release(gfx_page1_bank, gfx_page1_addr, max_page_size);
    if (gfx_page2_bank)
        Mem_Release(gfx_page2_bank, gfx_page2_addr, max_page_size);
    if (gfx_hover_bank)
        Mem_Release(gfx_hover_bank, gfx_hover_addr, max_hover_size);
    if (gfx_header_bank)
        Mem_Release(gfx_header_bank, gfx_header_addr, max_header_size);
    exit(0);
}

// set up a C_IMAGE control as a C_IMAGE_EXT control
void link_gfx_header(Ctrl* ctrl) {
    unsigned char bank = ctrl->bank;
    char* buffer = (char*)(ctrl->param);
    ctrl->type = C_IMAGE_EXT;
    if (Bank_ReadByte(bank, buffer) == (Bank_ReadByte(bank, buffer + 1) >> 1)) {
        Bank_WriteWord(bank, buffer + 3, (unsigned short)(buffer + 10));
        Bank_WriteWord(bank, buffer + 5, (unsigned short)(buffer + 9));
    }
}

void set_header(unsigned char id) {
    unsigned char fd;
    strcpy(textbuf, "CAT0.GFX");
    textbuf[3] = screenmode + '0';
    Dir_PathAdd(0, textbuf, pathbuf);
    fd = File_Open(_symbank, pathbuf);
    if (fd <= 7) {
        File_Seek(fd, id*max_header_size, SEEK_CUR);
        File_Read(fd, gfx_header_bank, gfx_header_addr, max_header_size);
        File_Close(fd);
        if (screenmode)
            link_gfx_header(&c2_image);
    } else {
        ferr();
    }
}

void load_page(unsigned char page) {
    unsigned char fd;
    if (page == 0) {
        form1.controls = &ctrls;
        Win_Redraw(winID, -1, 0);

    } else {
        strcpy(textbuf, "PBAC00A.GFX");
        textbuf[4] = page + '0';
        textbuf[5] = screenmode + '0';
        Dir_PathAdd(0, textbuf, pathbuf);
        fd = File_Open(_symbank, pathbuf);
        if (fd <= 7) {
            File_Read(fd, gfx_page1_bank, gfx_page1_addr, max_page_size);
            File_Close(fd);
            textbuf[6] = 'B';
            Dir_PathAdd(0, textbuf, pathbuf);
            fd = File_Open(_symbank, pathbuf);
            if (fd <= 7) {
                File_Read(fd, gfx_page2_bank, gfx_page2_addr, max_page_size);
                File_Close(fd);
                strcpy(textbuf, "PHOV00.GFX");
                textbuf[4] = page + '0';
                textbuf[5] = screenmode + '0';
                Dir_PathAdd(0, textbuf, pathbuf);
                fd = File_Open(_symbank, pathbuf);
                if (fd <= 7) {
                    File_Read(fd, gfx_hover_bank, gfx_hover_addr, max_hover_size);
                    File_Close(fd);
                } else {
                    ferr();
                    return;
                }
            } else {
                ferr();
                return;
            }
        } else {
            ferr();
            return;
        }
        c1_back1.bank = gfx_page1_bank;
        c1_back1.param = (unsigned short)gfx_page1_addr;
        c1_back2.bank = gfx_page2_bank;
        c1_back2.param = (unsigned short)gfx_page2_addr;
        if (screenmode) {
            link_gfx_header(&c1_back1);
            link_gfx_header(&c1_back2);
        }
        c1_hover.bank = gfx_hover_bank;
        form1.controls = &ctrls1;
        ctrls1.controls = 2;
        Win_Redraw(winID, -1, 0);
    }
    on_page = page;
}

void set_tab_title(unsigned char i, char* label) {
    unsigned char t;
    for (t = 0; t < 16; ++t)
        tab_titles[i][t] = latinize[label[t]];
    tab_titles[i][15] = 0;
    if (strlen(&tab_titles[i][0]) >= 15)
        strcpy(&tab_titles[i][12], "...");
    tabs[i].width = -1;
}

// Allocates memory for a new tab, with the initial label _label_.
// Returns 1 on OOM, 0 on success.
Ctrl_TextBox tab_proto = {
    0,               // text address
    0, 0, 0, 0,      // unused1, cursor, selection, len
    TAB_TEXTLEN, INPUT_READONLY | INPUT_ALTFONT, 0, 0,   // maxlen, flags, textcolor, unused2
    font_text, 0, 0, -1,     // font, unused3, lines, wrapwidth
    768, -8, 0,     // maxlines, xvisible, yvisible
    0,               // self
    200, 100, 0, 0,  // xtotal, ytotal, xoffset, yoffset
    WRAP_WINDOW, 20}; // wrapping, tabwidth
unsigned char new_tab(char* label) {
    char i;
    char bank1, bank2;
    char* text;
    char* ctrl;
    if (tab_i >= MAX_TABS)
        return 1;
    for (i = 1; i < 16; ++i) {
        if (Mem_Reserve(i, 1, TAB_TEXTLEN + sizeof(font_text), &bank1, &text))
            continue;
        if (Mem_Reserve(i, 2, sizeof(Ctrl_TextBox) + 768*2, &bank2, &ctrl)) {
            Mem_Release(bank1, text, TAB_TEXTLEN + sizeof(font_text));
            continue;
        }
        if (Mem_Reserve(0, 0, TAB_LINKLEN, &tab_linkbank[tab_i], &tab_link[tab_i])) {
            Mem_Release(bank1, text, TAB_TEXTLEN + sizeof(font_text));
            Mem_Release(bank2, ctrl, sizeof(Ctrl_TextBox) + 768*2);
            continue;
        }
        tab_bank[tab_i] = bank1;
        tab_text[tab_i] = text;
        tab_ctrl[tab_i] = ctrl;
        tab_hist_on[tab_i] = 0;
        tab_hist_max[tab_i] = 0;
        tab_proto.text = text;
        tab_proto.self = ctrl;
        tab_proto.font = text + TAB_TEXTLEN;
        Bank_WriteByte(bank1, text, 0);
        Bank_Copy(bank1, ctrl, _symbank, (char*)&tab_proto, sizeof(Ctrl_TextBox));
        Bank_Copy(bank1, tab_proto.font, _symbank, font_text, sizeof(font_text));
        Bank_WriteByte(tab_linkbank[tab_i], tab_link[tab_i], 0); // zero out title
        Bank_WriteWord(tab_linkbank[tab_i], tab_link[tab_i] + 100, 0); // zero out links
        set_tab_title(tab_i, label);
        ++tab_i;
        cd2_tabs.tabs = tab_i + (tab_i < MAX_TABS ? 1 : 0);
        if (artID)
            Win_Redraw(artID, -2, 8);
        return 0;
    }
    return 1;
}

void select_tab(void) {
    if (cd2_tabs.tabs < MAX_TABS && cd2_tabs.selected == cd2_tabs.tabs - 1) {
        if (new_tab("(new tab)")) {
            oom();
            cd2_tabs.selected = 0;
        }
    }
    c2_textbox1.bank = tab_bank[cd2_tabs.selected];
    c2_textbox1.param = (unsigned short)tab_ctrl[cd2_tabs.selected];
    Bank_Copy(_symbank, title, tab_linkbank[cd2_tabs.selected], tab_link[cd2_tabs.selected], sizeof(title));
    Win_Redraw(artID, -4, 8);
}

void reset_catlist(void) {
    list_sel = -1;
    old_list_sel = -1;
    cd2_titles[0].text = "(back)";
    cd2_titles[0].color = (COLOR_BLACK << 4) | COLOR_WHITE;
    cg_list.controls = 2;
    cd2_list.hfull = 9;
    cd2_list.yscroll = 0;
}

void render_category(unsigned long addr) {
    unsigned char i, f;
    char* ptr = articles + 5;

    // always show (back)
    reset_catlist();

    // if a real category, render it
    if (addr != -1) {
        Dir_PathAdd(0, "CONTENTS.DB", pathbuf);
        f = File_Open(_symbank, pathbuf);
        if (f < 8) {
            File_Seek(f, addr, SEEK_SET);
            File_Read(f, _symbank, articles, sizeof(articles));
            File_Close(f);
            i = 1;
            while (i <= articles[0]) {
                cd2_titles[i].color = (COLOR_BLACK << 4) | COLOR_WHITE;
                cd2_titles[i++].text = ptr;
                while (*ptr++);
                ptr += 4;
            }
            cg_list.controls = i + 1;
            cd2_list.hfull = i * 9;
        } else {
            return;
        }
    }

    // show changes
    if (artID)
        Win_Redraw(artID, 7, 0);
}

void render_article(unsigned long addr) {
    unsigned long raw_addr = addr;
    unsigned char i, f;
    strcpy(textbuf, "ARTICLEA.DB");
    for (i = 0; i < 16; ++i) {
        if (chunks_start[i] <= addr && (i == 15 || chunks_start[i+1] > addr)) {
            addr -= chunks_start[i];
            textbuf[7] = 'A' + i;
            break;
        }
    }
    Dir_PathAdd(0, textbuf, pathbuf);
    f = File_Open(_symbank, pathbuf);
    if (f < 8) {
        File_Seek(f, addr, SEEK_SET);
        File_Read(f, _symbank, (char*)&tab_len[cd2_tabs.selected], sizeof(unsigned short));
        File_ReadComp(f, tab_bank[cd2_tabs.selected], tab_text[cd2_tabs.selected], tab_len[cd2_tabs.selected]);
        File_Read(f, tab_linkbank[cd2_tabs.selected], tab_link[cd2_tabs.selected], TAB_LINKLEN);
        File_Close(f);
        tab_proto.text = tab_text[cd2_tabs.selected];
        tab_proto.self = tab_ctrl[cd2_tabs.selected];
        tab_proto.font = tab_text[cd2_tabs.selected] + TAB_TEXTLEN;
        Bank_Copy(tab_bank[cd2_tabs.selected], tab_ctrl[cd2_tabs.selected], _symbank, (char*)&tab_proto, sizeof(Ctrl_TextBox));
        Bank_Copy(_symbank, title, tab_linkbank[cd2_tabs.selected], tab_link[cd2_tabs.selected], 100);
        set_tab_title(cd2_tabs.selected, title);
        tab_hist[cd2_tabs.selected][tab_hist_on[cd2_tabs.selected]] = raw_addr;
        if (artID)
            Win_Redraw(artID, -4, 8);
    }
}

void history_forward(void) {
    unsigned char i;
    if (tab_hist_on[cd2_tabs.selected] == MAX_HISTORY-1) {
        for (i = 0; i < MAX_HISTORY-1; ++i)
            tab_hist[cd2_tabs.selected][i] = tab_hist[cd2_tabs.selected][i+1];
    } else if (*title) {
        tab_hist_max[cd2_tabs.selected] = ++tab_hist_on[cd2_tabs.selected];
    }
}

void find(void) {
    unsigned short this_len;
    unsigned long file_len, read_len;
    unsigned char fd, i, list_len, skip_count, save_current;
    unsigned long first_addr;
    char* ptr;
    char* ptr_end;
    char* ptr_find;
    char* ptr_title;
    char* ptr_title_start;
    char* last_article = 0;
    if (strlen(cd3_input1_buf)) {
        form3.status = "Searching article titles...";
        Win_Redraw_Status(findID);
        Dir_PathAdd(0, "CONTENTS.DB", pathbuf);
        fd = File_Open(_symbank, pathbuf);
        if (fd <= 7) {
            reset_catlist();
            file_len = File_Seek(fd, 0, SEEK_END);
            File_Seek(fd, 0, SEEK_SET);
            read_len = 0;
            list_len = 0;
            skip_count = 4;
            save_current = 0;
            i = 1;
            ptr_find = cd3_input1_buf;
            ptr_title = articles;
            ptr_title_start = articles;
            while (this_len = File_Read(fd, _symbank, textbuf, sizeof(textbuf))) {
                // check for cancel
                _symmsg[0] = 0;
                Msg_Receive(_sympid, -1, _symmsg);
                if (_symmsg[0] == MSR_DSK_WCLICK && _symmsg[1] == findID) {
                    if (_symmsg[2] == DSK_ACT_CLOSE || (_symmsg[2] == DSK_ACT_CONTENT && _symmsg[8] == 4)) {
                        // requested to close
                        File_Close(fd);
                        form1.modal = 0;
                        Win_Close(findID);
                        findID = 0;
                        return;
                    }
                }

                // progress bar
                ptr = textbuf;
                ptr_end = textbuf + this_len;
                read_len += this_len;
                c3_progress1.param = (((read_len << 8) / file_len) << 8) | PROGRESS_COLOR;
                Win_Redraw(findID, 5, 0);

                // search for text (state machine since we read CONTENTS.DB in arbitrary chunks)
                while (ptr < ptr_end) {
                    if (list_len) {
                        *ptr_title++ = *ptr;
                        if (ptr_title >= articles + sizeof(articles) - 2)
                            goto done_find; // out of title space
                        if (skip_count) {
                            --skip_count;
                        } else {
                            if (*ptr) {
                                if (toupper(latinize[*ptr]) == toupper(*ptr_find)) {
                                    ++ptr_find;
                                    if (!(*ptr_find)) {
                                        // found an article, add to list
                                        if (i >= 240)
                                            goto done_find; // out of list entries
                                        if (last_article != ptr_title_start) {
                                            cd2_titles[i].color = (COLOR_BLACK << 4) | COLOR_WHITE;
                                            cd2_titles[i++].text = ptr_title_start + 4;
                                            cg_list.controls = i + 1;
                                            cd2_list.hfull = i * 9;
                                            ptr_find = cd3_input1_buf;
                                            last_article = ptr_title_start;
                                            save_current = 1;
                                        }
                                    }
                                } else {
                                    ptr_find = cd3_input1_buf;
                                }
                            } else {
                                if (save_current) {
                                    save_current = 0;
                                    ptr_title_start = ptr_title;
                                } else {
                                    ptr_title = ptr_title_start;
                                }
                                skip_count = 4;
                                --list_len;
                            }
                        }
                    } else {
                        list_len = *ptr;
                    }
                    ++ptr;
                }
            }
            done_find:
            File_Close(fd);
            if (i > 1) {
                // success, load article list
                set_header(12);
                first_addr = *(unsigned long*)(cd2_titles[1].text - 4);
                if (first_addr & 0x80000000L) {
                    history_forward();
                    render_article(first_addr & 0x7FFFFFFFL);
                }
                form1.modal = 0;
                Win_Close(findID);
                findID = 0;
                artID = Win_Open(_symbank, &form2);
                Win_Close(winID);
                winID = 0;
            } else {
                // failure, stop here
                form3.status = "Nothing found";
                c3_progress1.param = (0 << 8) | (COLOR_ORANGE << 6) | (COLOR_RED << 4) | (COLOR_BLACK << 2) | COLOR_BLACK;
                Win_Redraw(findID, 5, 0);
                Win_Redraw_Status(findID);
            }
        } else {
            ferr();
        }
    }
}

int main(int argc, char *argv[]) {
    unsigned long count;
    unsigned long addr;
    unsigned char i, fd, motion;
    unsigned short x, y;
    unsigned char win, cursor = 0;
    Link* link_ptr;

    // unset high bit of font header if SymbOS <4.1
    if (_symversion < 41) {
        font_text[0] &= 0x7F;
        font_big[0] &= 0x7F;
    }

    // allocate and load splash graphics
    screenmode = (Screen_Colors() == 16 ? 1 : 0);
    if (screenmode) {
        c_area.param = COLOR_DBLUE | AREA_16COLOR;
        c_erase.param = COLOR_DBLUE | AREA_16COLOR;
        max_page_size = 9082;
        max_hover_size = 7090;
        max_header_size = 2122;
        gfx_splash_len = 13782;
        Dir_PathAdd(0, "SPLASH1.GFX", pathbuf);
    } else {
        gfx_splash_len = 6879;
        Dir_PathAdd(0, "SPLASH0.GFX", pathbuf);
    }
    if (Mem_Reserve(0, 1, gfx_splash_len, &gfx_splash_bank, &gfx_splash_addr)) {
        oom();
        quit();
    }
    fd = File_Open(_symbank, pathbuf);
    if (fd <= 7) {
        File_Read(fd, gfx_splash_bank, gfx_splash_addr, gfx_splash_len);
        File_Close(fd);
    } else {
        ferr();
        quit();
    }
    c_splash0.bank = gfx_splash_bank;
    c_splash1.bank = gfx_splash_bank;
    c_splash2.bank = gfx_splash_bank;
    c_splash3.bank = gfx_splash_bank;
    c_splash4.bank = gfx_splash_bank;
    c_splash5.bank = gfx_splash_bank;
    if (screenmode) {
        c_splash0.param = (unsigned short)gfx_splash_addr;
        c_splash1.param = (unsigned short)gfx_splash_addr + 3450;
        c_splash2.param = (unsigned short)gfx_splash_addr + 6792;
        c_splash3.param = (unsigned short)gfx_splash_addr + 10134;
        c_splash4.param = (unsigned short)gfx_splash_addr + 13476;
        c_splash5.param = (unsigned short)gfx_splash_addr + 13618;
        link_gfx_header(&c_splash0);
        link_gfx_header(&c_splash1);
        link_gfx_header(&c_splash2);
        link_gfx_header(&c_splash3);
        link_gfx_header(&c_splash4);
        link_gfx_header(&c_splash5);
    } else {
        c_splash0.param = (unsigned short)gfx_splash_addr;
        c_splash1.param = (unsigned short)gfx_splash_addr + 1723;
        c_splash2.param = (unsigned short)gfx_splash_addr + 3392;
        c_splash3.param = (unsigned short)gfx_splash_addr + 5061;
        c_splash4.param = (unsigned short)gfx_splash_addr + 6730;
        c_splash5.param = (unsigned short)gfx_splash_addr + 6799;
    }

    // allocate memory for browser page
    if (Mem_Reserve(0, 1, max_page_size, &gfx_page1_bank, &gfx_page1_addr)) {
        oom();
        quit();
    }
    if (Mem_Reserve(0, 1, max_page_size, &gfx_page2_bank, &gfx_page2_addr)) {
        oom();
        quit();
    }
    if (Mem_Reserve(0, 1, max_hover_size, &gfx_hover_bank, &gfx_hover_addr)) {
        oom();
        quit();
    }
    if (Mem_Reserve(0, 1, max_header_size, &gfx_header_bank, &gfx_header_addr)) {
        oom();
        quit();
    }
    c2_image.bank = gfx_header_bank;
    c2_image.param = (unsigned short)gfx_header_addr;

    // set up first tab
    if (new_tab("(new tab)")) {
        oom();
        quit();
    }
    c2_textbox1.bank = tab_bank[0];
    c2_textbox1.param = (unsigned short)tab_ctrl[0];

    // open main window
    form1.x = Screen_Width() / 2 - 114;
    form1.y = Screen_Height() / 2 - 93;
    if (form1.y < 8)
        form1.y = 8;
    form2.x = form1.x;
    form2.y = form1.y;
    form3.x = Screen_Width() / 2 - 70;
    form3.y = Screen_Height() / 2 - 32;
	winID = Win_Open(_symbank, &form1);

    // opening animation
    count = Sys_Counter();
    while (Sys_Counter() - count < 100) Idle();
    while (1) {
        count = Sys_Counter();
        while (Sys_Counter() - count < 5);
        if (c_splash0.y < 90)
            ++motion;
        else
            --motion;
        c_splash0.y += motion;
        c_erase.y += motion;
        if (c_splash0.y > 114)
            break;
        Win_Redraw(winID, -2, 1);
        Idle();
    }
    c_splash0.y = 114;
    ctrls.controls = 8;
    Win_Redraw(winID, -1, 0);

    // main event loop
	while (1) {
		_symmsg[0] = 0;
		Msg_Receive(_sympid, -1, _symmsg);
		if (_symmsg[0] == MSR_DSK_WCLICK) {
            // window event
            win = _symmsg[1];
			switch (_symmsg[2]) {
            case DSK_ACT_CONTENT:
                if (win == winID) {
                    if (on_page) {
                        if (_symmsg[8] < 3 && cursor) {
                            if (hover_dest[on_page-1][cursor-1]) {
                                addr_stack[0] = hover_dest[on_page-1][cursor-1];
                                set_header((on_page-1)*4 + (cursor-1));
                                render_category(addr_stack[0]);
                                artID = Win_Open(_symbank, &form2);
                                Win_Close(winID);
                                winID = 0;
                            } else {
                                load_page(0);
                            }
                        }
                    } else {
                        if (_symmsg[8] == 8) {
                            // clicked cursor image
                            if (cursor == 4) {
                                // about
                                history_forward();
                                set_header(3);
                                render_category(-1);
                                render_article(about_addr);
                                artID = Win_Open(_symbank, &form2);
                                Win_Close(winID);
                                winID = 0;
                            } else if (cursor == 5) {
                                // search
                                form3.status = "Ready";
                                c3_progress1.param = (0 << 8) | PROGRESS_COLOR;
                                findID = Win_Open(_symbank, &form3);
                                form1.modal = findID + 1;
                            } else {
                                // category
                                load_page(cursor);
                            }
                        }
                    }

                } else if (win == artID) {
                    if (_symmsg[8] >= 10) {
                        // select article title
                        switch (_symmsg[3]) {
                        case DSK_SUB_MLCLICK:
                            // left click
                            old_list_sel = list_sel;
                            list_sel = _symmsg[8] - 10;
                            itoa(list_sel, textbuf, 10);
                            cd2_titles[list_sel].color = (COLOR_WHITE << 4) | COLOR_BLACK;
                            if (old_list_sel != -1) {
                                cd2_titles[old_list_sel].color = (COLOR_BLACK << 4) | COLOR_WHITE;
                                Win_Redraw_Sub(artID, 7, old_list_sel + 1, 0);
                            }
                            Win_Redraw_Sub(artID, 7, list_sel + 1, 0);
                            break;

                        case DSK_SUB_MDCLICK:
                            // double click
                            list_sel = _symmsg[8] - 10;
                            if (list_sel == 0) {
                                if (addr_i == 0) {
                                    winID = Win_Open(_symbank, &form1);
                                    Win_Close(artID);
                                    artID = 0;
                                } else {
                                    render_category(addr_stack[--addr_i]);
                                }
                            } else {
                                addr = *(unsigned long*)(cd2_titles[list_sel].text - 4);
                                if (addr & 0x80000000L) {
                                    history_forward();
                                    render_article(addr & 0x7FFFFFFFL);
                                } else {
                                    addr_stack[++addr_i] = addr;
                                    render_category(addr_stack[addr_i]);
                                }
                            }
                            break;
                        }

                    } else if (_symmsg[8] == 3) {
                        // clicked "back"
                        if (tab_hist_on[cd2_tabs.selected])
                            render_article(tab_hist[cd2_tabs.selected][--tab_hist_on[cd2_tabs.selected]]);

                    } else if (_symmsg[8] == 4) {
                        // clicked "close tab"
                        if (tab_i) {
                            release_tab(cd2_tabs.selected);
                            for (i = cd2_tabs.selected; i < MAX_TABS-1; ++i) {
                                tab_bank[i] = tab_bank[i+1];
                                tab_linkbank[i] = tab_linkbank[i+1];
                                tab_text[i] = tab_text[i+1];
                                tab_ctrl[i] = tab_ctrl[i+1];
                                tab_link[i] = tab_link[i+1];
                                tab_len[i] = tab_len[i+1];
                                memcpy(&tab_hist[i][0], &tab_hist[i+1][0], MAX_HISTORY*sizeof(unsigned long));
                                tab_hist_on[i] = tab_hist_on[i+1];
                                tab_hist_max[i] = tab_hist_max[i+1];
                                memcpy(&tab_titles[i][0], &tab_titles[i+1][0], 16);
                                tabs[i].width = -1;
                            }
                            if (cd2_tabs.tabs == MAX_TABS && tab_titles[MAX_TABS-1][0] != '+') {
                                tab_titles[MAX_TABS-1][0] = '+';
                                tab_titles[MAX_TABS-1][1] = 0;
                            } else {
                                --cd2_tabs.tabs;
                            }
                            --tab_i;
                            if (cd2_tabs.selected)
                                --cd2_tabs.selected;
                            select_tab();
                        }

                    } else if (_symmsg[8] == 5) {
                        // clicked "forward"
                        if (tab_hist_on[cd2_tabs.selected] < tab_hist_max[cd2_tabs.selected])
                            render_article(tab_hist[cd2_tabs.selected][++tab_hist_on[cd2_tabs.selected]]);

                    } else if (_symmsg[8] == 7) {
                        // clicked tabs
                        select_tab();

                    } else if (_symmsg[8] == 8) {
                        // clicked textbox
                        Bank_Copy(_symbank, textbuf, tab_linkbank[cd2_tabs.selected], tab_link[cd2_tabs.selected], TAB_LINKLEN);
                        x = Bank_ReadWord(tab_bank[cd2_tabs.selected], tab_ctrl[cd2_tabs.selected] + 4); // get cursor position
                        Bank_WriteWord(tab_bank[cd2_tabs.selected], tab_ctrl[cd2_tabs.selected] + 4, 0); // reset cursor position (so next click behaves correctly)
                        if (x > 0) {
                            link_ptr = (Link*)&textbuf[102];
                            for (i = 0; i < *(unsigned short*)&textbuf[100]; ++i) {
                                if (x >= link_ptr->start && x <= link_ptr->end) {
                                    if ((Key_Status() & CTRL_DOWN) && cd2_tabs.tabs < MAX_TABS) {
                                        cd2_tabs.selected = cd2_tabs.tabs - 1;
                                        select_tab();
                                    }
                                    history_forward();
                                    render_article(link_ptr->addr & 0x7FFFFFFFL);
                                    break;
                                }
                                ++link_ptr;
                            }
                        }
                    }

                } else if (win == findID) {
                    if (_symmsg[8] == 3) {
                        // clicked find
                        find();

                    } else if (_symmsg[8] == 4) {
                        // clicked close
                        form1.modal = 0;
                        Win_Close(findID);
                        findID = 0;
                    }

                }
                break;

            case DSK_ACT_CLOSE: // Alt+F4 or click close
                if (win == winID) {
                    quit();
                } else if (win == artID) {
                    winID = Win_Open(_symbank, &form1);
                    Win_Close(artID);
                    artID = 0;
                } else if (win == findID) {
                    form1.modal = 0;
                    Win_Close(findID);
                    findID = 0;
                }
			}

		} else {
		    // hover behavior
		    x = Mouse_X() - form1.x;
		    y = Mouse_Y() - form1.y - 8;
		    if (!on_page) {
                // page 0 hover
                if (y >= c_splash1.y && y <= c_splash1.y + c_splash1.h) {
                    if (x >= c_splash1.x && x <= c_splash1.x + c_splash1.w) {
                        if (cursor != 1) {
                            cursor = 1;
                            c_cursor.x = c_splash1.x + 1;
                            c_cursor.y = c_splash1.y + 1;
                            c_cursor.w = 66;
                            c_cursor.h = 84;
                            ctrls.controls = 9;
                            Win_Redraw(winID, -6, 3);
                        }
                    } else if (x >= c_splash2.x && x <= c_splash2.x + c_splash2.w) {
                        if (cursor != 2) {
                            cursor = 2;
                            c_cursor.x = c_splash2.x + 1;
                            c_cursor.y = c_splash2.y + 13;
                            c_cursor.w = 66;
                            c_cursor.h = 84;
                            ctrls.controls = 9;
                            Win_Redraw(winID, -6, 3);
                        }
                    } else if (x >= c_splash3.x && x <= c_splash3.x + c_splash3.w) {
                        if (cursor != 3) {
                            cursor = 3;
                            c_cursor.x = c_splash3.x + 1;
                            c_cursor.y = c_splash3.y + 1;
                            c_cursor.w = 66;
                            c_cursor.h = 84;
                            ctrls.controls = 9;
                            Win_Redraw(winID, -6, 3);
                        }
                    } else if (cursor) {
                        cursor = 0;
                        ctrls.controls = 8;
                        Win_Redraw(winID, -5, 3);
                    }
                } else if (x >= c_splash4.x && x <= c_splash4.x + c_splash4.w && y >= c_splash4.y && y <= c_splash4.y + c_splash4.h) {
                    if (cursor != 4) {
                        cursor = 4;
                        c_cursor.x = c_splash4.x;
                        c_cursor.y = c_splash4.y;
                        c_cursor.w = 24;
                        c_cursor.h = 11;
                        ctrls.controls = 9;
                        Win_Redraw(winID, -6, 3);
                    }
                } else if (x >= c_splash5.x && x <= c_splash5.x + c_splash5.w && y >= c_splash5.y && y <= c_splash5.y + c_splash5.h) {
                    if (cursor != 5) {
                        cursor = 5;
                        c_cursor.x = c_splash5.x;
                        c_cursor.y = c_splash5.y;
                        c_cursor.w = 28;
                        c_cursor.h = 11;
                        ctrls.controls = 9;
                        Win_Redraw(winID, -6, 3);
                    }
                } else if (cursor) {
                    cursor = 0;
                    ctrls.controls = 8;
                    Win_Redraw(winID, -5, 3);
                }

		    } else {
		        // other page hover
                for (i = 0; ; ++i) {
                    if (i == 5) {
                        if (cursor) {
                            cursor = 0;
                            ctrls1.controls = 2;
                            Win_Redraw_Area(winID, -2, 0, c1_hover.x, c1_hover.y, c1_hover.w, c1_hover.h);
                        }
                        break;
                    }
                    if (x >= hover_coords[on_page-1][i][0] && y >= hover_coords[on_page-1][i][1] &&
                        x <= hover_coords[on_page-1][i][2] && y <= hover_coords[on_page-1][i][3]) {
                        if (cursor != i + 1) {
                            c1_hover.x = hover_xy[on_page-1][i][0];
                            c1_hover.y = hover_xy[on_page-1][i][1];
                            c1_hover.w = hover_xy[on_page-1][i][2];
                            c1_hover.h = hover_xy[on_page-1][i][3];
                            ctrls1.controls = 3;
                            c1_hover.param = (unsigned short)(gfx_hover_addr + hover_offsets[screenmode][on_page-1][i]);
                            if (screenmode)
                                link_gfx_header(&c1_hover);
                            Win_Redraw(winID, cursor ? -1 : 2, 0);
                            cursor = i + 1;
                        }
                        break;
                    }
                }
		    }
		}
		Idle();
	}
}
