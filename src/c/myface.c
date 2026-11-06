#include <pebble.h>

#define DIGIT_W   64
#define DIGIT_H   60
#define SEG_T     12   // segment thickness: bigger = bolder
#define DIGIT_GAP 8
#define ROW1_Y    2
#define ROW2_Y    (ROW1_Y + DIGIT_H + 4)
#define BAR_Y     130
#define BAR_THICK 10
#define BAR_THIN  2
#define BAR_MARGIN 4

// Segment bits: a=top, b=upper right, c=lower right, d=bottom,
// e=lower left, f=upper left, g=middle
static const uint8_t SEGMENTS[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

static Window *s_window;
static Layer *s_canvas;
static TextLayer *s_date_layer;
static int s_hour, s_min, s_battery = 100;
static char s_date_buf[16];

static void draw_digit(GContext *ctx, int d, int x, int y) {
  uint8_t s = SEGMENTS[d];
  const int w = DIGIT_W, h = DIGIT_H, t = SEG_T;
  const int half = h / 2;

  if (s & 0x01) graphics_fill_rect(ctx, GRect(x, y, w, t), 0, GCornerNone);                         // a
  if (s & 0x02) graphics_fill_rect(ctx, GRect(x + w - t, y, t, half + t / 2), 0, GCornerNone);      // b
  if (s & 0x04) graphics_fill_rect(ctx, GRect(x + w - t, y + half - t / 2, t, h - half + t / 2), 0, GCornerNone); // c
  if (s & 0x08) graphics_fill_rect(ctx, GRect(x, y + h - t, w, t), 0, GCornerNone);                 // d
  if (s & 0x10) graphics_fill_rect(ctx, GRect(x, y + half - t / 2, t, h - half + t / 2), 0, GCornerNone); // e
  if (s & 0x20) graphics_fill_rect(ctx, GRect(x, y, t, half + t / 2), 0, GCornerNone);              // f
  if (s & 0x40) graphics_fill_rect(ctx, GRect(x, y + (h - t) / 2, w, t), 0, GCornerNone);           // g
}

static void canvas_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int x0 = (b.size.w - (2 * DIGIT_W + DIGIT_GAP)) / 2;
  int x1 = x0 + DIGIT_W + DIGIT_GAP;

  // Digits
  graphics_context_set_fill_color(ctx, GColorWhite);
  draw_digit(ctx, s_hour / 10, x0, ROW1_Y);
  draw_digit(ctx, s_hour % 10, x1, ROW1_Y);
  draw_digit(ctx, s_min / 10,  x0, ROW2_Y);
  draw_digit(ctx, s_min % 10,  x1, ROW2_Y);

  // Battery bar: thick = charge remaining, thin = charge used
  int full_w = b.size.w - 2 * BAR_MARGIN;
  int fill_w = full_w * s_battery / 100;
  int mid_y = BAR_Y + BAR_THICK / 2;

  GColor bar_color = s_battery > 40 ? GColorGreen
                   : s_battery > 20 ? GColorYellow
                   : GColorRed;

  graphics_context_set_fill_color(ctx, bar_color);
  graphics_fill_rect(ctx, GRect(BAR_MARGIN, BAR_Y, fill_w, BAR_THICK), 0, GCornerNone);

  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_rect(ctx,
      GRect(BAR_MARGIN + fill_w, mid_y - BAR_THIN / 2, full_w - fill_w, BAR_THIN),
      0, GCornerNone);
}

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
}

static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
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
