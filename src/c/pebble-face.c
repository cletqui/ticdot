#include <pebble.h>
#include "message_keys.auto.h"

#define SETTINGS_PERSIST_KEY 1
#define STEP_GOAL_DEFAULT 10000

static Window *s_window;
static Layer *s_canvas;

// Geometry computed from display bounds — recalculated on unobstructed area change
static int s_outer_r;   // radius for outer dot ring (12 o'clock, battery, day dots)
static int s_step_r;    // radius for step + month dots (just outside outer ring)
static int s_hour_len;
static int s_min_len;

// Cached display state
static int s_battery_dots;
static bool s_battery_critical;
static bool s_connected;
static int s_step_lit;
static int s_step_cycle;
static int s_day;        // 1–31, displayed in binary across 5 dots

typedef struct {
    int step_goal;
    bool vibrate_disconnect;
    bool show_step_dots;
    uint8_t hour_color_idx;
    uint8_t over_goal_1_idx;
    uint8_t over_goal_2_idx;
} Settings;

static Settings s_settings;

// Index: 0=Orange 1=Red 2=Green 3=Blue 4=Cyan 5=Yellow 6=Magenta 7=White
static const GColor s_color_table[] = {
    GColorOrange, GColorRed, GColorIslamicGreen, GColorBlue,
    GColorCyan, GColorYellow, GColorMagenta, GColorWhite
};

static GColor get_color(uint8_t idx) {
    return s_color_table[idx % ARRAY_LENGTH(s_color_table)];
}

static void load_settings(void) {
    s_settings = (Settings){
        .step_goal = STEP_GOAL_DEFAULT,
        .vibrate_disconnect = true,
        .show_step_dots = true,
        .hour_color_idx = 0,
        .over_goal_1_idx = 0,
        .over_goal_2_idx = 4,
    };
    persist_read_data(SETTINGS_PERSIST_KEY, &s_settings, sizeof(s_settings));
}

static void save_settings(void) {
    persist_write_data(SETTINGS_PERSIST_KEY, &s_settings, sizeof(s_settings));
}

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
    int margin = PBL_IF_ROUND_ELSE(10, 6);
    s_outer_r  = min_dim / 2 - margin;
    s_step_r   = s_outer_r + 9;
    s_hour_len = s_outer_r * 55 / 94;
    s_min_len  = s_outer_r * 74 / 94;
}

static void update_step_state(void) {
    int32_t steps = 0;
#if defined(PBL_HEALTH)
    HealthServiceAccessibilityMask mask = health_service_metric_accessible(
        HealthMetricStepCount, time_start_of_today(), time(NULL));
    if (mask & HealthServiceAccessibilityMaskAvailable) {
        steps = (int32_t)health_service_sum_today(HealthMetricStepCount);
    }
#endif
    int goal = s_settings.step_goal > 0 ? s_settings.step_goal : STEP_GOAL_DEFAULT;
    int cycle = steps / goal;
    int lit   = (int32_t)(steps % goal) * 10 / goal;
    s_step_lit   = lit > 10 ? 10 : lit;
    s_step_cycle = cycle > 2 ? 2 : cycle;
}

static void update_date_state(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    s_day = t->tm_mday;  // 1–31, drawn in binary
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_unobstructed_bounds(layer);
    GPoint center = grect_center_point(&bounds);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

    // 12 o'clock dot — slightly bigger, red when Bluetooth disconnected
    graphics_context_set_fill_color(ctx, s_connected ? GColorWhite : GColorRed);
    graphics_fill_circle(ctx, point_on_circle(center, 0, s_outer_r), 4);

    // Battery dots (1–2 o'clock): 5 dots, 10° spacing, fills from 3-o'clock side upward
    // Each dot = 20% charge. Critical (≤20%): single dot near 3 o'clock turns red.
    for (int i = 0; i < 5; i++) {
        bool lit  = (i >= 5 - s_battery_dots);
        GColor color = !lit ? GColorDarkGray : (s_battery_critical ? GColorRed : GColorWhite);
        graphics_context_set_fill_color(ctx, color);
        graphics_fill_circle(ctx, point_on_circle(center, deg_to_trig(30 + i * 10), s_outer_r), 2);
    }

    // Step dots (4:30–7:30): 10 dots at 135°–225°, 10° spacing — outer step ring
    if (s_settings.show_step_dots) {
        GColor step_color = s_step_cycle == 0 ? GColorWhite
                          : s_step_cycle == 1  ? get_color(s_settings.over_goal_1_idx)
                          :                      get_color(s_settings.over_goal_2_idx);
        for (int i = 0; i < 10; i++) {
            bool lit = (i < s_step_lit);
            graphics_context_set_fill_color(ctx, lit ? step_color : GColorDarkGray);
            graphics_fill_circle(ctx, point_on_circle(center, deg_to_trig(135 + i * 10), s_step_r), 2);
        }
    }

    // Day-of-month dots (10–11 o'clock): 5 dots, binary encoding LSB→MSB left→right
    // Mirrors battery group: both groups span 30°–70° from 12 o'clock on their side.
    // Bit values: dot0=1 (290°, nearest 9), dot4=16 (330°, nearest 12)
    for (int i = 0; i < 5; i++) {
        bool lit = (s_day & (1 << i)) != 0;
        graphics_context_set_fill_color(ctx, lit ? GColorWhite : GColorDarkGray);
        graphics_fill_circle(ctx, point_on_circle(center, deg_to_trig(290 + i * 10), s_outer_r), 2);
    }

    // Clock hands
    int32_t hour_angle   = deg_to_trig(((t->tm_hour % 12) * 60 + t->tm_min) * 360 / (12 * 60));
    int32_t minute_angle = deg_to_trig(t->tm_min * 6);

    graphics_context_set_stroke_width(ctx, 10);
    graphics_context_set_stroke_color(ctx, get_color(s_settings.hour_color_idx));
    graphics_draw_line(ctx, center, point_on_circle(center, hour_angle, s_hour_len));
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_line(ctx, center, point_on_circle(center, minute_angle, s_min_len));
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_step_state();
    if (tick_time->tm_hour == 0 && tick_time->tm_min == 0) {
        update_date_state();
    }
    layer_mark_dirty(s_canvas);
}

static void battery_handler(BatteryChargeState state) {
    int dots      = (state.charge_percent + 19) / 20;  // 1–5 at 20% increments
    bool critical = (dots == 1);
    if (dots > 5) dots = 5;
    if (dots != s_battery_dots || critical != s_battery_critical) {
        s_battery_dots     = dots;
        s_battery_critical = critical;
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
    t = dict_find(iter, MESSAGE_KEY_StepGoal);
    if (t) s_settings.step_goal = t->value->int32;
    t = dict_find(iter, MESSAGE_KEY_VibrateOnDisconnect);
    if (t) s_settings.vibrate_disconnect = t->value->uint8 != 0;
    t = dict_find(iter, MESSAGE_KEY_ShowStepDots);
    if (t) s_settings.show_step_dots = t->value->uint8 != 0;
    t = dict_find(iter, MESSAGE_KEY_HourColor);
    if (t) s_settings.hour_color_idx = t->value->uint8;
    t = dict_find(iter, MESSAGE_KEY_OverGoalColor1);
    if (t) s_settings.over_goal_1_idx = t->value->uint8;
    t = dict_find(iter, MESSAGE_KEY_OverGoalColor2);
    if (t) s_settings.over_goal_2_idx = t->value->uint8;
    save_settings();
    update_step_state();
    layer_mark_dirty(s_canvas);
}

static void unobstructed_change(AnimationProgress progress, void *context) {
    Layer *window_layer = window_get_root_layer(s_window);
    compute_geometry(layer_get_unobstructed_bounds(window_layer));
    layer_mark_dirty(s_canvas);
}

static void prv_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    compute_geometry(layer_get_unobstructed_bounds(window_layer));
    s_canvas = layer_create(layer_get_bounds(window_layer));
    layer_set_update_proc(s_canvas, canvas_update_proc);
    layer_add_child(window_layer, s_canvas);
    unobstructed_area_service_subscribe(
        (UnobstructedAreaHandlers){ .change = unobstructed_change }, NULL);
}

static void prv_window_unload(Window *window) {
    unobstructed_area_service_unsubscribe();
    layer_destroy(s_canvas);
}

static void prv_init(void) {
    load_settings();

    BatteryChargeState bat = battery_state_service_peek();
    int dots           = (bat.charge_percent + 19) / 20;
    s_battery_dots     = dots > 5 ? 5 : dots;
    s_battery_critical = (s_battery_dots == 1);
    s_connected        = connection_service_peek_pebble_app_connection();

    update_step_state();
    update_date_state();

    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers){
        .load   = prv_window_load,
        .unload = prv_window_unload,
    });
    window_stack_push(s_window, true);

    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    battery_state_service_subscribe(battery_handler);
    connection_service_subscribe((ConnectionHandlers){
        .pebble_app_connection_handler = connection_handler,
    });
    app_message_register_inbox_received(inbox_received);
    app_message_open(128, 64);
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
