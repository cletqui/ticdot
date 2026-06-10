#include "message_keys.auto.h"
#include <pebble.h>

#define SETTINGS_PERSIST_KEY 1
#define STEP_GOAL_DEFAULT 10000

#define SPACING_DEG 10 // dot-to-dot spacing within a group
#define GAP1_DEG 15    // gap between BT dot and first group
#define GAP2_DEG 12    // gap between groups

static Window *s_window;
static Layer *s_canvas;

// Geometry — recomputed on bounds change
static int s_outer_r;
static int s_hour_len;
static int s_min_len;

// Display state — updated by event handlers, never computed in draw pass
static int s_battery_dots;
static bool s_is_charging;
static bool s_connected;
static int s_step_lit;
static int s_step_cycle;
static int s_day;
static int s_month;
static int s_weekday;
static int32_t s_hour_angle;
static int32_t s_min_angle;
static int s_notif_count;
static bool s_alarm_pending;
static bool s_event_pending;
static int s_heart_rate; // BPM, 0 = no data
static bool s_activity_active;

// Settings — fields appended at end only, never reordered, for persist compat
typedef struct {
  int step_goal;
  bool vibrate_disconnect;
  bool show_step_dots;
  bool show_date_dots;
  bool show_battery_dots;
  uint8_t hour_color_idx;
  uint8_t minute_color_idx;
  uint8_t over_goal_1_idx;
  uint8_t over_goal_2_idx;
  bool show_month_dots;
  bool show_weekday_dot;
  bool show_alarm_dot;
  bool show_notif_dot;
  bool show_event_dot;
  uint8_t bt_color_idx;
  uint8_t alarm_color_idx;
  uint8_t notif_normal_color_idx;
  uint8_t notif_alert_color_idx;
  uint8_t notif_threshold;
  uint8_t event_color_idx;
  uint8_t weekday_color_idx[7];
  bool show_hr_dot;
  bool show_activity_dot;
  uint8_t hr_color_idx;
  uint8_t hr_alert_color_idx;
  uint8_t hr_alert_bpm;
  uint8_t activity_color_idx;
} Settings;

static Settings s_settings;

// 0=Orange 1=Red 2=Green 3=Blue 4=Cyan 5=Yellow 6=Magenta 7=White 8=LightGray
static const GColor s_color_table[] = {
    GColorOrange, GColorRed,     GColorIslamicGreen, GColorBlue,     GColorCyan,
    GColorYellow, GColorMagenta, GColorWhite,        GColorLightGray};

static GColor get_color(uint8_t idx) {
  return s_color_table[idx % ARRAY_LENGTH(s_color_table)];
}

static void load_settings(void) {
  s_settings = (Settings){
      .step_goal = STEP_GOAL_DEFAULT,
      .vibrate_disconnect = true,
      .show_step_dots = true,
      .show_date_dots = true,
      .show_battery_dots = true,
      .hour_color_idx = 1,   // Red
      .minute_color_idx = 7, // White
      .over_goal_1_idx = 0,  // Orange
      .over_goal_2_idx = 4,  // Cyan
      .show_month_dots = true,
      .show_weekday_dot = true,
      .show_alarm_dot = true,
      .show_notif_dot = false,
      .show_event_dot = true,
      .bt_color_idx = 7,           // White
      .alarm_color_idx = 7,        // White
      .notif_normal_color_idx = 7, // White
      .notif_alert_color_idx = 1,  // Red
      .notif_threshold = 5,
      .event_color_idx = 7, // White
      // French/ancient planetary: Sun=Yellow Mon=LightGray Tue=Red Wed=Orange
      //                           Thu=Blue   Fri=Green     Sat=Magenta
      .weekday_color_idx = {5, 8, 1, 0, 3, 2, 6},
      .show_hr_dot = true,
      .show_activity_dot = true,
      .hr_color_idx = 7,       // White
      .hr_alert_color_idx = 1, // Red
      .hr_alert_bpm = 100,
      .activity_color_idx = 2, // Green
  };
  persist_read_data(SETTINGS_PERSIST_KEY, &s_settings, sizeof(s_settings));
}

static void save_settings(void) {
  persist_write_data(SETTINGS_PERSIST_KEY, &s_settings, sizeof(s_settings));
}

// Geometry

static GPoint point_on_circle(GPoint center, int32_t angle, int radius) {
  return (GPoint){
      .x = center.x + (int)(sin_lookup(angle) * radius / TRIG_MAX_RATIO),
      .y = center.y - (int)(cos_lookup(angle) * radius / TRIG_MAX_RATIO),
  };
}

static int32_t deg_to_trig(int degrees) {
  return (TRIG_MAX_ANGLE * degrees) / 360;
}

static void compute_geometry(GRect bounds) {
  int min_dim = bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h;
  s_outer_r = min_dim / 2 - PBL_IF_ROUND_ELSE(10, 6);
  s_hour_len = s_outer_r * 55 / 94;
  s_min_len = s_outer_r * 74 / 94;
}

// State updaters

static void update_battery_state(BatteryChargeState state) {
  int dots = (state.charge_percent + 19) / 20;
  s_battery_dots = dots > 5 ? 5 : dots;
  s_is_charging = state.is_charging;
}

static void update_step_state(void) {
  if (!s_settings.show_step_dots)
    return;
  int32_t steps = 0;
#if defined(PBL_HEALTH)
  HealthServiceAccessibilityMask mask = health_service_metric_accessible(
      HealthMetricStepCount, time_start_of_today(), time(NULL));
  if (mask & HealthServiceAccessibilityMaskAvailable) {
    steps = (int32_t)health_service_sum_today(HealthMetricStepCount);
  }
#endif
  int goal =
      s_settings.step_goal > 0 ? s_settings.step_goal : STEP_GOAL_DEFAULT;
  int cycle = (int)(steps / goal);
  s_step_cycle = cycle > 2 ? 2 : cycle;
  s_step_lit = (int32_t)(steps % goal) * 10 / goal;
}

static void update_health_state(void) {
#if defined(PBL_HEALTH)
  if (s_settings.show_hr_dot) {
    time_t now = time(NULL);
    HealthServiceAccessibilityMask mask = health_service_metric_accessible(
        HealthMetricHeartRateBPM, now - 300, now);
    s_heart_rate =
        (mask & HealthServiceAccessibilityMaskAvailable)
            ? (int)health_service_peek_current_value(HealthMetricHeartRateBPM)
            : 0;
  }
  if (s_settings.show_activity_dot) {
    HealthActivityMask activities = health_service_peek_current_activities();
    s_activity_active =
        (activities & HealthActivityWalk) || (activities & HealthActivityRun);
  }
#endif
}

#if defined(PBL_HEALTH)
static void health_event_handler(HealthEventType event, void *context) {
  if (event == HealthEventHeartRateUpdate ||
      event == HealthEventMovementUpdate) {
    update_health_state();
    layer_mark_dirty(s_canvas);
  }
}
#endif

// Called at init only — populates all time/date/angle state in one localtime()
// call
static void update_time_state(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  s_day = t->tm_mday;
  s_month = t->tm_mon + 1;
  s_weekday = t->tm_wday;
  s_hour_angle =
      deg_to_trig(((t->tm_hour % 12) * 60 + t->tm_min) * 360 / (12 * 60));
  s_min_angle = deg_to_trig(t->tm_min * 6);
}

// Draw pass — zero system calls, reads only pre-computed state

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_unobstructed_bounds(layer);
  GPoint center = grect_center_point(&bounds);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  // BT dot at 12 o'clock
  graphics_context_set_fill_color(
      ctx, s_connected ? get_color(s_settings.bt_color_idx) : GColorRed);
  graphics_fill_circle(ctx, point_on_circle(center, 0, s_outer_r), 4);

  // Right side: battery → alarm → notif → event → HR → activity (clockwise)
  int r_ang = GAP1_DEG;

  if (s_settings.show_battery_dots) {
    // Color progression: charging=Cyan, 4-5 dots=White, 3=Yellow, 2=Orange,
    // 1=Red
    GColor lit_color = s_is_charging         ? GColorCyan
                       : s_battery_dots >= 4 ? GColorWhite
                       : s_battery_dots == 3 ? GColorYellow
                       : s_battery_dots == 2 ? GColorOrange
                                             : GColorRed;
    for (int i = 0; i < 5; i++) {
      graphics_context_set_fill_color(
          ctx, i < 5 - s_battery_dots ? GColorDarkGray : lit_color);
      graphics_fill_circle(ctx,
                           point_on_circle(center,
                                           deg_to_trig(r_ang + i * SPACING_DEG),
                                           s_outer_r),
                           2);
    }
    r_ang += 4 * SPACING_DEG + GAP2_DEG;
  }

  if (s_settings.show_alarm_dot) {
    graphics_context_set_fill_color(
        ctx, s_alarm_pending ? get_color(s_settings.alarm_color_idx)
                             : GColorDarkGray);
    graphics_fill_circle(
        ctx, point_on_circle(center, deg_to_trig(r_ang), s_outer_r), 2);
    r_ang += GAP2_DEG;
  }

  if (s_settings.show_notif_dot) {
    GColor c = s_notif_count == 0 ? GColorDarkGray
               : s_notif_count >= s_settings.notif_threshold
                   ? get_color(s_settings.notif_alert_color_idx)
                   : get_color(s_settings.notif_normal_color_idx);
    graphics_context_set_fill_color(ctx, c);
    graphics_fill_circle(
        ctx, point_on_circle(center, deg_to_trig(r_ang), s_outer_r), 2);
    r_ang += GAP2_DEG;
  }

  if (s_settings.show_event_dot) {
    graphics_context_set_fill_color(
        ctx, s_event_pending ? get_color(s_settings.event_color_idx)
                             : GColorDarkGray);
    graphics_fill_circle(
        ctx, point_on_circle(center, deg_to_trig(r_ang), s_outer_r), 2);
    r_ang += GAP2_DEG;
  }

  if (s_settings.show_hr_dot) {
    GColor c = s_heart_rate == 0 ? GColorDarkGray
               : s_heart_rate >= s_settings.hr_alert_bpm
                   ? get_color(s_settings.hr_alert_color_idx)
                   : get_color(s_settings.hr_color_idx);
    graphics_context_set_fill_color(ctx, c);
    graphics_fill_circle(
        ctx, point_on_circle(center, deg_to_trig(r_ang), s_outer_r), 2);
    r_ang += GAP2_DEG;
  }

  if (s_settings.show_activity_dot) {
    graphics_context_set_fill_color(
        ctx, s_activity_active ? get_color(s_settings.activity_color_idx)
                               : GColorDarkGray);
    graphics_fill_circle(
        ctx, point_on_circle(center, deg_to_trig(r_ang), s_outer_r), 2);
  }

  // Steps: bottom arc centered at 6 o'clock, same radius as all dots
  if (s_settings.show_step_dots) {
    GColor step_c = s_step_cycle == 0   ? GColorWhite
                    : s_step_cycle == 1 ? get_color(s_settings.over_goal_1_idx)
                                        : get_color(s_settings.over_goal_2_idx);
    for (int i = 0; i < 10; i++) {
      graphics_context_set_fill_color(ctx,
                                      i < s_step_lit ? step_c : GColorDarkGray);
      graphics_fill_circle(ctx,
                           point_on_circle(center,
                                           deg_to_trig(225 - i * SPACING_DEG),
                                           s_outer_r),
                           2);
    }
  }

  // Left side: month → date → weekday (counter-clockwise)
  int l_ang = 360 - GAP1_DEG;

  if (s_settings.show_month_dots) {
    for (int i = 0; i < 4; i++) {
      bool lit = (s_month & (1 << (3 - i))) != 0;
      graphics_context_set_fill_color(ctx, lit ? GColorWhite : GColorDarkGray);
      graphics_fill_circle(ctx,
                           point_on_circle(center,
                                           deg_to_trig(l_ang - i * SPACING_DEG),
                                           s_outer_r),
                           2);
    }
    l_ang -= 3 * SPACING_DEG + GAP2_DEG;
  }

  if (s_settings.show_date_dots) {
    for (int i = 0; i < 5; i++) {
      bool lit = (s_day & (1 << (4 - i))) != 0;
      graphics_context_set_fill_color(ctx, lit ? GColorWhite : GColorDarkGray);
      graphics_fill_circle(ctx,
                           point_on_circle(center,
                                           deg_to_trig(l_ang - i * SPACING_DEG),
                                           s_outer_r),
                           2);
    }
    l_ang -= 4 * SPACING_DEG + GAP2_DEG;
  }

  if (s_settings.show_weekday_dot) {
    graphics_context_set_fill_color(
        ctx, get_color(s_settings.weekday_color_idx[s_weekday]));
    graphics_fill_circle(
        ctx, point_on_circle(center, deg_to_trig(l_ang), s_outer_r), 2);
  }

  // Clock hands — angles pre-computed in tick handler
  graphics_context_set_stroke_width(ctx, 10);
  graphics_context_set_stroke_color(ctx, get_color(s_settings.hour_color_idx));
  graphics_draw_line(ctx, center,
                     point_on_circle(center, s_hour_angle, s_hour_len));
  graphics_context_set_stroke_color(ctx,
                                    get_color(s_settings.minute_color_idx));
  graphics_draw_line(ctx, center,
                     point_on_circle(center, s_min_angle, s_min_len));
}

// Event handlers

static void tick_handler(struct tm *t, TimeUnits units_changed) {
  s_hour_angle =
      deg_to_trig(((t->tm_hour % 12) * 60 + t->tm_min) * 360 / (12 * 60));
  s_min_angle = deg_to_trig(t->tm_min * 6);

  if (t->tm_hour == 0 && t->tm_min == 0) {
    s_day = t->tm_mday;
    s_month = t->tm_mon + 1;
    s_weekday = t->tm_wday;
  }

  update_step_state();
  layer_mark_dirty(s_canvas);
}

static void battery_handler(BatteryChargeState state) {
  int prev_dots = s_battery_dots;
  bool prev_charging = s_is_charging;
  update_battery_state(state);
  if (s_battery_dots != prev_dots || s_is_charging != prev_charging) {
    layer_mark_dirty(s_canvas);
  }
}

static void connection_handler(bool connected) {
  s_connected = connected;
  if (!connected && s_settings.vibrate_disconnect) {
    vibes_short_pulse();
  }
  layer_mark_dirty(s_canvas);
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;

#define APPLY_BOOL(key, field)                                                 \
  t = dict_find(iter, key);                                                    \
  if (t)                                                                       \
    s_settings.field = t->value->uint8 != 0;
#define APPLY_U8(key, field)                                                   \
  t = dict_find(iter, key);                                                    \
  if (t)                                                                       \
    s_settings.field = t->value->uint8;
#define APPLY_I32(key, field)                                                  \
  t = dict_find(iter, key);                                                    \
  if (t)                                                                       \
    s_settings.field = t->value->int32;

  APPLY_I32(MESSAGE_KEY_StepGoal, step_goal)
  APPLY_BOOL(MESSAGE_KEY_VibrateOnDisconnect, vibrate_disconnect)
  APPLY_BOOL(MESSAGE_KEY_ShowStepDots, show_step_dots)
  APPLY_BOOL(MESSAGE_KEY_ShowDateDots, show_date_dots)
  APPLY_BOOL(MESSAGE_KEY_ShowBatteryDots, show_battery_dots)
  APPLY_U8(MESSAGE_KEY_HourColor, hour_color_idx)
  APPLY_U8(MESSAGE_KEY_MinuteColor, minute_color_idx)
  APPLY_U8(MESSAGE_KEY_OverGoalColor1, over_goal_1_idx)
  APPLY_U8(MESSAGE_KEY_OverGoalColor2, over_goal_2_idx)
  APPLY_BOOL(MESSAGE_KEY_ShowMonthDots, show_month_dots)
  APPLY_BOOL(MESSAGE_KEY_ShowWeekdayDot, show_weekday_dot)
  APPLY_BOOL(MESSAGE_KEY_ShowAlarmDot, show_alarm_dot)
  APPLY_BOOL(MESSAGE_KEY_ShowNotifDot, show_notif_dot)
  APPLY_BOOL(MESSAGE_KEY_ShowEventDot, show_event_dot)
  APPLY_U8(MESSAGE_KEY_BtColor, bt_color_idx)
  APPLY_U8(MESSAGE_KEY_AlarmColor, alarm_color_idx)
  APPLY_U8(MESSAGE_KEY_NotifNormalColor, notif_normal_color_idx)
  APPLY_U8(MESSAGE_KEY_NotifAlertColor, notif_alert_color_idx)
  APPLY_U8(MESSAGE_KEY_NotifThreshold, notif_threshold)
  APPLY_U8(MESSAGE_KEY_EventColor, event_color_idx)
  APPLY_BOOL(MESSAGE_KEY_ShowHrDot, show_hr_dot)
  APPLY_BOOL(MESSAGE_KEY_ShowActivityDot, show_activity_dot)
  APPLY_U8(MESSAGE_KEY_HrColor, hr_color_idx)
  APPLY_U8(MESSAGE_KEY_HrAlertColor, hr_alert_color_idx)
  APPLY_U8(MESSAGE_KEY_HrAlertBpm, hr_alert_bpm)
  APPLY_U8(MESSAGE_KEY_ActivityColor, activity_color_idx)

#undef APPLY_BOOL
#undef APPLY_U8
#undef APPLY_I32

  for (int day = 0; day < 7; day++) {
    t = dict_find(iter, MESSAGE_KEY_WeekdayColor0 + day);
    if (t)
      s_settings.weekday_color_idx[day] = t->value->uint8;
  }

  t = dict_find(iter, MESSAGE_KEY_NotifCount);
  if (t)
    s_notif_count = t->value->int32;

  t = dict_find(iter, MESSAGE_KEY_AlarmPending);
  if (t)
    s_alarm_pending = t->value->uint8 != 0;

  t = dict_find(iter, MESSAGE_KEY_EventPending);
  if (t)
    s_event_pending = t->value->uint8 != 0;

  save_settings();
  update_step_state();
  update_health_state();
  layer_mark_dirty(s_canvas);
}

// Window lifecycle

static void unobstructed_change(AnimationProgress progress, void *context) {
  compute_geometry(
      layer_get_unobstructed_bounds(window_get_root_layer(s_window)));
  layer_mark_dirty(s_canvas);
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  compute_geometry(layer_get_unobstructed_bounds(root));
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);
  unobstructed_area_service_subscribe(
      (UnobstructedAreaHandlers){.change = unobstructed_change}, NULL);
}

static void prv_window_unload(Window *window) {
  unobstructed_area_service_unsubscribe();
  layer_destroy(s_canvas);
}

// App init / deinit

static void prv_init(void) {
  load_settings();

  update_battery_state(battery_state_service_peek());
  s_connected = connection_service_peek_pebble_app_connection();
  update_step_state();
  update_health_state();
  update_time_state();

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
  app_message_register_inbox_received(inbox_received);
  app_message_open(512, 64);
#if defined(PBL_HEALTH)
  health_service_events_subscribe(health_event_handler, NULL);
#endif
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
