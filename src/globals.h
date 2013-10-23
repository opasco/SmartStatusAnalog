#pragma once
 
#define SM_RECONNECT_KEY            0xFC01
#define SM_SEQUENCE_NUMBER_KEY      0xFC02
#define SM_OPEN_SIRI_KEY            0xFC03
#define SM_STATUS_SCREEN_REQ_KEY    0xFC04
#define SM_PLAYPAUSE_KEY            0xFC05
#define SM_NEXT_TRACK_KEY           0xFC06
#define SM_PREVIOUS_TRACK_KEY       0xFC07
#define SM_VOLUME_UP_KEY            0xFC08
#define SM_VOLUME_DOWN_KEY          0xFC09
#define SM_COUNT_MAIL_KEY           0xFC0A
#define SM_COUNT_SMS_KEY            0xFC0B
#define SM_COUNT_PHONE_KEY          0xFC0C
#define SM_COUNT_BATTERY_KEY        0xFC0D
#define SM_SCREEN_ENTER_KEY         0xFC0E
#define SM_SCREEN_EXIT_KEY          0xFC0F
#define SM_WEATHER_COND_KEY         0xFC10
#define SM_WEATHER_TEMP_KEY         0xFC11
#define SM_WEATHER_ICON_KEY         0xFC12
#define SM_STATUS_SCREEN_UPDATE_KEY 0xFC13
#define SM_VOLUME_VALUE_KEY         0xFC14
#define SM_PLAY_STATUS_KEY          0xFC15
#define SM_ARTIST_KEY               0xFC16
#define SM_ALBUM_KEY                0xFC17
#define SM_TITLE_KEY                0xFC18
#define SM_WEATHER_HUMID_KEY        0xFC19
#define SM_WEATHER_WIND_KEY         0xFC1A
#define SM_WEATHER_DAY1_KEY         0xFC1B
#define SM_WEATHER_DAY2_KEY         0xFC1C
#define SM_WEATHER_DAY3_KEY         0xFC1D
#define SM_WEATHER_ICON1_KEY        0xFC1E
#define SM_WEATHER_ICON2_KEY        0xFC1F
#define SM_WEATHER_ICON3_KEY        0xFC20
#define SM_CALENDAR_UPDATE_KEY      0xFC21
#define SM_MENU_UPDATE_KEY          0xFC22
#define SM_STOCKS_GRAPH_KEY         0xFC23
#define SM_BITCOIN_GRAPH_KEY        0xFC24
#define SM_BITCOIN_LOW_KEY          0xFC25
#define SM_BITCOIN_HIGH_KEY         0xFC26
#define SM_BITCOIN_CURR_KEY         0xFC27
#define SM_BITCOIN_TITLE_KEY        0xFC28
#define SM_STOCKS_LOW_KEY           0xFC29
#define SM_STOCKS_HIGH_KEY          0xFC2A
#define SM_STOCKS_CURR_KEY          0xFC2B
#define SM_STOCKS_TITLE_KEY         0xFC2C
#define SM_LAUNCH_CAMERA_KEY        0xFC2D
#define SM_TAKE_PICTURE_KEY         0xFC2E
#define SM_URL1_KEY                 0xFC2F
#define SM_URL2_KEY                 0xFC30
#define SM_URL1_TEXT_KEY            0xFC31
#define SM_URL2_TEXT_KEY            0xFC32
#define SM_GPS_1_KEY                0xFC33
#define SM_GPS_2_KEY                0xFC34
#define SM_GPS_3_KEY                0xFC35
#define SM_GPS_4_KEY                0xFC36
#define SM_RESET_DST_KEY            0xFC37
#define SM_MESSAGES_UPDATE_KEY      0xFC38
#define SM_CALLS_UPDATE_KEY         0xFC38
#define SM_CAL_DETAILS_KEY          0xFC39
#define SM_DETAILS1_KEY             0xFC3A
#define SM_DETAILS2_KEY             0xFC3B
#define SM_CALL_SMS_KEY             0xFC3C
#define SM_CALL_SMS_UPDATE_KEY      0xFC3D
#define SM_CALL_SMS_CMD_KEY         0xFC3E
#define SM_SMS_SENT_KEY             0xFC3F
#define SM_FIND_MY_PHONE_KEY        0xFC40
#define SM_REMINDERS_KEY            0xFC41
#define SM_REMINDERS_DETAILS_KEY    0xFC42
#define SM_STATUS_CAL_TIME_KEY      0xFC43
#define SM_STATUS_CAL_TEXT_KEY      0xFC44
#define SM_STATUS_MUS_ARTIST_KEY    0xFC45
#define SM_STATUS_MUS_TITLE_KEY     0xFC46
#define SM_UPDATE_INTERVAL_KEY      0xFC47
#define SM_SONG_LENGTH_KEY          0xFC48
#define SM_STATUS_UPD_WEATHER_KEY   0xFC49
#define SM_STATUS_UPD_CAL_KEY       0xFC4A
 
 
 
#define STATUS_SCREEN_APP           NUM_APPS
 
typedef enum {CALENDAR_APP, MUSIC_APP, GPS_APP, STOCKS_APP, BITCOIN_APP, CAMERA_APP, WEATHER_APP, URL_APP, FINDPHONE_APP, REMINDERS_APP, STATUS_SCREEN_APP} AppIDs;
 
static char *app_names[] = {"Calendar", "Music", "GPS", "Stocks", "Bitcoin", "Camera", "Weather", "HTTP Request", "Find My Phone", "Reminders"};
 
 
// Analog clock data
 
#define NUM_CLOCK_TICKS 11
 
static const struct GPathInfo ANALOG_BG_POINTS[] = {
  { 4,
    (GPoint []) {
      {68, 0},
      {71, 1},
      {71, 12},
      {68, 12}
    }
  },
  { 4, (GPoint []){
      {72, 0},
      {75, 0},
      {75, 12},
      {72, 12}
    }
  },
  { 4, (GPoint []){
      {112, 10},
      {114, 12},
      {108, 23},
      {106, 21}
    }
  },
  { 4, (GPoint []){
      {132, 47},
      {144, 40},
      {144, 44},
      {135, 49}
    }
  },
  { 4, (GPoint []){
      {135, 118},
      {144, 123},
      {144, 126},
      {132, 120}
    }
  },
  { 4, (GPoint []){
      {108, 144},
      {114, 154},
      {112, 157},
      {106, 147}
    }
  },
  { 4, (GPoint []){
      {70, 155},
      {73, 155},
      {73, 167},
      {70, 167}
    }
  },
  { 4, (GPoint []){
      {32, 10},
      {30, 12},
      {36, 23},
      {38, 21}
    }
  },
  { 4, (GPoint []){
      {12, 47},
      {-1, 40},
      {-1, 44},
      {9, 49}
    }
  },
  { 4, (GPoint []){
      {9, 118},
      {-1, 123},
      {-1, 126},
      {12, 120}
    }
  },
  { 4, (GPoint []){
      {36, 144},
      {30, 154},
      {32, 157},
      {38, 147}
    }
  },
 
};
 
static const GPathInfo MINUTE_HAND_POINTS =
{
  3,
  (GPoint []) {
    { -8, 20 },
    { 8, 20 },
    { 0, -80 }
  }
};
 
static const GPathInfo HOUR_HAND_POINTS = {
  3, (GPoint []){
    {-6, 20},
    {6, 20},
    {0, -60}
  }
};
