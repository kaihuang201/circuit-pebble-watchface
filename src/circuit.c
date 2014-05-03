#include "pebble.h"

static Window* window;
static TextLayer* time_text_layer;
static TextLayer* day_text_layer;
static TextLayer* date_text_layer;
static TextLayer* extra_text_layer;

static GBitmap* image;
static GBitmap* image_power;
static GBitmap* image_charging;

static Layer* background_layer;
static Layer* battery_display_layer;
static Layer* bluetooth_display_layer;

static char time_text[6];
static char day_text[5];
static char date_text[8];
static char extra_text[18];

static bool bluetooth;
static BatteryChargeState battery_charge;

static GFont font_big, font_small;

static GPath *battery_segment_path;
static const GPathInfo BATTERY_PATH_POINTS = {
  5, (GPoint []) { {0, 0}, {3, -15}, {1, -16}, {-1, -16}, {-4, -15}, }
};


static void background_layer_update(Layer *layer_to_update, GContext* ctx) {
	GRect bounds = image->bounds;
	graphics_draw_bitmap_in_rect(ctx, image, (GRect) { .origin = { 0, 0 }, .size = bounds.size });
}

static void battery_display_layer_update(Layer *layer_to_update, GContext* ctx) {
	GPoint battery_center = GPoint(30,54);

	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, battery_center, 16);

	graphics_context_set_fill_color(ctx, GColorWhite);
	
	int angle = (36 * battery_charge.charge_percent)/10;
	for(; angle < 355; angle += 6) {
		gpath_rotate_to(battery_segment_path, (TRIG_MAX_ANGLE / 360) * angle);
		gpath_draw_filled(ctx, battery_segment_path);
	}
	
	
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_circle(ctx, battery_center, 9);

	graphics_context_set_stroke_color(ctx, GColorWhite);
	graphics_draw_circle(ctx, battery_center, 15);

	//draw charging or power icon
	if (battery_charge.is_charging) {
		graphics_draw_bitmap_in_rect(ctx, image_charging,
				(GRect) { .origin = {24, 48}, .size = {13, 13} });
	} else {
		graphics_draw_bitmap_in_rect(ctx, image_power,
				(GRect) { .origin = {24, 48}, .size = {13, 13} });
	}
}

void handle_battery_state(BatteryChargeState charge) {
	battery_charge = charge;
	layer_mark_dirty(battery_display_layer);
}


static void bluetooth_display_layer_update(Layer *layer_to_update, GContext* ctx) {
	graphics_context_set_fill_color(ctx, GColorWhite);
	if (bluetooth==false) graphics_fill_rect(ctx,
			(GRect) {.origin = {0, 0}, .size = {10, 14}},
			0,
			GCornersAll);
}

void handle_bluetooth_state(bool connected) {
	bluetooth = connected;
	layer_mark_dirty(bluetooth_display_layer);
}
//static int kk = 0;
void handle_minute_tick(struct tm* tick_time, TimeUnits units_changed) {

	if (!tick_time) {
		time_t now = time(NULL);
		tick_time = localtime(&now);
	}

	strftime(time_text, sizeof(time_text), "%H%M", tick_time);//"%H%M"
	text_layer_set_text(time_text_layer, time_text);

	strftime(day_text, sizeof(day_text), "%a", tick_time);
	int i = 0;
	char c;
	while (day_text[i]) {
		c = day_text[i];
		if (c>96 && c<123)
			c -= 32;
		day_text[i] = c;
		i++;
	}
	text_layer_set_text(day_text_layer, day_text);

	strftime(date_text, sizeof(date_text), "%b %e", tick_time);
	i = 0;
	while (date_text[i]) {
		c = date_text[i];
		if (c>96 && c<123)
			c -= 32;
		date_text[i] = c;
		i++;
	}
	text_layer_set_text(date_text_layer, date_text);

	strftime(extra_text, sizeof(extra_text), "Week %V of %Y", tick_time);
	text_layer_set_text(extra_text_layer, extra_text);

	//text_layer_set_text(additional_text_layer, date_text);
	layer_mark_dirty(battery_display_layer);
	layer_mark_dirty(background_layer);
}

void deinit() {
	tick_timer_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	battery_state_service_unsubscribe();

	gbitmap_destroy(image);
	gbitmap_destroy(image_power);
	gbitmap_destroy(image_charging);

	layer_destroy(background_layer);
	layer_destroy(battery_display_layer);
	layer_destroy(bluetooth_display_layer);
	
	text_layer_destroy(time_text_layer);
	text_layer_destroy(day_text_layer);
	text_layer_destroy(date_text_layer);
	text_layer_destroy(extra_text_layer);

	window_destroy(window);
}

void init() {
	window = window_create();
	window_stack_push(window, false);

	image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
	image_power = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_POWER);
	image_charging = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING);

	font_big = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_42));
	font_small= fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_12));

	Layer* window_layer = window_get_root_layer(window);

	GRect bounds = layer_get_frame(window_layer);
	background_layer = layer_create(bounds);
	layer_set_update_proc(background_layer, background_layer_update);
	layer_add_child(window_layer, background_layer);

	time_text_layer = text_layer_create(GRect(0, 85, 125, 50));
	text_layer_set_text_alignment(time_text_layer, GTextAlignmentRight);
	text_layer_set_text_color(time_text_layer, GColorWhite);
	text_layer_set_background_color(time_text_layer, GColorClear);
	text_layer_set_font(time_text_layer, font_big);
	layer_add_child(window_layer, text_layer_get_layer(time_text_layer));

	day_text_layer = text_layer_create(GRect(0, 8, 135, 28));
	text_layer_set_text_alignment(day_text_layer, GTextAlignmentRight);
	text_layer_set_text_color(day_text_layer, GColorWhite);
	text_layer_set_background_color(day_text_layer, GColorClear);
	text_layer_set_font(day_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	layer_add_child(window_layer, text_layer_get_layer(day_text_layer));

	date_text_layer = text_layer_create(GRect(0, 36, 135, 28));
	text_layer_set_text_alignment(date_text_layer, GTextAlignmentRight);
	text_layer_set_text_color(date_text_layer, GColorWhite);
	text_layer_set_background_color(date_text_layer, GColorClear);
	text_layer_set_font(date_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	layer_add_child(window_layer, text_layer_get_layer(date_text_layer));

	extra_text_layer = text_layer_create(GRect(15, 140, 135, 28));
	//text_layer_set_text_alignment(extra_text_layer, GTextAlignmentRight);
	text_layer_set_text_color(extra_text_layer, GColorWhite);
	text_layer_set_background_color(extra_text_layer, GColorClear);
	text_layer_set_font(extra_text_layer, font_small);
	layer_add_child(window_layer, text_layer_get_layer(extra_text_layer));

	battery_segment_path = gpath_create(&BATTERY_PATH_POINTS);
	gpath_move_to(battery_segment_path, GPoint(30,54));

	battery_display_layer = layer_create((GRect) {.origin = {0,0}, .size = bounds.size});
	layer_set_update_proc(battery_display_layer, battery_display_layer_update);
	layer_add_child(window_layer, battery_display_layer);

	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
	handle_minute_tick(NULL, MINUTE_UNIT);

	//bluetooth
	bluetooth_display_layer = layer_create((GRect) {.origin = {55, 22}, .size = {10, 14}});
	layer_set_update_proc(bluetooth_display_layer, bluetooth_display_layer_update);
	layer_add_child(window_layer, bluetooth_display_layer);
	handle_bluetooth_state(bluetooth_connection_service_peek());
	bluetooth_connection_service_subscribe(handle_bluetooth_state);

	//battery state
	handle_battery_state(battery_state_service_peek());
	battery_state_service_subscribe(handle_battery_state);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
