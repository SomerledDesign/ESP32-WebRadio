#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

// EEPROM writing routines (eg: remembers previous radio stn)
Preferences preferences;

// Station index/number
unsigned int currStnNo, prevStnNo;
signed int nextStnNo;

// Secret WiFi stuff from include/secrets.h
std::string ssid;
std::string wifiPassword;
bool wiFiDisconnected = true;

static const radioStationLayout kDefaultStations[stationCnt] = {
#include "stationList.h"
};

const radioStationLayout *radioStation = kDefaultStations;
static radioStationLayout *g_loadedStations = nullptr;

// Pushbutton connected to this pin to change station
int stnChangePin = 13;
int tftTouchedPin = 15;
uint prevTFTBright;

// Can we use the above button (not in the middle of changing stations)?
bool canChangeStn = true;

// Current state of WiFi (connected, idling)
int status = WL_IDLE_STATUS;

// Do we want to connect with track/artist info (metadata)
bool METADATA = true;

// The number of bytes between metadata (title track)
uint16_t metaDataInterval = 0; //bytes
uint16_t bytesUntilmetaData = 0;
int bitRate = 0;
bool redirected = false;
bool volumeMax = false;

// Dedicated 32-byte buffer for VS1053 aligned on 4-byte boundary for efficiency
uint8_t mp3buff[32] __attribute__((aligned(4)));

// Circular "Read Buffer" to stop stuttering on some stations
cbuf circBuffer(10);

// Internet stream buffer that we copy in chunks to the ring buffer
char readBuffer[100] __attribute__((aligned(4)));

// MP3 decoder
VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);

// Instantiate screen (object) using hardware SPI. Defaults to 320H x 240W
TFT_eSPI tft = TFT_eSPI();

// Start the WiFi client here
WiFiClient client;

namespace {
String decodeXmlEntities(String value)
{
	value.replace("&amp;", "&");
	value.replace("&quot;", "\"");
	value.replace("&apos;", "'");
	value.replace("&lt;", "<");
	value.replace("&gt;", ">");
	return value;
}

bool extractXmlAttr(const String &line, const char *key, String &out)
{
	String needle = String(key) + "=\"";
	int start = line.indexOf(needle);
	if (start < 0)
	{
		return false;
	}
	start += needle.length();
	int end = line.indexOf("\"", start);
	if (end < 0)
	{
		return false;
	}
	out = decodeXmlEntities(line.substring(start, end));
	return true;
}

void copyXmlAttr(const String &line, const char *key, char *dest, size_t dest_len, const char *fallback)
{
	String value;
	if (!extractXmlAttr(line, key, value) || value.length() == 0)
	{
		value = fallback;
	}
	value.toCharArray(dest, dest_len);
}

int parseIntAttr(const String &line, const char *key, int fallback)
{
	String value;
	if (!extractXmlAttr(line, key, value) || value.length() == 0)
	{
		return fallback;
	}
	char *endptr = nullptr;
	long parsed = strtol(value.c_str(), &endptr, 10);
	if (endptr == value.c_str())
	{
		return fallback;
	}
	return static_cast<int>(parsed);
}

bool parseStationLine(const String &line, radioStationLayout &station)
{
	if (line.startsWith("<stations") || !line.startsWith("<station"))
	{
		return false;
	}

	copyXmlAttr(line, "host", station.host, sizeof(station.host), "");
	if (station.host[0] == '\0')
	{
		return false;
	}

	copyXmlAttr(line, "path", station.path, sizeof(station.path), "/");

	String name;
	if (!extractXmlAttr(line, "friendlyName", name))
	{
		if (!extractXmlAttr(line, "name", name))
		{
			name = "Station";
		}
	}
	name.toCharArray(station.friendlyName, sizeof(station.friendlyName));

	copyXmlAttr(line, "genre", station.genre, sizeof(station.genre), "Unknown");

	station.port = parseIntAttr(line, "port", 80);
	station.useMetaData = static_cast<uint8_t>(parseIntAttr(line, "useMetaData", 0));

	return true;
}
} // namespace

bool loadStationsFromLittleFS(const char *path)
{
	if (!LittleFS.exists(path))
	{
		Serial.printf("Stations file not found: %s\n", path);
		return false;
	}

	File file = LittleFS.open(path, FILE_READ);
	if (!file)
	{
		Serial.printf("Failed to open stations file: %s\n", path);
		return false;
	}

	if (!g_loadedStations)
	{
		g_loadedStations = static_cast<radioStationLayout *>(
			heap_caps_malloc(sizeof(radioStationLayout) * stationCnt, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
		if (!g_loadedStations)
		{
			g_loadedStations = static_cast<radioStationLayout *>(malloc(sizeof(radioStationLayout) * stationCnt));
		}
		if (!g_loadedStations)
		{
			Serial.println("Station list allocation failed.");
			file.close();
			return false;
		}
	}

	memset(g_loadedStations, 0, sizeof(radioStationLayout) * stationCnt);

	size_t count = 0;
	while (file.available() && count < stationCnt)
	{
		String line = file.readStringUntil('\n');
		line.trim();
		if (line.length() == 0 || line.startsWith("<?") || line.startsWith("<!--"))
		{
			continue;
		}

		if (parseStationLine(line, g_loadedStations[count]))
		{
			count++;
		}
	}
	file.close();

	if (count != stationCnt)
	{
		Serial.printf("Stations loaded: %u (expected %d)\n", static_cast<unsigned>(count), stationCnt);
		return false;
	}

	radioStation = g_loadedStations;
	return true;
}
