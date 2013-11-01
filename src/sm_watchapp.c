#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
 
#include "globals.h"
 
 
#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }
 
PBL_APP_INFO(MY_UUID,
             "SmartStatusAnalog", "Olivier Pasco",
             1, 0, /* App version */
             RESOURCE_ID_APP_ICON,
             APP_INFO_STANDARD_APP);
 
#define STRING_LENGTH 255
#define NUM_WEATHER_IMAGES	8
#define SWAP_BOTTOM_LAYER_INTERVAL 15000
#define GPS_UPDATE_INTERVAL 60000
#define RECOVERY_ATTEMPT_INTERVAL 10000
#define DEFAULT_SONG_UPDATE_INTERVAL 5000
#define MINUTE_INTERVAL 60000

#define TIMER_COOKIE_WEATHER 1
#define TIMER_COOKIE_CALANDAR 2
#define TIMER_COOKIE_MUSIC 3
#define TIMER_COOKIE_LAYERSWAP 4
#define TIMER_COOKIE_NEXTDAYWEATHER 5
#define TIMER_COOKIE_GPS 6
#define TIMER_COOKIE_CONNECTIONRECOVER 7
#define TIMER_COOKIE_MINUTE 8

typedef enum {MUSIC_LAYER, LOCATION_LAYER, NUM_LAYERS} AnimatedLayers;
 
 
static AppMessageResult sm_message_out_get(DictionaryIterator **iter_out);
static void reset_sequence_number();
// char* int_to_str(int num, char *outbuf);
static void sendCommand(int key);
static void sendCommandInt(int key, int param);
static void rcv(DictionaryIterator *received, void *context);
static void dropped(void *context, AppMessageResult reason);
static void select_up_handler(ClickRecognizerRef recognizer, Window *window);
static void select_down_handler(ClickRecognizerRef recognizer, Window *window);
static void up_single_click_handler(ClickRecognizerRef recognizer, Window *window);
static void down_single_click_handler(ClickRecognizerRef recognizer, Window *window);
static void config_provider(ClickConfig **config, Window *window);
static void battery_layer_update_callback(Layer *me, GContext* ctx);
static void handle_status_appear(Window *window);
static void handle_status_disappear(Window *window);
static void handle_init(AppContextRef ctx);
static void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t);
static void handle_deinit(AppContextRef ctx);  
static void reset();   
     
AppContextRef g_app_context;
 
 
static Window window;
static PropertyAnimation ani_out, ani_in;
 
static Layer animated_layer[NUM_LAYERS], weather_layer;
static Layer battery_layer, battery_ind_layer, calendar_layer;
 
static TextLayer text_date_layer, text_time_layer;
 
static TextLayer text_weather_cond_layer, text_weather_temp_layer, text_weather_tomorrow_temp_layer, text_battery_layer;
static TextLayer calendar_date_layer, calendar_text_layer, text_status_layer;
static TextLayer music_artist_layer, music_song_layer, location_street_layer;
  
static BitmapLayer background_image, weather_image, weather_tomorrow_image, battery_image_layer;
 
static int32_t active_layer;
static int32_t updateGPSInterval = GPS_UPDATE_INTERVAL;
static bool connected = 0;
static bool inTimeOut = 0;
static bool inGPSUpdate = 0;
static bool sending = 0;
 
static char string_buffer[STRING_LENGTH], location_street_str[STRING_LENGTH], appointment_time[15];
static char weather_cond_str[STRING_LENGTH], weather_tomorrow_temp_str[STRING_LENGTH], weather_temp_str[5];
static int32_t weather_img, weather_tomorrow_img, batteryPercent;
 
static char calendar_date_str[STRING_LENGTH], calendar_text_str[STRING_LENGTH];
static char music_artist_str[STRING_LENGTH], music_title_str[STRING_LENGTH];
 
 
HeapBitmap battery_image;
HeapBitmap weather_status_small_imgs[NUM_WEATHER_IMAGES];
 
static AppTimerHandle timerUpdateCalendar = 0;
static AppTimerHandle timerUpdateWeather = 0;
static AppTimerHandle timerUpdateMusic = 0;
static AppTimerHandle timerSwapBottomLayer = 0;
static AppTimerHandle timerUpdateWeatherForecast = 0;
static AppTimerHandle timerUpdateGps = 0;
static AppTimerHandle timerRecoveryAttempt = 0;
static AppTimerHandle timerMinutes = 0;
 
const int WEATHER_SMALL_IMG_IDS[] = {
  RESOURCE_ID_IMAGE_SUN_SMALL,
  RESOURCE_ID_IMAGE_RAIN_SMALL,
  RESOURCE_ID_IMAGE_CLOUD_SMALL,
  RESOURCE_ID_IMAGE_SUN_CLOUD_SMALL,
  RESOURCE_ID_IMAGE_FOG_SMALL,
  RESOURCE_ID_IMAGE_WIND_SMALL,
  RESOURCE_ID_IMAGE_SNOW_SMALL,
  RESOURCE_ID_IMAGE_THUNDER_SMALL
};
 
 
 
static uint32_t s_sequence_number = 0xFFFFFFFE;
 
/* Convert letter to digit */
uint32_t letter2digit(char letter) {
    if((letter >= 48) && (letter <=57)) {
        return letter - 48;
    }
     
    return -1;
}
 
/* Convert string to number */
static uint32_t string2number(char *string) {
    int32_t result = 0;
    int32_t offset = strlen(string) - 1;
    int32_t digit = -1;
    int32_t unit = 1;
    int8_t letter;  
 
    for(unit = 1; offset >= 0; unit = unit * 10) {
        letter = string[offset];
        digit = letter2digit(letter);
        if(digit == -1) return -1;
        result = result + (unit * digit);
        offset--;
    }
     
    return result;
}
 
/* Convert time string ("HH:MM") to number of minutes */
static uint32_t timestr2minutes(char *timestr) {
	static char hourStr[3], minStr[3];
	int32_t hour, min;
	int8_t hDigits = 2;

	if(timestr[1] == ':') hDigits = 1;
	
	strncpy(hourStr, timestr, hDigits);
	strncpy(minStr, timestr+hDigits+1, 2);
	
	hour = string2number(hourStr);
	if(hour == -1) return -1;
	
	min = string2number(minStr);
	if(min == -1) return -1;
	
	return min + (hour * 60);
}
 
static void apptDisplay() {
	int32_t apptInMinutes, timeInMinutes;
	static char date_time_for_appt[] = "00/00 00:00";
	PblTm t;
	
	get_time(&t);
	
	string_format_time(date_time_for_appt, sizeof(date_time_for_appt), "%m/%d", &t);
	
	if(strncmp(date_time_for_appt, appointment_time, 5) != 0) {
		layer_set_hidden(&calendar_layer, 1);
		return;
	}

	/* Manage appoitment notification */
	apptInMinutes = timestr2minutes(appointment_time + 6);
	if(apptInMinutes >= 0) {
		timeInMinutes = (t.tm_hour * 60) + t.tm_min;
		if(apptInMinutes < timeInMinutes) {
			snprintf(date_time_for_appt, 11, "%d min in", (int)(timeInMinutes - apptInMinutes));
			text_layer_set_text(&calendar_date_layer, date_time_for_appt); 	
			layer_set_hidden(&calendar_layer, 0);  	
		}
		if(apptInMinutes > timeInMinutes) {
			if(((apptInMinutes - timeInMinutes) / 60) > 0) {
				snprintf(date_time_for_appt, 11, "In %dh %dm", 
						 (int)((apptInMinutes - timeInMinutes) / 60),
						 (int)((apptInMinutes - timeInMinutes) % 60));
			} else {
				snprintf(date_time_for_appt, 11, "In %d min", (int)(apptInMinutes - timeInMinutes));
			}
			text_layer_set_text(&calendar_date_layer, date_time_for_appt); 	
			layer_set_hidden(&calendar_layer, 0);  	
		}
		if(apptInMinutes == timeInMinutes) {
			text_layer_set_text(&calendar_date_layer, "Now!"); 	
			layer_set_hidden(&calendar_layer, 0);  	
			vibes_double_pulse();
		}
		if((apptInMinutes >= timeInMinutes) && ((apptInMinutes - timeInMinutes) == 15)) {
			vibes_short_pulse();
		}
	}
	
	layer_set_hidden(&calendar_layer, 0);
}
 
// Analog watch code.....
 
static struct SimpleAnalogData {
  Layer simple_bg_layer;
 
  Layer date_layer;
  TextLayer day_label;
  char day_buffer[6];
  TextLayer num_label;
  char num_buffer[4];
 
  GPath minute_arrow, hour_arrow;
  GPath tick_paths[NUM_CLOCK_TICKS];
  Layer hands_layer;
  Window window;
} s_data;
 
static void bg_update_proc(Layer* me, GContext* ctx) {
 
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, me->bounds, 0, GCornerNone);
 
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    gpath_draw_filled(ctx, &s_data.tick_paths[i]);
  }
}
 
static void hands_update_proc(Layer* me, GContext* ctx) {
  const GPoint center = grect_center_point(&me->bounds);
  const int16_t secondHandLength = me->bounds.size.w / 2;
 
  GPoint secondHand;
 
  PblTm t;
  get_time(&t);
 
  int32_t second_angle = TRIG_MAX_ANGLE * t.tm_sec / 60;
  secondHand.y = (int16_t)(-cos_lookup(second_angle) * (int32_t)secondHandLength / TRIG_MAX_RATIO) + center.y;
  secondHand.x = (int16_t)(sin_lookup(second_angle) * (int32_t)secondHandLength / TRIG_MAX_RATIO) + center.x;
 
  // second hand
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, secondHand, center);
 
  // minute/hour hand
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorBlack);
 
  gpath_rotate_to(&s_data.minute_arrow, TRIG_MAX_ANGLE * t.tm_min / 60);
  gpath_draw_filled(ctx, &s_data.minute_arrow);
  gpath_draw_outline(ctx, &s_data.minute_arrow);
 
  gpath_rotate_to(&s_data.hour_arrow, (TRIG_MAX_ANGLE * (((t.tm_hour % 12) * 6) + (t.tm_min / 10))) / (12 * 6));
  gpath_draw_filled(ctx, &s_data.hour_arrow);
  gpath_draw_outline(ctx, &s_data.hour_arrow);
 
  // dot in the middle
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(me->bounds.size.w / 2-1, me->bounds.size.h / 2-1, 3, 3), 0, GCornerNone);
}
 
static void date_update_proc(Layer* me, GContext* ctx) {
 
  PblTm t;
  get_time(&t);
 
  string_format_time(s_data.day_buffer, sizeof(s_data.day_buffer), "%b", &t);
  text_layer_set_text(&s_data.day_label, s_data.day_buffer);
 
  string_format_time(s_data.num_buffer, sizeof(s_data.num_buffer), "%d", &t);
  text_layer_set_text(&s_data.num_label, s_data.num_buffer);
}
 
 
// Communication functions
 
static AppMessageResult sm_message_out_get(DictionaryIterator **iter_out) {
    AppMessageResult result = app_message_out_get(iter_out);
    if(result != APP_MSG_OK) return result;
    dict_write_int32(*iter_out, SM_SEQUENCE_NUMBER_KEY, ++s_sequence_number);
    if(s_sequence_number == 0xFFFFFFFF) {
        s_sequence_number = 1;
    }
	text_layer_set_text(&text_status_layer, "Send.");
    return APP_MSG_OK;
}
 
static void reset_sequence_number() {
    DictionaryIterator *iter = NULL;
    app_message_out_get(&iter);
    if(!iter) return;
    dict_write_int32(iter, SM_SEQUENCE_NUMBER_KEY, 0xFFFFFFFF);
    app_message_out_send();
    app_message_out_release();
}
 
 
static void sendCommand(int key) {
	if(sending == 1) return;

	DictionaryIterator* iterout;
    sm_message_out_get(&iterout);
    if(!iterout) return;
     
	sending = 1;
    dict_write_int8(iterout, key, -1);
    app_message_out_send();
    app_message_out_release();  
}
 
 
static void sendCommandInt(int key, int param) {
	if(sending == 1) return;
	
    DictionaryIterator* iterout;
    sm_message_out_get(&iterout);
    if(!iterout) return;
     
	sending = 1;
    dict_write_int8(iterout, key, param);
    app_message_out_send();
    app_message_out_release();  
}
 
static void rcv(DictionaryIterator *received, void *context) {
	// Got a message callback
	Tuple *t;
	int interval;
	
	connected = 1;
	inTimeOut = 0;

	t=dict_find(received, SM_COUNT_BATTERY_KEY); 
	if (t!=NULL) {
		batteryPercent = t->value->uint8;
		layer_mark_dirty(&battery_ind_layer);
	}

	t=dict_find(received, SM_WEATHER_COND_KEY); 
	if (t!=NULL) {
		memcpy(weather_cond_str, t->value->cstring, strlen(t->value->cstring));
        weather_cond_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&text_weather_cond_layer, weather_cond_str); 	
	}

	t=dict_find(received, SM_WEATHER_TEMP_KEY); 
	if (t!=NULL) {
		memcpy(weather_temp_str, t->value->cstring, strlen(t->value->cstring));
        weather_temp_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&text_weather_temp_layer, weather_temp_str); 
	}

	t=dict_find(received, SM_WEATHER_ICON_KEY); 
	if (t!=NULL) {
		bitmap_layer_set_bitmap(&weather_image, &weather_status_small_imgs[t->value->uint8].bmp);	  	
	}

	t=dict_find(received, SM_WEATHER_ICON1_KEY); 
	if (t!=NULL) {
		bitmap_layer_set_bitmap(&weather_tomorrow_image, &weather_status_small_imgs[t->value->uint8].bmp);	  	
	}
	
	t=dict_find(received, SM_WEATHER_DAY1_KEY); 
	if (t!=NULL) {
		memcpy(weather_tomorrow_temp_str, t->value->cstring + 6, strlen(t->value->cstring));
        weather_tomorrow_temp_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&text_weather_tomorrow_temp_layer, weather_tomorrow_temp_str); 	

		sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
	}

	t=dict_find(received, SM_UPDATE_INTERVAL_KEY); 
	if (t!=NULL) {
		if(inGPSUpdate == 1) {
			updateGPSInterval = t->value->int32 * 1000;
			inGPSUpdate = 0;
		}
	}

	t=dict_find(received, SM_GPS_1_KEY); 
	if (t!=NULL) {
		memcpy(location_street_str, t->value->cstring, strlen(t->value->cstring));
        location_street_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&location_street_layer, location_street_str);
		
		sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
	}

	t=dict_find(received, SM_STATUS_CAL_TIME_KEY); 
	if (t!=NULL) {
		memcpy(calendar_date_str, t->value->cstring, strlen(t->value->cstring));
        calendar_date_str[strlen(t->value->cstring)] = '\0';
		//text_layer_set_text(&calendar_date_layer, calendar_date_str); 	
		strncpy(appointment_time, calendar_date_str, 11);
	}

	t=dict_find(received, SM_STATUS_CAL_TEXT_KEY); 
	if (t!=NULL) {
		memcpy(calendar_text_str, t->value->cstring, strlen(t->value->cstring));
        calendar_text_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&calendar_text_layer, calendar_text_str); 	
		
		if(strlen(calendar_text_str) <= 14)
			text_layer_set_font(&calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
		else
			if(strlen(calendar_text_str) <= 16)
				text_layer_set_font(&calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
			else 
				text_layer_set_font(&calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	}

	t=dict_find(received, SM_STATUS_MUS_ARTIST_KEY); 
	if (t!=NULL) {
		memcpy(music_artist_str, t->value->cstring, strlen(t->value->cstring));
        music_artist_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&music_artist_layer, music_artist_str); 	
	}

	t=dict_find(received, SM_STATUS_MUS_TITLE_KEY); 
	if (t!=NULL) {
		memcpy(music_title_str, t->value->cstring, strlen(t->value->cstring));
        music_title_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&music_song_layer, music_title_str); 	
	}

	t=dict_find(received, SM_STATUS_UPD_WEATHER_KEY); 
	if (t!=NULL) {
		interval = t->value->int32 * 1000;

		app_timer_cancel_event(g_app_context, timerUpdateWeather);
		timerUpdateWeather = app_timer_send_event(g_app_context, interval /* milliseconds */, TIMER_COOKIE_WEATHER);
	}

	t=dict_find(received, SM_STATUS_UPD_CAL_KEY); 
	if (t!=NULL) {
		interval = t->value->int32 * 1000;

		app_timer_cancel_event(g_app_context, timerUpdateCalendar);
		timerUpdateCalendar = app_timer_send_event(g_app_context, interval /* milliseconds */, TIMER_COOKIE_CALANDAR);
	}

	t=dict_find(received, SM_SONG_LENGTH_KEY); 
	if (t!=NULL) {
		interval = t->value->int32 * 1000;

		app_timer_cancel_event(g_app_context, timerUpdateMusic);
		timerUpdateMusic = app_timer_send_event(g_app_context, interval /* milliseconds */, TIMER_COOKIE_MUSIC);
	}
}
 
static void dropped(void *context, AppMessageResult reason){
	// DO SOMETHING WITH THE DROPPED REASON / DISPLAY AN ERROR / RESEND 
	text_layer_set_text(&text_status_layer, "Drop.");
	
	if(reason == APP_MSG_BUSY) {
		text_layer_set_text(&text_status_layer, ">Busy");
	}
	
	if(reason == APP_MSG_BUFFER_OVERFLOW) {
		text_layer_set_text(&text_status_layer, "Over.");
	}
	
	connected = 0;
	timerRecoveryAttempt = app_timer_send_event(g_app_context, RECOVERY_ATTEMPT_INTERVAL, TIMER_COOKIE_CONNECTIONRECOVER);
}
 
static void sent_ok(DictionaryIterator *sent, void *context) {
	sending = 0;
    text_layer_set_text(&text_status_layer, "Ok");
    connected = 1;
}
 
static void send_failed(DictionaryIterator *failed, AppMessageResult reason, void *context) {
	sending = 0;

	text_layer_set_text(&text_status_layer, "Err.");
	
	if(reason == APP_MSG_NOT_CONNECTED) {
		text_layer_set_text(&text_status_layer, "Disc.");
		if(connected == 1) {
			vibes_double_pulse();
		}
	}
	
	if(reason == APP_MSG_SEND_TIMEOUT) {
		text_layer_set_text(&text_status_layer, "T.Out");
		if(inTimeOut == 1) {
			vibes_double_pulse();
			inTimeOut = 2;
		}
		if(inTimeOut == 0) {
			inTimeOut = 1;
		}
	}
	
	if(reason == APP_MSG_BUSY) {
		text_layer_set_text(&text_status_layer, "<Busy");
	}
	
	if(reason == APP_MSG_SEND_REJECTED) {
		text_layer_set_text(&text_status_layer, "Nack");
	}
	
	connected = 0;
	timerRecoveryAttempt = app_timer_send_event(g_app_context, RECOVERY_ATTEMPT_INTERVAL, TIMER_COOKIE_CONNECTIONRECOVER);
}
 
 
static void select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;
 
    sendCommand(SM_PLAYPAUSE_KEY);
}
 
static void select_long_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;
 
    sendCommand(SM_FIND_MY_PHONE_KEY);
}
 
static void select_up_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;
 
}
 
 
static void select_down_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;
}
 
 
static void up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;
 
    sendCommand(SM_VOLUME_UP_KEY);
}
 
static void down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;
 
    sendCommand(SM_VOLUME_DOWN_KEY);
}
 
static void swap_bottom_layer() {
    //on a press of the bottom button, scroll in the next layer
 
    property_animation_init_layer_frame(&ani_out, &animated_layer[active_layer], &GRect(25, 25, 85, 50), &GRect(-85, 25, 85, 50));
    animation_schedule(&(ani_out.animation));
 
 
    active_layer = (active_layer + 1) % (NUM_LAYERS);
 
 
    property_animation_init_layer_frame(&ani_in, &animated_layer[active_layer], &GRect(144, 25, 85, 50), &GRect(25, 25, 85, 50));
    animation_schedule(&(ani_in.animation));
}
 
 
static void config_provider(ClickConfig **config, Window *window) {
  (void)window;
 
 
  config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_single_click_handler;
//  config[BUTTON_ID_SELECT]->raw.up_handler = (ClickHandler) select_up_handler;
//  config[BUTTON_ID_SELECT]->raw.down_handler = (ClickHandler) select_down_handler;
 
  config[BUTTON_ID_SELECT]->long_click.handler = (ClickHandler) select_long_click_handler;
//  config[BUTTON_ID_SELECT]->long_click.release_handler = (ClickHandler) select_long_release_handler;
 
 
  config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
  config[BUTTON_ID_UP]->click.repeat_interval_ms = 100;
//  config[BUTTON_ID_UP]->long_click.handler = (ClickHandler) up_long_click_handler;
//  config[BUTTON_ID_UP]->long_click.release_handler = (ClickHandler) up_long_release_handler;
 
  config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
  config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 100;
//  config[BUTTON_ID_DOWN]->long_click.handler = (ClickHandler) down_long_click_handler;
//  config[BUTTON_ID_DOWN]->long_click.release_handler = (ClickHandler) down_long_release_handler;
 
}
 
static void handleMinuteTimer() {
	PblTm t;
	
    static char date_text[] = "Xxxxxxxxx 00";
 
	get_time(&t);

	// Set the date
    string_format_time(date_text, sizeof(date_text), "%b %e", &t);
    text_layer_set_text(&text_date_layer, date_text);
	
	apptDisplay();

	//timerMinutes = app_timer_send_event(g_app_context, MINUTE_INTERVAL, TIMER_COOKIE_MINUTE);
}
 
static void battery_layer_update_callback(Layer *me, GContext* ctx) {
     
    //draw the remaining battery percentage
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_fill_color(ctx, GColorWhite);
 
    graphics_fill_rect(ctx, GRect(2+16-(int)((batteryPercent/100.0)*16.0), 2, (int)((batteryPercent/100.0)*16.0), 8), 0, GCornerNone);
     
}
 
 
static void handle_status_appear(Window *window)
{
    text_layer_set_text(&text_status_layer, "Hello");
     
    sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
	
	handleMinuteTimer();
 
    // Start UI timers  
	timerSwapBottomLayer = app_timer_send_event(g_app_context, SWAP_BOTTOM_LAYER_INTERVAL, TIMER_COOKIE_LAYERSWAP);
	timerUpdateGps = app_timer_send_event(g_app_context, updateGPSInterval, TIMER_COOKIE_GPS);
	//timerMinutes = app_timer_send_event(g_app_context, MINUTE_INTERVAL, TIMER_COOKIE_MINUTE);
}
 
static void handle_status_disappear(Window *window)
{
    text_layer_set_text(&text_status_layer, "Bye");
     
    sendCommandInt(SM_SCREEN_EXIT_KEY, STATUS_SCREEN_APP);
     
    app_timer_cancel_event(g_app_context, timerUpdateCalendar);
    app_timer_cancel_event(g_app_context, timerUpdateMusic);
    app_timer_cancel_event(g_app_context, timerUpdateWeather);
    app_timer_cancel_event(g_app_context, timerSwapBottomLayer);
    app_timer_cancel_event(g_app_context, timerUpdateGps);
    //app_timer_cancel_event(g_app_context, timerMinutes);
}
 
static void handle_init(AppContextRef ctx) {
    (void)ctx;
 
    g_app_context = ctx;
 
  window_init(&s_data.window, "Smart Status Analog");
 
  s_data.day_buffer[0] = '\0';
  s_data.num_buffer[0] = '\0';
 
  // init hand paths
  gpath_init(&s_data.minute_arrow, &MINUTE_HAND_POINTS);
  gpath_init(&s_data.hour_arrow, &HOUR_HAND_POINTS);
 
  const GPoint center = grect_center_point(&s_data.window.layer.bounds);
  gpath_move_to(&s_data.minute_arrow, center);
  gpath_move_to(&s_data.hour_arrow, center);
 
  // init clock face paths
  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    gpath_init(&s_data.tick_paths[i], &ANALOG_BG_POINTS[i]);
  }
 
  // init layers
  layer_init(&s_data.simple_bg_layer, s_data.window.layer.frame);
  s_data.simple_bg_layer.update_proc = &bg_update_proc;
  layer_add_child(&s_data.window.layer, &s_data.simple_bg_layer);
 
  // init date layer -> a plain parent layer to create a date update proc
  layer_init(&s_data.date_layer, s_data.window.layer.frame);
  s_data.date_layer.update_proc = &date_update_proc;
  //layer_add_child(&s_data.window.layer, &s_data.date_layer);
 
  // init day
  text_layer_init(&s_data.day_label, GRect(46, 114, 27, 20));
  text_layer_set_text(&s_data.day_label, s_data.day_buffer);
  text_layer_set_background_color(&s_data.day_label, GColorBlack);
  text_layer_set_text_color(&s_data.day_label, GColorWhite);
  GFont norm18 = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  text_layer_set_font(&s_data.day_label, norm18);
 
  layer_add_child(&s_data.date_layer, &s_data.day_label.layer);
 
  // init num
  text_layer_init(&s_data.num_label, GRect(73, 114, 18, 20));
 
  text_layer_set_text(&s_data.num_label, s_data.num_buffer);
  text_layer_set_background_color(&s_data.num_label, GColorBlack);
  text_layer_set_text_color(&s_data.num_label, GColorWhite);
  GFont bold18 = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  text_layer_set_font(&s_data.num_label, bold18);
 
  layer_add_child(&s_data.date_layer, &s_data.num_label.layer);
 
    window_set_window_handlers(&s_data.window, (WindowHandlers) {
        .appear = (WindowHandler)handle_status_appear,
        .disappear = (WindowHandler)handle_status_disappear
    });
 
    window_stack_push(&s_data.window, true /* Animated */);
    window_set_fullscreen(&s_data.window, true);
    window_set_background_color(&s_data.window, GColorBlack);
 
    resource_init_current_app(&APP_RESOURCES);
 
 
    //init weather images
    for (int8_t i=0; i<NUM_WEATHER_IMAGES; i++) {
        heap_bitmap_init(&weather_status_small_imgs[i], WEATHER_SMALL_IMG_IDS[i]);
    }
     
    // init battery layer
    layer_init(&battery_layer, GRect(108, 2, 49, 45));
    layer_add_child(&s_data.window.layer, &battery_layer);
 
    heap_bitmap_init(&battery_image, RESOURCE_ID_IMAGE_BATTERY);
 
    bitmap_layer_init(&battery_image_layer, GRect(12, 8, 23, 14));
    layer_add_child(&battery_layer, &battery_image_layer.layer);
    bitmap_layer_set_bitmap(&battery_image_layer, &battery_image.bmp);
 
    layer_init(&battery_ind_layer, GRect(14, 9, 19, 11));
    battery_ind_layer.update_proc = &battery_layer_update_callback;
    layer_add_child(&battery_layer, &battery_ind_layer);
 
    batteryPercent = 100;
    layer_mark_dirty(&battery_ind_layer);
 
    //init weather layer and add weather image, weather condition, temperature
    layer_init(&weather_layer, GRect(0, 126, 144, 40));
    layer_add_child(&s_data.window.layer, &weather_layer);
 
 
    weather_img = 0;
 
    bitmap_layer_init(&weather_image, GRect(2, 0, 20, 20)); // GRect(52, 2, 40, 40)
    layer_add_child(&weather_layer, &weather_image.layer);
    bitmap_layer_set_bitmap(&weather_image, &weather_status_small_imgs[0].bmp);
 
    text_layer_init(&text_weather_temp_layer, GRect(2, 18, 25, 20)); // GRect(98, 4, 47, 40)
    text_layer_set_text_alignment(&text_weather_temp_layer, GTextAlignmentCenter);
    text_layer_set_text_color(&text_weather_temp_layer, GColorWhite);
    text_layer_set_background_color(&text_weather_temp_layer, GColorClear);
    text_layer_set_font(&text_weather_temp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    layer_add_child(&weather_layer, &text_weather_temp_layer.layer);
    text_layer_set_text(&text_weather_temp_layer, "-°");   
     
    weather_tomorrow_img = 0;
 
    bitmap_layer_init(&weather_tomorrow_image, GRect(122, 0, 20, 20)); // GRect(52, 2, 40, 40)
    layer_add_child(&weather_layer, &weather_tomorrow_image.layer);
    bitmap_layer_set_bitmap(&weather_tomorrow_image, &weather_status_small_imgs[0].bmp);
 
    text_layer_init(&text_weather_tomorrow_temp_layer, GRect(108, 18, 36, 20)); // GRect(5, 2, 47, 40)
    text_layer_set_text_alignment(&text_weather_tomorrow_temp_layer, GTextAlignmentRight);
    text_layer_set_text_color(&text_weather_tomorrow_temp_layer, GColorWhite);
    text_layer_set_background_color(&text_weather_tomorrow_temp_layer, GColorClear);
    text_layer_set_font(&text_weather_tomorrow_temp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    layer_add_child(&weather_layer, &text_weather_tomorrow_temp_layer.layer);
    text_layer_set_text(&text_weather_tomorrow_temp_layer, "../..");    
     
    //init layers for time and date and status
    text_layer_init(&text_date_layer, window.layer.frame);
    text_layer_set_text_alignment(&text_date_layer, GTextAlignmentLeft);
    text_layer_set_text_color(&text_date_layer, GColorWhite);
    text_layer_set_background_color(&text_date_layer, GColorClear);
    layer_set_frame(&text_date_layer.layer, GRect(55, 130, 50, 20));
    //text_layer_set_font(&text_date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_21)));
    text_layer_set_font(&text_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    layer_add_child(&s_data.window.layer, &text_date_layer.layer);
 
 
    text_layer_init(&text_time_layer, window.layer.frame);
    text_layer_set_text_alignment(&text_time_layer, GTextAlignmentCenter);
    text_layer_set_text_color(&text_time_layer, GColorWhite);
    text_layer_set_background_color(&text_time_layer, GColorClear);
    layer_set_frame(&text_time_layer.layer, GRect(0, -5, 144, 50));
    text_layer_set_font(&text_time_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_49)));
    //layer_add_child(&s_data.window.layer, &text_time_layer.layer);
 
    text_layer_init(&text_status_layer, GRect(99, 85, 45, 20));
    text_layer_set_text_alignment(&text_status_layer, GTextAlignmentRight);
    text_layer_set_text_color(&text_status_layer, GColorWhite);
    text_layer_set_background_color(&text_status_layer, GColorClear);
    text_layer_set_font(&text_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    layer_add_child(&s_data.window.layer, &text_status_layer.layer);
    text_layer_set_text(&text_status_layer, "Init.");
 
 
    //init calendar layer
    layer_init(&calendar_layer, GRect(0, 85, 144, 50));
    layer_add_child(&s_data.window.layer, &calendar_layer);
     
    text_layer_init(&calendar_date_layer, GRect(1, 0, 132, 21));
    text_layer_set_text_alignment(&calendar_date_layer, GTextAlignmentLeft);
    text_layer_set_text_color(&calendar_date_layer, GColorWhite);
    text_layer_set_background_color(&calendar_date_layer, GColorClear);
    text_layer_set_font(&calendar_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    layer_add_child(&calendar_layer, &calendar_date_layer.layer);
    text_layer_set_text(&calendar_date_layer, "No Upcoming");   
 
 
    text_layer_init(&calendar_text_layer, GRect(20, 15, 102, 28));
    text_layer_set_text_alignment(&calendar_text_layer, GTextAlignmentLeft);
    text_layer_set_text_color(&calendar_text_layer, GColorWhite);
    text_layer_set_background_color(&calendar_text_layer, GColorClear);
    text_layer_set_font(&calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
    layer_add_child(&calendar_layer, &calendar_text_layer.layer);
    text_layer_set_text(&calendar_text_layer, "Appointment");
     
     
     
    //init music layer
    layer_init(&animated_layer[MUSIC_LAYER], GRect(144, 25, 85, 50));
    layer_add_child(&s_data.window.layer, &animated_layer[MUSIC_LAYER]);
     
    text_layer_init(&music_artist_layer, GRect(0, 0, 85, 15));
    text_layer_set_text_alignment(&music_artist_layer, GTextAlignmentCenter);
    text_layer_set_text_color(&music_artist_layer, GColorWhite);
    text_layer_set_background_color(&music_artist_layer, GColorClear);
    text_layer_set_font(&music_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    layer_add_child(&animated_layer[MUSIC_LAYER], &music_artist_layer.layer);
    text_layer_set_text(&music_artist_layer, "No Artist");  
 
 
    text_layer_init(&music_song_layer, GRect(0, 16, 85, 34));
    text_layer_set_text_alignment(&music_song_layer, GTextAlignmentCenter);
    text_layer_set_text_color(&music_song_layer, GColorWhite);
    text_layer_set_background_color(&music_song_layer, GColorClear);
    text_layer_set_font(&music_song_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    layer_add_child(&animated_layer[MUSIC_LAYER], &music_song_layer.layer);
    text_layer_set_text(&music_song_layer, "No Title");
     
     
    //init location layer
    layer_init(&animated_layer[LOCATION_LAYER], GRect(25, 25, 85, 50));
    layer_add_child(&s_data.window.layer, &animated_layer[LOCATION_LAYER]);
     
    text_layer_init(&location_street_layer, GRect(0, 0, 85, 47));
    text_layer_set_text_alignment(&location_street_layer, GTextAlignmentCenter);
    text_layer_set_text_color(&location_street_layer, GColorWhite);
    text_layer_set_background_color(&location_street_layer, GColorClear);
    text_layer_set_font(&location_street_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    layer_add_child(&animated_layer[LOCATION_LAYER], &location_street_layer.layer);
    text_layer_set_text(&location_street_layer, "Location not updated");    
 
  // init hands
  layer_init(&s_data.hands_layer, s_data.simple_bg_layer.frame);
  s_data.hands_layer.update_proc = &hands_update_proc;
  layer_add_child(&s_data.window.layer, &s_data.hands_layer);
 
    window_set_click_config_provider(&s_data.window, (ClickConfigProvider) config_provider);
 
    active_layer = LOCATION_LAYER;
     
    timerUpdateWeatherForecast = app_timer_send_event(g_app_context, 5000 /* milliseconds */, 5);
}

static void handle_deinit(AppContextRef ctx) {
  (void)ctx;
 
    for (int8_t i=0; i<NUM_WEATHER_IMAGES; i++) {
        heap_bitmap_deinit(&weather_status_small_imgs[i]);
    }
}
 
 
void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie) {
  (void)ctx;
  (void)handle;

/* Request new data from the phone once the timers expire */
	if (cookie == TIMER_COOKIE_WEATHER) {
		sendCommandInt(SM_SCREEN_ENTER_KEY, WEATHER_APP);
		sendCommand(SM_STATUS_UPD_WEATHER_KEY);	
	}
	if (cookie == TIMER_COOKIE_CALANDAR) {
		sendCommand(SM_STATUS_UPD_CAL_KEY);	
	}

	if (cookie == TIMER_COOKIE_MUSIC) {
		sendCommand(SM_SONG_LENGTH_KEY);	
	}

	if (cookie == TIMER_COOKIE_LAYERSWAP) {
		swap_bottom_layer();	

		timerSwapBottomLayer = app_timer_send_event(g_app_context, SWAP_BOTTOM_LAYER_INTERVAL, TIMER_COOKIE_LAYERSWAP);
	}
	
	if (cookie == TIMER_COOKIE_NEXTDAYWEATHER) {
		sendCommandInt(SM_SCREEN_ENTER_KEY, WEATHER_APP);
		sendCommand(SM_STATUS_UPD_WEATHER_KEY);	
		
		apptDisplay();
	}
		
	if (cookie == TIMER_COOKIE_GPS) {
		timerUpdateGps = 0;

		inGPSUpdate = 1;
		sendCommandInt(SM_SCREEN_ENTER_KEY, GPS_APP);
		
		timerUpdateGps = app_timer_send_event(g_app_context, updateGPSInterval, TIMER_COOKIE_GPS);
	}
	
	if (cookie == TIMER_COOKIE_CONNECTIONRECOVER) {
		sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
	}

	if (cookie == TIMER_COOKIE_MINUTE) {
		handleMinuteTimer();
	}
}
 
static void handle_second_tick(AppContextRef ctx, PebbleTickEvent* t) {
	if(t->tick_time->tm_sec == 0) {
		handleMinuteTimer();
	}
	
  	layer_mark_dirty(&s_data.window.layer);
}
 
void pbl_main(void *params) {
 
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
    .messaging_info = {
        .buffer_sizes = {
            .inbound = 124,
            .outbound = 32
        },
        .default_callbacks.callbacks = {
            .out_sent = sent_ok,
            .out_failed = send_failed,
            .in_received = rcv,
            .in_dropped = dropped
        }
    },
    .tick_info = {
      .tick_handler = &handle_second_tick,
      .tick_units = SECOND_UNIT
    },
    .timer_handler = &handle_timer,
 
  };
  app_event_loop(params, &handlers);
}
