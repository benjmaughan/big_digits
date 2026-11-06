/*
 * Big Digits: a high-visibility watchface for the original Pebble Time
 * ====================================================================
 *
 * Target platform : basalt (Pebble Time), 144 x 168 pixels, color
 *
 * WHAT IT DRAWS
 *   - The time as two rows of huge seven-segment digits (HH over MM).
 *   - A battery bar.
 *   - The date (e.g. "Fri Nov 06").
 *
 * USER SETTINGS (edited on the phone with Clay, stored on the watch)
 *   1. High contrast      : pure white on black, no battery color coding.
 *   2. Digit gaps         : LCD-style pointed segments with small gaps,
 *                           instead of solid, merged bars.
 *   3. Segmented battery  : 10 full/hollow blocks instead of a smooth bar.
 *   4. Layout             : where the battery and date go (see below).
 *
 * LAYOUTS
 *   LAYOUT_STANDARD
 *       HH
 *       MM
 *       battery bar (horizontal)
 *       date (horizontal)
 *
 *   LAYOUT_SIDE_BAR
 *       HH | battery bar (vertical, right edge, full height)
 *       MM |
 *       date (horizontal, bottom-left, shifted left to clear the bar)
 *
 *   LAYOUT_SIDE_BAR_VDATE
 *       HH | battery bar | date written vertically, one character per
 *       MM |             | line, upright:  F r i / N o v / 0 7
 *
 * HOW THE CODE IS ORGANISED
 *   1. Constants       (all the numbers you might want to tweak)
 *   2. Settings        (load / save / defaults)
 *   3. Layout          (works out where everything goes for each layout)
 *   4. Drawing         (digits, battery, date)
 *   5. Time + events   (minute tick, battery events, settings from phone)
 *   6. Window + app lifecycle
 */

#include <pebble.h>

/* ======================================================================
 * 1. CONSTANTS
 * ====================================================================== */

/* ---- Layout identifiers (stored in the settings; keep these stable) ---- */
enum {
  LAYOUT_STANDARD        = 0,  /* battery under the time, date at bottom     */
  LAYOUT_SIDE_BAR        = 1,  /* vertical battery bar at right edge         */
  LAYOUT_SIDE_BAR_VDATE  = 2,  /* vertical bar, date stacked vertically      */
  LAYOUT_COUNT                 /* number of layouts; used for range checks   */
};

/* ---- STANDARD layout: digits ---- */
#define STD_DIGIT_W     64   /* width of one digit in pixels                 */
#define STD_DIGIT_H     60   /* height of one digit in pixels                */
#define STD_SEG_T        9   /* segment thickness: bigger = bolder digits    */
#define STD_DIGIT_GAP    8   /* horizontal space between the two digits      */
#define STD_ROW1_Y       2   /* top of the hours row                         */
#define STD_ROW2_Y      66   /* top of the minutes row (2 + 60 + 4 gap)      */

/* ---- Horizontal battery bar (STANDARD layout) ---- */
#define HBAR_Y          130  /* top edge of the bar                          */
#define HBAR_MARGIN       3  /* space left and right of the bar              */

/* ---- Horizontal date (STANDARD and SIDE_BAR layouts) ---- */
#define DATE_Y          138  /* top of the date text box                     */
#define DATE_H           30  /* height of the date text box                  */

/* ---- Vertical battery bar (both side-bar layouts) ---- */
#define VBAR_W           10  /* thickness of the bar                         */
#define VBAR_MARGIN_Y     4  /* gap above and below the bar                  */

/* ---- Shared horizontal spacing for the side-bar layouts ---- */
#define SIDE_MARGIN       2  /* outer margin at the far left and far right   */
#define BAR_GAP          10  /* gap between the digits and the battery bar   */

/* ---- SIDE_BAR layout: digits ---- */
#define SB_DIGIT_H       65  /* digit height (width is worked out to fit)    */
#define SB_SEG_T          9  /* segment thickness                            */
#define SB_DIGIT_GAP      8  /* gap between the two digits of a row          */
#define SB_ROW1_Y         2
#define SB_ROW2_Y        76

/* ---- SIDE_BAR_VDATE layout: digits and date column ---- */
#define SBV_DIGIT_H      76  /* taller digits: the date no longer needs the
                                bottom strip, so they use the whole height   */
#define SBV_SEG_T        10  /* slightly thinner: the digits are narrower    */
#define SBV_DIGIT_GAP     6
#define SBV_ROW_GAP       6  /* vertical space between hours and minutes     */
#define SBV_DATE_COL_W   20  /* width of the vertical date column            */
#define SBV_DATE_GAP      3  /* gap between the bar and the date column      */

/* ---- Vertical date text (SIDE_BAR_VDATE layout) ---- */
#define VDATE_PITCH      17  /* vertical distance from one character to the
                                next                                         */
#define VDATE_GROUP_GAP   9  /* extra blank space between Fri / Nov / 07     */
#define VDATE_NUDGE       3  /* pulls glyphs up: Pebble fonts leave a few
                                pixels of empty space above the letters      */

/* ---- Battery bar styling ---- */
#define BAR_THICK        10  /* horizontal bar height (vertical bar width is
                                VBAR_W)                                      */
#define BAR_THIN          2  /* thickness of the "used" line, normal mode    */
#define BAR_THIN_HC       3  /* thickness of the "used" line, high contrast  */

/* ---- Segmented battery (10 blocks) ---- */
#define BLOCKS           10  /* number of blocks; each is worth 10%          */
#define HBLOCK_W          9  /* block width when the bar is horizontal       */
#define VBLOCK_H         13  /* block height when the bar is vertical        */
#define BLOCK_GAP         3  /* space between neighbouring blocks            */
#define BLOCK_BORDER      2  /* outline thickness of a hollow block          */

/* ---- Persistent storage key ---- */
#define SETTINGS_KEY      2  /* bumped from 1 because the struct changed     */

/* ======================================================================
 * 2. SETTINGS
 * ====================================================================== */

/*
 * All user-adjustable options. The whole struct is saved to the watch's
 * persistent storage in one go, so it survives restarts and battery swaps.
 */
typedef struct {
  bool    high_contrast;      /* pure white/black, no color-coded battery    */
  bool    digit_gaps;         /* LCD-style segments with gaps                */
  bool    segmented_battery;  /* 10 blocks instead of a smooth bar           */
  uint8_t layout;             /* one of the LAYOUT_* values                  */
} Settings;

static Settings s_settings;

/*
 * Load saved settings, falling back to defaults when nothing valid is
 * stored. The size check protects against reading data written by an older
 * version of this watchface that used a different struct.
 */
static void settings_load(void) {
  /* Defaults first, so any field not loaded below has a sane value. */
  s_settings = (Settings){
    .high_contrast     = false,
    .digit_gaps        = true,
    .segmented_battery = true,
    /* .layout            = LAYOUT_STANDARD, */
    .layout            = LAYOUT_SIDE_BAR,
    /* .layout            = LAYOUT_SIDE_BAR_VDATE, */
  };

  if (persist_exists(SETTINGS_KEY) &&
      persist_get_size(SETTINGS_KEY) == (int)sizeof(Settings)) {
    persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
  }

  /* Guard against corrupt data: an out-of-range layout falls back. */
  if (s_settings.layout >= (uint8_t)LAYOUT_COUNT) {
    s_settings.layout = LAYOUT_STANDARD;
  }
}

static void settings_save(void) {
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

/* ======================================================================
 * Runtime state
 * ====================================================================== */

static Window *s_window;   /* the watchface window                           */
static Layer  *s_canvas;   /* full-screen layer; everything is drawn on it   */

static int s_hour, s_min;          /* time to display (hour already 12/24h)  */
static int s_battery = 100;        /* battery charge, 0-100                  */

/* Date pieces. They are kept separately so the vertical layout can draw
 * "Fri", "Nov" and "07" as distinct groups, and the horizontal layouts
 * can use the combined string "Fri Nov 07". */
static char s_dow[8];              /* day of week, e.g. "Fri"                */
static char s_mon[8];              /* month, e.g. "Nov"                      */
static char s_dom[4];              /* day of month, e.g. "07"                */
static char s_date_full[24];       /* "Fri Nov 07"                           */

/*
 * Seven-segment lookup table. Bit n set = segment n is lit for that digit.
 *
 *        --a--
 *       |     |
 *       f     b
 *       |     |
 *        --g--
 *       |     |
 *       e     c
 *       |     |
 *        --d--
 *
 * Bits: 0=a, 1=b, 2=c, 3=d, 4=e, 5=f, 6=g.
 * Example: 0x3F = a,b,c,d,e,f lit = the digit 0.
 */
static const uint8_t SEGMENTS[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

/* ======================================================================
 * 3. LAYOUT
 * ====================================================================== */

/*
 * The geometry for the current layout. layout_compute() fills this in at the
 * start of every redraw so the drawing code never has to know which layout
 * is active; it just follows these numbers.
 */
typedef struct {
  /* Time digits */
  int digit_w, digit_h;   /* size of one digit                              */
  int seg_t;              /* segment thickness                              */
  int digit_gap;          /* gap between the two digits in a row            */
  int x0;                 /* left edge of the first digit in each row       */
  int row1_y, row2_y;     /* top edge of the hours row and the minutes row  */

  /* Battery */
  bool battery_vertical;  /* true = fills bottom-up, false = fills left-right*/
  GRect battery_rect;     /* full extent of the bar                          */

  /* Date */
  bool date_vertical;     /* true = one character per line                   */
  GRect date_rect;        /* box the date is drawn in                        */
} Layout;

/*
 * Work out all positions for the layout chosen in the settings.
 * `b` is the bounds of the full-screen canvas (144 x 168 on Pebble Time).
 */
static void layout_compute(Layout *L, GRect b) {
  const int w = b.size.w;
  const int h = b.size.h;

  switch (s_settings.layout) {

    case LAYOUT_SIDE_BAR: {
      /*
       * Horizontal budget, left to right:
       *   SIDE_MARGIN | digits | BAR_GAP | bar | SIDE_MARGIN
       * The digits get whatever width is left after the bar and margins,
       * so they are narrower than in the standard layout.
       */
      const int avail = w - SIDE_MARGIN - BAR_GAP - VBAR_W - SIDE_MARGIN;

      L->digit_h   = SB_DIGIT_H;
      L->seg_t     = SB_SEG_T;
      L->digit_gap = SB_DIGIT_GAP;
      L->digit_w   = (avail - L->digit_gap) / 2;
      L->x0        = SIDE_MARGIN;
      L->row1_y    = SB_ROW1_Y;
      L->row2_y    = SB_ROW2_Y;

      /* Bar: full height, flush with the right-hand margin. */
      L->battery_vertical = true;
      L->battery_rect = GRect(w - SIDE_MARGIN - VBAR_W, VBAR_MARGIN_Y,
                              VBAR_W, h - 2 * VBAR_MARGIN_Y);

      /* Date: same strip at the bottom, but only as wide as the area to
       * the left of the bar. If the text is still too wide it drops to a
       * smaller font automatically (see draw_date_horizontal). */
      L->date_vertical = false;
      L->date_rect = GRect(SIDE_MARGIN, DATE_Y, avail, DATE_H);
      break;
    }

    case LAYOUT_SIDE_BAR_VDATE: {
      /*
       * Horizontal budget, left to right:
       *   SIDE_MARGIN | digits | BAR_GAP | bar | SBV_DATE_GAP |
       *   date column | SIDE_MARGIN
       */
      const int avail = w - SIDE_MARGIN - BAR_GAP - VBAR_W
                          - SBV_DATE_GAP - SBV_DATE_COL_W - SIDE_MARGIN;

      L->digit_h   = SBV_DIGIT_H;
      L->seg_t     = SBV_SEG_T;
      L->digit_gap = SBV_DIGIT_GAP;
      L->digit_w   = (avail - L->digit_gap) / 2;
      L->x0        = SIDE_MARGIN;

      /* Two tall rows, centred vertically on the screen. */
      const int rows_h = 2 * L->digit_h + SBV_ROW_GAP;
      L->row1_y = (h - rows_h) / 2;
      L->row2_y = L->row1_y + L->digit_h + SBV_ROW_GAP;

      /* Bar sits just right of the digits, left of the date column. */
      const int bar_x = w - SIDE_MARGIN - SBV_DATE_COL_W
                          - SBV_DATE_GAP - VBAR_W;
      L->battery_vertical = true;
      L->battery_rect = GRect(bar_x, VBAR_MARGIN_Y,
                              VBAR_W, h - 2 * VBAR_MARGIN_Y);

      /* Date column: the full height at the far right edge. */
      L->date_vertical = true;
      L->date_rect = GRect(w - SIDE_MARGIN - SBV_DATE_COL_W, 0,
                           SBV_DATE_COL_W, h);
      break;
    }

    case LAYOUT_STANDARD:
    default: {
      /* Digits fill the width, centred. */
      L->digit_w   = STD_DIGIT_W;
      L->digit_h   = STD_DIGIT_H;
      L->seg_t     = STD_SEG_T;
      L->digit_gap = STD_DIGIT_GAP;
      L->x0        = (w - (2 * L->digit_w + L->digit_gap)) / 2;
      L->row1_y    = STD_ROW1_Y;
      L->row2_y    = STD_ROW2_Y;

      /* Battery: a horizontal bar across the screen below the digits. */
      L->battery_vertical = false;
      L->battery_rect = GRect(HBAR_MARGIN, HBAR_Y,
                              w - 2 * HBAR_MARGIN, BAR_THICK);

      /* Date: full-width strip at the bottom. */
      L->date_vertical = false;
      L->date_rect = GRect(0, DATE_Y, w, DATE_H);
      break;
    }
  }
}

/* ======================================================================
 * 4. DRAWING
 * ====================================================================== */

/* Shorthand: fill a solid rectangle in the current fill color. */
static void fill(GContext *ctx, int x, int y, int w, int h) {
  graphics_fill_rect(ctx, GRect(x, y, w, h), 0, GCornerNone);
}

/* ---------- Digits ---------- */

/*
 * Draw a horizontal hexagonal segment (a, d or g) with pointed ends, like
 * an LED/LCD digit. It is built from 1-pixel-tall slices: the middle slices
 * are full length and the top and bottom slices are shortened ("inset"),
 * which produces the 45-degree points on the left and right ends.
 *
 *   x, y : top-left corner of the segment's bounding box
 *   len  : total length of the segment
 *   t    : thickness of the segment
 */
static void draw_hseg(GContext *ctx, int x, int y, int len, int t) {
  for (int i = 0; i < t; i++) {
    /* Distance of this slice from the middle of the segment, which sets
     * how far the slice is pulled in from each end. */
    int inset = abs(2 * i - (t - 1)) / 2;
    fill(ctx, x + inset, y + i, len - 2 * inset, 1);
  }
}

/* The same idea rotated 90 degrees: a vertical segment (b, c, e or f)
 * built from 1-pixel-wide slices, pointed at the top and bottom. */
static void draw_vseg(GContext *ctx, int x, int y, int len, int t) {
  for (int j = 0; j < t; j++) {
    int inset = abs(2 * j - (t - 1)) / 2;
    fill(ctx, x + j, y + inset, 1, len - 2 * inset);
  }
}

/*
 * Draw one digit (0-9) with its top-left corner at (x, y).
 * Two styles, chosen by the "digit gaps" setting:
 *
 *   Continuous : seven plain rectangles that overlap at the corners,
 *                giving a solid, heavy block-style digit.
 *   Gapped     : seven pointed hexagonal segments separated by a small
 *                gap at every corner, like a Casio LCD.
 */
static void draw_digit(GContext *ctx, int d, int x, int y, const Layout *L) {
  const uint8_t s = SEGMENTS[d];     /* which segments are lit              */
  const int w = L->digit_w;
  const int h = L->digit_h;
  const int t = L->seg_t;
  const int half = t / 2;

  if (!s_settings.digit_gaps) {
    /* ---- Continuous style ---- */
    const int hh    = h / 2;          /* vertical middle of the digit       */
    const int mid_y = (h - t) / 2;    /* top edge of the middle bar (g)     */

    if (s & 0x01) fill(ctx, x,         y,                  w, t);                 /* a */
    if (s & 0x02) fill(ctx, x + w - t, y,                  t, hh + half);         /* b */
    if (s & 0x04) fill(ctx, x + w - t, y + hh - half,      t, h - hh + half);     /* c */
    if (s & 0x08) fill(ctx, x,         y + h - t,          w, t);                 /* d */
    if (s & 0x10) fill(ctx, x,         y + hh - half,      t, h - hh + half);     /* e */
    if (s & 0x20) fill(ctx, x,         y,                  t, hh + half);         /* f */
    if (s & 0x40) fill(ctx, x,         y + mid_y,          w, t);                 /* g */
  } else {
    /* ---- Gapped (LCD) style ---- */
    const int g    = 2;                       /* gap at each segment corner */
    const int ymid = y + h / 2;               /* centre line of the digit   */

    /* Horizontal segments start and end just inside the vertical ones. */
    const int hx   = x + half + g;
    const int hlen = w - t - 2 * g;

    /* Upper vertical segments (f and b) run from just below segment a
     * to just above segment g. */
    const int up_y   = y + half + g;
    const int up_len = (ymid - g) - up_y;

    /* Lower vertical segments (e and c) run from just below g to just
     * above segment d. */
    const int low_y   = ymid + g;
    const int low_len = (y + h - half - g) - low_y;

    if (s & 0x01) draw_hseg(ctx, hx, y,                hlen,    t);  /* a */
    if (s & 0x40) draw_hseg(ctx, hx, ymid - half,      hlen,    t);  /* g */
    if (s & 0x08) draw_hseg(ctx, hx, y + h - t,        hlen,    t);  /* d */
    if (s & 0x20) draw_vseg(ctx, x,         up_y,      up_len,  t);  /* f */
    if (s & 0x02) draw_vseg(ctx, x + w - t, up_y,      up_len,  t);  /* b */
    if (s & 0x10) draw_vseg(ctx, x,         low_y,     low_len, t);  /* e */
    if (s & 0x04) draw_vseg(ctx, x + w - t, low_y,     low_len, t);  /* c */
  }
}

/* ---------- Battery ---------- */

/*
 * Choose the two battery colors.
 *   on  : the "charge remaining" part
 *   off : the "charge used" part
 * High contrast uses plain white for both. Normal mode color-codes the
 * remaining charge: green above 40%, yellow 21-40%, red 20% and below.
 */
static void battery_colors(GColor *on, GColor *off) {
  if (s_settings.high_contrast) {
    *on  = GColorWhite;
    *off = GColorWhite;
  } else {
    *on  = (s_battery > 40) ? GColorGreen
         : (s_battery > 20) ? GColorYellow
         :                    GColorRed;
    *off = GColorLightGray;
  }
}

/*
 * Draw one block of the segmented battery.
 *   filled : solid block in the "on" color
 *   hollow : outline in the "off" color with a black interior
 */
static void draw_block(GContext *ctx, int x, int y, int w, int h,
                       bool filled, GColor on, GColor off) {
  graphics_context_set_fill_color(ctx, filled ? on : off);
  fill(ctx, x, y, w, h);
  if (!filled) {
    /* Punch out the middle so only a border of BLOCK_BORDER px remains. */
    graphics_context_set_fill_color(ctx, GColorBlack);
    fill(ctx, x + BLOCK_BORDER, y + BLOCK_BORDER,
         w - 2 * BLOCK_BORDER, h - 2 * BLOCK_BORDER);
  }
}

/*
 * Draw the battery in whichever style and orientation is active.
 *
 * Continuous style : a thick bar for the charge remaining and a thin line
 *                    for the charge used, so the bar visibly "thins out"
 *                    as the battery drains.
 * Segmented style  : 10 blocks, each filled or hollow, rounded to the
 *                    nearest 10%.
 *
 * Horizontal bars fill from left to right; vertical bars fill from the
 * bottom up (like a thermometer).
 */
static void draw_battery(GContext *ctx, const Layout *L) {
  GColor on, off;
  battery_colors(&on, &off);

  const GRect r = L->battery_rect;

  if (s_settings.segmented_battery) {
    /* ---- Segmented: 10 blocks ---- */
    const int filled = (s_battery + 5) / 10;   /* 0-10, nearest 10% */

    if (L->battery_vertical) {
      const int total = BLOCKS * VBLOCK_H + (BLOCKS - 1) * BLOCK_GAP;
      const int y0 = r.origin.y + (r.size.h - total) / 2;   /* centre */
      for (int i = 0; i < BLOCKS; i++) {
        /* Block 0 is the bottom one, so it is lit first as charge rises. */
        const int by = y0 + (BLOCKS - 1 - i) * (VBLOCK_H + BLOCK_GAP);
        draw_block(ctx, r.origin.x, by, r.size.w, VBLOCK_H,
                   i < filled, on, off);
      }
    } else {
      const int total = BLOCKS * HBLOCK_W + (BLOCKS - 1) * BLOCK_GAP;
      const int x0 = r.origin.x + (r.size.w - total) / 2;   /* centre */
      for (int i = 0; i < BLOCKS; i++) {
        const int bx = x0 + i * (HBLOCK_W + BLOCK_GAP);
        draw_block(ctx, bx, r.origin.y, HBLOCK_W, r.size.h,
                   i < filled, on, off);
      }
    }
  } else {
    /* ---- Continuous: thick "remaining" bar + thin "used" line ---- */
    const int thin = s_settings.high_contrast ? BAR_THIN_HC : BAR_THIN;

    if (L->battery_vertical) {
      const int fill_h = r.size.h * s_battery / 100;        /* thick part */
      const int fill_y = r.origin.y + r.size.h - fill_h;    /* its top    */

      graphics_context_set_fill_color(ctx, on);
      fill(ctx, r.origin.x, fill_y, r.size.w, fill_h);

      /* Thin line occupies the empty space above, centred in the bar. */
      graphics_context_set_fill_color(ctx, off);
      fill(ctx, r.origin.x + (r.size.w - thin) / 2, r.origin.y,
           thin, r.size.h - fill_h);
    } else {
      const int fill_w = r.size.w * s_battery / 100;

      graphics_context_set_fill_color(ctx, on);
      fill(ctx, r.origin.x, r.origin.y, fill_w, r.size.h);

      /* Thin line occupies the empty space to the right, centred. */
      graphics_context_set_fill_color(ctx, off);
      fill(ctx, r.origin.x + fill_w, r.origin.y + (r.size.h - thin) / 2,
           r.size.w - fill_w, thin);
    }
  }
}

/* ---------- Date ---------- */

/*
 * Draw the date on one line, e.g. "Fri Nov 07".
 * It prefers the big bold font. If the text would be wider than the box
 * (possible in the side-bar layout with a long name like "Wed Sep 30"),
 * it falls back to the next size down so nothing is cut off.
 */
static void draw_date_horizontal(GContext *ctx, GRect area) {
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  int y_nudge = 0;

  /* Measure the text in the big font (the 1000-px-wide box just means
   * "don't wrap while measuring"). */
  GSize size = graphics_text_layout_get_content_size(
      s_date_full, font, GRect(0, 0, 1000, 40),
      GTextOverflowModeWordWrap, GTextAlignmentLeft);

  if (size.w > area.size.w) {
    font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    y_nudge = 4;   /* the smaller font sits higher in its box; drop it */
  }

  area.origin.y += y_nudge;
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_date_full, font, area,
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

/*
 * Draw the date vertically, one upright character per line, with a blank
 * gap between groups:
 *
 *     F
 *     r
 *     i
 *
 *     N
 *     o
 *     v
 *
 *     0
 *     7
 *
 * Each character is drawn separately (rather than as one multi-line text
 * block) so that the line spacing is exactly VDATE_PITCH and the group gaps
 * are exactly VDATE_GROUP_GAP, and the whole stack can be centred
 * vertically in the column.
 */
static void draw_date_vertical(GContext *ctx, GRect col) {
  const char *groups[3] = { s_dow, s_mon, s_dom };
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  /* Count characters so the stack can be centred. */
  int chars = 0;
  for (int g = 0; g < 3; g++) {
    chars += (int)strlen(groups[g]);
  }
  const int total = chars * VDATE_PITCH + 2 * VDATE_GROUP_GAP;
  int y = col.origin.y + (col.size.h - total) / 2;

  graphics_context_set_text_color(ctx, GColorWhite);

  for (int g = 0; g < 3; g++) {
    for (const char *p = groups[g]; *p != '\0'; p++) {
      char one[2] = { *p, '\0' };   /* a one-character string */
      /* The box is taller than one line so glyphs are never clipped;
       * VDATE_NUDGE lifts the letters to cancel the font's top padding. */
      graphics_draw_text(ctx, one, font,
                         GRect(col.origin.x, y - VDATE_NUDGE,
                               col.size.w, VDATE_PITCH + 8),
                         GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentCenter, NULL);
      y += VDATE_PITCH;
    }
    y += VDATE_GROUP_GAP;   /* blank gap before the next group */
  }
}

/* ---------- Full redraw ---------- */

/*
 * Called by Pebble whenever the canvas needs repainting (after
 * layer_mark_dirty). Everything on the face is drawn here.
 */
static void canvas_update(Layer *layer, GContext *ctx) {
  Layout L;
  layout_compute(&L, layer_get_bounds(layer));

  /* Digits: hours on the top row, minutes below. */
  const int xa = L.x0;                              /* first digit  */
  const int xb = L.x0 + L.digit_w + L.digit_gap;    /* second digit */

  graphics_context_set_fill_color(ctx, GColorWhite);
  draw_digit(ctx, s_hour / 10, xa, L.row1_y, &L);
  draw_digit(ctx, s_hour % 10, xb, L.row1_y, &L);
  draw_digit(ctx, s_min  / 10, xa, L.row2_y, &L);
  draw_digit(ctx, s_min  % 10, xb, L.row2_y, &L);

  /* Battery (changes fill color internally). */
  draw_battery(ctx, &L);

  /* Date. */
  if (L.date_vertical) {
    draw_date_vertical(ctx, L.date_rect);
  } else {
    draw_date_horizontal(ctx, L.date_rect);
  }
}

/* ======================================================================
 * 5. TIME + EVENTS
 * ====================================================================== */

/* Read the clock, store the values to display, and request a redraw. */
static void update_time(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  int h = t->tm_hour;
  if (!clock_is_24h_style()) {
    h %= 12;            /* 13:00 -> 1 */
    if (h == 0) h = 12; /* 00:00 -> 12 */
  }
  s_hour = h;
  s_min  = t->tm_min;

  strftime(s_dow, sizeof(s_dow), "%a", t);   /* Fri */
  strftime(s_mon, sizeof(s_mon), "%b", t);   /* Nov */
  strftime(s_dom, sizeof(s_dom), "%d", t);   /* 07  */
  snprintf(s_date_full, sizeof(s_date_full), "%s %s %s",
           s_dow, s_mon, s_dom);

  if (s_canvas) {
    layer_mark_dirty(s_canvas);
  }
}

/* Called once a minute by the tick timer service. */
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

/* Called whenever the battery level or charging state changes. */
static void battery_handler(BatteryChargeState state) {
  s_battery = state.charge_percent;
  if (s_canvas) {
    layer_mark_dirty(s_canvas);
  }
}

/*
 * Read an integer out of an AppMessage tuple. Clay sends toggles as
 * integers but dropdown ("select") values as text such as "1", so accept
 * either form.
 */
static int tuple_to_int(const Tuple *t) {
  if (t->type == TUPLE_CSTRING) {
    return atoi(t->value->cstring);
  }
  return (int)t->value->int32;
}

/*
 * Called when the phone sends new settings (the user pressed Save in the
 * settings page). Each key is optional, so only fields present in the
 * message are changed. The result is saved and the face is redrawn.
 */
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;

  if ((t = dict_find(iter, MESSAGE_KEY_HighContrast))) {
    s_settings.high_contrast = (tuple_to_int(t) == 1);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_DigitGaps))) {
    s_settings.digit_gaps = (tuple_to_int(t) == 1);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_SegmentedBattery))) {
    s_settings.segmented_battery = (tuple_to_int(t) == 1);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_Layout))) {
    int v = tuple_to_int(t);
    s_settings.layout = (v >= 0 && v < LAYOUT_COUNT) ? (uint8_t)v
                                                     : LAYOUT_STANDARD;
  }

  settings_save();
  if (s_canvas) {
    layer_mark_dirty(s_canvas);
  }
}

/* ======================================================================
 * 6. WINDOW + APP LIFECYCLE
 * ====================================================================== */

/* Create the full-screen drawing layer and show the first frame. */
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorBlack);

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  s_battery = battery_state_service_peek().charge_percent;
  update_time();
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;   /* the event handlers check for NULL */
}

static void init(void) {
  settings_load();

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  /* Redraw once a minute (saves battery vs. every second). */
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);

  /* Receive settings from the phone. Small buffers are plenty. */
  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 128);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
