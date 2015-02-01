/*

	 Handwritten watch

	 A digital watch with large handwritten digits.

 */

#include <pebble.h>
#include "autoconfig.h"

//==============================================================================
// CONSTANTS
//==============================================================================

//
// There's only enough memory to load about 6 of 10 required images
// so we have to swap them in & out...
//
// We have one "slot" per digit location on screen.
//
// Because layers can only have one parent we load a digit for each
// slot--even if the digit image is already in another slot.
//
// Slot on-screen layout:
//     0
//     1
//     2
#define INVERTEDCOLOR 1

#define TOTAL_IMAGE_SLOTS 3

#define NUMBER_OF_IMAGES 24

#define EMPTY_SLOT -1

//==============================================================================
// PROPERTIES
//==============================================================================

static Window *window;

/**
 * These images are 144 x 42 pixels (i.e. a quarter of the display),
 * black and white with the text adjusted to the left.
 */
const int IMAGE_RESOURCE_IDS[NUMBER_OF_IMAGES] = {
	RESOURCE_ID_IMAGE_NUM_0, RESOURCE_ID_IMAGE_NUM_1, RESOURCE_ID_IMAGE_NUM_2,
	RESOURCE_ID_IMAGE_NUM_3, RESOURCE_ID_IMAGE_NUM_4, RESOURCE_ID_IMAGE_NUM_5,
	RESOURCE_ID_IMAGE_NUM_6, RESOURCE_ID_IMAGE_NUM_7, RESOURCE_ID_IMAGE_NUM_8,
	RESOURCE_ID_IMAGE_NUM_9, RESOURCE_ID_IMAGE_NUM_10, RESOURCE_ID_IMAGE_NUM_11,
	RESOURCE_ID_IMAGE_NUM_12, RESOURCE_ID_IMAGE_NUM_13, RESOURCE_ID_IMAGE_NUM_14,
	RESOURCE_ID_IMAGE_NUM_15, RESOURCE_ID_IMAGE_NUM_16, RESOURCE_ID_IMAGE_NUM_17,
	RESOURCE_ID_IMAGE_NUM_18, RESOURCE_ID_IMAGE_NUM_19, RESOURCE_ID_IMAGE_NUM_20,
	RESOURCE_ID_IMAGE_NUM_30, RESOURCE_ID_IMAGE_NUM_40, RESOURCE_ID_IMAGE_NUM_50
};

static GBitmap *images[TOTAL_IMAGE_SLOTS];
static BitmapLayer *image_layers[TOTAL_IMAGE_SLOTS];

/**
 * The state is either "empty" or the digit of the image currently in
 * the slot--which was going to be used to assist with de-duplication
 * but we're not doing that due to the one parent-per-layer
 * restriction mentioned above.
 */
static int image_slot_state[TOTAL_IMAGE_SLOTS] = {
	EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT
};

//==============================================================================
// METHODS
//==============================================================================

/**
 * Loads the digit image from the application's resources and
 * displays it on-screen in the correct location.
 * Each slot is a quarter of the screen.
 */
static void load_digit_image_into_slot(int slot_number, int digit_value)
{
	// TODO: Signal these error(s)?
	if ((slot_number < 0) || (slot_number >= TOTAL_IMAGE_SLOTS)) {
		return;
	}

	if ((digit_value < 0) || (digit_value > 59)) {
		return;
	}

	if (image_slot_state[slot_number] != EMPTY_SLOT) {
		return;
	}

	image_slot_state[slot_number] = digit_value;

	images[slot_number] = gbitmap_create_with_resource(IMAGE_RESOURCE_IDS[digit_value]);

	GRect frame = (GRect) {
		.origin = { 0 , 21 + (slot_number) * (168/4) },
		.size = images[slot_number]->bounds.size
	};

	BitmapLayer *bitmap_layer = bitmap_layer_create(frame);

	image_layers[slot_number] = bitmap_layer;

	// Here we decide if the watch will have inverted colors
	if (getInverted()) {
		bitmap_layer_set_compositing_mode(bitmap_layer, GCompOpAssignInverted);
	}

	bitmap_layer_set_bitmap(bitmap_layer, images[slot_number]);

	Layer *window_layer = window_get_root_layer(window);

	layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer));
}

/**
 * Removes the digit from the display and unloads the image resource
 * to free up RAM.
 * Can handle being called on an already empty slot.
 */
static void unload_digit_image_from_slot(int slot_number)
{
	if (image_slot_state[slot_number] == EMPTY_SLOT) {
		return;
	}

	layer_remove_from_parent(bitmap_layer_get_layer(image_layers[slot_number]));
	bitmap_layer_destroy(image_layers[slot_number]);
	gbitmap_destroy(images[slot_number]);

	image_slot_state[slot_number] = EMPTY_SLOT;
}

/**
 * Displays a text value between 1 and 19, 20, 30, 40, 50, 0(o'clock) on screen.
 */
static void display_value(unsigned short value, unsigned short slot_number)
{
	// value = value % 100; // Maximum of two digits per row.

	// Column order is: | Column 0 | Column 1 |
	// (We process the columns in reverse order because that makes
	// extracting the digits from the value easier.)

	// int slot_number = (row_number * 2) + column_number;

	unload_digit_image_from_slot(slot_number);

	load_digit_image_into_slot(slot_number, value);

	//value = value / 10;
}

static unsigned short get_display_hour(unsigned short hour)
{
	//if (clock_is_24h_style()) {
	// return hour;
	//}

	unsigned short display_hour = hour % 12;

	// Converts "0" to "12"
	return display_hour ? display_hour : 12;
}

/**
 * Display hour for a given time to the screen.
 *
 * @param TimeUnits tick_time
 * @return void
 */
static void display_hour(struct tm *tick_time)
{
	int theHour = get_display_hour(tick_time->tm_hour);

	if (image_slot_state[0] != theHour) {
		display_value(theHour, 0);
	}
}

/**
 * Display minutes for a given time to the screen.
 *
 * @param TimeUnits tick_time
 * @return void
 */
static void display_minutes(struct tm *tick_time)
{
	// TODO-AD: To refactor to make it more readable. <xel1045@gmail.com>
	int minutes = tick_time->tm_sec;

	if (minutes <= 20) {
		display_value(minutes, 1);
		unload_digit_image_from_slot(2);
	} else if (minutes % 10 == 0) {
		display_value((minutes-20)/10+20, 1);
		unload_digit_image_from_slot(2);
	} else if (minutes % 10 != 0) {
		display_value( ((((minutes-minutes%10))-20)/10+20), 1);
		display_value((minutes%10), 2);
	}
}

/**
 * Display a time value to the screen.
 *
 * @param TimeUnits tick_time
 * @return void
 */
static void display_time(struct tm *tick_time)
{
	display_hour(tick_time);
	display_minutes(tick_time);
}

/**
 * Gets the current local time and display it.
 *
 * @return void
 */
static void display_current_time()
{
	time_t now = time(NULL);
	struct tm *tick_time = localtime(&now);

	display_time(tick_time);
}

/**
 * Subscribe to the tick timer event service. This handler gets called on every
 * requested unit change.
 *
 * @param TimeUnits    tick_time
 * @param TickHandler  units_changed
 * @return void
 */
static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed)
{
	if (units_changed & HOUR_UNIT) {
		display_hour(tick_time);
	}

	//if (units_changed & MINUTE_UNIT) {
		display_minutes(tick_time);
	//}
}


static void set_color(bool inverse)
{
	GColor backgroundColor = inverse ? GColorWhite : GColorBlack;

	window_set_background_color(window, GColorBlack);
}


static void in_received_handler(DictionaryIterator *iter, void *context)
{
	// Let Pebble Autoconfig handle received settings
	autoconfig_in_received_handler(iter, context);

	// here the new settings are available
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting updated. Inverted: %d", getInverted());

	set_color(getInverted());

	// REPAINT THE DIGITS WITH THE PROPER COLOR
	for (int i = 0; i < TOTAL_IMAGE_SLOTS; i++) {
		unload_digit_image_from_slot(i);
	}

	display_current_time();
}

static void init(void)
{
	// call autoconf init (load previous settings and register app message handlers)
	autoconfig_init();

	// here the previous settings are already loaded
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Settings read. Inverted: %d", getInverted());

	// Register our custom receive handler which in turn will call Pebble Autoconfigs receive handler
	app_message_register_inbox_received(in_received_handler);

	window = window_create();
	window_stack_push(window, true);

	// Depending on option, the background will be black or white
	set_color(getInverted());

	// Avoids a blank screen on watch start.
	display_current_time();

	tick_timer_service_subscribe(SECOND_UNIT, handle_minute_tick);
}

static void deinit()
{
	for (int i = 0; i < TOTAL_IMAGE_SLOTS; i++) {
		unload_digit_image_from_slot(i);
	}

	// Let Pebble Autoconfig write settings to Pebbles persistant memory
	autoconfig_deinit();

	window_destroy(window);
}

int main(void)
{
	init();
	app_event_loop();
	deinit();
}
