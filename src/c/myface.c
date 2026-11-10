/*
 * Big Digits: a high-visibility watchface for the original Pebble Time
 * ====================================================================
 *
 * Target platform : basalt (Pebble Time), 144 x 168 pixels, color
 *
 * WHAT IT DRAWS
 *   - The time as two rows of huge seven-segment digits (HH over MM).
 *   - A battery bar, optionally with a thinner phone battery bar beside it.
 *   - A Bluetooth-disconnected indicator (only visible while disconnected),
 *     with optional vibration when the connection drops or returns.
 *   - The date, either as text ("Fri Nov 06") or numerically with bars
 *     between the fields ("06 | 11 | 26", i.e. dd | mm | yy).
 *
 * USER SETTINGS (edited on the phone with Clay, stored on the watch)
 *   1. Color scheme       : Standard (white on black, color-coded battery),
 *                           High contrast (pure white on black), or
 *                           Classic LCD (black on a pale yellow-green, like
 *                           an old digital watch or calculator).
 *   2. Digit gaps         : LCD-style pointed segments with small gaps,
 *                           instead of solid, merged bars.
 *   3. Segmented battery  : 10 full/hollow blocks instead of a smooth bar.
 *   4. Layout             : where the battery and date go (see below).
 *   5. Date format        : text ("Fri Nov 06") or numeric ("06 | 11 | 26").
 *                           In the numeric format the fields are separated
 *                           by vertical bars when the date is horizontal,
 *                           and by horizontal bars when it is vertical.
 *   6. Bluetooth vibration: off / on disconnect / on disconnect and
 *                           reconnect. (The indicator itself always shows
 *                           while disconnected.)
 *   7. Phone battery      : show a second bar for the phone's battery,
 *                           below (horizontal) or right of (vertical) the
 *                           watch bar, sharing the same space.
 *   8. Segmented phone bar: blocks or smooth, independent of setting 3.
 *
 * QUICK TWEAK GUIDE
 *   Everything below is a #define in section 1. Edit, rebuild, repeat.
 *
 *   Date text size / font
 *     Horizontal date ........ DATE_FONT_KEY, DATE_FONT_Y_NUDGE,
 *                              DATE_FALLBACK_FONT_KEY, DATE_FALLBACK_Y_NUDGE
 *     Vertical date, text .... VDATE_FONT_KEY, VDATE_PITCH,
 *                              VDATE_GROUP_GAP, VDATE_NUDGE
 *     Vertical date, numeric . VDATE_NUM_FONT_KEY, VDATE_NUM_PITCH,
 *                              VDATE_NUM_GAP, VDATE_NUM_NUDGE
 *     Width of vertical date column ... SBV_DATE_COL_W
 *   Numeric-date separator bars
 *     Horizontal date ........ DATE_SEP_THICK, DATE_SEP_LEN, DATE_SEP_PAD,
 *                              DATE_SEP_Y_NUDGE
 *     Vertical date .......... DATE_SEP_THICK, VDATE_SEP_LEN
 *   Battery bar
 *     Width of vertical bar .. VBAR_W   (digits resize themselves to suit)
 *     Height of horizontal bar BAR_THICK
 *     Gap between digits and bar ... BAR_GAP
 *     Bar length (vertical) .. VBAR_MARGIN_Y (smaller = longer bar)
 *     Segmented blocks ....... VBLOCK_H, HBLOCK_W, BLOCK_GAP, BLOCK_BORDER
 *     Thin "used" line ....... BAR_THIN, BAR_THIN_HC
 *   Phone battery bar
 *     Thickness / gap ........ WATCH_BAR_THICK_DUAL, PHONE_BAR_THICK,
 *                              PHONE_BAR_GAP (digits resize to suit);
 *                              SIDE_BAR layout: SB_WATCH_BAR_THICK_DUAL,
 *                              SB_PHONE_BAR_THICK
 *     Colors (Standard) ...... PHONE_COLOR_HIGH, PHONE_COLOR_MID,
 *                              PHONE_COLOR_LOW
 *   Time digits
 *     Standard layout ........ STD_*      Side-bar layout ... SB_*
 *     Vertical-date layout ... SBV_*
 *   Bluetooth indicator
 *     Size ................... BT_ICON_W, BT_ICON_H, BT_ICON_STROKE
 *     Space it takes from the battery bar ... BT_ICON_GAP
 *     Vibration patterns ..... BT_VIBE_DISCONNECT_PATTERN,
 *                              BT_VIBE_RECONNECT_PATTERN
 *   Notification (Quick View) resizing ... MIN_SEG_T, fit_rows()
 *   Default settings ......... settings_load() in section 2
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
 *   2. Settings        (load / save / defaults, and the color palettes)
 *   3. Layout          (works out where everything goes for each layout)
 *   4. Drawing         (digits, battery, date)
 *   5. Time + events   (minute tick, battery events, messages from phone,
 *                       notifications covering the screen)
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

/* ---- Date format identifiers (stored in the settings; keep stable) ---- */
enum {
  DATE_FORMAT_TEXT     = 0,  /* "Fri Nov 06"                                 */
  DATE_FORMAT_NUMERIC  = 1,  /* "06 | 11 | 26"  (day | month | 2-digit year) */
  DATE_FORMAT_COUNT          /* number of formats; used for range checks     */
};

/* ---- Bluetooth vibration identifiers (stored in the settings) ---- */
enum {
  BT_VIBE_OFF         = 0,  /* never vibrate                                 */
  BT_VIBE_DISCONNECT  = 1,  /* vibrate when the connection is lost           */
  BT_VIBE_BOTH        = 2,  /* vibrate on loss and again on reconnection     */
  BT_VIBE_COUNT             /* number of options; used for range checks      */
};

/* ---- Color scheme identifiers (stored in the settings; keep stable) ---- */
enum {
  SCHEME_STANDARD       = 0,  /* white on black, color-coded battery         */
  SCHEME_HIGH_CONTRAST  = 1,  /* pure white on black, nothing else           */
  SCHEME_LCD            = 2,  /* black on pale yellow-green, like old LCDs   */
  SCHEME_COUNT                /* number of schemes; used for range checks    */
};

/*
 * Classic LCD colors. Pebble Time can only show 64 colors (4 levels each of
 * red, green and blue), so an exact LCD green isn't possible; these are the
 * closest matches. To change the look, swap LCD_BACKGROUND for another
 * palette color, e.g.:
 *   GColorBrass       a greyer, more khaki yellow-green
 *   GColorMintGreen   a paler, cooler green
 *   GColorSpringBud   a stronger, more saturated lime
 *   GColorPastelYellow  a pale cream
 */
#define LCD_BACKGROUND  GColorInchworm   /* the "unlit" LCD surface          */
#define LCD_INK         GColorBlack      /* the "lit" segments and text      */

/* ---- STANDARD layout: digits ---- */
#define STD_DIGIT_W     64   /* width of one digit in pixels                 */
#define STD_DIGIT_H     60   /* height of one digit in pixels                */
#define STD_SEG_T       10   /* segment thickness: bigger = bolder digits    */
#define STD_DIGIT_GAP    8   /* horizontal space between the two digits      */
#define STD_ROW1_Y       2   /* top of the hours row                         */
#define STD_ROW2_Y      66   /* top of the minutes row (2 + 60 + 4 gap)      */

/* ---- Horizontal battery bar (STANDARD layout) ---- */
#define HBAR_Y          130  /* top edge of the bar                          */
#define HBAR_MARGIN       3  /* space left and right of the bar              */

/* ---- Horizontal date (STANDARD and SIDE_BAR layouts) ---- */
#define DATE_Y          138  /* top of the date text box                     */
#define DATE_H           30  /* height of the date text box                  */

/* ---- Date font (horizontal date: STANDARD and SIDE_BAR layouts) ----
 *
 * The date is drawn in DATE_FONT_KEY if it fits the space available, and in
 * DATE_FALLBACK_FONT_KEY if it doesn't, so it is never cut off.
 *
 * To try another system font, change DATE_FONT_KEY. Samples of every font
 * are at https://developer.repebble.com/guides/app-resources/system-fonts/
 * Only fonts that contain LETTERS will work: the "NUMBERS" fonts (LECO,
 * BITHAM_xx_MEDIUM_NUMBERS) and ROBOTO_BOLD_SUBSET_49 only have digits.
 * Candidates with letters: FONT_KEY_GOTHIC_14/18/24/28 (each also _BOLD),
 * FONT_KEY_ROBOTO_CONDENSED_21, FONT_KEY_DROID_SERIF_28_BOLD,
 * FONT_KEY_BITHAM_30_BLACK.
 *
 * Fonts differ in how much blank space they leave above the letters, so
 * each has a *_Y_NUDGE: positive moves the text down, negative moves it up.
 */
/* #define DATE_FONT_KEY           FONT_KEY_GOTHIC_24_BOLD */
#define DATE_FONT_KEY           FONT_KEY_GOTHIC_28_BOLD
#define DATE_FONT_Y_NUDGE        0
#define DATE_FALLBACK_FONT_KEY  FONT_KEY_BITHAM_30_BLACK
/* #define DATE_FALLBACK_FONT_KEY  FONT_KEY_GOTHIC_18_BOLD */
#define DATE_FALLBACK_Y_NUDGE    4

/*
 * Optional: use a custom TrueType font for the date instead of
 * DATE_FONT_KEY, for example a 14-segment LCD font such as DSEG14 to match
 * the digits. Leave this at 0 until you have added the font file and the
 * resource entry to package.json (see the instructions that came with this
 * file); with 0, none of the custom-font code is compiled.
 *
 *   DATE_CUSTOM_FONT_RES   : RESOURCE_ID_ + the "name" in package.json
 *   DATE_CUSTOM_Y_NUDGE    : vertical adjustment for this font
 *   DATE_CUSTOM_UPPERCASE  : 1 = draw the date in capitals (segment fonts
 *                            usually look best, and are safest, in capitals)
 */
#define DATE_USE_CUSTOM_FONT     0
#define DATE_CUSTOM_FONT_RES     RESOURCE_ID_FONT_DSEG14_20
#define DATE_CUSTOM_Y_NUDGE      0
#define DATE_CUSTOM_UPPERCASE    1

/*
 * ---- Numeric date: separator bars ----
 * In the numeric format ("06 | 11 | 26") the separators are drawn as filled
 * rectangles rather than "|" characters, so their size is fully adjustable
 * and doesn't depend on the font containing that symbol.
 *
 * Horizontal date: each separator is a vertical bar DATE_SEP_THICK wide and
 * DATE_SEP_LEN tall, with DATE_SEP_PAD pixels of space on either side.
 * Vertical date: each separator is a horizontal bar DATE_SEP_THICK tall and
 * VDATE_SEP_LEN wide, centred in the gap between the groups.
 */
#define DATE_SEP_THICK      1  /* thickness of a separator bar, both ways    */
#define DATE_SEP_LEN       20  /* length (height) of a separator in the
                                  horizontal date                            */
#define DATE_SEP_PAD        5  /* blank space each side of a separator in the
                                  horizontal date                            */
#define DATE_SEP_Y_NUDGE   +5  /* move horizontal-date separators down (+) or
                                  up (-) relative to the digits              */
#define VDATE_SEP_LEN      12  /* length (width) of a separator in the
                                  vertical date                              */

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
#define SB_ROW2_Y        73

/* ---- SIDE_BAR_VDATE layout: digits and date column ---- */
#define SBV_DIGIT_H      76  /* taller digits: the date no longer needs the
                                bottom strip, so they use the whole height   */
#define SBV_SEG_T        10  /* slightly thinner: the digits are narrower    */
#define SBV_DIGIT_GAP     6
#define SBV_ROW_GAP       6  /* vertical space between hours and minutes     */
#define SBV_DATE_COL_W   20  /* width of the vertical date column            */
#define SBV_DATE_GAP      3  /* gap between the bar and the date column      */

/*
 * ---- Vertical date, text format (SIDE_BAR_VDATE layout): F r i / N o v / 0 7
 * One character is drawn per line. PITCH is the distance from one character
 * to the next, so it controls how tightly the stack is packed; the font
 * controls how big each character is. Keep the characters narrower than
 * SBV_DATE_COL_W.
 */
#define VDATE_FONT_KEY   FONT_KEY_GOTHIC_18_BOLD
#define VDATE_PITCH      17  /* vertical distance from one character to the
                                next                                         */
#define VDATE_GROUP_GAP   9  /* extra blank space between Fri / Nov / 07     */
#define VDATE_NUDGE       3  /* pulls glyphs up: Pebble fonts leave a few
                                pixels of empty space above the letters      */

/*
 * ---- Vertical date, numeric format: 0 6 / 1 1 / 2 6 ----
 * Only six characters, so they can be bigger than the text format's. The
 * GAP is the space between groups; a separator bar is drawn in the middle
 * of each gap. Stack height = 6 * PITCH + 2 * GAP, which must be at most
 * the screen height (168).
 */
#define VDATE_NUM_FONT_KEY  FONT_KEY_GOTHIC_28_BOLD
#define VDATE_NUM_PITCH     22  /* distance from one digit to the next       */
#define VDATE_NUM_GAP       14  /* space between dd / mm / yy (holds the bar)*/
#define VDATE_NUM_NUDGE     8   /* lifts digits to cancel the font's top
                                   padding; raise it to move digits up       */

/*
 * ---- Bluetooth-disconnected indicator ----
 * While the watch is disconnected from the phone, a Bluetooth symbol with a
 * slash through it is drawn at one end of the battery bar, and the bar gives
 * up that much length to make room (it returns to full length when the
 * connection comes back). The segmented battery shrinks its blocks if it has
 * to, so all 10 still fit.
 *
 * The icon is drawn from lines, so there are no image files to manage.
 */
#define BT_ICON_W         10  /* icon width in pixels                        */
#define BT_ICON_H         16  /* icon height in pixels                       */
#define BT_ICON_GAP        4  /* space between the icon and the bar          */
#define BT_ICON_STROKE     1  /* line thickness of the Bluetooth symbol      */
#define BT_ICON_SLASH      2  /* line thickness of the slash through it;
                                 0 = no slash (just the plain symbol)        */
#define BT_ICON_KNOCKOUT   2  /* extra width of a background-colored channel
                                 cut around the slash so it stands out from
                                 the symbol. 0 = none                        */

/*
 * Vibration patterns, as alternating durations in milliseconds, starting
 * with a vibration: { buzz, pause, buzz, pause, ... }.
 *   disconnect : two firm buzzes
 *   reconnect  : one short buzz
 * Vibration is skipped while Quiet Time is on.
 */
#define BT_VIBE_DISCONNECT_PATTERN  { 300, 120, 300 }
#define BT_VIBE_RECONNECT_PATTERN   { 120 }

/* ---- Battery bar styling ---- */
#define BAR_THICK        10  /* horizontal bar height (vertical bar width is
                                VBAR_W)                                      */
#define BAR_THIN          2  /* thickness of the "used" line, normal mode    */
#define BAR_THIN_HC       3  /* thickness of the "used" line in the single-
                                color schemes (high contrast and LCD), where
                                it can't be told apart by color              */

/* ---- Segmented battery (10 blocks) ---- */
#define BLOCKS           10  /* number of blocks; each is worth 10%          */
#define HBLOCK_W          9  /* block width when the bar is horizontal       */
#define VBLOCK_H         13  /* block height when the bar is vertical        */
#define BLOCK_GAP         3  /* space between neighbouring blocks            */
#define BLOCK_BORDER      2  /* outline thickness of a hollow block. Reduced
                                automatically on thin bars so a hollow block
                                always keeps a visible hole in the middle    */

/*
 * ---- Phone battery bar ----
 * An optional second bar showing the phone's battery, drawn alongside the
 * watch battery bar: below it when the bar is horizontal, to its right when
 * the bar is vertical. When the phone bar is turned on in the settings, the
 * two bars together take up
 *
 *   WATCH_BAR_THICK_DUAL + PHONE_BAR_GAP + PHONE_BAR_THICK   (6+2+6 = 14 px)
 *
 * instead of the single bar's BAR_THICK / VBAR_W (10 px), and the time
 * digits shrink to make room: shorter in the standard layout, narrower in
 * the side-bar layouts. The date keeps its size.
 *
 * The phone sends its battery level from the PebbleKit JS side
 * (src/pkjs/index.js). Until a level has arrived, or while the phone is
 * disconnected, the phone bar is hidden and the watch bar fills the whole
 * space. The digits don't change size when that happens; they only depend
 * on the setting.
 */
#define WATCH_BAR_THICK_DUAL  6  /* watch bar thickness beside the phone bar */
#define PHONE_BAR_THICK       6  /* thickness of the phone bar               */
#define PHONE_BAR_GAP         2  /* space between the watch bar and phone bar*/
#define DUAL_BAR_THICK  (WATCH_BAR_THICK_DUAL + PHONE_BAR_GAP + PHONE_BAR_THICK)

/* The SIDE_BAR layout (vertical bars, horizontal date) uses thicker bars:
 * 10 + 2 + 10 = 22 px. Its digits get narrower to suit; the date box keeps
 * its single-bar width unless that would run into the bars. */
#define SB_WATCH_BAR_THICK_DUAL 10
#define SB_PHONE_BAR_THICK      10
#define SB_DUAL_BAR_THICK  (SB_WATCH_BAR_THICK_DUAL + PHONE_BAR_GAP \
                            + SB_PHONE_BAR_THICK)

/* Phone bar colors in the Standard scheme (same thresholds as the watch
 * bar). Any of the 64 Pebble Time colors can be used, e.g. GColorPictonBlue,
 * GColorCyan, GColorVividViolet, GColorMagenta, GColorChromeYellow. */
#define PHONE_COLOR_HIGH  GColorVividCerulean  /* above 40%                  */
#define PHONE_COLOR_MID   GColorOrange         /* 21-40%                     */
#define PHONE_COLOR_LOW   GColorRed            /* 20% and below              */

/*
 * ---- Timeline Quick View ----
 * When a notification or upcoming event "peeks" in from the bottom of the
 * screen, the face fits itself into the part left visible (about 117 of the
 * 168 rows on Pebble Time), following the notification as it slides in and
 * out:
 *   - the date is hidden, since it would be underneath the notification;
 *   - the digits get shorter (and their segments proportionally thinner,
 *     but never thinner than MIN_SEG_T) to fit above it;
 *   - vertical battery bars get shorter; a horizontal bar moves up.
 * The side-bar-with-vertical-date layout switches to the plain side-bar
 * arrangement while covered, because its date column can't fit.
 */
#define MIN_SEG_T         6  /* thinnest the digit segments may get          */

/* ---- Persistent storage key ---- */
#define SETTINGS_KEY      5  /* change only if existing fields are reordered
                                or removed; new fields go at the end of the
                                struct (see settings_load)                   */

/* ======================================================================
 * 2. SETTINGS
 * ====================================================================== */

/*
 * All user-adjustable options. The whole struct is saved to the watch's
 * persistent storage in one go, so it survives restarts and battery swaps.
 */
typedef struct {
  uint8_t color_scheme;       /* one of the SCHEME_* values                  */
  bool    digit_gaps;         /* LCD-style segments with gaps                */
  bool    segmented_battery;  /* 10 blocks instead of a smooth bar           */
  uint8_t layout;             /* one of the LAYOUT_* values                  */
  uint8_t date_format;        /* one of the DATE_FORMAT_* values             */
  uint8_t bt_vibrate;         /* one of the BT_VIBE_* values                 */
  /* ---- Added later: keep new fields below this line ---- */
  bool    phone_battery;      /* show the phone battery bar                  */
  bool    phone_segmented;    /* phone bar as 10 blocks instead of smooth    */
} Settings;

/* Size of the settings saved by the previous version (everything up to
 * phone_battery), so those can still be read after an upgrade. */
#define SETTINGS_V1_SIZE  ((int)offsetof(Settings, phone_battery))

static Settings s_settings;

/*
 * Load saved settings, falling back to defaults when nothing valid is
 * stored. The size check protects against reading data written by an older
 * version of this watchface that used a different struct.
 */
static void settings_load(void) {
  /* Defaults first, so any field not loaded below has a sane value. */
  s_settings = (Settings){
    .color_scheme      = SCHEME_STANDARD,
    /* .color_scheme      = SCHEME_HIGH_CONTRAST, */
    /* .color_scheme      = SCHEME_LCD, */
    .digit_gaps        = true,
    .segmented_battery = true,
    /* .layout            = LAYOUT_STANDARD, */
    .layout            = LAYOUT_SIDE_BAR,
    /* .layout            = LAYOUT_SIDE_BAR_VDATE, */
    /* .date_format       = DATE_FORMAT_TEXT, */
    .date_format       = DATE_FORMAT_NUMERIC,
    /* .bt_vibrate        = BT_VIBE_OFF, */
    /* .bt_vibrate        = BT_VIBE_DISCONNECT, */
    .bt_vibrate        = BT_VIBE_BOTH,
    .phone_battery     = true,
    .phone_segmented   = true,
  };

  /* Read either the current struct, or the shorter one saved by the
   * previous version (whose fields are a prefix of this one); in the latter
   * case the newer fields keep their defaults. */
  if (persist_exists(SETTINGS_KEY)) {
    const int size = persist_get_size(SETTINGS_KEY);
    if (size == (int)sizeof(Settings) || size == SETTINGS_V1_SIZE) {
      persist_read_data(SETTINGS_KEY, &s_settings, size);
    }
  }

  /* Guard against corrupt data: out-of-range values fall back. */
  if (s_settings.layout >= (uint8_t)LAYOUT_COUNT) {
    s_settings.layout = LAYOUT_STANDARD;
  }
  if (s_settings.color_scheme >= (uint8_t)SCHEME_COUNT) {
    s_settings.color_scheme = SCHEME_STANDARD;
  }
  if (s_settings.date_format >= (uint8_t)DATE_FORMAT_COUNT) {
    s_settings.date_format = DATE_FORMAT_TEXT;
  }
  if (s_settings.bt_vibrate >= (uint8_t)BT_VIBE_COUNT) {
    s_settings.bt_vibrate = BT_VIBE_DISCONNECT;
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

static GFont s_date_font;          /* font for the horizontal date (see
                                      DATE_FONT_KEY / DATE_USE_CUSTOM_FONT)  */

static bool s_bt_connected = true; /* is the phone connected?               */

static int s_hour, s_min;          /* time to display (hour already 12/24h)  */
static int s_battery = 100;        /* battery charge, 0-100                  */
static int s_phone_battery = -1;   /* phone battery, 0-100, or -1 = unknown
                                      (not yet received, or disconnected)    */

/* Date pieces. They are kept separately so the vertical layout can draw
 * "Fri", "Nov" and "07" as distinct groups, and the horizontal layouts
 * can use the combined string "Fri Nov 07". */
static char s_dow[8];              /* day of week, e.g. "Fri"                */
static char s_mon[8];              /* month, e.g. "Nov"                      */
static char s_dom[4];              /* day of month, e.g. "07"                */
static char s_mon_num[4];          /* month as a number, e.g. "11"           */
static char s_yr[4];               /* two-digit year, e.g. "26"              */
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
  GRect battery_rect;     /* full extent of the watch battery bar            */
  bool show_phone;        /* draw the phone battery bar                      */
  GRect phone_rect;       /* the phone bar (only valid if show_phone)        */
  int phone_thick;        /* thickness of the phone bar for this layout      */

  /* Date */
  bool show_date;         /* false while a notification covers the bottom    */
  bool date_vertical;     /* true = one character per line                   */
  GRect date_rect;        /* box the date is drawn in                        */

  /* Bluetooth-disconnected indicator */
  bool show_bt;           /* true while disconnected                         */
  GRect bt_rect;          /* where the icon goes (only valid if show_bt)     */
} Layout;

/*
 * Squeeze the two rows of digits into the vertical space from `top` to
 * `bottom`, with `gap` pixels between the rows. Digits that already fit
 * keep their size; otherwise they get shorter, and their segments thinner
 * in proportion (never below MIN_SEG_T), so they keep their look. Only
 * the height changes: the width is left as the layout set it.
 */
static void fit_rows(Layout *L, int top, int bottom, int gap) {
  const int max_h = (bottom - top - gap) / 2;
  if (L->digit_h > max_h) {
    int t = L->seg_t * max_h / L->digit_h;
    if (t < MIN_SEG_T) t = MIN_SEG_T;
    L->seg_t   = t;
    L->digit_h = max_h;
  }
  L->row1_y = top;
  L->row2_y = top + L->digit_h + gap;
}

/*
 * Work out all positions for the layout chosen in the settings.
 *
 *   b     : the bounds of the full-screen canvas (144 x 168 on Pebble Time)
 *   vis_h : how much of that height is visible. Less than b.size.h while a
 *           Timeline Quick View notification covers the bottom of the
 *           screen, and in between while it slides in or out.
 *
 * While part of the screen is covered, the date is hidden (it would be
 * under the notification) and the digits and battery bar are fitted into
 * the visible area above it. See "TIMELINE QUICK VIEW" in section 1.
 */
static void layout_compute(Layout *L, GRect b, int vis_h) {
  const int w = b.size.w;
  const int h = b.size.h;
  const bool covered = vis_h < h;

  L->show_bt   = false;   /* switched on at the end if we are disconnected */
  L->show_date = !covered;

  /* While covered, the side-bar-with-vertical-date layout switches to the
   * plain side-bar arrangement: its date column runs the full height, so it
   * can't fit above the notification, and without it the digits can use
   * that width. */
  int layout = s_settings.layout;
  const bool vdate_covered = covered && layout == LAYOUT_SIDE_BAR_VDATE;
  if (vdate_covered) {
    layout = LAYOUT_SIDE_BAR;
  }

  /* Thickness of the battery bar space: wider when it holds two bars. */
  const bool dual  = s_settings.phone_battery;
  const int  hbar  = dual ? DUAL_BAR_THICK : BAR_THICK;   /* horizontal */
  const int  vbar  = dual ? DUAL_BAR_THICK : VBAR_W;      /* vertical   */
  L->phone_thick   = PHONE_BAR_THICK;   /* SIDE_BAR overrides this below */

  switch (layout) {

    case LAYOUT_SIDE_BAR: {
      /*
       * Horizontal budget, left to right:
       *   SIDE_MARGIN | digits | BAR_GAP | bar | SIDE_MARGIN
       * The digits get whatever width is left after the bar and margins,
       * so they are narrower than in the standard layout.
       */
      /* This layout has its own, thicker dual-bar size. (The vertical-date
       * layout borrows this arrangement while covered, but keeps its own
       * bar size.) */
      const int sb_bar = vdate_covered ? vbar
                       : dual          ? SB_DUAL_BAR_THICK : VBAR_W;
      L->phone_thick   = vdate_covered ? PHONE_BAR_THICK : SB_PHONE_BAR_THICK;

      const int avail = w - SIDE_MARGIN - BAR_GAP - sb_bar - SIDE_MARGIN;

      L->digit_h   = SB_DIGIT_H;
      L->seg_t     = SB_SEG_T;
      L->digit_gap = SB_DIGIT_GAP;
      L->digit_w   = (avail - L->digit_gap) / 2;
      L->x0        = SIDE_MARGIN;
      L->row1_y    = SB_ROW1_Y;
      L->row2_y    = SB_ROW2_Y;

      /* Bar: full (visible) height, flush with the right-hand margin. */
      L->battery_vertical = true;
      L->battery_rect = GRect(w - SIDE_MARGIN - sb_bar, VBAR_MARGIN_Y,
                              sb_bar, vis_h - 2 * VBAR_MARGIN_Y);

      /* Covered: the date is hidden, so the digits can use everything
       * above the notification, with the same margin at the bottom as at
       * the top. */
      if (covered) {
        fit_rows(L, SB_ROW1_Y, vis_h - SB_ROW1_Y,
                 SB_ROW2_Y - SB_ROW1_Y - SB_DIGIT_H);
      }

      /* Date: same strip at the bottom, but only as wide as the area to
       * the left of a single bar. It keeps that width when the bars are
       * wider (the extra comes out of the gap beside the date), except
       * that it never reaches into the bars themselves; with 22 px bars it
       * loses 2 px. The text is centred, so this only matters for a date
       * more than 118 px wide. If the text is still too
       * wide it drops to a smaller font automatically (see
       * draw_date_horizontal). */
      L->date_vertical = false;
      int date_w = w - SIDE_MARGIN - BAR_GAP - VBAR_W - SIDE_MARGIN;
      /* Very wide bars would overlap that box; stop it at the bars. */
      const int bar_left = w - SIDE_MARGIN - sb_bar;
      if (SIDE_MARGIN + date_w > bar_left) {
        date_w = bar_left - SIDE_MARGIN;
      }
      L->date_rect = GRect(SIDE_MARGIN, DATE_Y, date_w, DATE_H);
      break;
    }

    case LAYOUT_SIDE_BAR_VDATE: {
      /*
       * Horizontal budget, left to right:
       *   SIDE_MARGIN | digits | BAR_GAP | bar | SBV_DATE_GAP |
       *   date column | SIDE_MARGIN
       */
      const int avail = w - SIDE_MARGIN - BAR_GAP - vbar
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
                          - SBV_DATE_GAP - vbar;
      L->battery_vertical = true;
      L->battery_rect = GRect(bar_x, VBAR_MARGIN_Y,
                              vbar, h - 2 * VBAR_MARGIN_Y);

      /* Date column: the full height at the far right edge. */
      L->date_vertical = true;
      L->date_rect = GRect(w - SIDE_MARGIN - SBV_DATE_COL_W, 0,
                           SBV_DATE_COL_W, h);
      break;
    }

    case LAYOUT_STANDARD:
    default: {
      /*
       * A wider (two-bar) battery strip grows upwards, keeping its bottom
       * edge and the date where they are; the digits get shorter by the
       * same amount, split between the two rows.
       */
      const int extra = hbar - BAR_THICK;

      /* Digits fill the width, centred. */
      L->digit_w   = STD_DIGIT_W;
      L->digit_h   = STD_DIGIT_H - (extra + 1) / 2;
      L->seg_t     = STD_SEG_T;
      L->digit_gap = STD_DIGIT_GAP;
      L->x0        = (w - (2 * L->digit_w + L->digit_gap)) / 2;
      L->row1_y    = STD_ROW1_Y;
      L->row2_y    = STD_ROW1_Y + L->digit_h
                       + (STD_ROW2_Y - STD_ROW1_Y - STD_DIGIT_H);

      /* Battery: a horizontal bar across the screen below the digits. */
      L->battery_vertical = false;
      L->battery_rect = GRect(HBAR_MARGIN, HBAR_Y - extra,
                              w - 2 * HBAR_MARGIN, hbar);

      /* Covered: the date is hidden; the bar moves down to just above the
       * notification and the digits fill the space above the bar. */
      if (covered) {
        const int row_gap = STD_ROW2_Y - STD_ROW1_Y - STD_DIGIT_H;
        int bar_y = vis_h - HBAR_MARGIN - hbar;
        if (bar_y > L->battery_rect.origin.y) {
          bar_y = L->battery_rect.origin.y;   /* never lower than normal */
        }
        L->battery_rect.origin.y = bar_y;
        fit_rows(L, STD_ROW1_Y, bar_y - row_gap, row_gap);
      }

      /* Date: full-width strip at the bottom. */
      L->date_vertical = false;
      L->date_rect = GRect(0, DATE_Y, w, DATE_H);
      break;
    }
  }

  /*
   * Bluetooth indicator. While disconnected, take a slice off one end of
   * the battery bar and put the icon there:
   *   vertical bar   : the icon goes at the top, centred on the bar
   *   horizontal bar : the icon goes at the left, centred on the bar
   * Because everything else is positioned from battery_rect, the bar (and
   * its fill level) simply scales down to the shorter length.
   */
  if (!s_bt_connected) {
    GRect *bar = &L->battery_rect;
    L->show_bt = true;

    if (L->battery_vertical) {
      const int cx = bar->origin.x + bar->size.w / 2;
      L->bt_rect = GRect(cx - BT_ICON_W / 2, bar->origin.y,
                         BT_ICON_W, BT_ICON_H);
      bar->origin.y += BT_ICON_H + BT_ICON_GAP;
      bar->size.h   -= BT_ICON_H + BT_ICON_GAP;
    } else {
      const int cy = bar->origin.y + bar->size.h / 2;
      L->bt_rect = GRect(bar->origin.x, cy - BT_ICON_H / 2,
                         BT_ICON_W, BT_ICON_H);
      bar->origin.x += BT_ICON_W + BT_ICON_GAP;
      bar->size.w   -= BT_ICON_W + BT_ICON_GAP;
    }
  }

  /*
   * Phone battery bar. Split the bar's thickness in two: the watch bar keeps
   * the top (horizontal) or left (vertical) part, and the phone bar takes
   * the bottom or right part, with PHONE_BAR_GAP between them. This happens
   * after the Bluetooth adjustment so both bars are the same length. With
   * the space sized as DUAL_BAR_THICK (SB_DUAL_BAR_THICK in the SIDE_BAR
   * layout), the watch bar comes out at WATCH_BAR_THICK_DUAL
   * (SB_WATCH_BAR_THICK_DUAL).
   */
  L->show_phone = s_settings.phone_battery && s_phone_battery >= 0;
  if (L->show_phone) {
    GRect *bar = &L->battery_rect;
    if (L->battery_vertical) {
      const int watch_w = bar->size.w - PHONE_BAR_GAP - L->phone_thick;
      L->phone_rect = GRect(bar->origin.x + watch_w + PHONE_BAR_GAP,
                            bar->origin.y, L->phone_thick, bar->size.h);
      bar->size.w = watch_w;
    } else {
      const int watch_h = bar->size.h - PHONE_BAR_GAP - L->phone_thick;
      L->phone_rect = GRect(bar->origin.x,
                            bar->origin.y + watch_h + PHONE_BAR_GAP,
                            bar->size.w, L->phone_thick);
      bar->size.h = watch_h;
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

/* ---------- Colors ---------- */

/*
 * The set of colors used to draw one frame. Working these out in one place
 * means the drawing code never needs to know which color scheme is active;
 * it just uses whatever the palette says.
 */
typedef struct {
  GColor background;   /* the screen, and the inside of hollow battery blocks */
  GColor digits;       /* the time digits and the date text                    */
  GColor bar_on;       /* battery: the charge remaining                        */
  GColor phone_on;     /* phone battery: the charge remaining                  */
  GColor bar_off;      /* battery: the charge used (both bars)                 */
  GColor alert;        /* the Bluetooth-disconnected indicator                 */
} Palette;

/*
 * Fill in the palette for the current color scheme.
 *
 *   Standard      : white on black. The remaining charge is color-coded:
 *                   green above 40%, yellow 21-40%, red 20% and below, with
 *                   the used portion in light gray.
 *   High contrast : pure white on black, battery included.
 *   Classic LCD   : black "ink" on a pale yellow-green background, battery
 *                   included, like the display of an old digital watch.
 *
 * In the two single-color schemes the "remaining" and "used" parts of the
 * battery are the same color; they are told apart by thickness (thick bar
 * vs. thin line) or by solid vs. hollow blocks.
 *
 * In the Standard scheme the phone bar has its own colors, so it can't be
 * mistaken for the watch bar: blue above 40%, orange 21-40%, and red at 20%
 * and below (red is kept so a nearly flat phone still looks urgent). In the
 * single-color schemes it matches the watch bar.
 */
static GColor level_color(int percent) {
  return (percent > 40) ? GColorGreen
       : (percent > 20) ? GColorYellow
       :                  GColorRed;
}

static GColor phone_level_color(int percent) {
  return (percent > 40) ? PHONE_COLOR_HIGH
       : (percent > 20) ? PHONE_COLOR_MID
       :                  PHONE_COLOR_LOW;
}

static void palette_get(Palette *p) {
  switch (s_settings.color_scheme) {
    case SCHEME_HIGH_CONTRAST:
      p->background = GColorBlack;
      p->digits     = GColorWhite;
      p->bar_on     = GColorWhite;
      p->phone_on   = GColorWhite;
      p->bar_off    = GColorWhite;
      p->alert      = GColorWhite;
      break;

    case SCHEME_LCD:
      p->background = LCD_BACKGROUND;
      p->digits     = LCD_INK;
      p->bar_on     = LCD_INK;
      p->phone_on   = LCD_INK;
      p->bar_off    = LCD_INK;
      p->alert      = LCD_INK;
      break;

    case SCHEME_STANDARD:
    default:
      p->background = GColorBlack;
      p->digits     = GColorWhite;
      p->bar_on     = level_color(s_battery);
      p->phone_on   = phone_level_color(s_phone_battery);
      p->bar_off    = GColorLightGray;
      p->alert      = GColorRed;
      break;
  }
}

/* ---------- Battery ---------- */

/*
 * Draw one block of the segmented battery.
 *   filled : solid block in the `on` color
 *   hollow : outline in the "off" color with the background color inside,
 *            `border` pixels thick
 */
static void draw_block(GContext *ctx, int x, int y, int w, int h, int border,
                       bool filled, GColor on, const Palette *P) {
  graphics_context_set_fill_color(ctx, filled ? on : P->bar_off);
  fill(ctx, x, y, w, h);
  if (!filled) {
    /* Punch out the middle so only the border remains. */
    graphics_context_set_fill_color(ctx, P->background);
    fill(ctx, x + border, y + border, w - 2 * border, h - 2 * border);
  }
}

/*
 * Draw one battery bar (the watch's or the phone's) in rectangle `r`.
 *
 *   level     : charge, 0-100
 *   vertical  : true = fills bottom-up, false = fills left to right
 *   segmented : 10 blocks instead of a continuous bar
 *   on        : color of the charge remaining
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
static void draw_battery_bar(GContext *ctx, GRect r, int level, bool vertical,
                             bool segmented, GColor on, const Palette *P) {
  /* How thick the bar is across its short side. */
  const int thick = vertical ? r.size.w : r.size.h;

  if (segmented) {
    /* ---- Segmented: 10 blocks ---- */
    const int filled = (level + 5) / 10;   /* 0-10, nearest 10% */

    /* Thin bars (e.g. when the phone bar shares the space) get a thinner
     * outline so a hollow block still has a hole in the middle. */
    int border = BLOCK_BORDER;
    if (2 * border > thick - 2) border = (thick - 2) / 2;
    if (border < 1) border = 1;

    /*
     * Each block is its preferred size (VBLOCK_H / HBLOCK_W), but is made
     * smaller if the bar is too short for ten of them (which happens while
     * the Bluetooth icon is taking up one end of the bar).
     */
    if (vertical) {
      int block_h = (r.size.h - (BLOCKS - 1) * BLOCK_GAP) / BLOCKS;
      if (block_h > VBLOCK_H) block_h = VBLOCK_H;

      const int total = BLOCKS * block_h + (BLOCKS - 1) * BLOCK_GAP;
      /* Sit at the bottom of the bar so it doesn't jump around when the
       * icon appears at the top. */
      const int y0 = r.origin.y + r.size.h - total;
      for (int i = 0; i < BLOCKS; i++) {
        /* Block 0 is the bottom one, so it is lit first as charge rises. */
        const int by = y0 + (BLOCKS - 1 - i) * (block_h + BLOCK_GAP);
        draw_block(ctx, r.origin.x, by, r.size.w, block_h, border,
                   i < filled, on, P);
      }
    } else {
      int block_w = (r.size.w - (BLOCKS - 1) * BLOCK_GAP) / BLOCKS;
      if (block_w > HBLOCK_W) block_w = HBLOCK_W;

      const int total = BLOCKS * block_w + (BLOCKS - 1) * BLOCK_GAP;
      /* Sit at the right end of the bar so it doesn't jump around when the
       * icon appears at the left. */
      const int x0 = r.origin.x + r.size.w - total;
      for (int i = 0; i < BLOCKS; i++) {
        const int bx = x0 + i * (block_w + BLOCK_GAP);
        draw_block(ctx, bx, r.origin.y, block_w, r.size.h, border,
                   i < filled, on, P);
      }
    }
  } else {
    /* ---- Continuous: thick "remaining" bar + thin "used" line ---- */
    /* In the single-color schemes the thin line needs to be a bit thicker
     * to stay clearly visible, since it can't be told apart by color. It is
     * capped at half the bar's thickness so it still looks thin on a narrow
     * bar. */
    int thin = (s_settings.color_scheme == SCHEME_STANDARD)
                 ? BAR_THIN : BAR_THIN_HC;
    if (thin > thick / 2) thin = thick / 2;
    if (thin < 1) thin = 1;

    if (vertical) {
      const int fill_h = r.size.h * level / 100;            /* thick part */
      const int fill_y = r.origin.y + r.size.h - fill_h;    /* its top    */

      graphics_context_set_fill_color(ctx, on);
      fill(ctx, r.origin.x, fill_y, r.size.w, fill_h);

      /* Thin line occupies the empty space above, centred in the bar. */
      graphics_context_set_fill_color(ctx, P->bar_off);
      fill(ctx, r.origin.x + (r.size.w - thin) / 2, r.origin.y,
           thin, r.size.h - fill_h);
    } else {
      const int fill_w = r.size.w * level / 100;

      graphics_context_set_fill_color(ctx, on);
      fill(ctx, r.origin.x, r.origin.y, fill_w, r.size.h);

      /* Thin line occupies the empty space to the right, centred. */
      graphics_context_set_fill_color(ctx, P->bar_off);
      fill(ctx, r.origin.x + fill_w, r.origin.y + (r.size.h - thin) / 2,
           r.size.w - fill_w, thin);
    }
  }
}

/* Draw the watch battery bar, and the phone bar beside it if enabled. */
static void draw_battery(GContext *ctx, const Layout *L, const Palette *P) {
  draw_battery_bar(ctx, L->battery_rect, s_battery, L->battery_vertical,
                   s_settings.segmented_battery, P->bar_on, P);
  if (L->show_phone) {
    draw_battery_bar(ctx, L->phone_rect, s_phone_battery, L->battery_vertical,
                     s_settings.phone_segmented, P->phone_on, P);
  }
}

/* ---------- Bluetooth indicator ---------- */

/*
 * Draw a Bluetooth symbol with a slash through it ("Bluetooth off") inside
 * `r`. The symbol is the classic Bluetooth rune, built from five lines
 * between six points on the edge of the box:
 *
 *   T = top centre       U = bottom centre      stem   : T to U
 *   C = upper right      D = lower left         top arm: T to C, then C to D
 *   B = lower right      A = upper left         low arm: U to B, then B to A
 *
 * The two arms cross over the stem in the middle, which gives the rune its
 * shape. The slash is then drawn across it, optionally with a channel in
 * the background color cut around it first (BT_ICON_KNOCKOUT).
 */
static void draw_bt_icon(GContext *ctx, GRect r, const Palette *P) {
  const int x0 = r.origin.x;
  const int y0 = r.origin.y;
  const int w  = r.size.w;
  const int h  = r.size.h;

  const GPoint T = GPoint(x0 + w / 2, y0);                /* top of stem     */
  const GPoint U = GPoint(x0 + w / 2, y0 + h - 1);        /* bottom of stem  */
  const GPoint C = GPoint(x0 + w - 1,  y0 + h / 4);       /* upper right     */
  const GPoint B = GPoint(x0 + w - 1,  y0 + (3 * h) / 4); /* lower right     */
  const GPoint A = GPoint(x0,          y0 + h / 4);       /* upper left      */
  const GPoint D = GPoint(x0,          y0 + (3 * h) / 4); /* lower left      */

  graphics_context_set_stroke_color(ctx, P->alert);
  graphics_context_set_stroke_width(ctx, BT_ICON_STROKE);
  graphics_draw_line(ctx, T, U);
  graphics_draw_line(ctx, T, C);
  graphics_draw_line(ctx, C, D);
  graphics_draw_line(ctx, U, B);
  graphics_draw_line(ctx, B, A);

  /* The slash, top-left to bottom-right (it runs a little past the box). */
  const GPoint s0 = GPoint(x0 - 1,     y0 - 1);
  const GPoint s1 = GPoint(x0 + w,     y0 + h);

#if BT_ICON_SLASH > 0
#if BT_ICON_KNOCKOUT > 0
  graphics_context_set_stroke_color(ctx, P->background);
  graphics_context_set_stroke_width(ctx, BT_ICON_SLASH + BT_ICON_KNOCKOUT);
  graphics_draw_line(ctx, s0, s1);
#endif

  graphics_context_set_stroke_color(ctx, P->alert);
  graphics_context_set_stroke_width(ctx, BT_ICON_SLASH);
  graphics_draw_line(ctx, s0, s1);
#endif
}

/* ---------- Date ---------- */

/* Width in pixels of `text` drawn on one line in `font`. (The 1000-px-wide
 * measuring box just means "don't wrap while measuring".) */
static int text_width(const char *text, GFont font) {
  GSize size = graphics_text_layout_get_content_size(
      text, font, GRect(0, 0, 1000, 100),
      GTextOverflowModeWordWrap, GTextAlignmentLeft);
  return size.w;
}

#if DATE_USE_CUSTOM_FONT && DATE_CUSTOM_UPPERCASE
/* Convert a string to capitals in place (ASCII letters only). */
static void uppercase(char *str) {
  for (; *str != '\0'; str++) {
    if (*str >= 'a' && *str <= 'z') {
      *str = (char)(*str - 'a' + 'A');
    }
  }
}
#endif

/*
 * Draw the text-format date on one line, e.g. "Fri Nov 07".
 * It prefers the font chosen by DATE_FONT_KEY (or the custom font). If the
 * text would be wider than the box (possible in the side-bar layout with a
 * long name like "Wed Sep 30"), it falls back to DATE_FALLBACK_FONT_KEY so
 * nothing is cut off. Note that this means the date can change size from
 * day to day if the preferred font only just fits.
 */
static void draw_date_text_horizontal(GContext *ctx, GRect area, GColor color) {
  const char *text = s_date_full;
  GFont font = s_date_font;

#if DATE_USE_CUSTOM_FONT
  int y_nudge = DATE_CUSTOM_Y_NUDGE;
#else
  int y_nudge = DATE_FONT_Y_NUDGE;
#endif

#if DATE_USE_CUSTOM_FONT && DATE_CUSTOM_UPPERCASE
  char caps[sizeof(s_date_full)];
  snprintf(caps, sizeof(caps), "%s", s_date_full);
  uppercase(caps);
  text = caps;
#endif

  if (text_width(text, font) > area.size.w) {
    /* Too wide: switch to the fallback font, which is always a system
     * font, and use the date exactly as written (not in capitals). */
    text = s_date_full;
    font = fonts_get_system_font(DATE_FALLBACK_FONT_KEY);
    y_nudge = DATE_FALLBACK_Y_NUDGE;
  }

  area.origin.y += y_nudge;
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, area,
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

/*
 * Draw the numeric date on one line with vertical bars between the fields:
 *
 *     06 | 11 | 26      (day | month | two-digit year)
 *
 * The three fields are drawn as separate pieces of text, with the bars
 * drawn as filled rectangles between them. The whole group is centred in
 * `area`. As with the text format, if everything would be wider than the
 * area it switches to the fallback font.
 */
static void draw_date_numeric_horizontal(GContext *ctx, GRect area,
                                         GColor color) {
  const char *fields[3] = { s_dom, s_mon_num, s_yr };
  GFont font = s_date_font;

#if DATE_USE_CUSTOM_FONT
  int y_nudge = DATE_CUSTOM_Y_NUDGE;
#else
  int y_nudge = DATE_FONT_Y_NUDGE;
#endif

  /* Width of the two separators together, including the padding. */
  const int sep_total = 2 * (2 * DATE_SEP_PAD + DATE_SEP_THICK);
  int widths[3];
  int total = 0;

  /* Measure; if too wide for the area, measure again in the fallback. */
  for (int attempt = 0; attempt < 2; attempt++) {
    total = sep_total;
    for (int i = 0; i < 3; i++) {
      widths[i] = text_width(fields[i], font);
      total += widths[i];
    }
    if (total <= area.size.w || attempt == 1) {
      break;
    }
    font = fonts_get_system_font(DATE_FALLBACK_FONT_KEY);
    y_nudge = DATE_FALLBACK_Y_NUDGE;
  }

  graphics_context_set_text_color(ctx, color);
  graphics_context_set_fill_color(ctx, color);

  const int text_y = area.origin.y + y_nudge;
  /* Separators are centred vertically in the area, then follow the text's
   * nudge so they stay level with the digits if the font changes. */
  const int bar_y = area.origin.y + (area.size.h - DATE_SEP_LEN) / 2
                    + y_nudge + DATE_SEP_Y_NUDGE;

  int x = area.origin.x + (area.size.w - total) / 2;   /* centre the group */

  for (int i = 0; i < 3; i++) {
    /* Draw the field centred in a box a little wider than the measured
     * text, so rounding never clips a glyph or triggers an ellipsis. */
    graphics_draw_text(ctx, fields[i], font,
                       GRect(x - 2, text_y, widths[i] + 4, area.size.h),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    x += widths[i];

    if (i < 2) {
      x += DATE_SEP_PAD;
      fill(ctx, x, bar_y, DATE_SEP_THICK, DATE_SEP_LEN);
      x += DATE_SEP_THICK + DATE_SEP_PAD;
    }
  }
}

/* Draw the horizontal date in whichever format is selected. */
static void draw_date_horizontal(GContext *ctx, GRect area, GColor color) {
  if (s_settings.date_format == DATE_FORMAT_NUMERIC) {
    draw_date_numeric_horizontal(ctx, area, color);
  } else {
    draw_date_text_horizontal(ctx, area, color);
  }
}

/*
 * Draw the date vertically, one upright character per line, in groups.
 *
 *   Text format             Numeric format (dd / mm / yy)
 *
 *     F                       0
 *     r                       6
 *     i                      ---
 *                             1
 *     N                       1
 *     o                      ---
 *     v                       2
 *                             6
 *     0
 *     7
 *
 * In the text format the groups are separated by blank space; in the numeric
 * format by a short horizontal bar (the vertical counterpart of the "|"
 * used when the date is horizontal).
 *
 * Each character is drawn separately (rather than as one multi-line text
 * block) so that the line spacing is exactly the PITCH and the group gaps
 * are exactly the GAP, and the whole stack can be centred vertically in the
 * column. The font, pitch, gap and nudge come from the VDATE_* constants
 * for the selected format.
 */
static void draw_date_vertical(GContext *ctx, GRect col, GColor color) {
  const bool numeric = (s_settings.date_format == DATE_FORMAT_NUMERIC);

  /* Pick the three groups of characters and the metrics for this format. */
  const char *groups[3];
  GFont font;
  int pitch, gap, nudge;

  if (numeric) {
    groups[0] = s_dom;      groups[1] = s_mon_num;   groups[2] = s_yr;
    font  = fonts_get_system_font(VDATE_NUM_FONT_KEY);
    pitch = VDATE_NUM_PITCH;
    gap   = VDATE_NUM_GAP;
    nudge = VDATE_NUM_NUDGE;
  } else {
    groups[0] = s_dow;      groups[1] = s_mon;       groups[2] = s_dom;
    font  = fonts_get_system_font(VDATE_FONT_KEY);
    pitch = VDATE_PITCH;
    gap   = VDATE_GROUP_GAP;
    nudge = VDATE_NUDGE;
  }

  /* Count characters so the whole stack can be centred vertically. */
  int chars = 0;
  for (int g = 0; g < 3; g++) {
    chars += (int)strlen(groups[g]);
  }
  const int total = chars * pitch + 2 * gap;
  int y = col.origin.y + (col.size.h - total) / 2;

  graphics_context_set_text_color(ctx, color);
  graphics_context_set_fill_color(ctx, color);   /* for the separator bars */

  for (int g = 0; g < 3; g++) {
    for (const char *p = groups[g]; *p != '\0'; p++) {
      char one[2] = { *p, '\0' };   /* a one-character string */
      /* The box is taller than one line so glyphs are never clipped;
       * the nudge lifts the characters to cancel the font's top padding. */
      graphics_draw_text(ctx, one, font,
                         GRect(col.origin.x, y - nudge, col.size.w, pitch + 12),
                         GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentCenter, NULL);
      y += pitch;
    }

    if (g < 2) {
      /* Between groups: a horizontal bar centred in the gap (numeric
       * format only; the text format just leaves the gap blank). */
      if (numeric) {
        fill(ctx, col.origin.x + (col.size.w - VDATE_SEP_LEN) / 2,
             y + (gap - DATE_SEP_THICK) / 2, VDATE_SEP_LEN, DATE_SEP_THICK);
      }
      y += gap;
    }
  }
}

/* ---------- Full redraw ---------- */

/*
 * Called by Pebble whenever the canvas needs repainting (after
 * layer_mark_dirty). Everything on the face is drawn here.
 */
static void canvas_update(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);

  /* The part of the screen not covered by a Timeline Quick View
   * notification (the whole screen when there isn't one). */
#if PBL_API_EXISTS(layer_get_unobstructed_bounds)
  const int vis_h = layer_get_unobstructed_bounds(layer).size.h;
#else
  const int vis_h = bounds.size.h;
#endif

  Layout L;
  layout_compute(&L, bounds, vis_h);

  Palette P;
  palette_get(&P);

  /* Paint the whole screen in the background color first. Layers are
   * transparent by default, so without this the previous frame (or the
   * window color) would show through. */
  graphics_context_set_fill_color(ctx, P.background);
  fill(ctx, bounds.origin.x, bounds.origin.y, bounds.size.w, bounds.size.h);

  /* Digits: hours on the top row, minutes below. */
  const int xa = L.x0;                              /* first digit  */
  const int xb = L.x0 + L.digit_w + L.digit_gap;    /* second digit */

  graphics_context_set_fill_color(ctx, P.digits);
  draw_digit(ctx, s_hour / 10, xa, L.row1_y, &L);
  draw_digit(ctx, s_hour % 10, xb, L.row1_y, &L);
  draw_digit(ctx, s_min  / 10, xa, L.row2_y, &L);
  draw_digit(ctx, s_min  % 10, xb, L.row2_y, &L);

  /* Battery (sets its own fill colors internally). */
  draw_battery(ctx, &L, &P);

  /* Bluetooth-disconnected indicator, at the end of the battery bar. */
  if (L.show_bt) {
    draw_bt_icon(ctx, L.bt_rect, &P);
  }

  /* Date, in the same color as the digits (hidden under a notification). */
  if (!L.show_date) {
    /* nothing */
  } else if (L.date_vertical) {
    draw_date_vertical(ctx, L.date_rect, P.digits);
  } else {
    draw_date_horizontal(ctx, L.date_rect, P.digits);
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
  strftime(s_mon_num, sizeof(s_mon_num), "%m", t);   /* 11 */
  strftime(s_yr, sizeof(s_yr), "%y", t);             /* 26 */
  snprintf(s_date_full, sizeof(s_date_full), "%s %s %s",
           s_dow, s_mon, s_dom);

  if (s_canvas) {
    layer_mark_dirty(s_canvas);
  }
}

/*
 * Keep the window's own background in step with the color scheme. The canvas
 * paints over the whole screen anyway, but this avoids a flash of the wrong
 * color while the window slides in.
 */
static void apply_window_background(void) {
  if (s_window) {
    Palette p;
    palette_get(&p);
    window_set_background_color(s_window, p.background);
  }
}

/*
 * Play a vibration pattern, unless Quiet Time is on. `segments` alternates
 * vibrate / pause durations in milliseconds, starting with a vibration.
 */
static void vibrate(uint32_t *segments, uint32_t count) {
  if (quiet_time_is_active()) {
    return;
  }
  VibePattern pattern = { .durations = segments, .num_segments = count };
  vibes_enqueue_custom_pattern(pattern);
}

/*
 * Ask the phone for its current battery level. The JS side also sends it
 * whenever the level changes, but those updates can be missed while the
 * watch is disconnected, so we ask again on reconnection.
 */
static void request_phone_battery(void) {
  if (!s_settings.phone_battery) {
    return;
  }
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) == APP_MSG_OK) {
    dict_write_uint8(out, MESSAGE_KEY_PhoneBatteryRequest, 1);
    app_message_outbox_send();
  }
}

/*
 * Called by the system when the connection to the phone is made or lost.
 * It only fires on a change, so we never vibrate for a state we were
 * already in (and not at all when the watchface is first loaded while
 * disconnected; the indicator just appears).
 */
static void connection_handler(bool connected) {
  static uint32_t lost_pattern[]     = BT_VIBE_DISCONNECT_PATTERN;
  static uint32_t restored_pattern[] = BT_VIBE_RECONNECT_PATTERN;

  const bool changed = (connected != s_bt_connected);
  s_bt_connected = connected;

  if (changed) {
    if (!connected && s_settings.bt_vibrate != BT_VIBE_OFF) {
      vibrate(lost_pattern, sizeof(lost_pattern) / sizeof(lost_pattern[0]));
    } else if (connected && s_settings.bt_vibrate == BT_VIBE_BOTH) {
      vibrate(restored_pattern,
              sizeof(restored_pattern) / sizeof(restored_pattern[0]));
    }
  }

  /* The phone's battery level is stale while disconnected, so hide the
   * phone bar until a fresh one arrives. */
  if (!connected) {
    s_phone_battery = -1;
  } else if (changed) {
    request_phone_battery();
  }

  /* Redraw either way: the icon appears/disappears and the bar resizes. */
  if (s_canvas) {
    layer_mark_dirty(s_canvas);
  }
}

#if PBL_API_EXISTS(unobstructed_area_service_subscribe)
/*
 * Called on every animation frame while a Timeline Quick View notification
 * slides in or out. The canvas works out the visible area itself each time
 * it is drawn, so all we need to do is redraw.
 */
static void unobstructed_change(AnimationProgress progress, void *context) {
  if (s_canvas) {
    layer_mark_dirty(s_canvas);
  }
}
#endif

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
 * Called when the phone sends a message. Two kinds arrive:
 *   - new settings (the user pressed Save in the settings page). Each key
 *     is optional, so only fields present in the message are changed, and
 *     the result is saved.
 *   - the phone's battery level, sent by src/pkjs/index.js. This is not
 *     a setting, so it is just stored in memory.
 * Either way the face is redrawn.
 */
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;

  if ((t = dict_find(iter, MESSAGE_KEY_PhoneBatteryLevel))) {
    int v = tuple_to_int(t);
    s_phone_battery = (v < 0) ? -1 : (v > 100) ? 100 : v;
  }

  /* A battery-only message stops here, without touching storage. */
  const bool has_settings =
      dict_find(iter, MESSAGE_KEY_ColorScheme)   ||
      dict_find(iter, MESSAGE_KEY_DigitGaps)     ||
      dict_find(iter, MESSAGE_KEY_SegmentedBattery) ||
      dict_find(iter, MESSAGE_KEY_Layout)        ||
      dict_find(iter, MESSAGE_KEY_DateFormat)    ||
      dict_find(iter, MESSAGE_KEY_BtVibrate)     ||
      dict_find(iter, MESSAGE_KEY_PhoneBattery)  ||
      dict_find(iter, MESSAGE_KEY_PhoneSegmented);
  if (!has_settings) {
    if (s_canvas) {
      layer_mark_dirty(s_canvas);
    }
    return;
  }

  if ((t = dict_find(iter, MESSAGE_KEY_ColorScheme))) {
    int v = tuple_to_int(t);
    s_settings.color_scheme = (v >= 0 && v < SCHEME_COUNT) ? (uint8_t)v
                                                           : SCHEME_STANDARD;
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

  if ((t = dict_find(iter, MESSAGE_KEY_DateFormat))) {
    int v = tuple_to_int(t);
    s_settings.date_format = (v >= 0 && v < DATE_FORMAT_COUNT) ? (uint8_t)v
                                                               : DATE_FORMAT_TEXT;
  }

  if ((t = dict_find(iter, MESSAGE_KEY_BtVibrate))) {
    int v = tuple_to_int(t);
    s_settings.bt_vibrate = (v >= 0 && v < BT_VIBE_COUNT) ? (uint8_t)v
                                                          : BT_VIBE_DISCONNECT;
  }

  if ((t = dict_find(iter, MESSAGE_KEY_PhoneBattery))) {
    const bool was_on = s_settings.phone_battery;
    s_settings.phone_battery = (tuple_to_int(t) == 1);
    if (s_settings.phone_battery && !was_on) {
      request_phone_battery();
    }
  }
  if ((t = dict_find(iter, MESSAGE_KEY_PhoneSegmented))) {
    s_settings.phone_segmented = (tuple_to_int(t) == 1);
  }

  settings_save();
  apply_window_background();
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

  apply_window_background();

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

  /* Load the date font. System fonts need no cleanup; a custom font is
   * loaded from the app's resources and must be unloaded in deinit(). */
#if DATE_USE_CUSTOM_FONT
  s_date_font = fonts_load_custom_font(
      resource_get_handle(DATE_CUSTOM_FONT_RES));
#else
  s_date_font = fonts_get_system_font(DATE_FONT_KEY);
#endif

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  /* Redraw once a minute (saves battery vs. every second). */
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);

  /* Find out the current connection state without vibrating, then listen
   * for changes. */
  s_bt_connected = connection_service_peek_pebble_app_connection();
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = connection_handler,
  });

  /* Resize the face when a Timeline Quick View notification covers the
   * bottom of the screen. */
#if PBL_API_EXISTS(unobstructed_area_service_subscribe)
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers) {
    .change = unobstructed_change,
  }, NULL);
#endif

  /* Receive settings and the phone battery level from the phone, and send
   * phone-battery requests. Small buffers are plenty. */
  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 128);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
#if PBL_API_EXISTS(unobstructed_area_service_unsubscribe)
  unobstructed_area_service_unsubscribe();
#endif
  window_destroy(s_window);
#if DATE_USE_CUSTOM_FONT
  fonts_unload_custom_font(s_date_font);
#endif
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
