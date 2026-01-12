// Ensure this header file is only included once
#ifndef _ESP32RADIO_
#define _ESP32RADIO_

/*
	Includes go here
*/
// Utility to write to (psuedo) EEPROM
#include <Preferences.h>

// Standard ESP32 WiFi
#include <WiFi.h>

// MP3+ decoder
#include <VS1053.h>

// Standard input/output streams, required for <locale> but this might get removed
#include <iostream>

// SPIFFS (in-memory SPI File System) replacement
#include <LITTLEFS.h>

// Display / TFT
#include <User_Setup.h>
#include "TFT_eSPI.h"
#include "Free_Fonts.h"

// Your preferred TFT / LED screen definition here
// As I'm using the TFT_eSPI from Bodmer they are all in User_Setup.h

// Circular buffer - ESP32 built in (hacked by me to use PSRAM)
#include <cbuf.h>

// EEPROM writing routines (eg: remembers previous radio stn)
extern Preferences preferences;

// Station index/number
extern unsigned int currStnNo, prevStnNo;
extern signed int nextStnNo;

// Secret WiFi stuff from include/secrets.h
extern std::string ssid;
extern std::string wifiPassword;
extern bool wiFiDisconnected;

// All connections are assumed insecure http:// not https://
const int8_t stationCnt = 64; // start counting from 1!

/* 
    Example URL: [port optional, 80 assumed]
        stream.antenne1.de[:80]/a1stg/livestream1.aac Antenne1 (Stuttgart)
*/

struct radioStationLayout
{
	char host[64];
	char path[128];
	int port;
	char friendlyName[64];
	uint8_t useMetaData;
	char genre[32];
};

extern const radioStationLayout *radioStation;

// Pushbutton connected to this pin to change station
extern int stnChangePin;
extern int tftTouchedPin;
extern uint prevTFTBright;

// Can we use the above button (not in the middle of changing stations)?
extern bool canChangeStn;

// Current state of WiFi (connected, idling)
extern int status;

// Do we want to connect with track/artist info (metadata)
extern bool METADATA;

// Whether to request ICY data or not. Overridden if set to 0 in radio stations.
#define ICYDATAPIN 36 // Input only pin, requires pull-up 10K resistor

// The number of bytes between metadata (title track)
extern uint16_t metaDataInterval; //bytes
extern uint16_t bytesUntilmetaData;
extern int bitRate;
extern bool redirected;
extern bool volumeMax;

// Dedicated 32-byte buffer for VS1053 aligned on 4-byte boundary for efficiency
extern uint8_t mp3buff[32] __attribute__((aligned(4)));

// Circular "Read Buffer" to stop stuttering on some stations
#ifdef BOARD_HAS_PSRAM
#define CIRCULARBUFFERSIZE 150000 // Divide by 32 to see how many 2mS samples this can store
#else
#define CIRCULARBUFFERSIZE 10000
#endif
extern cbuf circBuffer;
#define streamingCharsMax 32

// Internet stream buffer that we copy in chunks to the ring buffer
extern char readBuffer[100] __attribute__((aligned(4)));

// Wiring of VS1053 board (SPI connected in a standard way) on ESP32 only
#define VS1053_CS 32
#define VS1053_DCS 33
#define VS1053_DREQ 35
#define VOLUME 100 // treble/bass works better if NOT 100 here

// WiFi specific defines
#define WIFITIMEOUTSECONDS 20

// Forward declarations of functions TODO: clean up & describe FIXME:
bool stationConnect(int station_no);
std::string readLITTLEFSInfo(char *itemRequired);
std::string getWiFiPassword();
std::string getSSID();
void connectToWifi();
const char *wl_status_to_string(wl_status_t status);
void initDisplay();
bool loadStationsFromLittleFS(const char *path = "/stations.xml");
void changeStation(int8_t plusOrMinus);
bool _GLIBCXX_ALWAYS_INLINE readMetaData();
void getRedirectedStationInfo(String header, int currStationNo);
void setupDisplayModule();
void displayStationName(const char *stationName);
void displayTrackArtist(std::string);

bool getNextButtonPress(uint16_t x, uint16_t y);
bool getPrevButtonPress(uint16_t x, uint16_t y);

void drawPrevButton();
void drawNextButton();
void drawMuteButton(bool invert);
void drawBrightButton(bool invert);
void drawDimButton(bool invert);
void drawMuteBitmap(bool isMuted);

void getBrightButtonPress();
void getDimButtonPress();

std::string toTitle(std::string s, const std::locale &loc = std::locale());
void drawBufferLevel(size_t bufferLevel, bool override = false);
void checkForStationChange();
void populateRingBuffer();

void taskSetup();

// Global objects are defined in src/globals.cpp

// MP3 decoder
extern VS1053 player;

// Instantiate screen (object) using hardware SPI. Defaults to 320H x 240W
extern TFT_eSPI tft;

// Start the WiFi client here
extern WiFiClient client;

#endif
