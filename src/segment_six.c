#include "pebble.h"
#define KEY_NEXT_TIDE 1
#define KEY_HIGHLOW 2
#define KEY_GETTIDE 3
#define KEY_READY 4
#define KEY_PC 5
#define KEY_LAST_LEVEL 6
#define KEY_NEXT_LEVEL 7
#define KEY_FLW_LEVEL 9
#define KEY_CONFIG_LEVELS 10
  
#define ANIMATION_DURATION 500
#define ANIMATION_DELAY    600
#define FINAL_RADIUS 60
#define RISING_OFFSET 20
int rising_offset = RISING_OFFSET;
  
  
bool s_animating = false;
int s_radius;
int s_anim_next_tide;
int s_anim_angle;

static void do_animation();
static void rising_offset_update(Animation *anim, AnimationProgress dist_normalized);
static void tide_radius_update(Animation *anim, AnimationProgress dist_normalized);
static void bezel_update(Animation *anim, AnimationProgress dist_normalized);
static void tide_hand_update(Animation *anim, AnimationProgress dist_normalized);
static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers);



static const char* tide_labels[] = {"5", "4", "3", "2", "1" };  
static const GRect tide_pos1[] = {{{90,27},{18,18}}, {{109,43},{18,18}}, {{118,72},{18,18}}, {{110,97},{18,18}}, {{90,117},{18,18}}};
static const GRect tide_pos2[] = {{{40,117},{18,18}}, {{18,98},{18,18}}, {{13,72},{18,18}}, {{19,43},{18,18}}, {{40,27},{18,18}}};
static const GPathInfo MINUTE_SEGMENT_PATH_POINTS = {
  3,
  (GPoint[]) {
    {0, 0},
    {-6, -70}, // 80 = radius + fudge; 8 = 80*tan(6 degrees); 6 degrees per minute;
    {6,  -70},
  }
};

static const GPathInfo HOUR_SEGMENT_PATH_POINTS = {
  3,
  (GPoint[]) {
    {0, 0},
    {-20, -70}, // 50 = radius + fudge; _ = 50*tan(15 degrees); 30 degrees per hour;
    {20,  -70},
  }
};
#ifndef PBL_COLOR
static GPath *s_hour_hand_path;
static const GPathInfo HOUR_HAND_PATH_POINTS = {
  6,
  (GPoint[]) {
    {-2,-55},
      {0,-57},
    {2,-55},
    {4,0},
    {0,2},
    {-4,0},
  }
};

#endif
static void request_tide();
static bool showLevels = false;
static bool levelsAlways = false;
static char highlow[10] = "";
static int next_tide = -1;
static int pc = 0;
static char llvl[10], nlvl[10], flvl[10];
static Window *s_main_window;
static Layer *s_minute_display_layer, *s_hour_display_layer, *s_segment_layer;

static GPath *s_minute_segment_path, *s_hour_segment_path;

static void paint(GContext *ctx, GPath *path, int angle, GColor colour) {
  angle = (s_animating ? (angle - (360 - s_anim_angle)) : angle);
  gpath_rotate_to(path, (TRIG_MAX_ANGLE / 360) * angle);
  graphics_context_set_fill_color(ctx, colour);
  gpath_draw_filled(ctx, path);
}


void bt_handler(bool connected) {
  // Vibe pattern: ON for 200ms, OFF for 100ms, ON for 400ms:
static const uint32_t const segments[] = { 50, 100, 50, 100, 50, 100, 50, 100, 50, 100, 50, 100, 50, 100, 50, 100, 50, 100, 50, 100, 50};
VibePattern pat = {
  .durations = segments,
  .num_segments = ARRAY_LENGTH(segments),
};
  if (connected) {
    // APP_LOG(APP_LOG_LEVEL_INFO, "Phone is connected!");
    vibes_double_pulse();
    request_tide();
  } else {
    // APP_LOG(APP_LOG_LEVEL_INFO, "Phone is not connected!");
    // vibes_short_pulse();
    vibes_enqueue_custom_pattern(pat);
  }
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  if (next_tide > 0)
    next_tide--;
  if (next_tide == 0)
    request_tide();    
//  layer_mark_dirty(s_minute_display_layer);
//  layer_mark_dirty(s_hour_display_layer);
  layer_mark_dirty(s_segment_layer);
}



static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  static int showingLevels = -1;
  if (levelsAlways)
      tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  if (showLevels && showingLevels == -1) {
    showingLevels = 5;
  }
  else if (showLevels && showingLevels > 0) {
    showingLevels--;
  }
  else if (showLevels && showingLevels == 0) {
    showLevels = false;
    showingLevels = -1;
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
    layer_mark_dirty(s_segment_layer);
  }
  if ((units_changed & MINUTE_UNIT) != 0)
    handle_minute_tick(tick_time, units_changed);
  
}




static void segment_update_proc(Layer *layer, GContext* ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  int hour = t->tm_hour % 12;
  int minute = t->tm_min;

  int s_mode_next_tide = (s_animating ? s_anim_next_tide : next_tide);
  int s_mode_radius = (s_animating ? s_radius : FINAL_RADIUS);
  
  bool fillb = true;
  for (unsigned int i = 15; i < 375; i += 30) {
    fillb = !fillb;
    #ifdef PBL_COLOR
    paint(ctx, s_hour_segment_path, i, fillb ? GColorBlue : GColorBabyBlueEyes);
    #else
    paint(ctx, s_hour_segment_path, i, fillb ? GColorBlack : GColorWhite);
    #endif
    if (i == 15 || i == 195) {
      for (int j = 4; j < 15; j+= 7) {
        fillb = !fillb;
        #ifdef PBL_COLOR
        paint(ctx, s_minute_segment_path, j + (i - 15), !fillb ? GColorBlue : GColorBabyBlueEyes);
        #else
        paint(ctx, s_minute_segment_path, j + (i - 15), !fillb ? GColorBlack : GColorWhite);
        #endif
      }
    }
  }

  graphics_context_set_stroke_color(ctx, GColorBlack);
  #ifdef PBL_COLOR
  graphics_context_set_stroke_width(ctx, 4);
  graphics_draw_circle(ctx, center, 71);
  #else
  graphics_draw_circle(ctx, center, 72);
  graphics_draw_circle(ctx, center, 71);
  graphics_draw_circle(ctx, center, 70);
  
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_circle(ctx, center, 69);
  graphics_draw_circle(ctx, center, 68);
  #endif
    
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, center, 59);
  #ifndef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_circle(ctx, center, 59);
  graphics_draw_circle(ctx, center, 60);
  #endif
    
  graphics_context_set_text_color(ctx, GColorWhite);
  
  // Draw the hours to high/low tide
  for (int i=0; i<5; i++) {
    graphics_draw_text(ctx, tide_labels[i], fonts_get_system_font(FONT_KEY_GOTHIC_18), tide_pos1[i], GTextOverflowModeFill, GTextAlignmentCenter, NULL);  
    graphics_draw_text(ctx, tide_labels[i], fonts_get_system_font(FONT_KEY_GOTHIC_18), tide_pos2[i], GTextOverflowModeFill, GTextAlignmentCenter, NULL);  
  }
  
  // Draw various text items
  graphics_draw_text(ctx, "High Tide", fonts_get_system_font(FONT_KEY_GOTHIC_14), GRect(45,0,60,14), GTextOverflowModeFill, GTextAlignmentCenter, NULL);  
  graphics_draw_text(ctx, "Low Tide", fonts_get_system_font(FONT_KEY_GOTHIC_14), GRect(45,150,60,14), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  
  #ifdef PBL_COLOR
   graphics_context_set_text_color(ctx, GColorRajah);
  #endif
    if (showLevels || levelsAlways) {
    
    if (highlow[0] == 'H') {
      graphics_draw_text(ctx, "Rising", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(33,100+(RISING_OFFSET - rising_offset),80,18), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
      graphics_draw_text(ctx, nlvl, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(0,0,50,18), GTextOverflowModeFill, GTextAlignmentLeft, NULL);  
      graphics_draw_text(ctx, nlvl, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(94,0,50,18), GTextOverflowModeFill, GTextAlignmentRight, NULL);
      graphics_draw_text(ctx, llvl, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(0,140,50,18), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      graphics_draw_text(ctx, flvl, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(94,140,50,18), GTextOverflowModeFill, GTextAlignmentRight, NULL);  
    } else {
      graphics_draw_text(ctx, "Falling", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(33,100-(RISING_OFFSET - rising_offset),80,18), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
      graphics_draw_text(ctx, flvl, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(0,0,50,18), GTextOverflowModeFill, GTextAlignmentLeft, NULL);  
      graphics_draw_text(ctx, llvl, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(94,0,50,18), GTextOverflowModeFill, GTextAlignmentRight, NULL);
      graphics_draw_text(ctx, nlvl, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(0,140,50,18), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      graphics_draw_text(ctx, nlvl, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(94,140,50,18), GTextOverflowModeFill, GTextAlignmentRight, NULL);  
    }
  }
      
  if (s_mode_next_tide > 0) {
  // Draw the tide hand
  
  #ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_context_set_stroke_width(ctx, 6);
  #else
  graphics_context_set_stroke_color(ctx, GColorWhite);
  #endif
  
int nt = s_mode_next_tide > 360 ? 360 : s_mode_next_tide;

  if (highlow[0] == 'L')
    nt = nt + 360;

    GPoint tidepc_hand = (GPoint) {
    .x = (int16_t)(-sin_lookup(TRIG_MAX_ANGLE * nt / 720) * (int32_t)(s_mode_radius * pc / 100.0) / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * nt / 720) * (int32_t)(s_mode_radius * pc / 100.0) / TRIG_MAX_RATIO) + center.y,
  };

   
  GPoint tide_hand = (GPoint) {
    .x = (int16_t)(-sin_lookup(TRIG_MAX_ANGLE * nt / 720) * (int32_t)(s_mode_radius) / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * nt / 720) * (int32_t)(s_mode_radius) / TRIG_MAX_RATIO) + center.y,
  };
#ifdef PBL_COLOR 
  graphics_draw_line(ctx, center, tide_hand); 
#else
  for (int x=0; x<4; x++)
    for (int y=0; y<4; y++)
        graphics_draw_line(ctx, GPoint(center.x+x-2, center.y+y-2), tide_hand);

  /*
  APP_LOG(APP_LOG_LEVEL_INFO, "nt: %d", nt);
  gpath_rotate_to(s_hour_hand_path, TRIG_MAX_ANGLE * ((720-nt)/720.0));
  graphics_context_set_fill_color(ctx, GColorWhite);
  gpath_draw_filled(ctx, s_hour_hand_path);
  */
#endif
  // Circle in the middle (Red on Time, White on BW)
  graphics_draw_circle(ctx, center, 4);

  
  
  #ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorChromeYellow);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, tidepc_hand, 1);
  #endif
  }
//  graphics_draw_line(ctx, center, tidepc_hand);
  
  graphics_context_set_stroke_color(ctx, GColorWhite);

  
    
    
    
    
  //
  // Draw the hour hand & tail
  //
  GPoint hour_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * (hour / 12.0 + minute / 720.0)) * (int32_t)(30) / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * (hour / 12.0 + minute / 720.0)) * (int32_t)(30) / TRIG_MAX_RATIO) + center.y,
  };
  // APP_LOG(APP_LOG_LEVEL_INFO, "x: %d y: %d next_tide: %d", (int) tide_hand.x, (int) tide_hand.y, next_tide);
  #ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 6);
  GPoint hour_hand_offset = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * (hour / 12.0 + minute / 720.0)) * (int32_t)(15) / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * (hour / 12.0 + minute / 720.0)) * (int32_t)(15) / TRIG_MAX_RATIO) + center.y,
  };
  graphics_draw_line(ctx, hour_hand_offset, hour_hand);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, center, hour_hand_offset);
  #else  
  for (int x=0; x<2; x++)
    for (int y=0; y<2; y++)
        graphics_draw_line(ctx, GPoint(center.x+x-1, center.y+y-1), GPoint(hour_hand.x+x-1,hour_hand.y+y-1));
  #endif
    
  hour_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * (((hour + 6) % 12) / 12.0 + minute / 720.0)) * (int32_t)(10) / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * (((hour + 6) % 12) / 12.0 + minute / 720.0)) * (int32_t)(10) / TRIG_MAX_RATIO) + center.y,
  };
  // APP_LOG(APP_LOG_LEVEL_INFO, "x: %d y: %d next_tide: %d", (int) tide_hand.x, (int) tide_hand.y, next_tide);
  #ifdef PBL_COLOR
  // graphics_draw_line(ctx, center, hour_hand);
  #else
  for (int x=0; x<2; x++)
    for (int y=0; y<2; y++)
        graphics_draw_line(ctx, GPoint(center.x+x-1, center.y+y-1), GPoint(hour_hand.x+x-1,hour_hand.y+y-1));
  #endif  
    
    
  
  // Draw the minute hand
  GPoint minute_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * minute / 60) * (int32_t)(50) / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * minute / 60) * (int32_t)(50) / TRIG_MAX_RATIO) + center.y,
  };
  
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "x: %d y: %d next_tide: %d", (int) tide_hand.x, (int) tide_hand.y, next_tide);
  #ifdef PBL_COLOR
  GPoint minute_hand_offset = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * minute / 60) * (int32_t)(15) / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * minute / 60) * (int32_t)(15) / TRIG_MAX_RATIO) + center.y,
  };
  graphics_context_set_stroke_width(ctx, 6);
  graphics_draw_line(ctx, minute_hand_offset, minute_hand);
  
  // Draw thin line for minutes
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, center, minute_hand_offset);
  minute_hand_offset = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * minute / 60) * (int32_t)(18) / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * minute / 60) * (int32_t)(18) / TRIG_MAX_RATIO) + center.y,
  };
  minute_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * minute / 60) * (int32_t)(45) / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * minute / 60) * (int32_t)(45) / TRIG_MAX_RATIO) + center.y,
  };
  graphics_context_set_stroke_width(ctx, 2);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, minute_hand_offset, minute_hand);
  #else
  for (int x=0; x<2; x++)
    for (int y=0; y<2; y++)
        graphics_draw_line(ctx, GPoint(center.x+x-1, center.y+y-1), GPoint(minute_hand.x+x-1,minute_hand.y+y-1));
  #endif    
    
    
  // Draw the minute tail
  minute_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * ((minute+30)%60) / 60) * (int32_t)(10) / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * ((minute+30)%60) / 60) * (int32_t)(10) / TRIG_MAX_RATIO) + center.y,
  };
  #ifdef PBL_COLOR
  //graphics_draw_line(ctx, center, minute_hand);
    
  #else
  for (int x=0; x<2; x++)
    for (int y=0; y<2; y++)
        graphics_draw_line(ctx, GPoint(center.x+x-1, center.y+y-1), GPoint(minute_hand.x+x-1,minute_hand.y+y-1));
  #endif    
    
    
    
    
    
    
  // White circle in the middle
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, 4); // White circle in the middle
  // Pin in the middle!
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_pixel(ctx, center);
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  showLevels = true;
  tick_timer_service_subscribe(SECOND_UNIT | MINUTE_UNIT, handle_second_tick);
  do_animation();
  // layer_mark_dirty(s_segment_layer);
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void do_animation() {
if (!s_animating) {   
  // Implementations need to be static
      static AnimationImplementation bezel_impl = {
        .update = bezel_update
      };
      animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &bezel_impl, false);

      static AnimationImplementation rising_offset_impl = {
        .update = rising_offset_update
      };
      animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &rising_offset_impl, false);
        
      static AnimationImplementation radius_impl = {
        .update = tide_radius_update
      };
      animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &radius_impl, false);
    
      static AnimationImplementation hand_impl = {
        .update = tide_hand_update
      };
      animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &hand_impl, true);
      }
}


static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Store incoming information
//APP_LOG(APP_LOG_LEVEL_INFO, "Message");
  // Read first item
  Tuple *t = dict_read_first(iterator);
          DictionaryIterator *iter;
  showLevels = true;
  // For all items
  while(t != NULL) {
    // Which key was received?
    switch(t->key) {
    case KEY_NEXT_TIDE:
      next_tide = (int)t->value->int32;
      APP_LOG(APP_LOG_LEVEL_INFO, "NextTide: %d", (int)t->value->int32);
      // Prepare animations
      do_animation();
      break;
    case KEY_HIGHLOW:
      snprintf(highlow, sizeof(highlow), t->value->cstring);
      APP_LOG(APP_LOG_LEVEL_INFO, "HighLow: %s", (char *)t->value->cstring);
      break;
    case KEY_PC:
      pc = (int)t->value->int32;
      APP_LOG(APP_LOG_LEVEL_INFO, "PC: %d", (int)t->value->int32);
      break;
    case KEY_READY:
      break;

      case KEY_FLW_LEVEL:
      snprintf(flvl, sizeof(flvl), "%s", t->value->cstring);
      APP_LOG(APP_LOG_LEVEL_INFO, "FollowingLevel: %s", (char *)t->value->cstring);
      break;
      
    case KEY_LAST_LEVEL:
      snprintf(llvl, sizeof(llvl), "%s", t->value->cstring);

      // APP_LOG(APP_LOG_LEVEL_INFO, "LastLevel: %s", (char *)t->value->cstring);
      break;

    case KEY_NEXT_LEVEL:
      snprintf(nlvl, sizeof(nlvl), "%s", t->value->cstring);

     // APP_LOG(APP_LOG_LEVEL_INFO, "NextLevel: %s", (char *)t->value->cstring);
      break;
    
    case KEY_CONFIG_LEVELS:
      levelsAlways = ((int)t->value->int32 == 1);
     APP_LOG(APP_LOG_LEVEL_INFO, "Levelsalways: %d", (int)t->value->int32);
      if (!levelsAlways)
            tick_timer_service_subscribe(SECOND_UNIT | MINUTE_UNIT, handle_second_tick);
      break;

      default:
      APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
      break;
    }

    // Look for next item
    t = dict_read_next(iterator);
  }
//  s_animating = true;
//  s_radius = FINAL_RADIUS / 2;
//  s_anim_next_tide = 330;

  // layer_mark_dirty(s_segment_layer);
  
}


void request_tide() {
      DictionaryIterator *iter;
      app_message_outbox_begin(&iter);
      dict_write_uint32(iter, KEY_GETTIDE, 0);
      app_message_outbox_send();
}



static void main_window_load(Window *window) {

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_minute_display_layer = layer_create(bounds);
  // layer_set_update_proc(s_minute_display_layer, minute_display_update_proc);
  layer_add_child(window_layer, s_minute_display_layer);

  s_minute_segment_path = gpath_create(&MINUTE_SEGMENT_PATH_POINTS);
  gpath_move_to(s_minute_segment_path, grect_center_point(&bounds));

  #ifndef PBL_COLOR
  s_hour_hand_path = gpath_create(&HOUR_HAND_PATH_POINTS);
  gpath_move_to(s_hour_hand_path, grect_center_point(&bounds));
  #endif
  
  s_hour_display_layer = layer_create(bounds);
  // layer_set_update_proc(s_hour_display_layer, hour_display_update_proc);
  layer_add_child(window_layer, s_hour_display_layer);

  s_hour_segment_path = gpath_create(&HOUR_SEGMENT_PATH_POINTS);
  gpath_move_to(s_hour_segment_path, grect_center_point(&bounds));
  
  // Set up the hour segments to high & low tide
  s_segment_layer = layer_create(bounds);
  layer_set_update_proc(s_segment_layer, segment_update_proc);
  layer_add_child(window_layer, s_segment_layer);
  
  accel_tap_service_subscribe(tap_handler);
}

static void main_window_unload(Window *window) {
  gpath_destroy(s_minute_segment_path);
  gpath_destroy(s_hour_segment_path);

  layer_destroy(s_minute_display_layer);
  layer_destroy(s_hour_display_layer);
}

static void init() {

    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_sent(outbox_sent_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_open(200, 200);
  // Open AppMessage

    
    s_main_window = window_create();

    window_set_background_color(s_main_window, GColorBlack);
    window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
#ifndef PBL_COLOR
      window_set_fullscreen(s_main_window, true);
#endif

  window_stack_push(s_main_window, true);

  // tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
    bluetooth_connection_service_subscribe(bt_handler);
    tick_timer_service_subscribe(SECOND_UNIT | MINUTE_UNIT, handle_second_tick);

}

static void deinit() {
  window_destroy(s_main_window);
  bluetooth_connection_service_unsubscribe();
  tick_timer_service_unsubscribe();
}

int main() {
  init();
  app_event_loop();
  deinit();
}



// Animation Stuff

static void animation_started(Animation *anim, void *context) {
  s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
  s_animating = false;
}

static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers) {
  Animation *anim = animation_create();
  animation_set_duration(anim, duration);
  animation_set_delay(anim, delay);
  animation_set_curve(anim, AnimationCurveEaseInOut);
  animation_set_implementation(anim, implementation);
  if(handlers) {
    animation_set_handlers(anim, (AnimationHandlers) {
      .started = animation_started,
      .stopped = animation_stopped
    }, NULL);
  }
  animation_schedule(anim);
}


static int anim_percentage(AnimationProgress dist_normalized, int max) {
  int i = (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
  //APP_LOG(APP_LOG_LEVEL_INFO, "anim pc: %d", i);
  return i;
}

static void bezel_update(Animation *anim, AnimationProgress dist_normalized) {

  s_anim_angle = anim_percentage(dist_normalized, 180);
  //APP_LOG(APP_LOG_LEVEL_INFO, "Anim s_radius: %d", s_radius);
  layer_mark_dirty(s_segment_layer);

}
           
           
static void tide_radius_update(Animation *anim, AnimationProgress dist_normalized) {

  s_radius = anim_percentage(dist_normalized, FINAL_RADIUS);
  //APP_LOG(APP_LOG_LEVEL_INFO, "Anim s_radius: %d", s_radius);
  layer_mark_dirty(s_segment_layer);

}

static void tide_hand_update(Animation *anim, AnimationProgress dist_normalized) {

  s_anim_next_tide = anim_percentage(dist_normalized, next_tide);
  //APP_LOG(APP_LOG_LEVEL_INFO, "Anim s_anim_next_tide: %d", s_anim_next_tide);
  layer_mark_dirty(s_segment_layer);

}

static void rising_offset_update(Animation *anim, AnimationProgress dist_normalized) {

  rising_offset = anim_percentage(dist_normalized, RISING_OFFSET);
  //APP_LOG(APP_LOG_LEVEL_INFO, "Anim s_radius: %d", s_radius);
  layer_mark_dirty(s_segment_layer);

}


