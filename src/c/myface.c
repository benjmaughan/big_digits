#include <pebble.h>

// ---------- Layout ----------
#define DIGIT_W      64
#define DIGIT_H      60
#define SEG_T        12   // segment thickness
#define SEG_GAP      3    // gap between segments when "digit gaps" is on
#define DIGIT_GAP    8
#define ROW1_Y       2
#define ROW2_Y       (ROW1_Y + DIGIT_H + 4)

#define BAR_Y        130
#define BAR_THICK    10
#define BAR_THIN     2
#define BAR_THIN_HC  3
#define BAR_MARGIN   4

#define BLOCKS       10
#define BLOCK_W      11
#define BLOCK_GAP    3
#define BLOCK_BORDER 2

// ---------- Settings ----------
#define SETTINGS_KEY 1

typedef struct {
  bool high_contrast;
  bool digit_gaps;
  bool segmented_battery;
} Settings;

static Settings s_settings;

static void settings_load(void) {
  s_settings = (Settings){ false, false, false };  // defaults
  persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

static void settings_save(void) {
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

// ---------- State ----------
static Window *s_window;
static Layer *s_canvas;
static TextLayer *s_date_layer;
static int s_hour, s_min, s_battery = 100;
static char s_date_buf[16];

// Segment bits: a=top, b=upper right, c=lower right, d=bottom,
// e=lower left, f=upper left, g=middle
static const uint8_t SEGMENTS[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

// ---------- Drawing ----------
static void fill(GContext *ctx, int x, int y, int w, int h) {
  graphics_fill_rect(ctx, GRect(x, y, w, h), 0, GCornerNone);
}

static void draw_digit(GContext *ctx, int d, int x, int y, bool gaps) {
  const uint8_t s = SEGMENTS[d];
  const int w = DIGIT_W, h = DIGIT_H, t = SEG_T;
  const int mid_y = (h - t) / 2;  // top edge of the middle bar

  if (!gaps) {
    // Continuous: segments overlap at the corners
    const int half = h / 2;
    if (s & 0x01) fill(ctx, x, y, w, t);                                    // a
    if (s & 0x02) fill(ctx, x + w - t, y, t, half + t / 2);                 // b
    if (s & 0x04) fill(ctx, x + w - t, y + half - t / 2, t, h - half + t / 2); // c
    if (s & 0x08) fill(ctx, x, y + h - t, w, t);                            // d
    if (s & 0x10) fill(ctx, x, y + half - t / 2, t, h - half + t / 2);      // e
    if (s & 0x20) fill(ctx, x, y, t, half + t / 2);                         // f
    if (s & 0x40) fill(ctx, x, y + mid_y, w, t);                            // g
  } else {
    // Separate segments: verticals run corner to corner, horizontals fit between
    const int g = SEG_GAP;
    const int hx = x + t + g;
    const int hw = w - 2 * t - 2 * g;
    const int up_h = mid_y - g;
    const int low_y = mid_y + t + g;
    const int low_h = h - low_y;

    if (s & 0x01) fill(ctx, hx, y, hw, t);                  // a
    if (s & 0x02) fill(ctx, x + w - t, y, t, up_h);         // b
    if (s & 0x04) fill(ctx, x + w - t, y + low_y, t, low_h); // c
    if (s & 0x08) fill(ctx, hx, y + h - t, hw, t);          // d
    if (s & 0x10) fill(ctx, x, y + low_y, t, low_h);        // e
    if (s & 0x20) fill(ctx, x, y, t, up_h);                 // f
    if (s & 0x40) fill(ctx, hx, y + mid_y, hw, t);          // g
  }
}

static void draw_battery(GContext *ctx, GRect b) {
  const bool hc = s_settings.high_contrast;

  GColor on = hc ? GColorWhite
            : s_battery > 40 ? GColorGreen
            : s_battery > 20 ? GColorYellow
            : GColorRed;
  GColor off = hc ? GColorWhite : GColorLightGray;

  if (s_settings.segmented_battery) {
    // 10 blocks: filled = charge remaining, hollow outline = used
    const int total = BLOCKS * BLOCK_W + (BLOCKS - 1) * BLOCK_GAP;
    const int x0 = (b.size.w - total) / 2;
    const int filled = (s_battery + 5) / 10;  // rounds to nearest 10%

    for (int i = 0; i < BLOCKS; i++) {
      int bx = x0 + i * (BLOCK_W + BLOCK_GAP);
      graphics_context_set_fill_color(ctx, i < filled ? on : off);
      fill(ctx, bx, BAR_Y, BLOCK_W, BAR_THICK);
      if (i >= filled) {
        graphics_context_set_fill_color(ctx, GColorBlack);
        fill(ctx, bx + BLOCK_BORDER, BAR_Y + BLOCK_BORDER,
             BLOCK_W - 2 * BLOCK_BORDER, BAR_THICK - 2 * BLOCK_BORDER);
      }
    }
  } else {
    // Continuous: thick bar = remaining, thin line = used
    const int thin = hc ? BAR_THIN_HC : BAR_THIN;
    const int full_w = b.size.w - 2 * BAR_MARGIN;
    const int fill_w = full_w * s_battery / 100;
    const int mid_y = BAR_Y + BAR_THICK / 2;

    graphics_context_set_fill_color(ctx, on);
    fill(ctx, BAR_MARGIN, BAR_Y, fill_w, BAR_THICK);

    graphics_context_set_fill_color(ctx, off);
    fill(ctx, BAR_MARGIN + fill_w, mid_y - thin / 2, full_w - fill_w, thin);
  }
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int x0 = (b.size.w - (2 * DIGIT_W + DIGIT_GAP)) / 2;
  int x1 = x0 + DIGIT_W + DIGIT_GAP;

  graphics_context_set_fill_color(ctx, GColorWhite);
  draw_digit(ctx, s_hour / 10, x0, ROW1_Y, s_settings.digit_gaps);
  draw_digit(ctx, s_hour % 10, x1, ROW1_Y, s_settings.digit_gaps);
  draw_digit(ctx, s_min / 10,  x0, ROW2_Y, s_settings.digit_gaps);
  draw_digit(ctx, s_min % 10,  x1, ROW2_Y, s_settings.digit_gaps);

  draw_battery(ctx, b);
}

// ---------- Updates ----------
static void update_time(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  int h = t->tm_hour;
  if (!clock_is_24h_style()) {
    h %= 12;
    if (h == 0) h = 12;
  }
  s_hour = h;
  s_min = t->tm_min;

  strftime(s_date_buf, sizeof(s_date_buf), "%a %b %d", t);
  text_layer_set_text(s_date_layer, s_date_buf);
  layer_mark_dirty(s_canvas);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void battery_handler(BatteryChargeState state) {
  s_battery = state.charge_percent;
  layer_mark_dirty(s_canvas);
}

// Receives settings from the phone (Clay)
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;
  if ((t = dict_find(iter, MESSAGE_KEY_HighContrast))) {
    s_settings.high_contrast = (t->value->int32 == 1);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_DigitGaps))) {
    s_settings.digit_gaps = (t->value->int32 == 1);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_SegmentedBattery))) {
    s_settings.segmented_battery = (t->value->int32 == 1);
  }
  settings_save();
  if (s_canvas) layer_mark_dirty(s_canvas);
}

// ---------- Window ----------
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorBlack);

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  s_date_layer = text_layer_create(GRect(0, 138, bounds.size.w, 30));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_date_layer));

  s_battery = battery_state_service_peek().charge_percent;
  update_time();
}

static void window_unload(Window *window) {
  text_layer_destroy(s_date_layer);
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

static void init(void) {
  settings_load();

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);

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
