#include <pebble.h>

static Window *s_window;
static Layer *s_canvas;
static int s_battery_filled;
static bool s_connected;

static GPoint point_on_circle(GPoint center, int32_t angle, int radius) {
  return (GPoint){
      .x = center.x + (int)(sin_lookup(angle) * radius / TRIG_MAX_RATIO),
      .y = center.y - (int)(cos_lookup(angle) * radius / TRIG_MAX_RATIO),
  };
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // 12 o'clock dot — red when Bluetooth disconnected
  graphics_context_set_fill_color(ctx, s_connected ? GColorWhite : GColorRed);
  graphics_fill_circle(ctx, point_on_circle(center, 0, 94), 3);

  // Battery arc — 10 dots along bottom, light gray filled / dark gray empty
  for (int i = 0; i < 10; i++) {
    int32_t angle = (TRIG_MAX_ANGLE * (150 + i * 7)) / 360;
    graphics_context_set_fill_color(
        ctx, (i < s_battery_filled) ? GColorLightGray : GColorDarkGray);
    graphics_fill_circle(ctx, point_on_circle(center, angle, 103), 2);
  }

  int32_t hour_angle =
      (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 60) + t->tm_min)) / (12 * 60);
  int32_t minute_angle = (TRIG_MAX_ANGLE * t->tm_min) / 60;

  graphics_context_set_stroke_width(ctx, 10);

  graphics_context_set_stroke_color(ctx, GColorOrange);
  graphics_draw_line(ctx, center, point_on_circle(center, hour_angle, 63));

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, center, point_on_circle(center, minute_angle, 84));
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_canvas);
}

static void battery_handler(BatteryChargeState state) {
  s_battery_filled = (state.charge_percent + 9) / 10;
  layer_mark_dirty(s_canvas);
}

static void connection_handler(bool connected) {
  s_connected = connected;
  if (!connected)
    vibes_short_pulse();
  layer_mark_dirty(s_canvas);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(window_layer, s_canvas);
}

static void prv_window_unload(Window *window) { layer_destroy(s_canvas); }

static void prv_init(void) {
  s_battery_filled = (battery_state_service_peek().charge_percent + 9) / 10;
  s_connected = connection_service_peek_pebble_app_connection();

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = prv_window_load,
                                           .unload = prv_window_unload,
                                       });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  connection_service_subscribe((ConnectionHandlers){
      .pebble_app_connection_handler = connection_handler,
  });
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
