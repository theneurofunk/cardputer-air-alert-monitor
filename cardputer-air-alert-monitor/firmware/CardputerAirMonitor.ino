#include <M5Cardputer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>

// ============================================================
// AIR ALERT MONITOR - M5STACK CARDPUTER
// ============================================================
//
// HOME:
// M = Menu
// R = Manual refresh / next DEMO
// N = Next area page
// P = Previous area page
//
// DEFAULT:
// Kyiv City       UID 31
// Zhytomyr Raion  UID 59
//
// ============================================================


// ============================================================
// API TOKEN
// ============================================================

// For Live Mode:
// 1. Request your personal API token from https://devs.alerts.in.ua/
// 2. Paste it below.
// 3. Compile and flash the firmware again.
//
// Demo Mode works without a token.
const char* API_TOKEN = "PASTE_YOUR_TOKEN_HERE";


// ============================================================
// SETTINGS
// ============================================================

const uint32_t API_INTERVAL_MS = 20000;
const uint32_t WIFI_RETRY_MS   = 10000;

const int MAX_AREAS = 6;
const int AREAS_PER_PAGE = 2;


// ============================================================
// API
// ============================================================

const char* STATUS_URL =
  "https://api.alerts.in.ua/v1/iot/active_air_raid_alerts.json";

const char* DETAILS_URL =
  "https://api.alerts.in.ua/v1/alerts/active.json";


// ============================================================
// THREAT FLAGS
// ============================================================

const uint16_t TH_TACTICAL_AIR  = 1 << 0;
const uint16_t TH_STRATEGIC_AIR = 1 << 1;
const uint16_t TH_MIG31K        = 1 << 2;
const uint16_t TH_BALLISTIC     = 1 << 3;
const uint16_t TH_CRUISE        = 1 << 4;
const uint16_t TH_MISSILES      = 1 << 5;
const uint16_t TH_DRONES        = 1 << 6;
const uint16_t TH_GUIDED_BOMBS  = 1 << 7;
const uint16_t TH_AIR_DEFENSE   = 1 << 8;
const uint16_t TH_UNKNOWN       = 1 << 9;


// ============================================================
// GLOBALS
// ============================================================

Preferences prefs;

String savedSSID = "";
String savedPassword = "";

int speakerVolume = 170;

bool demoMode = false;
int demoScenario = 0;

int homePage = 0;

uint32_t lastApiPoll = 0;
uint32_t lastApiOK = 0;
uint32_t lastWifiRetry = 0;
uint32_t lastHomeRedraw = 0;

String lastError = "";


// ============================================================
// API DIAGNOSTICS
// ============================================================

int lastStatusHttp = 0;
int lastDetailsHttp = 0;

bool lastJsonOK = false;

int lastDetailsPayloadBytes = 0;
int lastDetailsAlertCount = 0;
int lastDetailsAirRaidCount = 0;
int lastDetailsMatchCount = 0;
int lastMatchedThreatCount = 0;


// ============================================================
// WATCH AREA
// ============================================================

struct WatchArea {
  int uid;
  String name;

  int parentOblastUid;
  String raionUk;

  char state;
  bool stateInitialized;

  uint16_t threatMask;
  bool threatsInitialized;

  time_t alertStartedEpoch;
  String alertStartedText;
};

WatchArea areas[MAX_AREAS];
int areaCount = 0;


// ============================================================
// LIVE DEBUG
// ============================================================

struct LiveDebugRecord {
  bool everSeen;
  bool candidateNow;
  bool matchedNow;

  int candidateCount;
  int score;

  int locationUid;
  int oblastUid;

  String title;
  String titleEn;
  String locationType;
  String raion;

  String startedRaw;
  String startedLocal;

  int threatCount;
  String threatsText;

  uint32_t lastSeenMillis;
};

LiveDebugRecord debugRecords[MAX_AREAS];


// ============================================================
// DISPLAY HELPERS
// ============================================================

void clearScreen(uint16_t color = BLACK) {
  M5Cardputer.Display.fillScreen(color);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(0, 0);
}

void drawHeader(
  const String& text,
  uint16_t color = CYAN
) {
  M5Cardputer.Display.fillRect(
    0,
    0,
    M5Cardputer.Display.width(),
    16,
    color
  );

  M5Cardputer.Display.setTextColor(
    BLACK,
    color
  );

  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(4, 4);
  M5Cardputer.Display.print(text);
}

void messageScreen(
  const String& title,
  const String& text,
  uint16_t color = CYAN
) {
  clearScreen();
  drawHeader(title, color);

  M5Cardputer.Display.setTextColor(
    WHITE,
    BLACK
  );

  M5Cardputer.Display.setCursor(5, 28);
  M5Cardputer.Display.println(text);
}


// ============================================================
// SOUND
// ============================================================

void beep(
  int frequency,
  int duration
) {
  if (speakerVolume <= 0) {
    return;
  }

  M5Cardputer.Speaker.setVolume(
    speakerVolume
  );

  M5Cardputer.Speaker.tone(
    frequency,
    duration
  );

  delay(duration + 40);
}

void soundAlert() {
  beep(900, 140);
  beep(1350, 140);

  delay(70);

  beep(900, 140);
  beep(1600, 300);
}

void soundPartial() {
  beep(1000, 170);
  beep(1000, 170);
}

void soundClear() {
  beep(1600, 140);
  beep(1200, 150);
  beep(850, 280);
}

void soundNewThreat() {
  beep(1700, 100);
  beep(2000, 100);
  beep(1700, 140);
}

void soundError() {
  beep(350, 120);
  beep(280, 180);
}


// ============================================================
// KEYBOARD
// ============================================================

char normalizeKey(char c) {
  if (
    c >= 'A' &&
    c <= 'Z'
  ) {
    c = c - 'A' + 'a';
  }

  return c;
}

char waitKey() {
  while (true) {
    M5Cardputer.update();

    if (
      M5Cardputer.Keyboard.isChange() &&
      M5Cardputer.Keyboard.isPressed()
    ) {
      Keyboard_Class::KeysState status =
        M5Cardputer.Keyboard.keysState();

      for (auto c : status.word) {
        delay(100);
        return normalizeKey(c);
      }

      if (status.enter) {
        delay(100);
        return '\n';
      }

      if (status.del) {
        delay(100);
        return '\b';
      }
    }

    delay(10);
  }
}


// ============================================================
// TEXT INPUT
// ============================================================

String readText(
  const String& title,
  bool hidden = false,
  int maxLength = 64
) {
  String value = "";

  clearScreen();
  drawHeader(title);

  M5Cardputer.Display.setTextColor(
    WHITE,
    BLACK
  );

  M5Cardputer.Display.setCursor(5, 25);
  M5Cardputer.Display.println("Type + ENTER");

  while (true) {
    M5Cardputer.update();

    if (
      M5Cardputer.Keyboard.isChange() &&
      M5Cardputer.Keyboard.isPressed()
    ) {
      Keyboard_Class::KeysState status =
        M5Cardputer.Keyboard.keysState();

      for (auto c : status.word) {
        if (
          c >= 32 &&
          c <= 126 &&
          value.length() < maxLength
        ) {
          value += c;
        }
      }

      if (
        status.del &&
        value.length() > 0
      ) {
        value.remove(
          value.length() - 1
        );
      }

      if (status.enter) {
        delay(150);
        return value;
      }

      M5Cardputer.Display.fillRect(
        0,
        50,
        240,
        55,
        BLACK
      );

      M5Cardputer.Display.setCursor(
        5,
        58
      );

      M5Cardputer.Display.setTextColor(
        GREEN,
        BLACK
      );

      if (hidden) {
        int count =
          value.length();

        if (count > 30) {
          count = 30;
        }

        for (
          int i = 0;
          i < count;
          i++
        ) {
          M5Cardputer.Display.print("*");
        }
      } else {
        String shown =
          value;

        if (
          shown.length() > 35
        ) {
          shown =
            shown.substring(
              shown.length() - 35
            );
        }

        M5Cardputer.Display.print(shown);
      }
    }

    delay(10);
  }
}


// ============================================================
// TOKEN
// ============================================================

bool tokenConfigured() {
  String token =
    String(API_TOKEN);

  if (token.length() < 10) {
    return false;
  }

  if (
    token ==
    "PASTE_YOUR_TOKEN_HERE"
  ) {
    return false;
  }

  return true;
}


// ============================================================
// TIME
// ============================================================

int64_t daysFromCivil(
  int year,
  unsigned month,
  unsigned day
) {
  year -= month <= 2;

  const int era =
    (
      year >= 0
      ? year
      : year - 399
    ) / 400;

  const unsigned yoe =
    (unsigned)(
      year -
      era * 400
    );

  const unsigned doy =
    (
      153 *
      (
        month +
        (
          month > 2
          ? -3
          : 9
        )
      ) +
      2
    ) /
    5 +
    day -
    1;

  const unsigned doe =
    yoe * 365 +
    yoe / 4 -
    yoe / 100 +
    doy;

  return
    era * 146097 +
    (int)doe -
    719468;
}

bool parseIsoUtc(
  const String& iso,
  time_t& result
) {
  if (
    iso.length() < 19
  ) {
    return false;
  }

  int year =
    iso.substring(0, 4).toInt();

  int month =
    iso.substring(5, 7).toInt();

  int day =
    iso.substring(8, 10).toInt();

  int hour =
    iso.substring(11, 13).toInt();

  int minute =
    iso.substring(14, 16).toInt();

  int second =
    iso.substring(17, 19).toInt();

  if (
    year < 2020 ||
    month < 1 ||
    month > 12 ||
    day < 1 ||
    day > 31
  ) {
    return false;
  }

  int64_t days =
    daysFromCivil(
      year,
      month,
      day
    );

  int64_t seconds =
    days * 86400LL +
    hour * 3600LL +
    minute * 60LL +
    second;

  result =
    (time_t)seconds;

  return true;
}

String formatKyivTime(
  time_t epoch
) {
  if (epoch <= 0) {
    return "--:--";
  }

  struct tm localTm;

  localtime_r(
    &epoch,
    &localTm
  );

  char buffer[8];

  snprintf(
    buffer,
    sizeof(buffer),
    "%02d:%02d",
    localTm.tm_hour,
    localTm.tm_min
  );

  return String(buffer);
}


// ============================================================
// AREA HELPERS
// ============================================================

bool isOblastUid(int uid) {
  switch (uid) {
    case 3:
    case 4:
    case 5:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
      return true;
  }

  return false;
}

void enrichArea(
  WatchArea& area
) {
  area.parentOblastUid =
    0;

  area.raionUk =
    "";

  if (area.uid == 59) {
    area.parentOblastUid =
      10;

    area.raionUk =
      "Житомирський район";
  }
}

void initializeArea(
  WatchArea& area
) {
  enrichArea(area);

  area.state =
    '?';

  area.stateInitialized =
    false;

  area.threatMask =
    0;

  area.threatsInitialized =
    false;

  area.alertStartedEpoch =
    0;

  area.alertStartedText =
    "--:--";
}


// ============================================================
// DEBUG INIT
// ============================================================

void clearDebugRecord(
  int index
) {
  if (
    index < 0 ||
    index >= MAX_AREAS
  ) {
    return;
  }

  debugRecords[index].everSeen =
    false;

  debugRecords[index].candidateNow =
    false;

  debugRecords[index].matchedNow =
    false;

  debugRecords[index].candidateCount =
    0;

  debugRecords[index].score =
    -1;

  debugRecords[index].locationUid =
    0;

  debugRecords[index].oblastUid =
    0;

  debugRecords[index].title =
    "";

  debugRecords[index].titleEn =
    "";

  debugRecords[index].locationType =
    "";

  debugRecords[index].raion =
    "";

  debugRecords[index].startedRaw =
    "";

  debugRecords[index].startedLocal =
    "--:--";

  debugRecords[index].threatCount =
    0;

  debugRecords[index].threatsText =
    "-";

  debugRecords[index].lastSeenMillis =
    0;
}

void clearAllDebugRecords() {
  for (
    int i = 0;
    i < MAX_AREAS;
    i++
  ) {
    clearDebugRecord(i);
  }
}

void beginDebugFetch() {
  for (
    int i = 0;
    i < areaCount;
    i++
  ) {
    debugRecords[i].candidateNow =
      false;

    debugRecords[i].matchedNow =
      false;

    debugRecords[i].candidateCount =
      0;

    debugRecords[i].score =
      -1;
  }
}


// ============================================================
// AREA STORAGE
// ============================================================

void saveAreas() {
  prefs.putInt(
    "acount",
    areaCount
  );

  for (
    int i = 0;
    i < MAX_AREAS;
    i++
  ) {
    char uidKey[8];
    char nameKey[8];

    snprintf(
      uidKey,
      sizeof(uidKey),
      "uid%d",
      i
    );

    snprintf(
      nameKey,
      sizeof(nameKey),
      "nam%d",
      i
    );

    if (i < areaCount) {
      prefs.putInt(
        uidKey,
        areas[i].uid
      );

      prefs.putString(
        nameKey,
        areas[i].name
      );
    } else {
      prefs.remove(uidKey);
      prefs.remove(nameKey);
    }
  }
}

void setDefaultAreas() {
  areaCount = 2;

  areas[0].uid =
    31;

  areas[0].name =
    "Kyiv City";

  initializeArea(
    areas[0]
  );

  areas[1].uid =
    59;

  areas[1].name =
    "Zhytomyr Raion";

  initializeArea(
    areas[1]
  );

  clearAllDebugRecords();

  saveAreas();
}

void loadAreas() {
  if (
    !prefs.isKey(
      "acount"
    )
  ) {
    setDefaultAreas();
    return;
  }

  areaCount =
    prefs.getInt(
      "acount",
      0
    );

  if (areaCount < 0) {
    areaCount = 0;
  }

  if (
    areaCount > MAX_AREAS
  ) {
    areaCount =
      MAX_AREAS;
  }

  for (
    int i = 0;
    i < areaCount;
    i++
  ) {
    char uidKey[8];
    char nameKey[8];

    snprintf(
      uidKey,
      sizeof(uidKey),
      "uid%d",
      i
    );

    snprintf(
      nameKey,
      sizeof(nameKey),
      "nam%d",
      i
    );

    areas[i].uid =
      prefs.getInt(
        uidKey,
        0
      );

    areas[i].name =
      prefs.getString(
        nameKey,
        "Unknown"
      );

    initializeArea(
      areas[i]
    );
  }

  clearAllDebugRecords();
}

bool areaAlreadyAdded(
  int uid
) {
  for (
    int i = 0;
    i < areaCount;
    i++
  ) {
    if (
      areas[i].uid ==
      uid
    ) {
      return true;
    }
  }

  return false;
}

bool addArea(
  int uid,
  const String& name
) {
  if (
    areaCount >=
    MAX_AREAS
  ) {
    messageScreen(
      "AREAS",
      "Maximum 6 areas",
      RED
    );

    soundError();

    delay(900);

    return false;
  }

  if (
    areaAlreadyAdded(uid)
  ) {
    messageScreen(
      "AREAS",
      "Already added",
      YELLOW
    );

    delay(900);

    return false;
  }

  areas[areaCount].uid =
    uid;

  areas[areaCount].name =
    name;

  initializeArea(
    areas[areaCount]
  );

  clearDebugRecord(
    areaCount
  );

  areaCount++;

  saveAreas();

  beep(
    1500,
    100
  );

  return true;
}


// ============================================================
// SETTINGS
// ============================================================

void loadSettings() {
  prefs.begin(
    "airmonitor",
    false
  );

  savedSSID =
    prefs.getString(
      "ssid",
      ""
    );

  savedPassword =
    prefs.getString(
      "pass",
      ""
    );

  speakerVolume =
    prefs.getInt(
      "volume",
      170
    );

  demoMode =
    prefs.getBool(
      "demo",
      false
    );

  if (
    speakerVolume < 0 ||
    speakerVolume > 255
  ) {
    speakerVolume =
      170;
  }

  loadAreas();
}


// ============================================================
// WIFI
// ============================================================

bool connectWiFi(
  const String& ssid,
  const String& password,
  bool showProgress
) {
  if (
    ssid.length() == 0
  ) {
    return false;
  }

  WiFi.mode(WIFI_STA);

  WiFi.disconnect();

  delay(200);

  WiFi.begin(
    ssid.c_str(),
    password.c_str()
  );

  if (showProgress) {
    clearScreen();

    drawHeader(
      "CONNECTING WIFI",
      BLUE
    );

    M5Cardputer.Display.setTextColor(
      WHITE,
      BLACK
    );

    M5Cardputer.Display.setCursor(
      5,
      30
    );

    M5Cardputer.Display.println(
      ssid
    );
  }

  uint32_t started =
    millis();

  while (
    WiFi.status() !=
      WL_CONNECTED &&
    millis() - started <
      15000
  ) {
    M5Cardputer.update();

    delay(100);
  }

  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {
    if (showProgress) {
      M5Cardputer.Display.setTextColor(
        GREEN,
        BLACK
      );

      M5Cardputer.Display.setCursor(
        5,
        80
      );

      M5Cardputer.Display.println(
        "CONNECTED"
      );

      M5Cardputer.Display.print(
        "IP: "
      );

      M5Cardputer.Display.println(
        WiFi.localIP()
      );

      beep(
        1500,
        100
      );

      delay(700);
    }

    return true;
  }

  if (showProgress) {
    M5Cardputer.Display.setTextColor(
      RED,
      BLACK
    );

    M5Cardputer.Display.setCursor(
      5,
      80
    );

    M5Cardputer.Display.println(
      "FAILED"
    );

    soundError();

    delay(900);
  }

  return false;
}


// ============================================================
// WIFI MENU
// ============================================================

void wifiMenu() {
  String oldSSID =
    savedSSID;

  String oldPassword =
    savedPassword;

  clearScreen();

  drawHeader(
    "WIFI SCAN",
    BLUE
  );

  M5Cardputer.Display.setTextColor(
    WHITE,
    BLACK
  );

  M5Cardputer.Display.setCursor(
    5,
    30
  );

  M5Cardputer.Display.println(
    "Scanning..."
  );

  WiFi.mode(WIFI_STA);

  WiFi.disconnect();

  delay(200);

  int count =
    WiFi.scanNetworks();

  if (count <= 0) {
    messageScreen(
      "WIFI",
      "No networks found",
      RED
    );

    delay(900);

    if (
      oldSSID.length() > 0
    ) {
      connectWiFi(
        oldSSID,
        oldPassword,
        false
      );
    }

    return;
  }

  const int PAGE_SIZE = 5;

  int page = 0;

  while (true) {
    int pages =
      (
        count +
        PAGE_SIZE -
        1
      ) /
      PAGE_SIZE;

    clearScreen();

    drawHeader(
      "SELECT WIFI",
      BLUE
    );

    int start =
      page *
      PAGE_SIZE;

    for (
      int row = 0;
      row < PAGE_SIZE;
      row++
    ) {
      int index =
        start + row;

      if (
        index >= count
      ) {
        break;
      }

      String ssid =
        WiFi.SSID(index);

      if (
        ssid.length() > 20
      ) {
        ssid =
          ssid.substring(
            0,
            20
          );
      }

      M5Cardputer.Display.setTextColor(
        WHITE,
        BLACK
      );

      M5Cardputer.Display.setCursor(
        5,
        22 +
        row * 17
      );

      M5Cardputer.Display.print(
        row + 1
      );

      M5Cardputer.Display.print(
        " "
      );

      M5Cardputer.Display.print(
        ssid
      );

      M5Cardputer.Display.print(
        " "
      );

      M5Cardputer.Display.print(
        WiFi.RSSI(index)
      );
    }

    M5Cardputer.Display.setTextColor(
      CYAN,
      BLACK
    );

    M5Cardputer.Display.setCursor(
      5,
      116
    );

    M5Cardputer.Display.print(
      "N next P prev Q back"
    );

    char key =
      waitKey();

    if (key == 'q') {
      WiFi.scanDelete();

      if (
        oldSSID.length() > 0
      ) {
        connectWiFi(
          oldSSID,
          oldPassword,
          false
        );
      }

      return;
    }

    if (key == 'n') {
      if (
        page <
        pages - 1
      ) {
        page++;
      }

      continue;
    }

    if (key == 'p') {
      if (page > 0) {
        page--;
      }

      continue;
    }

    if (
      key >= '1' &&
      key <= '5'
    ) {
      int selected =
        page *
        PAGE_SIZE +
        (
          key - '1'
        );

      if (
        selected >= count
      ) {
        continue;
      }

      String newSSID =
        WiFi.SSID(selected);

      String password =
        readText(
          "WIFI PASSWORD",
          true,
          63
        );

      WiFi.scanDelete();

      if (
        connectWiFi(
          newSSID,
          password,
          true
        )
      ) {
        savedSSID =
          newSSID;

        savedPassword =
          password;

        prefs.putString(
          "ssid",
          savedSSID
        );

        prefs.putString(
          "pass",
          savedPassword
        );

        return;
      }

      if (
        oldSSID.length() > 0
      ) {
        connectWiFi(
          oldSSID,
          oldPassword,
          false
        );
      }

      return;
    }
  }
}


// ============================================================
// AREA MENUS
// ============================================================

void addManualArea() {
  String uidText =
    readText(
      "LOCATION UID",
      false,
      5
    );

  int uid =
    uidText.toInt();

  if (uid <= 0) {
    messageScreen(
      "AREA",
      "Invalid UID",
      RED
    );

    soundError();

    delay(800);

    return;
  }

  String name =
    readText(
      "DISPLAY NAME",
      false,
      22
    );

  if (
    name.length() == 0
  ) {
    name =
      "UID " +
      String(uid);
  }

  addArea(
    uid,
    name
  );
}

void addAreaMenu() {
  clearScreen();

  drawHeader(
    "ADD AREA",
    MAGENTA
  );

  M5Cardputer.Display.setTextColor(
    WHITE,
    BLACK
  );

  M5Cardputer.Display.setCursor(
    5,
    22
  );

  M5Cardputer.Display.println(
    "1 Kyiv City"
  );

  M5Cardputer.Display.println(
    "2 Zhytomyr Raion"
  );

  M5Cardputer.Display.println(
    "3 Kyiv Oblast"
  );

  M5Cardputer.Display.println(
    "4 Zhytomyr Oblast"
  );

  M5Cardputer.Display.println(
    "5 Manual UID"
  );

  M5Cardputer.Display.println(
    "Q Back"
  );

  char key =
    waitKey();

  if (key == '1') {
    addArea(
      31,
      "Kyiv City"
    );
  }

  else if (key == '2') {
    addArea(
      59,
      "Zhytomyr Raion"
    );
  }

  else if (key == '3') {
    addArea(
      14,
      "Kyiv Oblast"
    );
  }

  else if (key == '4') {
    addArea(
      10,
      "Zhytomyr Oblast"
    );
  }

  else if (key == '5') {
    addManualArea();
  }
}

void deleteAreaMenu() {
  if (
    areaCount == 0
  ) {
    return;
  }

  clearScreen();

  drawHeader(
    "DELETE AREA",
    RED
  );

  M5Cardputer.Display.setTextColor(
    WHITE,
    BLACK
  );

  for (
    int i = 0;
    i < areaCount;
    i++
  ) {
    M5Cardputer.Display.setCursor(
      5,
      22 +
      i * 16
    );

    M5Cardputer.Display.print(
      i + 1
    );

    M5Cardputer.Display.print(
      " "
    );

    M5Cardputer.Display.print(
      areas[i].name
    );
  }

  M5Cardputer.Display.setCursor(
    5,
    120
  );

  M5Cardputer.Display.print(
    "Q cancel"
  );

  char key =
    waitKey();

  if (key == 'q') {
    return;
  }

  if (
    key >= '1' &&
    key <= '6'
  ) {
    int index =
      key - '1';

    if (
      index >=
      areaCount
    ) {
      return;
    }

    for (
      int i = index;
      i <
        areaCount - 1;
      i++
    ) {
      areas[i] =
        areas[i + 1];

      debugRecords[i] =
        debugRecords[i + 1];
    }

    areaCount--;

    clearDebugRecord(
      areaCount
    );

    homePage = 0;

    saveAreas();

    beep(
      1000,
      100
    );
  }
}

void areasMenu() {
  while (true) {
    clearScreen();

    drawHeader(
      "MONITORED AREAS",
      MAGENTA
    );

    M5Cardputer.Display.setTextColor(
      WHITE,
      BLACK
    );

    for (
      int i = 0;
      i < areaCount;
      i++
    ) {
      M5Cardputer.Display.setCursor(
        5,
        21 +
        i * 15
      );

      M5Cardputer.Display.print(
        i + 1
      );

      M5Cardputer.Display.print(
        " "
      );

      String name =
        areas[i].name;

      if (
        name.length() > 18
      ) {
        name =
          name.substring(
            0,
            18
          );
      }

      M5Cardputer.Display.print(
        name
      );

      M5Cardputer.Display.print(
        " ["
      );

      M5Cardputer.Display.print(
        areas[i].uid
      );

      M5Cardputer.Display.print(
        "]"
      );
    }

    M5Cardputer.Display.setTextColor(
      CYAN,
      BLACK
    );

    M5Cardputer.Display.setCursor(
      5,
      116
    );

    M5Cardputer.Display.print(
      "A add D del X reset Q back"
    );

    char key =
      waitKey();

    if (key == 'q') {
      return;
    }

    if (key == 'a') {
      addAreaMenu();
    }

    else if (key == 'd') {
      deleteAreaMenu();
    }

    else if (key == 'x') {
      setDefaultAreas();

      homePage = 0;

      messageScreen(
        "AREAS",
        "Defaults restored",
        GREEN
      );

      delay(700);
    }
  }
}


// ============================================================
// VOLUME
// ============================================================

void volumeMenu() {
  clearScreen();

  drawHeader("VOLUME");

  M5Cardputer.Display.setTextColor(
    WHITE,
    BLACK
  );

  M5Cardputer.Display.setCursor(
    8,
    25
  );

  M5Cardputer.Display.println(
    "1 Mute"
  );

  M5Cardputer.Display.println(
    "2 Quiet"
  );

  M5Cardputer.Display.println(
    "3 Normal"
  );

  M5Cardputer.Display.println(
    "4 Loud"
  );

  M5Cardputer.Display.println(
    "5 Maximum"
  );

  M5Cardputer.Display.println(
    "Q Back"
  );

  char key =
    waitKey();

  if (key == 'q') {
    return;
  }

  if (key == '1') {
    speakerVolume = 0;
  }

  else if (key == '2') {
    speakerVolume = 60;
  }

  else if (key == '3') {
    speakerVolume = 130;
  }

  else if (key == '4') {
    speakerVolume = 190;
  }

  else if (key == '5') {
    speakerVolume = 250;
  }

  if (
    key >= '1' &&
    key <= '5'
  ) {
    prefs.putInt(
      "volume",
      speakerVolume
    );

    M5Cardputer.Speaker.setVolume(
      speakerVolume
    );

    if (
      speakerVolume > 0
    ) {
      beep(
        1200,
        100
      );
    }
  }
}


// ============================================================
// THREAT HELPERS
// ============================================================

uint16_t threatBit(
  const char* type
) {
  if (
    type == nullptr
  ) {
    return 0;
  }

  if (
    strcmp(
      type,
      "tactic_aircraft_activity"
    ) == 0
  ) {
    return TH_TACTICAL_AIR;
  }

  if (
    strcmp(
      type,
      "strategic_aircraft_activity"
    ) == 0
  ) {
    return TH_STRATEGIC_AIR;
  }

  if (
    strcmp(
      type,
      "mig31k_departure"
    ) == 0
  ) {
    return TH_MIG31K;
  }

  if (
    strcmp(
      type,
      "ballistic_missiles"
    ) == 0
  ) {
    return TH_BALLISTIC;
  }

  if (
    strcmp(
      type,
      "cruise_missiles"
    ) == 0
  ) {
    return TH_CRUISE;
  }

  if (
    strcmp(
      type,
      "unspecified_missiles"
    ) == 0
  ) {
    return TH_MISSILES;
  }

  if (
    strcmp(
      type,
      "drones"
    ) == 0
  ) {
    return TH_DRONES;
  }

  if (
    strcmp(
      type,
      "guided_aerial_bombs"
    ) == 0
  ) {
    return TH_GUIDED_BOMBS;
  }

  if (
    strcmp(
      type,
      "air_defense"
    ) == 0
  ) {
    return TH_AIR_DEFENSE;
  }

  return TH_UNKNOWN;
}

String threatName(
  uint16_t bit
) {
  if (
    bit ==
    TH_TACTICAL_AIR
  ) {
    return "TACTICAL AIR";
  }

  if (
    bit ==
    TH_STRATEGIC_AIR
  ) {
    return "STRATEGIC AIR";
  }

  if (
    bit ==
    TH_MIG31K
  ) {
    return "MIG-31K";
  }

  if (
    bit ==
    TH_BALLISTIC
  ) {
    return "BALLISTIC";
  }

  if (
    bit ==
    TH_CRUISE
  ) {
    return "CRUISE";
  }

  if (
    bit ==
    TH_MISSILES
  ) {
    return "MISSILES";
  }

  if (
    bit ==
    TH_DRONES
  ) {
    return "DRONES";
  }

  if (
    bit ==
    TH_GUIDED_BOMBS
  ) {
    return "GUIDED BOMBS";
  }

  if (
    bit ==
    TH_AIR_DEFENSE
  ) {
    return "AIR DEF";
  }

  return "UNKNOWN";
}

String threatMaskText(
  uint16_t mask,
  int maxLength = 30
) {
  if (mask == 0) {
    return "-";
  }

  const uint16_t bits[] = {
    TH_BALLISTIC,
    TH_CRUISE,
    TH_DRONES,
    TH_MIG31K,
    TH_STRATEGIC_AIR,
    TH_TACTICAL_AIR,
    TH_MISSILES,
    TH_GUIDED_BOMBS,
    TH_AIR_DEFENSE,
    TH_UNKNOWN
  };

  String output = "";

  for (
    int i = 0;
    i < 10;
    i++
  ) {
    if (
      (
        mask &
        bits[i]
      ) == 0
    ) {
      continue;
    }

    String addition = "";

    if (
      output.length() > 0
    ) {
      addition = ",";
    }

    addition +=
      threatName(
        bits[i]
      );

    if (
      output.length() +
      addition.length() >
      maxLength
    ) {
      output += "...";
      break;
    }

    output += addition;
  }

  return output;
}


// ============================================================
// STATE
// ============================================================

String stateText(
  char state
) {
  if (state == 'A') {
    return "ALERT";
  }

  if (state == 'P') {
    return "PARTIAL";
  }

  if (state == 'N') {
    return "CLEAR";
  }

  return "?";
}

uint16_t stateColor(
  char state
) {
  if (state == 'A') {
    return RED;
  }

  if (state == 'P') {
    return YELLOW;
  }

  if (state == 'N') {
    return GREEN;
  }

  return WHITE;
}


// ============================================================
// NOTIFICATIONS
// ============================================================

void showStateNotification(
  int index,
  char state
) {
  if (
    index < 0 ||
    index >= areaCount
  ) {
    return;
  }

  uint16_t background =
    BLACK;

  uint16_t foreground =
    WHITE;

  String title = "";

  if (state == 'A') {
    background = RED;
    title = "AIR ALERT";
  }

  else if (state == 'P') {
    background = YELLOW;
    foreground = BLACK;
    title = "PARTIAL ALERT";
  }

  else if (state == 'N') {
    background = GREEN;
    foreground = BLACK;
    title = "ALL CLEAR";
  }

  clearScreen(
    background
  );

  M5Cardputer.Display.setTextColor(
    foreground,
    background
  );

  M5Cardputer.Display.setTextSize(2);

  M5Cardputer.Display.setCursor(
    10,
    25
  );

  M5Cardputer.Display.println(
    title
  );

  M5Cardputer.Display.setTextSize(1);

  M5Cardputer.Display.setCursor(
    10,
    70
  );

  M5Cardputer.Display.println(
    areas[index].name
  );

  if (state == 'A') {
    soundAlert();
  }

  else if (state == 'P') {
    soundPartial();
  }

  else if (state == 'N') {
    soundClear();
  }

  delay(1100);
}

void showThreatNotification(
  int index,
  uint16_t mask
) {
  if (
    index < 0 ||
    index >= areaCount
  ) {
    return;
  }

  clearScreen();

  drawHeader(
    "NEW THREAT",
    RED
  );

  M5Cardputer.Display.setTextColor(
    WHITE,
    BLACK
  );

  M5Cardputer.Display.setCursor(
    5,
    28
  );

  M5Cardputer.Display.println(
    areas[index].name
  );

  M5Cardputer.Display.setTextColor(
    RED,
    BLACK
  );

  M5Cardputer.Display.setTextSize(2);

  M5Cardputer.Display.setCursor(
    5,
    55
  );

  M5Cardputer.Display.println(
    threatMaskText(
      mask,
      19
    )
  );

  soundNewThreat();

  delay(1200);
}


// ============================================================
// DEMO MODE
// ============================================================

int findAreaByUid(
  int uid
) {
  for (
    int i = 0;
    i < areaCount;
    i++
  ) {
    if (
      areas[i].uid ==
      uid
    ) {
      return i;
    }
  }

  return -1;
}

void demoSetArea(
  int uid,
  char newState,
  uint16_t newThreats,
  const String& startTime,
  bool notify
) {
  int index =
    findAreaByUid(uid);

  if (index < 0) {
    return;
  }

  char oldState =
    areas[index].state;

  uint16_t oldThreats =
    areas[index].threatMask;

  areas[index].state =
    newState;

  areas[index].stateInitialized =
    true;

  areas[index].threatMask =
    newThreats;

  areas[index].threatsInitialized =
    true;

  if (
    newState == 'N'
  ) {
    areas[index].
    alertStartedEpoch =
      0;

    areas[index].
    alertStartedText =
      "--:--";
  } else {
    areas[index].
    alertStartedText =
      startTime;
  }

  if (!notify) {
    return;
  }

  if (
    oldState !=
    newState
  ) {
    showStateNotification(
      index,
      newState
    );
  }

  uint16_t added =
    newThreats &
    ~oldThreats;

  if (added != 0) {
    showThreatNotification(
      index,
      added
    );
  }
}

void applyDemoScenario(
  int scenario,
  bool notify = true
) {
  demoScenario =
    scenario;

  if (scenario == 0) {
    demoSetArea(
      31,
      'N',
      0,
      "--:--",
      notify
    );

    demoSetArea(
      59,
      'N',
      0,
      "--:--",
      notify
    );
  }

  else if (scenario == 1) {
    demoSetArea(
      31,
      'A',
      TH_DRONES,
      "20:14",
      notify
    );

    demoSetArea(
      59,
      'N',
      0,
      "--:--",
      notify
    );
  }

  else if (scenario == 2) {
    demoSetArea(
      31,
      'A',
      TH_DRONES |
      TH_BALLISTIC,
      "20:14",
      notify
    );

    demoSetArea(
      59,
      'N',
      0,
      "--:--",
      notify
    );
  }

  else if (scenario == 3) {
    demoSetArea(
      31,
      'N',
      0,
      "--:--",
      notify
    );

    demoSetArea(
      59,
      'A',
      TH_DRONES,
      "20:37",
      notify
    );
  }

  else if (scenario == 4) {
    demoSetArea(
      31,
      'A',
      TH_CRUISE,
      "20:14",
      notify
    );

    demoSetArea(
      59,
      'A',
      TH_DRONES |
      TH_CRUISE,
      "20:37",
      notify
    );
  }

  else if (scenario == 5) {
    demoSetArea(
      31,
      'A',
      TH_MIG31K,
      "21:02",
      notify
    );

    demoSetArea(
      59,
      'A',
      TH_MIG31K,
      "21:02",
      notify
    );
  }
}

void nextDemoScenario() {
  demoScenario++;

  if (
    demoScenario > 5
  ) {
    demoScenario = 0;
  }

  applyDemoScenario(
    demoScenario,
    true
  );
}

void demoMenu() {
  while (true) {
    clearScreen();

    drawHeader(
      demoMode
      ? "DEMO MODE: ON"
      : "DEMO MODE: OFF",
      demoMode
      ? YELLOW
      : CYAN
    );

    M5Cardputer.Display.setTextColor(
      WHITE,
      BLACK
    );

    M5Cardputer.Display.setCursor(
      6,
      20
    );

    M5Cardputer.Display.println(
      "1 Toggle ON/OFF"
    );

    M5Cardputer.Display.println(
      "2 ALL CLEAR"
    );

    M5Cardputer.Display.println(
      "3 Kyiv: DRONES"
    );

    M5Cardputer.Display.println(
      "4 Kyiv: DRONE+BALL"
    );

    M5Cardputer.Display.println(
      "5 Zhytomyr: DRONES"
    );

    M5Cardputer.Display.println(
      "6 BOTH AREAS"
    );

    M5Cardputer.Display.println(
      "7 MIG-31K"
    );

    M5Cardputer.Display.println(
      "Q Back"
    );

    char key =
      waitKey();

    if (key == 'q') {
      return;
    }

    if (key == '1') {
      demoMode =
        !demoMode;

      prefs.putBool(
        "demo",
        demoMode
      );

      if (demoMode) {
        demoScenario = 0;

        applyDemoScenario(
          0,
          false
        );
      } else {
        for (
          int i = 0;
          i < areaCount;
          i++
        ) {
          initializeArea(
            areas[i]
          );
        }
      }
    }

    else if (
      key >= '2' &&
      key <= '7'
    ) {
      if (!demoMode) {
        demoMode = true;

        prefs.putBool(
          "demo",
          true
        );
      }

      applyDemoScenario(
        key - '2',
        true
      );

      return;
    }
  }
}


// ============================================================
// API HELPERS
// ============================================================

void addError(
  const String& text
) {
  if (
    lastError.length() == 0
  ) {
    lastError = text;
  } else {
    lastError += "/";
    lastError += text;
  }
}

int jsonUid(
  JsonVariantConst value
) {
  if (
    value.is<int>()
  ) {
    return value.as<int>();
  }

  if (
    value.is<long>()
  ) {
    return (int)value.as<long>();
  }

  const char* text =
    value.as<const char*>();

  if (
    text == nullptr
  ) {
    return 0;
  }

  return atoi(text);
}


// ============================================================
// MATCH HELPERS
// ============================================================

bool containsZhytomyrRaion(
  const String& raion
) {
  return
    raion.indexOf(
      "Житомир"
    ) >= 0;
}

bool containsKyivTitle(
  const String& title,
  const String& titleEn
) {
  if (
    title.indexOf(
      "Київ"
    ) >= 0
  ) {
    return true;
  }

  if (
    titleEn.indexOf(
      "Kyiv"
    ) >= 0
  ) {
    return true;
  }

  return false;
}


// ============================================================
// REAL MATCH
// ============================================================

bool alertMatchesArea(
  const WatchArea& area,
  int locationUid,
  int oblastUid,
  const String& title,
  const String& titleEn,
  const String& raion
) {
  if (
    locationUid ==
    area.uid
  ) {
    return true;
  }

  if (
    area.uid == 31
  ) {
    if (
      containsKyivTitle(
        title,
        titleEn
      )
    ) {
      return true;
    }
  }

  if (
    area.uid == 59
  ) {
    if (
      locationUid == 10
    ) {
      return true;
    }

    if (
      oblastUid == 10 &&
      containsZhytomyrRaion(
        raion
      )
    ) {
      return true;
    }
  }

  if (
    isOblastUid(
      area.uid
    )
  ) {
    if (
      locationUid ==
      area.uid
    ) {
      return true;
    }

    if (
      oblastUid ==
      area.uid
    ) {
      return true;
    }
  }

  if (
    area.raionUk.length() > 0 &&
    raion ==
    area.raionUk
  ) {
    return true;
  }

  return false;
}


// ============================================================
// DEBUG CANDIDATE SCORE
// ============================================================

int debugCandidateScore(
  const WatchArea& area,
  int locationUid,
  int oblastUid,
  const String& title,
  const String& titleEn,
  const String& raion
) {
  if (
    locationUid ==
    area.uid
  ) {
    return 100;
  }

  if (
    area.uid == 31
  ) {
    if (
      containsKyivTitle(
        title,
        titleEn
      )
    ) {
      return 90;
    }

    return -1;
  }

  if (
    area.uid == 59
  ) {
    if (
      oblastUid == 10 &&
      containsZhytomyrRaion(
        raion
      )
    ) {
      return 90;
    }

    if (
      locationUid == 10
    ) {
      return 70;
    }

    if (
      oblastUid == 10
    ) {
      return 20;
    }

    return -1;
  }

  if (
    isOblastUid(
      area.uid
    ) &&
    oblastUid ==
    area.uid
  ) {
    return 70;
  }

  return -1;
}


// ============================================================
// STORE LIVE DEBUG CANDIDATE
// ============================================================

void considerDebugCandidate(
  int areaIndex,
  int score,
  bool matched,
  int locationUid,
  int oblastUid,
  const String& title,
  const String& titleEn,
  const String& locationType,
  const String& raion,
  const String& startedRaw,
  const String& startedLocal,
  int threatCount,
  const String& threatsText
) {
  if (
    areaIndex < 0 ||
    areaIndex >= areaCount ||
    score < 0
  ) {
    return;
  }

  LiveDebugRecord& d =
    debugRecords[areaIndex];

  d.candidateNow =
    true;

  d.candidateCount++;

  if (matched) {
    d.matchedNow =
      true;
  }

  if (
    score <
    d.score
  ) {
    return;
  }

  d.score =
    score;

  d.everSeen =
    true;

  d.locationUid =
    locationUid;

  d.oblastUid =
    oblastUid;

  d.title =
    title;

  d.titleEn =
    titleEn;

  d.locationType =
    locationType;

  d.raion =
    raion;

  d.startedRaw =
    startedRaw;

  d.startedLocal =
    startedLocal;

  d.threatCount =
    threatCount;

  d.threatsText =
    threatsText;

  d.lastSeenMillis =
    millis();
}


// ============================================================
// COMPACT STATUS API
// ============================================================

bool fetchStatusMap() {
  lastStatusHttp = 0;

  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {
    addError("WIFI");
    return false;
  }

  if (!tokenConfigured()) {
    addError("TOKEN");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;

  https.setTimeout(
    10000
  );

  if (
    !https.begin(
      client,
      STATUS_URL
    )
  ) {
    addError(
      "STATUS INIT"
    );

    return false;
  }

  https.addHeader(
    "Authorization",
    String("Bearer ") +
    API_TOKEN
  );

  https.addHeader(
    "Accept-Encoding",
    "identity"
  );

  int code =
    https.GET();

  lastStatusHttp =
    code;

  if (
    code != 200
  ) {
    addError(
      "S" +
      String(code)
    );

    https.end();

    return false;
  }

  String data =
    https.getString();

  https.end();

  data.trim();

  if (
    data.length() >= 2 &&
    data[0] == '"' &&
    data[
      data.length() - 1
    ] == '"'
  ) {
    data =
      data.substring(
        1,
        data.length() - 1
      );
  }

  for (
    int i = 0;
    i < areaCount;
    i++
  ) {
    int uid =
      areas[i].uid;

    char newState =
      '?';

    if (
      uid >= 0 &&
      uid <
      data.length()
    ) {
      char c =
        data.charAt(uid);

      if (
        c == 'A' ||
        c == 'P' ||
        c == 'N'
      ) {
        newState =
          c;
      }
    }

    if (
      newState == '?'
    ) {
      continue;
    }

    if (
      !areas[i].
      stateInitialized
    ) {
      areas[i].state =
        newState;

      areas[i].
      stateInitialized =
        true;

      if (
        newState == 'A' ||
        newState == 'P'
      ) {
        showStateNotification(
          i,
          newState
        );
      }

      continue;
    }

    char oldState =
      areas[i].state;

    if (
      oldState !=
      newState
    ) {
      areas[i].state =
        newState;

      if (
        newState == 'N'
      ) {
        areas[i].
        alertStartedEpoch =
          0;

        areas[i].
        alertStartedText =
          "--:--";

        areas[i].
        threatMask =
          0;
      }

      showStateNotification(
        i,
        newState
      );
    }
  }

  return true;
}


// ============================================================
// ACTIVE DETAILS API
// ============================================================

bool fetchAlertDetails() {
  lastDetailsHttp =
    0;

  lastJsonOK =
    false;

  lastDetailsPayloadBytes =
    0;

  lastDetailsAlertCount =
    0;

  lastDetailsAirRaidCount =
    0;

  lastDetailsMatchCount =
    0;

  lastMatchedThreatCount =
    0;

  beginDebugFetch();

  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {
    addError("WIFI");
    return false;
  }

  if (!tokenConfigured()) {
    addError("TOKEN");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;

  https.setTimeout(
    15000
  );

  if (
    !https.begin(
      client,
      DETAILS_URL
    )
  ) {
    addError(
      "DETAIL INIT"
    );

    return false;
  }

  https.addHeader(
    "Authorization",
    String("Bearer ") +
    API_TOKEN
  );

  https.addHeader(
    "Accept-Encoding",
    "identity"
  );

  int code =
    https.GET();

  lastDetailsHttp =
    code;

  if (
    code != 200
  ) {
    addError(
      "D" +
      String(code)
    );

    https.end();

    return false;
  }

  String payload =
    https.getString();

  https.end();

  lastDetailsPayloadBytes =
    payload.length();

  if (
    payload.length() == 0
  ) {
    addError(
      "EMPTY JSON"
    );

    return false;
  }

  JsonDocument doc;

  DeserializationError err =
    deserializeJson(
      doc,
      payload
    );

  payload = "";

  if (err) {
    addError(
      "JSON"
    );

    return false;
  }

  lastJsonOK =
    true;

  JsonArray alerts =
    doc[
      "alerts"
    ].as<JsonArray>();

  if (
    alerts.isNull()
  ) {
    addError(
      "NO ALERTS"
    );

    return false;
  }

  lastDetailsAlertCount =
    alerts.size();

  uint16_t newMasks[
    MAX_AREAS
  ];

  time_t earliestStart[
    MAX_AREAS
  ];

  for (
    int i = 0;
    i < MAX_AREAS;
    i++
  ) {
    newMasks[i] = 0;
    earliestStart[i] = 0;
  }

  for (
    JsonObject alert :
    alerts
  ) {
    const char* alertTypeRaw =
      alert[
        "alert_type"
      ] |
      "";

    if (
      strcmp(
        alertTypeRaw,
        "air_raid"
      ) != 0
    ) {
      continue;
    }

    lastDetailsAirRaidCount++;

    int locationUid =
      jsonUid(
        alert[
          "location_uid"
        ]
      );

    int oblastUid =
      jsonUid(
        alert[
          "location_oblast_uid"
        ]
      );

    String title =
      String(
        alert[
          "location_title"
        ] |
        ""
      );

    String titleEn =
      String(
        alert[
          "location_title_en"
        ] |
        ""
      );

    String locationType =
      String(
        alert[
          "location_type"
        ] |
        ""
      );

    String raion =
      String(
        alert[
          "location_raion"
        ] |
        ""
      );

    String startedRaw =
      String(
        alert[
          "started_at"
        ] |
        ""
      );

    time_t alertEpoch = 0;

    if (
      startedRaw.length() >= 19
    ) {
      parseIsoUtc(
        startedRaw,
        alertEpoch
      );
    }

    String startedLocal =
      formatKyivTime(
        alertEpoch
      );

    uint16_t currentMask =
      0;

    int currentThreatCount =
      0;

    JsonArray threats =
      alert[
        "threats"
      ].as<JsonArray>();

    if (
      !threats.isNull()
    ) {
      for (
        JsonObject threat :
        threats
      ) {
        const char* type =
          threat[
            "threat_type"
          ] |
          "";

        currentMask |=
          threatBit(type);

        currentThreatCount++;
      }
    }

    String currentThreatText =
      threatMaskText(
        currentMask,
        35
      );

    for (
      int i = 0;
      i < areaCount;
      i++
    ) {
      bool matched =
        alertMatchesArea(
          areas[i],
          locationUid,
          oblastUid,
          title,
          titleEn,
          raion
        );

      int dbgScore =
        debugCandidateScore(
          areas[i],
          locationUid,
          oblastUid,
          title,
          titleEn,
          raion
        );

      considerDebugCandidate(
        i,
        dbgScore,
        matched,
        locationUid,
        oblastUid,
        title,
        titleEn,
        locationType,
        raion,
        startedRaw,
        startedLocal,
        currentThreatCount,
        currentThreatText
      );

      if (!matched) {
        continue;
      }

      lastDetailsMatchCount++;

      newMasks[i] |=
        currentMask;

      lastMatchedThreatCount +=
        currentThreatCount;

      if (
        alertEpoch > 0
      ) {
        if (
          earliestStart[i] == 0 ||
          alertEpoch <
          earliestStart[i]
        ) {
          earliestStart[i] =
            alertEpoch;
        }
      }
    }
  }

  for (
    int i = 0;
    i < areaCount;
    i++
  ) {
    uint16_t oldMask =
      areas[i].threatMask;

    uint16_t newMask =
      newMasks[i];

    if (
      earliestStart[i] > 0
    ) {
      areas[i].
      alertStartedEpoch =
        earliestStart[i];

      areas[i].
      alertStartedText =
        formatKyivTime(
          earliestStart[i]
        );
    }

    else if (
      areas[i].state ==
      'N'
    ) {
      areas[i].
      alertStartedEpoch =
        0;

      areas[i].
      alertStartedText =
        "--:--";
    }

    if (
      !areas[i].
      threatsInitialized
    ) {
      areas[i].
      threatMask =
        newMask;

      areas[i].
      threatsInitialized =
        true;

      continue;
    }

    uint16_t added =
      newMask &
      ~oldMask;

    areas[i].
    threatMask =
      newMask;

    if (
      added != 0
    ) {
      showThreatNotification(
        i,
        added
      );
    }
  }

  return true;
}


// ============================================================
// REFRESH
// ============================================================

void refreshAll() {
  if (demoMode) {
    nextDemoScenario();
    return;
  }

  lastError = "";

  bool statusOK =
    fetchStatusMap();

  bool detailsOK =
    fetchAlertDetails();

  if (
    statusOK ||
    detailsOK
  ) {
    lastApiOK =
      millis();
  }
}


// ============================================================
// LIVE DEBUG DISPLAY
// ============================================================

void drawLiveDebugPage1(
  int index
) {
  clearScreen();

  drawHeader(
    "LIVE DEBUG 1/2",
    MAGENTA
  );

  M5Cardputer.Display.setTextColor(
    WHITE,
    BLACK
  );

  WatchArea& a =
    areas[index];

  LiveDebugRecord& d =
    debugRecords[index];

  int y = 18;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    a.name
  );

  M5Cardputer.Display.print(
    " UID:"
  );

  M5Cardputer.Display.println(
    a.uid
  );

  y += 13;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "State: "
  );

  M5Cardputer.Display.setTextColor(
    stateColor(
      a.state
    ),
    BLACK
  );

  M5Cardputer.Display.println(
    stateText(
      a.state
    )
  );

  M5Cardputer.Display.setTextColor(
    WHITE,
    BLACK
  );

  y += 13;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Candidate now: "
  );

  M5Cardputer.Display.println(
    d.candidateNow
    ? "YES"
    : "NO"
  );

  y += 13;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Matched now: "
  );

  M5Cardputer.Display.setTextColor(
    d.matchedNow
    ? GREEN
    : RED,
    BLACK
  );

  M5Cardputer.Display.println(
    d.matchedNow
    ? "YES"
    : "NO"
  );

  M5Cardputer.Display.setTextColor(
    WHITE,
    BLACK
  );

  y += 13;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Candidates: "
  );

  M5Cardputer.Display.println(
    d.candidateCount
  );

  y += 13;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Raw UID: "
  );

  if (d.everSeen) {
    M5Cardputer.Display.println(
      d.locationUid
    );
  } else {
    M5Cardputer.Display.println(
      "-"
    );
  }

  y += 13;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Oblast UID: "
  );

  if (d.everSeen) {
    M5Cardputer.Display.println(
      d.oblastUid
    );
  } else {
    M5Cardputer.Display.println(
      "-"
    );
  }

  y += 13;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Type: "
  );

  if (d.everSeen) {
    M5Cardputer.Display.println(
      d.locationType
    );
  } else {
    M5Cardputer.Display.println(
      "-"
    );
  }

  M5Cardputer.Display.setTextColor(
    CYAN,
    BLACK
  );

  M5Cardputer.Display.setCursor(
    4,
    120
  );

  M5Cardputer.Display.print(
    "N next  Q back"
  );
}

void drawLiveDebugPage2(
  int index
) {
  clearScreen();

  drawHeader(
    "LIVE DEBUG 2/2",
    MAGENTA
  );

  M5Cardputer.Display.setTextColor(
    WHITE,
    BLACK
  );

  LiveDebugRecord& d =
    debugRecords[index];

  int y = 18;

  String title =
    d.titleEn;

  if (
    title.length() == 0
  ) {
    title = "(no EN title)";
  }

  if (
    title.length() > 27
  ) {
    title =
      title.substring(
        0,
        27
      );
  }

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Name: "
  );

  M5Cardputer.Display.println(
    d.everSeen
    ? title
    : "-"
  );

  y += 13;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Since raw: "
  );

  if (
    d.startedRaw.length() >= 16
  ) {
    M5Cardputer.Display.println(
      d.startedRaw.substring(
        11,
        16
      )
    );
  } else {
    M5Cardputer.Display.println(
      "-"
    );
  }

  y += 13;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Since Kyiv: "
  );

  M5Cardputer.Display.println(
    d.everSeen
    ? d.startedLocal
    : "-"
  );

  y += 13;

  bool raionHit =
    containsZhytomyrRaion(
      d.raion
    );

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Zhyt raion hit: "
  );

  M5Cardputer.Display.println(
    raionHit
    ? "YES"
    : "NO"
  );

  y += 13;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Threat items: "
  );

  M5Cardputer.Display.println(
    d.everSeen
    ? d.threatCount
    : 0
  );

  y += 13;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Threat: "
  );

  String threats =
    d.threatsText;

  if (
    threats.length() > 26
  ) {
    threats =
      threats.substring(
        0,
        26
      );
  }

  M5Cardputer.Display.println(
    d.everSeen
    ? threats
    : "-"
  );

  y += 13;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Score: "
  );

  if (d.everSeen) {
    M5Cardputer.Display.println(
      d.score
    );
  } else {
    M5Cardputer.Display.println(
      "-"
    );
  }

  y += 13;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Stored raw: "
  );

  M5Cardputer.Display.println(
    d.everSeen
    ? "YES"
    : "NO"
  );

  M5Cardputer.Display.setTextColor(
    CYAN,
    BLACK
  );

  M5Cardputer.Display.setCursor(
    4,
    120
  );

  M5Cardputer.Display.print(
    "P prev  Q back"
  );
}

void liveDebugViewer(
  int index
) {
  if (
    index < 0 ||
    index >= areaCount
  ) {
    return;
  }

  int page = 0;

  while (true) {
    if (page == 0) {
      drawLiveDebugPage1(
        index
      );
    } else {
      drawLiveDebugPage2(
        index
      );
    }

    char key =
      waitKey();

    if (key == 'q') {
      return;
    }

    if (key == 'n') {
      page = 1;
    }

    if (key == 'p') {
      page = 0;
    }
  }
}

void liveDebugMenu() {
  while (true) {
    clearScreen();

    drawHeader(
      "LIVE DEBUG",
      MAGENTA
    );

    M5Cardputer.Display.setTextColor(
      WHITE,
      BLACK
    );

    for (
      int i = 0;
      i < areaCount;
      i++
    ) {
      M5Cardputer.Display.setCursor(
        5,
        22 +
        i * 16
      );

      M5Cardputer.Display.print(
        i + 1
      );

      M5Cardputer.Display.print(
        " "
      );

      String name =
        areas[i].name;

      if (
        name.length() > 17
      ) {
        name =
          name.substring(
            0,
            17
          );
      }

      M5Cardputer.Display.print(
        name
      );

      M5Cardputer.Display.print(
        " "
      );

      if (
        debugRecords[i].
        matchedNow
      ) {
        M5Cardputer.Display.setTextColor(
          GREEN,
          BLACK
        );

        M5Cardputer.Display.print(
          "MATCH"
        );
      }

      else if (
        debugRecords[i].
        candidateNow
      ) {
        M5Cardputer.Display.setTextColor(
          YELLOW,
          BLACK
        );

        M5Cardputer.Display.print(
          "CAND"
        );
      }

      else if (
        debugRecords[i].
        everSeen
      ) {
        M5Cardputer.Display.setTextColor(
          CYAN,
          BLACK
        );

        M5Cardputer.Display.print(
          "LAST"
        );
      }

      else {
        M5Cardputer.Display.print(
          "-"
        );
      }

      M5Cardputer.Display.setTextColor(
        WHITE,
        BLACK
      );
    }

    M5Cardputer.Display.setTextColor(
      CYAN,
      BLACK
    );

    M5Cardputer.Display.setCursor(
      5,
      118
    );

    M5Cardputer.Display.print(
      "R refresh  Q back"
    );

    char key =
      waitKey();

    if (key == 'q') {
      return;
    }

    if (key == 'r') {
      if (!demoMode) {
        messageScreen(
          "LIVE DEBUG",
          "Refreshing..."
        );

        refreshAll();
      }

      continue;
    }

    if (
      key >= '1' &&
      key <= '6'
    ) {
      int index =
        key - '1';

      if (
        index <
        areaCount
      ) {
        liveDebugViewer(
          index
        );
      }
    }
  }
}


// ============================================================
// HOME
// ============================================================

void drawHome() {
  clearScreen();

  if (demoMode) {
    drawHeader(
      "AIR MONITOR [DEMO]",
      YELLOW
    );
  } else {
    drawHeader(
      "AIR ALERT MONITOR",
      CYAN
    );
  }

  if (
    !demoMode &&
    !tokenConfigured()
  ) {
    M5Cardputer.Display.setTextColor(
      RED,
      BLACK
    );

    M5Cardputer.Display.setCursor(
      5,
      28
    );

    M5Cardputer.Display.println(
      "API TOKEN NOT SET"
    );

    M5Cardputer.Display.setTextColor(
      CYAN,
      BLACK
    );

    M5Cardputer.Display.setCursor(
      5,
      118
    );

    M5Cardputer.Display.print(
      "M Menu"
    );

    return;
  }

  if (
    areaCount == 0
  ) {
    M5Cardputer.Display.setTextColor(
      WHITE,
      BLACK
    );

    M5Cardputer.Display.setCursor(
      5,
      30
    );

    M5Cardputer.Display.println(
      "No monitored areas"
    );

    return;
  }

  int pageCount =
    (
      areaCount +
      AREAS_PER_PAGE -
      1
    ) /
    AREAS_PER_PAGE;

  if (
    homePage >= pageCount
  ) {
    homePage =
      pageCount - 1;
  }

  if (
    homePage < 0
  ) {
    homePage = 0;
  }

  int start =
    homePage *
    AREAS_PER_PAGE;

  for (
    int row = 0;
    row <
      AREAS_PER_PAGE;
    row++
  ) {
    int index =
      start + row;

    if (
      index >=
      areaCount
    ) {
      break;
    }

    int y =
      19 +
      row * 41;

    String name =
      areas[index].name;

    if (
      name.length() > 18
    ) {
      name =
        name.substring(
          0,
          18
        );
    }

    M5Cardputer.Display.setTextColor(
      WHITE,
      BLACK
    );

    M5Cardputer.Display.setCursor(
      4,
      y
    );

    M5Cardputer.Display.print(
      name
    );

    M5Cardputer.Display.setTextColor(
      stateColor(
        areas[index].state
      ),
      BLACK
    );

    M5Cardputer.Display.setCursor(
      174,
      y
    );

    M5Cardputer.Display.print(
      stateText(
        areas[index].state
      )
    );

    M5Cardputer.Display.setTextColor(
      CYAN,
      BLACK
    );

    M5Cardputer.Display.setCursor(
      4,
      y + 12
    );

    M5Cardputer.Display.print(
      "Since: "
    );

    if (
      areas[index].state ==
      'N'
    ) {
      M5Cardputer.Display.print(
        "--:--"
      );
    } else {
      M5Cardputer.Display.print(
        areas[index].
        alertStartedText
      );
    }

    M5Cardputer.Display.setTextColor(
      YELLOW,
      BLACK
    );

    M5Cardputer.Display.setCursor(
      4,
      y + 24
    );

    M5Cardputer.Display.print(
      "Threat: "
    );

    M5Cardputer.Display.print(
      threatMaskText(
        areas[index].
        threatMask,
        29
      )
    );
  }

  M5Cardputer.Display.setCursor(
    4,
    102
  );

  if (demoMode) {
    M5Cardputer.Display.setTextColor(
      YELLOW,
      BLACK
    );

    M5Cardputer.Display.print(
      "DEMO #"
    );

    M5Cardputer.Display.print(
      demoScenario
    );
  } else {
    if (
      WiFi.status() ==
      WL_CONNECTED
    ) {
      M5Cardputer.Display.setTextColor(
        GREEN,
        BLACK
      );

      M5Cardputer.Display.print(
        "WiFi "
      );

      M5Cardputer.Display.print(
        WiFi.RSSI()
      );

      M5Cardputer.Display.print(
        "dBm "
      );
    } else {
      M5Cardputer.Display.setTextColor(
        RED,
        BLACK
      );

      M5Cardputer.Display.print(
        "WiFi OFF "
      );
    }

    if (
      lastError.length() > 0
    ) {
      M5Cardputer.Display.setTextColor(
        RED,
        BLACK
      );

      M5Cardputer.Display.print(
        lastError
      );
    }

    else if (
      lastApiOK > 0
    ) {
      M5Cardputer.Display.setTextColor(
        GREEN,
        BLACK
      );

      M5Cardputer.Display.print(
        "API "
      );

      M5Cardputer.Display.print(
        (
          millis() -
          lastApiOK
        ) /
        1000
      );

      M5Cardputer.Display.print(
        "s"
      );
    }
  }

  M5Cardputer.Display.setTextColor(
    CYAN,
    BLACK
  );

  M5Cardputer.Display.setCursor(
    4,
    121
  );

  if (demoMode) {
    M5Cardputer.Display.print(
      "M Menu R Next demo"
    );
  } else {
    M5Cardputer.Display.print(
      "M Menu R Refresh"
    );
  }

  if (
    pageCount > 1
  ) {
    M5Cardputer.Display.print(
      " N/P"
    );
  }
}


// ============================================================
// SOUND TEST
// ============================================================

void testSounds() {
  while (true) {
    clearScreen();

    drawHeader(
      "SOUND TEST"
    );

    M5Cardputer.Display.setTextColor(
      WHITE,
      BLACK
    );

    M5Cardputer.Display.setCursor(
      8,
      27
    );

    M5Cardputer.Display.println(
      "1 Air alert"
    );

    M5Cardputer.Display.println(
      "2 Partial"
    );

    M5Cardputer.Display.println(
      "3 All clear"
    );

    M5Cardputer.Display.println(
      "4 New threat"
    );

    M5Cardputer.Display.println(
      "Q Back"
    );

    char key =
      waitKey();

    if (key == '1') {
      soundAlert();
    }

    else if (key == '2') {
      soundPartial();
    }

    else if (key == '3') {
      soundClear();
    }

    else if (key == '4') {
      soundNewThreat();
    }

    else if (key == 'q') {
      return;
    }
  }
}


// ============================================================
// SYSTEM INFO
// ============================================================

void systemInfo() {
  clearScreen();

  drawHeader(
    "SYSTEM INFO"
  );

  int batteryVoltageMv =
    M5Cardputer.Power.
    getBatteryVoltage();

  int batteryLevel =
    M5Cardputer.Power.
    getBatteryLevel();

  float batteryVoltage =
    batteryVoltageMv /
    1000.0f;

  M5Cardputer.Display.setTextColor(
    WHITE,
    BLACK
  );

  int y = 18;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Battery: "
  );

  M5Cardputer.Display.print(
    batteryLevel
  );

  M5Cardputer.Display.print(
    "% "
  );

  M5Cardputer.Display.print(
    batteryVoltage,
    2
  );

  M5Cardputer.Display.println(
    " V"
  );

  y += 11;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "WiFi: "
  );

  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {
    M5Cardputer.Display.print(
      WiFi.RSSI()
    );

    M5Cardputer.Display.println(
      " dBm"
    );
  } else {
    M5Cardputer.Display.println(
      "OFF"
    );
  }

  y += 11;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Heap: "
  );

  M5Cardputer.Display.print(
    ESP.getFreeHeap() /
    1024
  );

  M5Cardputer.Display.println(
    " KB"
  );

  y += 11;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Status API: "
  );

  M5Cardputer.Display.println(
    lastStatusHttp
  );

  y += 11;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Detail API: "
  );

  M5Cardputer.Display.println(
    lastDetailsHttp
  );

  y += 11;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Payload: "
  );

  M5Cardputer.Display.print(
    lastDetailsPayloadBytes
  );

  M5Cardputer.Display.println(
    " B"
  );

  y += 11;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "JSON: "
  );

  M5Cardputer.Display.println(
    lastJsonOK
    ? "OK"
    : "FAIL"
  );

  y += 11;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Alerts:"
  );

  M5Cardputer.Display.print(
    lastDetailsAlertCount
  );

  M5Cardputer.Display.print(
    " Air:"
  );

  M5Cardputer.Display.println(
    lastDetailsAirRaidCount
  );

  y += 11;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Matches: "
  );

  M5Cardputer.Display.println(
    lastDetailsMatchCount
  );

  y += 11;

  M5Cardputer.Display.setCursor(
    4,
    y
  );

  M5Cardputer.Display.print(
    "Threat items: "
  );

  M5Cardputer.Display.println(
    lastMatchedThreatCount
  );

  waitKey();
}


// ============================================================
// MAIN MENU
// ============================================================

void mainMenu() {
  while (true) {
    clearScreen();

    drawHeader(
      "MAIN MENU"
    );

    M5Cardputer.Display.setTextColor(
      WHITE,
      BLACK
    );

    M5Cardputer.Display.setCursor(
      8,
      17
    );

    M5Cardputer.Display.println(
      "1 WiFi"
    );

    M5Cardputer.Display.println(
      "2 Areas"
    );

    M5Cardputer.Display.println(
      "3 Volume"
    );

    M5Cardputer.Display.println(
      "4 Test sounds"
    );

    M5Cardputer.Display.println(
      "5 DEMO MODE"
    );

    M5Cardputer.Display.println(
      "6 Refresh"
    );

    M5Cardputer.Display.println(
      "7 System info"
    );

    M5Cardputer.Display.println(
      "8 LIVE DEBUG"
    );

    M5Cardputer.Display.println(
      "9 Reboot"
    );

    M5Cardputer.Display.println(
      "Q Back"
    );

    char key =
      waitKey();

    if (key == '1') {
      wifiMenu();
    }

    else if (key == '2') {
      areasMenu();
    }

    else if (key == '3') {
      volumeMenu();
    }

    else if (key == '4') {
      testSounds();
    }

    else if (key == '5') {
      demoMenu();
      return;
    }

    else if (key == '6') {
      if (demoMode) {
        nextDemoScenario();
      } else {
        messageScreen(
          "API",
          "Refreshing..."
        );

        refreshAll();

        delay(300);
      }

      return;
    }

    else if (key == '7') {
      systemInfo();
    }

    else if (key == '8') {
      liveDebugMenu();
    }

    else if (key == '9') {
      messageScreen(
        "SYSTEM",
        "Rebooting..."
      );

      delay(400);

      ESP.restart();
    }

    else if (key == 'q') {
      return;
    }
  }
}


// ============================================================
// SETUP
// ============================================================

void setup() {
  auto cfg =
    M5.config();

  M5Cardputer.begin(
    cfg
  );

  M5Cardputer.Display.setRotation(
    1
  );

  M5Cardputer.Display.setTextWrap(
    false
  );

  setenv(
    "TZ",
    "EET-2EEST,M3.5.0/3,M10.5.0/4",
    1
  );

  tzset();

  clearScreen();

  M5Cardputer.Display.setTextColor(
    CYAN,
    BLACK
  );

  M5Cardputer.Display.setTextSize(
    2
  );

  M5Cardputer.Display.setCursor(
    25,
    35
  );

  M5Cardputer.Display.println(
    "AIR MONITOR"
  );

  M5Cardputer.Display.setTextSize(
    1
  );

  M5Cardputer.Display.setCursor(
    75,
    70
  );

  M5Cardputer.Display.println(
    "Starting..."
  );

  loadSettings();

  M5Cardputer.Speaker.setVolume(
    speakerVolume
  );

  WiFi.mode(
    WIFI_STA
  );

  WiFi.setAutoReconnect(
    true
  );

  WiFi.persistent(
    false
  );

  if (demoMode) {
    applyDemoScenario(
      demoScenario,
      false
    );
  } else {
    if (
      savedSSID.length() > 0
    ) {
      connectWiFi(
        savedSSID,
        savedPassword,
        true
      );
    }

    if (
      WiFi.status() ==
      WL_CONNECTED &&
      tokenConfigured()
    ) {
      refreshAll();
    }
  }

  lastApiPoll =
    millis();

  drawHome();
}


// ============================================================
// LOOP
// ============================================================

void loop() {
  M5Cardputer.update();

  if (
    M5Cardputer.Keyboard.isChange() &&
    M5Cardputer.Keyboard.isPressed()
  ) {
    Keyboard_Class::KeysState status =
      M5Cardputer.Keyboard.keysState();

    for (
      auto c :
      status.word
    ) {
      c =
        normalizeKey(c);

      if (c == 'm') {
        delay(150);

        mainMenu();

        drawHome();
      }

      else if (c == 'r') {
        if (demoMode) {
          nextDemoScenario();
        } else {
          refreshAll();

          lastApiPoll =
            millis();
        }

        drawHome();
      }

      else if (c == 'n') {
        int pageCount =
          (
            areaCount +
            AREAS_PER_PAGE -
            1
          ) /
          AREAS_PER_PAGE;

        if (
          homePage <
          pageCount - 1
        ) {
          homePage++;
        }

        drawHome();
      }

      else if (c == 'p') {
        if (
          homePage > 0
        ) {
          homePage--;
        }

        drawHome();
      }
    }
  }

  if (!demoMode) {
    if (
      WiFi.status() !=
      WL_CONNECTED &&
      savedSSID.length() > 0 &&
      millis() -
      lastWifiRetry >=
      WIFI_RETRY_MS
    ) {
      lastWifiRetry =
        millis();

      WiFi.begin(
        savedSSID.c_str(),
        savedPassword.c_str()
      );
    }

    if (
      WiFi.status() ==
      WL_CONNECTED &&
      tokenConfigured() &&
      millis() -
      lastApiPoll >=
      API_INTERVAL_MS
    ) {
      lastApiPoll =
        millis();

      refreshAll();

      drawHome();
    }
  }

  if (
    millis() -
    lastHomeRedraw >=
    1000
  ) {
    lastHomeRedraw =
      millis();

    drawHome();
  }

  delay(10);
}
