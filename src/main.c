#include "pebble.h"

#define FULL_W 144
#define FULL_H 168
#define GRECT_FULL_WINDOW GRect(0,0,FULL_W,FULL_H)

static Window* window;
static GBitmap *background_image_container;

static Layer *minute_display_layer;
static Layer *hour_display_layer;
static Layer *center_display_layer;

#define DATE_WIDTH 32
#define DATE_HEIGHT 48

static TextLayer *date_layer;
static char date_text[] = "Wed\n13";

static bool bt_ok = false;
static uint8_t battery_level;
static bool battery_plugged;

static GBitmap *icon_battery;
static GBitmap *icon_battery_charge;
static GBitmap *icon_bt;

#define BT_BAT_WIDTH 24
#define BT_BAT_HEIGHT 28

static Layer *ind_layer;
static Layer *bt_battery_layer;
static Layer *battery_layer;
static Layer *bt_layer;

// following macros define margins on side of the screen
// used by digits. In fact they are assymtetric 35 and 28 
// left and right, and 25 and 26 top and bottom. But we take
// max of both to make things even.
#define X_MARGIN 35
#define Y_MARGIN 26

#define Q_WIDTH  ((FULL_W-(2*X_MARGIN))/2)
#define Q_HEIGHT ((FULL_H-(2*Y_MARGIN))/2)

// Bounding boxes for quadrant rectangles
static struct GRect q_rects[4] = {
    {{FULL_W/2, Y_MARGIN} , {Q_WIDTH, Q_HEIGHT}},
    {{FULL_W/2, FULL_H/2} , {Q_WIDTH, Q_HEIGHT}},
    {{X_MARGIN, FULL_H/2} , {Q_WIDTH, Q_HEIGHT}},
    {{X_MARGIN, Y_MARGIN} , {Q_WIDTH, Q_HEIGHT}}
};

static Layer *background_layer;
static Layer *window_layer;

const GPathInfo MINUTE_HAND_PATH_POINTS = { 4, (GPoint[] ) { { -4, 15 },
                                                             { 4, 15 }, { 4, -70 }, { -4, -70 }, } };

const GPathInfo HOUR_HAND_PATH_POINTS = { 4, (GPoint[] ) { { -4, 15 },
                                                           { 4, 15 }, { 4, -50 }, { -4, -50 }, } };

static GPath *hour_hand_path;
static GPath *minute_hand_path;

int qudrantFromHours(int h) { return (h%12)/3; }
int quandrantFromMinutes(int m) {  return m/15; }

GRect quadrant_fit(int q, int16_t w, int16_t h)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG,"qfit <-: q=%d w=%d h=%d", q,w,h);
    GRect *qr = q_rects+q;
    APP_LOG(APP_LOG_LEVEL_DEBUG,"qfit (q): (%d %d) (%d %d)", qr->origin.x, qr->origin.y, qr->size.w, qr->size.h);
    int16_t fx = qr->origin.x + (qr->size.w - w)/2;
    int16_t fy = qr->origin.y + (qr->size.h - h)/2;
    APP_LOG(APP_LOG_LEVEL_DEBUG,"qfit ->: (%d %d) (%d %d)", fx, fy, w, h);
    return GRect(fx, fy, w, h);
}

struct qpair
{
    int bat;
    int date;
};

// There are certain aesthetical considerations in chosing quadrants,
// which are difficult to formalize as an alogorith. We hardcoded
// laoyout preferences instead.
static struct qpair preferred_quadrants[16] = {
    {-1,-1},
    {3,2},
    {3,2},
    {3,2},
    {0,1},
    {3,1},
    {3,0},
    {-1,-1},
    {0,1},
    {2,1},
    {0,2},
    {-1,-1},
    {0,1},
    {-1,-1},
    {-1,-1},
    {-1,-1}
};

static struct qpair ind_q;


struct qpair find_free_quandrants()
{
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
    // find out which quadrants are taken:
    int qh = qudrantFromHours(t->tm_hour);
    int qm = quandrantFromMinutes(t->tm_min);

    int i = (1<<qh) | (1<<qm);
    APP_LOG(APP_LOG_LEVEL_DEBUG,"used quadrants: h=%d m=%d index=%d", qh, qm, i);
    APP_LOG(APP_LOG_LEVEL_DEBUG,"prefered quadrants: %d,%d", preferred_quadrants[i].bat, preferred_quadrants[i].date);

    return preferred_quadrants[i];
}


void chase_indicators()
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "chase_indicators: called");
    struct qpair new_q = find_free_quandrants();
    if(new_q.bat != ind_q.bat)
    {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "chase_indicators: moving bat from %d to %d",
                ind_q.bat, new_q.bat);
        ind_q.bat = new_q.bat;
        layer_set_frame(bt_battery_layer, quadrant_fit(ind_q.bat, BT_BAT_WIDTH, BT_BAT_HEIGHT));
        //layer_mark_dirty(ind_layer);
    }

    if(new_q.date != ind_q.date)
    {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "chase_indicators: moving date from %d to %d",
                ind_q.date, new_q.date);
        ind_q.date = new_q.date;
        Layer *l = text_layer_get_layer(date_layer);
        layer_set_frame(l, quadrant_fit(ind_q.date, DATE_WIDTH, DATE_HEIGHT));
        //layer_mark_dirty(ind_layer);
    }
}

void ind_layer_update_callback(Layer *me, GContext* ctx) {
	(void) me;

    APP_LOG(APP_LOG_LEVEL_DEBUG,"ind_layer_update_callback: called");
    chase_indicators();
}

void center_display_layer_update_callback(Layer *me, GContext* ctx) {
	(void) me;

	GPoint center = grect_center_point(&GRECT_FULL_WINDOW);
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, center, 4);
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_circle(ctx, center, 3);
}

void minute_display_layer_update_callback(Layer *me, GContext* ctx) {
	(void) me;

    APP_LOG(APP_LOG_LEVEL_DEBUG,"minute_layer_update_callback: called");
	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	unsigned int angle = t->tm_min * 6 + t->tm_sec / 10;

	gpath_rotate_to(minute_hand_path, (TRIG_MAX_ANGLE / 360) * angle);

	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_context_set_stroke_color(ctx, GColorBlack);

	gpath_draw_filled(ctx, minute_hand_path);
	gpath_draw_outline(ctx, minute_hand_path);
}

void hour_display_layer_update_callback(Layer *me, GContext* ctx) {
	(void) me;

    APP_LOG(APP_LOG_LEVEL_DEBUG,"hour_layer_update_callback: called");
	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	unsigned int angle = t->tm_hour * 30 + t->tm_min / 2;

	gpath_rotate_to(hour_hand_path, (TRIG_MAX_ANGLE / 360) * angle);
    
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_context_set_stroke_color(ctx, GColorBlack);

	gpath_draw_filled(ctx, hour_hand_path);
	gpath_draw_outline(ctx, hour_hand_path);
}

void draw_date()
{
	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	strftime(date_text, sizeof(date_text), "%a\n%d", t);
	text_layer_set_text(date_layer, date_text);
}

/*
 * Battery icon callback handler
 */
void battery_layer_update_callback(Layer *layer, GContext *ctx) {

    graphics_context_set_compositing_mode(ctx, GCompOpAssign);

    if (!battery_plugged) {
        graphics_draw_bitmap_in_rect(ctx, icon_battery, GRect(0, 0, 24, 12));
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_fill_color(ctx, GColorWhite);
        graphics_fill_rect(ctx, GRect(7, 4, (uint8_t)((battery_level / 100.0) * 11.0), 4), 0, GCornerNone);
    } else {
        graphics_draw_bitmap_in_rect(ctx, icon_battery_charge, GRect(0, 0, 24, 12));
    }
}



void battery_state_handler(BatteryChargeState charge)
{
	battery_level = charge.charge_percent;
	battery_plugged = charge.is_plugged;
	layer_mark_dirty(battery_layer);
}

/*
 * Bluetooth icon callback handler
 */
void bt_layer_update_callback(Layer *layer, GContext *ctx) {
    if (bt_ok)
        graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    else
        graphics_context_set_compositing_mode(ctx, GCompOpClear);
    graphics_draw_bitmap_in_rect(ctx, icon_bt, GRect(0, 0, 9, 12));
}

void bt_connection_handler(bool connected) {
	bt_ok = connected;
	layer_mark_dirty(bt_layer);
}

void draw_background_callback(Layer *layer, GContext *ctx) {
	graphics_context_set_compositing_mode(ctx, GCompOpAssign);
	graphics_draw_bitmap_in_rect(ctx, background_image_container,
                                 GRECT_FULL_WINDOW);
}

void init()
{
	// Window
	window = window_create();
	window_stack_push(window, true /* Animated */);
	window_layer = window_get_root_layer(window);

	// Background image
	background_image_container = gbitmap_create_with_resource(
        RESOURCE_ID_IMAGE_BACKGROUND);
	background_layer = layer_create(GRECT_FULL_WINDOW);
	layer_set_update_proc(background_layer, &draw_background_callback);
	layer_add_child(window_layer, background_layer);


    // Indicators layer
    ind_layer = layer_create(GRECT_FULL_WINDOW);

    // initial position of date, and bt/battery indicators
    ind_q = find_free_quandrants();

	// Date setup
	date_layer = text_layer_create(quadrant_fit(ind_q.date, DATE_WIDTH, DATE_HEIGHT));
	text_layer_set_text_color(date_layer, GColorWhite);
	text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
	text_layer_set_background_color(date_layer, GColorClear);
	text_layer_set_font(date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(ind_layer, text_layer_get_layer(date_layer));

	draw_date();

	// Status setup
	icon_battery = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_ICON);
	icon_battery_charge = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_CHARGE);
	icon_bt = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH);

    bt_battery_layer = layer_create(quadrant_fit(ind_q.bat, BT_BAT_WIDTH, BT_BAT_HEIGHT));
    
	BatteryChargeState initial = battery_state_service_peek();
	battery_level = initial.charge_percent;
	battery_plugged = initial.is_plugged;
	battery_layer = layer_create(GRect(0,0,24,12)); //24*12
	layer_add_child(bt_battery_layer, battery_layer);
	layer_set_update_proc(battery_layer, &battery_layer_update_callback);

	bt_ok = bluetooth_connection_service_peek();
	bt_layer = layer_create(GRect(8,16,9,12)); //9*12
	layer_add_child(bt_battery_layer, bt_layer);
	layer_set_update_proc(bt_layer, &bt_layer_update_callback);

	layer_set_update_proc(ind_layer, &ind_layer_update_callback);
	layer_add_child(ind_layer, bt_battery_layer);
	layer_add_child(window_layer, ind_layer);
    
	// Hands setup
	hour_display_layer = layer_create(GRECT_FULL_WINDOW);
	layer_set_update_proc(hour_display_layer,
                          &hour_display_layer_update_callback);
	layer_add_child(window_layer, hour_display_layer);

	hour_hand_path = gpath_create(&HOUR_HAND_PATH_POINTS);
	gpath_move_to(hour_hand_path, grect_center_point(&GRECT_FULL_WINDOW));

	minute_display_layer = layer_create(GRECT_FULL_WINDOW);
	layer_set_update_proc(minute_display_layer,
                          &minute_display_layer_update_callback);
	layer_add_child(window_layer, minute_display_layer);

	minute_hand_path = gpath_create(&MINUTE_HAND_PATH_POINTS);
	gpath_move_to(minute_hand_path, grect_center_point(&GRECT_FULL_WINDOW));

	center_display_layer = layer_create(GRECT_FULL_WINDOW);
	layer_set_update_proc(center_display_layer,
                          &center_display_layer_update_callback);
	layer_add_child(window_layer, center_display_layer);
}

void deinit()
{
	gbitmap_destroy(background_image_container);
	gbitmap_destroy(icon_battery);
	gbitmap_destroy(icon_battery_charge);
	gbitmap_destroy(icon_bt);
	text_layer_destroy(date_layer);
	layer_destroy(minute_display_layer);
	layer_destroy(hour_display_layer);
	layer_destroy(center_display_layer);
	layer_destroy(battery_layer);
	layer_destroy(bt_layer);
	layer_destroy(bt_battery_layer);
	layer_destroy(ind_layer);
	layer_destroy(background_layer);
	layer_destroy(window_layer);

	gpath_destroy(hour_hand_path);
	gpath_destroy(minute_hand_path);
}

void handle_tick(struct tm *tick_time, TimeUnits units_changed)
{
    layer_mark_dirty(minute_display_layer);
    if(tick_time->tm_min == 0)
    {
        layer_mark_dirty(hour_display_layer);
        if (tick_time->tm_hour == 0)
            draw_date();
    }
    layer_mark_dirty(ind_layer);
}

int main(void)
{
	init();
	tick_timer_service_subscribe(MINUTE_UNIT, &handle_tick);
	bluetooth_connection_service_subscribe(&bt_connection_handler);
	battery_state_service_subscribe	(&battery_state_handler);
	app_event_loop();
	deinit();
}


