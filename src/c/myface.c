#include <pebble.h>

static Window *s_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;

static void update_time(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  static char time_buf[8];
  static char date_buf[16];

  strftime(time_buf, sizeof(time_buf),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", t);
  strftime(date_buf, sizeof(date_buf), "%a %b %d", t);

  text_layer_set_text(s_time_layer, time_buf);
  text_layer_set_text(s_date_layer, date_buf);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorOxfordBlue);

  // Time
  s_time_layer = text_layer_create(
      GRect(0, bounds.size.h / 2 - 40, bounds.size.w, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer,
                      fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  // Date
  s_date_layer = text_layer_create(
      GRect(0, bounds.size.h / 2 + 16, bounds.size.w, 30));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorMelon);
  text_layer_set_font(s_date_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_date_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
}

static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  update_time();
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
