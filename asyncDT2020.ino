#define CONFIG_ASYNC_TCP_USE_WDT 1

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "esp_wifi.h"
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Update.h>

// GPIO where the DS18B20 is connected to
#define WATERING_PIN 27

#define SUNLIGHT_PIN 32
#define MOISTURE_PIN 33
const int oneWireBus = 26;

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

#define LLMAX 9223372036854775807
#define CONFIG_ASYNC_TCP_USE_WDT 1

#define MAX_DEVICES 4
#define EEPROM_SIZE 1024
#define VERSION_BYTE 1000
#define EEPROM_VERSION 11

const std::string hexlokup = "0123456789ABCDEF";

struct color
{
	uint8_t r; // h
	uint8_t g; // s
	uint8_t b; // v

	color(uint8_t red, uint8_t green, uint8_t blue)
	{
		this->r = red;
		this->g = green;
		this->b = blue;
	}
};
color rgbtohsv(color rgb)
{
	color hsv(0, 0, 0);
	unsigned char rgbMin, rgbMax;

	rgbMin = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
	rgbMax = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);

	hsv.b = rgbMax;
	if (hsv.b == 0)
	{
		hsv.r = 0;
		hsv.g = 0;
		return hsv;
	}

	hsv.g = 255 * long(rgbMax - rgbMin) / hsv.b;
	if (hsv.g == 0)
	{
		hsv.r = 0;
		return hsv;
	}

	if (rgbMax == rgb.r)
		hsv.r = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
	else if (rgbMax == rgb.g)
		hsv.r = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
	else
		hsv.r = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);

	return hsv;
}
color hsvtorgb(color hsv)
{
	color rgb(0, 0, 0);
	unsigned char region, remainder, p, q, t;

	if (hsv.g == 0)
	{
		rgb.r = hsv.b;
		rgb.g = hsv.b;
		rgb.b = hsv.b;
		return rgb;
	}

	region = hsv.r / 43;
	remainder = (hsv.r - (region * 43)) * 6;

	p = (hsv.b * (255 - hsv.g)) >> 8;
	q = (hsv.b * (255 - ((hsv.g * remainder) >> 8))) >> 8;
	t = (hsv.b * (255 - ((hsv.g * (255 - remainder)) >> 8))) >> 8;

	switch (region)
	{
	case 0:
		rgb.r = hsv.b;
		rgb.g = t;
		rgb.b = p;
		break;
	case 1:
		rgb.r = q;
		rgb.g = hsv.b;
		rgb.b = p;
		break;
	case 2:
		rgb.r = p;
		rgb.g = hsv.b;
		rgb.b = t;
		break;
	case 3:
		rgb.r = p;
		rgb.g = q;
		rgb.b = hsv.b;
		break;
	case 4:
		rgb.r = t;
		rgb.g = p;
		rgb.b = hsv.b;
		break;
	default:
		rgb.r = hsv.b;
		rgb.g = p;
		rgb.b = q;
		break;
	}

	return rgb;
}
color hsvaverage(color rgb1, color rgb2, float percentin)
{
	float percent = percentin;
	if (percent < 0)
	{
		percent = 0;
	}
	else if (percent > 1)
	{
		percent = 1;
	}

	color hsvrgb1 = rgbtohsv(rgb1);
	color hsvrgb2 = rgbtohsv(rgb2);
	color hsvnewcolor(0, 0, 0);

	hsvnewcolor.r = (uint8_t)((float)hsvrgb1.r + ((float)(hsvrgb2.r - hsvrgb1.r) * percent));
	hsvnewcolor.g = (uint8_t)((float)hsvrgb1.g + ((float)(hsvrgb2.g - hsvrgb1.g) * percent));
	hsvnewcolor.b = (uint8_t)((float)hsvrgb1.b + ((float)(hsvrgb2.b - hsvrgb1.b) * percent));

	return hsvtorgb(hsvnewcolor);
}

#define REDPIN 13
#define GREENPIN 12
#define BLUEPIN 14

// BOARD STATE
bool isSlave = true;
bool hasWiFi = false; // does the board have a wifi password + ssid to try to connect to? this is only false on an uninitialised board

char ssid[64]; // the SSID of the master board if slave, the SSID of the LAN if master
char password[64];

short moistureBegin = 0;  // when to start watering
short moistureEnd = 5000; // when to stop
uint8_t checkingTime = 5; // minutes for stable reading to begin watering
short cooldown = 60;	  // how long between waterings in minutes

char lightingtype = 's';	// 's' = static, 'a' or 'b' or 'c' = dynamic on lighting moisture or temperature, we're using HSV
color lowlighting(0, 0, 0); // for dynamic - colour when low value reading
color highlighting(0, 0, 0);
short lowvalue = 0;		// what is the low value reading
short highvalue = 4096; // what is the high value reading

char boardname[23];	   // a STATIC, UNCHANGING boardname
char boardpassword[8]; // a STATIC, UNCHANGING boardpassword

uint8_t devices = 0; // at most 20
char boardnames[MAX_DEVICES][23];
uint8_t mac_addrs[MAX_DEVICES][6];
// END BOARD STATE

#define MAX_WATERING_S 90	 // 90s
#define SERVER_POST_BLOCKS 6 // 5 minutes (30 * 10s);
// DYANMIC STATE
//long long semaphoresetat = LLMAX;
short mreadings[60];				  // this is where moisture readings will be stored
short treadings[60];				  // this is where temperature readings will be stored
short lreadings[60];				  // this is where light readings will be stored
short pmreadings[SERVER_POST_BLOCKS]; // this is where moisture readings will be stored
short ptreadings[SERVER_POST_BLOCKS]; // this is where temperature readings will be stored
short plreadings[SERVER_POST_BLOCKS]; // this is where light readings will be stored
short smreadings[10];				  // this is where moisture readings will be stored
short streadings[10];				  // this is where temperature readings will be stored
short slreadings[10];				  // this is where light readings will be stored
short scurridx = 0;
short maincurridx = 0;
short pcurridx = 0;

short lasttemperatureReading = 0;
short lastmoistureReading = 0;
short lastsunlightReading = 0;

short readingNo = 0;
long long lastreading = 0;
long long temperatureReadyAt = LLMAX;
bool watering = false;			 // this is for the board personally
char wateringreason = '_';		 // _ = blank, m = moisture, u = user
long long endwateringat = LLMAX; // when should the user's watering be forced to end?
long long beganwateringat = 0;	 // when did moisture trigger watering to begin
long long cooldownendsat = 0;	 // when can the board water again?

std::string readstrfromarr(const char strIn[], int length)
{
	std::string output;
	for (int i = 0; i < length; i++)
	{
		if (strIn[i] != '\0')
		{
			output.push_back(strIn[i]);
		}
		else
		{
			break;
		}
	}
	return output;
}

const char *root_ca =
	"-----BEGIN CERTIFICATE-----\n"
	"MIIDSjCCAjKgAwIBAgIQRK+wgNajJ7qJMDmGLvhAazANBgkqhkiG9w0BAQUFADA/\n"
	"MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n"
	"DkRTVCBSb290IENBIFgzMB4XDTAwMDkzMDIxMTIxOVoXDTIxMDkzMDE0MDExNVow\n"
	"PzEkMCIGA1UEChMbRGlnaXRhbCBTaWduYXR1cmUgVHJ1c3QgQ28uMRcwFQYDVQQD\n"
	"Ew5EU1QgUm9vdCBDQSBYMzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n"
	"AN+v6ZdQCINXtMxiZfaQguzH0yxrMMpb7NnDfcdAwRgUi+DoM3ZJKuM/IUmTrE4O\n"
	"rz5Iy2Xu/NMhD2XSKtkyj4zl93ewEnu1lcCJo6m67XMuegwGMoOifooUMM0RoOEq\n"
	"OLl5CjH9UL2AZd+3UWODyOKIYepLYYHsUmu5ouJLGiifSKOeDNoJjj4XLh7dIN9b\n"
	"xiqKqy69cK3FCxolkHRyxXtqqzTWMIn/5WgTe1QLyNau7Fqckh49ZLOMxt+/yUFw\n"
	"7BZy1SbsOFU5Q9D8/RhcQPGX69Wam40dutolucbY38EVAjqr2m7xPi71XAicPNaD\n"
	"aeQQmxkqtilX4+U9m5/wAl0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNV\n"
	"HQ8BAf8EBAMCAQYwHQYDVR0OBBYEFMSnsaR7LHH62+FLkHX/xBVghYkQMA0GCSqG\n"
	"SIb3DQEBBQUAA4IBAQCjGiybFwBcqR7uKGY3Or+Dxz9LwwmglSBd49lZRNI+DT69\n"
	"ikugdB/OEIKcdBodfpga3csTS7MgROSR6cz8faXbauX+5v3gTt23ADq1cEmv8uXr\n"
	"AvHRAosZy5Q6XkjEGB5YGV8eAlrwDPGxrancWYaLbumR9YbK+rlmM6pZW87ipxZz\n"
	"R8srzJmwN0jP41ZL9c8PDHIyh8bwRLtTcm1D9SZImlJnt1ir/md2cXjbDaJWFBM5\n"
	"JDGFoqgCWjBH4d1QB7wCCZAA62RjYJsWvIjJEubSfZGL+T0yjWW06XyxV3bqxbYo\n"
	"Ob8VZRzI9neWagqNdwvYkQsEjgfbKbYK7p2CNTUQ\n"
	"-----END CERTIFICATE-----\n";

void sendWateringPacket(std::string boardnamestrin, bool wateringin, char reason)
{
	HTTPClient https;
	if (isSlave)
	{
		https.begin("http://192.168.5.1/givewateringdata");
	}
	else
	{
		https.begin("https://api.irrigation.smartlylinked.com/givewateringdata", root_ca); //Specify the URL and certificate
	}
	https.setTimeout(2250);
	https.addHeader("Content-Type", "application/x-www-form-urlencoded");
	int httpCode = https.POST((std::string("password=ad9uwxcCkeTBSekNnrCMjj5m&boardname=") + boardnamestrin + "&wateringstate=" + (wateringin ? "watering" : "notwatering") + "&reason=" + reason).c_str()); //Make the request

	if (httpCode > 0)
	{ //Check for the returning code

		String payload = https.getString();
		Serial.println(httpCode);
		Serial.println(payload);
	}

	else
	{
		Serial.println("Error on HTTP request");
	}

	https.end(); //Free the resources
}
void sendDataPacket(std::string boardnamestrin, int subPMt, int subPTt, int subPLt)
{
	HTTPClient https;

	if (isSlave)
	{
		https.begin("http://192.168.5.1/givedata");
		Serial.println("slave data post");
	}
	else
	{
		https.begin("https://api.irrigation.smartlylinked.com/givedata", root_ca); //Specify the URL and certificate
	}
	https.setConnectTimeout(2000);
	https.setTimeout(2250);
	https.addHeader("Content-Type", "application/x-www-form-urlencoded");
	int httpCode = https.POST((std::string("password=ad9uwxcCkeTBSekNnrCMjj5m&boardname=") + boardnamestrin + "&moisture=" + String(subPMt).c_str() + "&temperature=" + String(subPTt).c_str() + "&sunlight=" + String(subPLt).c_str()).c_str()); //Make the request

	if (httpCode > 0)
	{ //Check for the returning code

		String payload = https.getString();
		Serial.println(httpCode);
		Serial.println(payload);
	}

	else
	{
		Serial.println(httpCode);
		Serial.println("Error on HTTP request");
	}

	https.end(); //Free the resources
}

bool restartRequired = false; // for restarts triggered through OTA

//bool semaphoreSet = false; // when a board takes the semaphore, it will set this to true
//char semaphore[23];      // when a board takes the semaphore, it will write its name here
// END DYNAMIC STATE

// APPLICATION FLOW
//#// 0. all devices initialise themselves. ALL BOARDS ARE ALWAYS AP+STA
//#// 0.1 all devices will also give themselves a hostname, which is their boardname
//#// 1. user will connect to the master and will give it the wifi password and a command to switch to master
//#// 1.1 when the master receives this on its async callback, it'll WiFi.begin to start wifi
//#// 1.2 the connection will be attempted. the app will check if the connection was successful after a couple of seconds.
//#// 1.3 the board will not care if the connection was/is successful or not
//#// 2. user will connect to the slave and will give it the name and password of the master
//#// 2.1   when the slave receives this, a semaphore will be set that will update wifi on the main loop to connect to the master
//#// 2.2 if the slave wishes to issue a POST to the master, it will used a fixed master IP
//#// 3. user will be able to connect to the master from their LAN and issue commands
//#// 4. when a command is issued, a boardname goes with it
//#// 4.1 the boardname will be used to resolve the hostname into an IP
//#// 4.2 the master will then use a client to POST to the slaves

// lighting comes in 3 layers: moisture lighting, system code lighting and user lighting
color moisturelighting(200, 100, 0);
color newmoisturelighting(200, 100, 0);

bool systemactive = false;
color systemlighting(0, 0, 0);
// this has a timeout and a flash
long long systemstopafter = 0;	// if the esp32 timer is bigger than this amount, deactivate systemactive
long long flashtoggleafter = 0; // if the esp32 timer is bigger than this amount, switch the flash state
long long flashperiod = 0;
bool flashison = true;	 // on or off phase of a flash?
bool isflashing = false; // becomes false after the deactivation

bool useractive = false;
color userlighting(0, 0, 0);
// this has a timeout
long long userstopafter = 0;

bool beginWatering(bool user, bool force)
{
	Serial.println("beginning watering");
	if (watering)
	{
		return false;
	}
	// this function doesn't check moisture - it's assumed that the user/board knows what they're doing here
	if ((esp_timer_get_time() > cooldownendsat) || force)
	{
		Serial.println("watering verified");
		// no cooldown restriction on watering, so begin
		endwateringat = esp_timer_get_time() + (MAX_WATERING_S * 1000 * 1000); // the app can set this differently for the user
		cooldownendsat = esp_timer_get_time() + ((long long)MAX_WATERING_S * 1000 * 1000) + ((long long)cooldown * 60 * 1000 * 1000);
		Serial.println((double)cooldownendsat);
		Serial.println((double)esp_timer_get_time());
		Serial.println((double)(((long long)MAX_WATERING_S * 1000 * 1000) + ((long long)cooldown * 60 * 1000 * 1000)));
		Serial.println(MAX_WATERING_S);
		Serial.println(cooldown);
		Serial.println((double)(((long long)MAX_WATERING_S * 1000 * 1000)));
		Serial.println((double)(((long long)cooldown * 60 * 1000 * 1000)));
		beganwateringat = esp_timer_get_time();
		wateringreason = (user ? 'u' : 'm');
		watering = true;
		systemstopafter = LLMAX;
		flashperiod = (1000 * 1000);
		isflashing = true;
		flashtoggleafter = 0;
		systemlighting.r = 0;
		systemlighting.g = 190;
		systemlighting.b = 255;
		systemactive = true;
		sendWateringPacket(readstrfromarr(boardname, 23), watering, wateringreason);
		return true;
	}
	else
	{
		return false;
	}
}
void stopWatering()
{
	Serial.println("stopping watering");
	if (watering)
	{
		endwateringat = LLMAX;
		systemactive = false;
		watering = false;
		cooldownendsat = esp_timer_get_time() + ((long long)cooldown * 60 * 1000 * 1000);
		sendWateringPacket(readstrfromarr(boardname, 23), watering, wateringreason);
	}
}

void boardnametoip(const char boardnamein[], char ip[])
{
	short idx = -1;
	for (short i = 0; i < devices; i++)
	{
		if (readstrfromarr(boardnames[i], 23) == readstrfromarr(boardnamein, 23))
		{
			idx = i;
		}
	}
	if (idx != -1)
	{
		Serial.println("found board");
		// step 2: check if it's connected to us
		wifi_sta_list_t wifi_sta_list;
		tcpip_adapter_sta_list_t adapter_sta_list;

		//memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
		//memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

		esp_wifi_ap_get_sta_list(&wifi_sta_list);
		tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);
		Serial.println("created connlist");

		bool completed = false;
		for (int i = 0; i < adapter_sta_list.num; i++)
		{
			Serial.println("up here");
			tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];

			Serial.print("station nr ");
			Serial.println(i);

			Serial.print("MAC: ");

			for (int i = 0; i < 6; i++)
			{

				Serial.printf("%02X", station.mac[i]);
				if (i < 5)
					Serial.print(":");
			}

			Serial.print("\nIP: ");
			Serial.println(ip4addr_ntoa(&(station.ip)));

			bool found = true;
			for (int j = 0; j < 6; j++)
			{
				if (station.mac[j] != mac_addrs[idx][j])
				{
					found = false;
				}
			}
			if (found)
			{
				completed = true;
				strncpy(ip, ip4addr_ntoa(&(station.ip)), strlen(ip4addr_ntoa(&(station.ip))));
				Serial.println(ip);
			}
		}

		//free(&wifi_sta_list);
		//free(&adapter_sta_list);

		if (!completed)
		{
			ip[0] = 'D'; // the board isn't connected
		}
	}
	else
	{
		ip[0] = 'N'; // the board is not in the table code
	}
}

short maxdevices()
{
	short maxd = MAX_DEVICES;
	if (isSlave)
	{
		maxd = 0;
	}
	return maxd;
}
short devicescount()
{
	if (devices <= maxdevices())
		return devices;
	else
		return maxdevices();
}
void writestrtoarr(std::string strIn, char strOut[], int length)
{
	for (int i = 0; i < strIn.size(); i++)
	{
		strOut[i] = strIn[i];
	}
	if (strIn.size() < length)
	{
		strOut[strIn.size()] = '\0';
	}
}
void writecolor(color *input)
{
	ledcWrite(0, input->r);
	ledcWrite(1, input->g);
	ledcWrite(2, input->b);
}
void saveState()
{
	int eeAddress = 0;
	EEPROM.put(eeAddress, isSlave);
	eeAddress += sizeof(bool);
	EEPROM.put(eeAddress, hasWiFi);
	eeAddress += sizeof(bool);
	for (short i = 0; i < 64; i++)
	{
		EEPROM.put(eeAddress, ssid[i]);
		eeAddress += sizeof(char);
	}
	for (short i = 0; i < 64; i++)
	{
		EEPROM.put(eeAddress, password[i]);
		eeAddress += sizeof(char);
	}
	// 130
	EEPROM.put(eeAddress, moistureBegin);
	eeAddress += sizeof(short);
	EEPROM.put(eeAddress, moistureEnd);
	eeAddress += sizeof(short);
	EEPROM.put(eeAddress, checkingTime);
	eeAddress += sizeof(uint8_t);
	EEPROM.put(eeAddress, cooldown);
	eeAddress += sizeof(short);

	EEPROM.put(eeAddress, lightingtype);
	eeAddress += sizeof(char);
	EEPROM.put(eeAddress, lowlighting);
	eeAddress += sizeof(color);
	EEPROM.put(eeAddress, highlighting);
	eeAddress += sizeof(color);
	EEPROM.put(eeAddress, lowvalue);
	eeAddress += sizeof(short);
	EEPROM.put(eeAddress, highvalue);
	eeAddress += sizeof(short);

	for (short i = 0; i < 23; i++)
	{
		EEPROM.put(eeAddress, boardname[i]);
		eeAddress += sizeof(char);
	}

	for (short i = 0; i < 8; i++)
	{
		EEPROM.put(eeAddress, boardpassword[i]);
		eeAddress += sizeof(char);
	}

	EEPROM.put(eeAddress, devices);
	eeAddress += sizeof(uint8_t);
	for (short i = 0; i < devices; i++)
	{
		for (short j = 0; j < 23; j++)
		{
			EEPROM.put(eeAddress, boardnames[i][j]);
			eeAddress += sizeof(char);
		}
	}
	for (short i = 0; i < devices; i++)
	{
		for (short j = 0; j < 6; j++)
		{
			EEPROM.put(eeAddress, mac_addrs[i][j]);
			eeAddress += sizeof(uint8_t);
		}
	}

	EEPROM.write(VERSION_BYTE, EEPROM_VERSION);
	EEPROM.commit();
}
void readState()
{
	int eeAddress = 0;
	EEPROM.get(eeAddress, isSlave);
	eeAddress += sizeof(bool);
	EEPROM.get(eeAddress, hasWiFi);
	eeAddress += sizeof(bool);
	for (short i = 0; i < 64; i++)
	{
		EEPROM.get(eeAddress, ssid[i]);
		eeAddress += sizeof(char);
	}
	for (short i = 0; i < 64; i++)
	{
		EEPROM.get(eeAddress, password[i]);
		eeAddress += sizeof(char);
	}
	// 130
	EEPROM.get(eeAddress, moistureBegin);
	eeAddress += sizeof(short);
	EEPROM.get(eeAddress, moistureEnd);
	eeAddress += sizeof(short);
	EEPROM.get(eeAddress, checkingTime);
	eeAddress += sizeof(uint8_t);
	EEPROM.get(eeAddress, cooldown);
	eeAddress += sizeof(short);

	EEPROM.get(eeAddress, lightingtype);
	eeAddress += sizeof(char);
	EEPROM.get(eeAddress, lowlighting);
	eeAddress += sizeof(color);
	EEPROM.get(eeAddress, highlighting);
	eeAddress += sizeof(color);
	EEPROM.get(eeAddress, lowvalue);
	eeAddress += sizeof(short);
	EEPROM.get(eeAddress, highvalue);
	eeAddress += sizeof(short);
	for (short i = 0; i < 23; i++)
	{
		EEPROM.get(eeAddress, boardname[i]);
		eeAddress += sizeof(char);
	}

	for (short i = 0; i < 8; i++)
	{
		EEPROM.get(eeAddress, boardpassword[i]);
		eeAddress += sizeof(char);
	}

	EEPROM.get(eeAddress, devices);
	eeAddress += sizeof(uint8_t);
	for (short i = 0; i < devices; i++)
	{
		for (short j = 0; j < 23; j++)
		{
			EEPROM.get(eeAddress, boardnames[i][j]);
			eeAddress += sizeof(char);
		}
	}
	for (short i = 0; i < devices; i++)
	{
		for (short j = 0; j < 6; j++)
		{
			EEPROM.get(eeAddress, mac_addrs[i][j]);
			eeAddress += sizeof(uint8_t);
		}
	}
}
void printState()
{
	Serial.print("isSlave: ");
	Serial.println(isSlave);
	Serial.print("hasWiFi: ");
	Serial.println(hasWiFi);
	Serial.print("ssid: ");
	for (short i = 0; i < 64; i++)
	{
		Serial.print(ssid[i]);
	}
	Serial.println("");
	Serial.print("password: ");
	for (short i = 0; i < 64; i++)
	{
		Serial.print(password[i]);
	}
	Serial.println("");
	Serial.print("boardpassword: ");
	for (short i = 0; i < 8; i++)
	{
		Serial.print(boardpassword[i]);
	}
	Serial.println("");
	Serial.print("boardname: ");
	for (short i = 0; i < 23; i++)
	{
		Serial.print(boardname[i]);
	}
	Serial.println("");
	Serial.print("moistureBegin: ");
	Serial.println(moistureBegin);
	Serial.print("moistureEnd: ");
	Serial.println(moistureEnd);
	Serial.print("checkingTime: ");
	Serial.println(checkingTime);
	Serial.print("cooldown: ");
	Serial.println(cooldown);
	Serial.print("lightingtype: ");
	Serial.println(lightingtype);
	Serial.print("lowlighting: ");
	Serial.print(lowlighting.r);
	Serial.print(",");
	Serial.print(lowlighting.g);
	Serial.print(",");
	Serial.println(lowlighting.b);
	Serial.print("highlighting: ");
	Serial.print(highlighting.r);
	Serial.print(",");
	Serial.print(highlighting.g);
	Serial.print(",");
	Serial.println(highlighting.b);
	Serial.print("lowvalue: ");
	Serial.println(lowvalue);
	Serial.print("highvalue: ");
	Serial.println(highvalue);
	Serial.print("devices: ");
	Serial.println(devices);

	for (short i = 0; i < devices; i++)
	{
		Serial.print("device ");
		Serial.print(i);
		Serial.print(": ");
		Serial.print("   name: ");
		for (short j = 0; j < 23; j++)
		{
			Serial.print(boardnames[i][j]);
		}
		Serial.print(" ");
		for (short j = 0; j < 6; j++)
		{
			Serial.print((int)mac_addrs[i][j]);
			Serial.print(":");
		}
		Serial.println("");
	}
}
AsyncWebServer server(80);
HTTPClient *http;

std::string httpresponse = "blanks";
void makehttprequest(const char url[], bool post, const char postdata[])
{
	Serial.println(heap_caps_check_integrity_all(true));
	http = new HTTPClient();
	http->begin(url);
	http->setTimeout(2250);
	Serial.println(heap_caps_check_integrity_all(true));

	int httpResponseCode;
	if (post)
	{
		http->addHeader("Content-Type", "application/x-www-form-urlencoded");
		httpResponseCode = http->POST(postdata);
	}
	else
	{
		httpResponseCode = http->GET();
	}

	String payload = "{}";

	Serial.println(heap_caps_check_integrity_all(true));
	if (httpResponseCode > 0)
	{
		Serial.print("HTTP Response code: ");
		Serial.println(httpResponseCode);
		payload = http->getString();
	}
	else
	{
		Serial.print("Error code: ");
		Serial.println(httpResponseCode);
	}
	Serial.println(heap_caps_check_integrity_all(true));
	// Free resources
	http->end();
	Serial.println(heap_caps_check_integrity_all(true));
	Serial.println(payload);

	httpresponse = std::string(String(httpResponseCode).c_str()) + "\n" + std::string(payload.c_str());
	Serial.println("Ending serialisation");
	Serial.println(httpresponse.c_str());
	//delete httpdoc;
	Serial.println(heap_caps_check_integrity_all(true));
	Serial.println("Deleting 2");
	delete http;
	Serial.println(heap_caps_check_integrity_all(true));
	Serial.println("completing 2");
}

void notFound(AsyncWebServerRequest *request)
{
	request->send(404, "text/plain", "Not found");
}

long long lastwificonn;

void addServerMethods()
{

	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", readstrfromarr(boardname, 23).c_str());
	});

	server.on("/mac", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", WiFi.macAddress());
	});

	server.on("/ipaddr", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", WiFi.localIP().toString());
	});

	// Send a GET request to <IP>/get?message=<message>
	server.on("/connected", HTTP_GET, [](AsyncWebServerRequest *request) {
		Serial.println("Connection status request");
		Serial.println(WiFi.status() == WL_CONNECTED);
		request->send(200, "text/plain", (WiFi.status() == WL_CONNECTED ? "YES" : "NO"));
	});

	// Send a POST request to <IP>/post with a form field message set to <message>
	server.on("/setwifi", HTTP_POST, [](AsyncWebServerRequest *request) {
		Serial.println("setwifi request");
		if (request->hasParam("ssid", true) && request->hasParam("password", true))
		{
			std::string ssidpost(request->getParam("ssid", true)->value().c_str());
			std::string passwordpost(request->getParam("password", true)->value().c_str());
			Serial.println(ssidpost.c_str());
			Serial.println(passwordpost.c_str());
			hasWiFi = true;
			writestrtoarr(ssidpost, ssid, 64);
			writestrtoarr(passwordpost, password, 64);
			saveState();
			request->send(200, "text/plain", "OK");
			WiFi.begin(ssid, password);
		}
		else
		{
			Serial.println("Missing params");
			request->send(200, "text/plain", "MISSINGPARAMS");
		}
	});

	// Send a POST request to <IP>/post with a form field message set to <message>
	server.on("/setlighting", HTTP_POST, [](AsyncWebServerRequest *request) {
		Serial.println("setlighting request");
		if (request->hasParam("r", true) && request->hasParam("g", true) && request->hasParam("b", true) && request->hasParam("time", true))
		{
			int r = request->getParam("r", true)->value().toInt();
			int g = request->getParam("g", true)->value().toInt();
			int b = request->getParam("b", true)->value().toInt();
			int time = request->getParam("time", true)->value().toInt();
			userlighting.r = (uint8_t)r;
			userlighting.g = (uint8_t)g;
			userlighting.b = (uint8_t)b;
			userstopafter = (long long)esp_timer_get_time() + ((long long)(time) * (long long)1000000);
			useractive = true;
			request->send(200, "text/plain", "OK");
		}
		else
		{
			Serial.println("Missing params");
			request->send(200, "text/plain", "MISSINGPARAMS");
		}
	});

	server.on("/moisturesettings", HTTP_POST, [](AsyncWebServerRequest *request) {
		if (request->hasParam("moistureBegin", true) && request->hasParam("moistureEnd", true))
		{
			int newMoistureBegin = request->getParam("moistureBegin", true)->value().toInt();
			int newMoistureEnd = request->getParam("moistureEnd", true)->value().toInt();

			moistureBegin = newMoistureBegin;
			moistureEnd = newMoistureEnd;
			saveState();
			request->send(200, "text/plain", "OK");
		}
		else
		{
			Serial.println("Missing params");
			request->send(200, "text/plain", "MISSINGPARAMS");
		}
	});

	server.on("/advancedsettings", HTTP_POST, [](AsyncWebServerRequest *request) {
		if (request->hasParam("checkingTime", true) && request->hasParam("cooldown", true))
		{
			int newCheckingTime = request->getParam("checkingTime", true)->value().toInt();
			int newCooldown = request->getParam("cooldown", true)->value().toInt();

			checkingTime = newCheckingTime;
			cooldown = newCooldown;
			saveState();
			request->send(200, "text/plain", "OK");
		}
		else
		{
			Serial.println("Missing params");
			request->send(200, "text/plain", "MISSINGPARAMS");
		}
	});

	server.on("/lightsettings", HTTP_POST, [](AsyncWebServerRequest *request) {
		// we're going to have to check for 8 params here lol
		// i'll just grind through them
		if (request->hasParam("lightingtype", true) && request->hasParam("lowvalue", true) && request->hasParam("highvalue", true) &&
			request->hasParam("lowlightingr", true) && request->hasParam("lowlightingg", true) && request->hasParam("lowlightingb", true) &&
			request->hasParam("highlightingr", true) && request->hasParam("highlightingg", true) && request->hasParam("highlightingb", true))
		{
			char newlightingtype = request->getParam("lightingtype", true)->value().c_str()[0];
			int newlowvalue = request->getParam("lowvalue", true)->value().toInt();
			int newhighvalue = request->getParam("highvalue", true)->value().toInt();

			int newlowlightingr = request->getParam("lowlightingr", true)->value().toInt();
			int newlowlightingg = request->getParam("lowlightingg", true)->value().toInt();
			int newlowlightingb = request->getParam("lowlightingb", true)->value().toInt();

			int newhighlightingr = request->getParam("highlightingr", true)->value().toInt();
			int newhighlightingg = request->getParam("highlightingg", true)->value().toInt();
			int newhighlightingb = request->getParam("highlightingb", true)->value().toInt();

			lightingtype = newlightingtype;
			lowvalue = newlowvalue;

			lowlighting.r = newlowlightingr;
			lowlighting.g = newlowlightingg;
			lowlighting.b = newlowlightingb;

			highlighting.r = newhighlightingr;
			highlighting.g = newhighlightingg;
			highlighting.b = newhighlightingb;
			saveState();
			request->send(200, "text/plain", "OK");
		}
		else
		{
			Serial.println("Missing params");
			request->send(200, "text/plain", "MISSINGPARAMS");
		}
	});

	server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
		Serial.println("settings get request");
		const int capacity = JSON_OBJECT_SIZE(50);
		StaticJsonDocument<capacity> settingsdoc;
		settingsdoc["boardname"] = String(readstrfromarr(boardname, 23).c_str());
		settingsdoc["version"] = "0.1";
		Serial.println(String(readstrfromarr(boardname, 23).c_str()));
		settingsdoc["isSlave"] = isSlave;
		settingsdoc["hasWiFi"] = hasWiFi;
		settingsdoc["moistureBegin"] = moistureBegin;
		settingsdoc["moistureEnd"] = moistureEnd;
		settingsdoc["checkingTime"] = checkingTime;
		settingsdoc["cooldown"] = cooldown;
		settingsdoc["lightingtype"] = lightingtype;
		JsonObject lowlightingdoc = settingsdoc.createNestedObject("lowlighting");
		lowlightingdoc["r"] = lowlighting.r;
		lowlightingdoc["g"] = lowlighting.g;
		lowlightingdoc["b"] = lowlighting.b;
		JsonObject highlightingdoc = settingsdoc.createNestedObject("highlighting");
		highlightingdoc["r"] = highlighting.r;
		highlightingdoc["g"] = highlighting.g;
		highlightingdoc["b"] = highlighting.b;
		settingsdoc["lowvalue"] = lowvalue;
		settingsdoc["highvalue"] = highvalue;
		String output;
		serializeJson(settingsdoc, output);
		Serial.println(output);
		request->send(200, "text/plain", output);
	});

	server.on("/boardname", HTTP_GET, [](AsyncWebServerRequest *request) {
		Serial.println("boardname get request");
		request->send(200, "text/plain", readstrfromarr(boardname, 23).c_str());
	});

	server.on("/devices", HTTP_GET, [](AsyncWebServerRequest *request) {
		Serial.println("devices get request");
		std::string toreturn = "";
		for (int i = 0; i < devicescount(); i++)
		{
			for (int j = 0; j < 6; j++)
			{
				toreturn += hexlokup[(mac_addrs[i][j] - (mac_addrs[i][j] % 16)) / 16];
				toreturn += hexlokup[mac_addrs[i][j] % 16];
				toreturn += ":";
			}
			toreturn += " ";
			toreturn += readstrfromarr(boardnames[i], 23);
			toreturn += "\n";
		}
		request->send(200, "text/plain", toreturn.c_str());
	});

	server.on("/adddevice", HTTP_POST, [](AsyncWebServerRequest *request) {
		Serial.println("adddevice request");
		if (request->hasParam("boardname", true) && request->hasParam("macaddress", true))
		{
			std::string newboardname(request->getParam("boardname", true)->value().c_str());
			std::string macaddrstr(request->getParam("macaddress", true)->value().c_str());
			Serial.println(newboardname.c_str());
			if (newboardname.size() > 23 || macaddrstr.size() != 17)
			{
				request->send(200, "text/plain", "SIZEERR");
			}
			else
			{
				Serial.println(maxdevices());
				Serial.println(devices);
				if (devices >= maxdevices())
				{
					if (devices > maxdevices())
					{
						devices = maxdevices();
						saveState();
					}
					request->send(200, "text/plain", "DEVICESFULL");
				}
				else
				{
					writestrtoarr(newboardname, boardnames[devices], 23);
					for (int i = 0; i < 6; i++)
					{
						// convert a mac str into a mac address
						// AA:BB:CC:DD:EE:FF
						short bit1 = hexlokup.find(macaddrstr[(i * 3)]);
						short bit2 = hexlokup.find(macaddrstr[(i * 3) + 1]);
						if (bit1 == std::string::npos || bit2 == std::string::npos)
						{
							request->send(200, "text/plain", "BADMAC");
							return;
						}
						uint8_t piece = (bit1 * 16) + bit2;
						mac_addrs[devices][i] = piece;
					}
					devices += 1;
					saveState();
					request->send(200, "text/plain", "OK");
				}
			}
		}
		else
		{
			Serial.println("Missing params");
			request->send(200, "text/plain", "MISSINGPARAMS");
		}
	});

	server.on("/removedevice", HTTP_POST, [](AsyncWebServerRequest *request) { // TODO: this would be better if it took an actual name
		Serial.println("adddevice request");
		if (request->hasParam("boardidx", true))
		{
			int idx = request->getParam("boardidx", true)->value().toInt();
			Serial.println(idx);
			if (idx < devices)
			{
				if (devices > maxdevices())
				{
					devices = maxdevices();
				}
				for (short i = idx; i < devices - 1; i++)
				{
					for (short j = 0; j < 23; j++)
					{
						boardnames[i][j] = boardnames[i + 1][j];
					}
				}
				devices -= 1;
				saveState();
				request->send(200, "text/plain", "OK");
			}
			else
			{
				request->send(200, "text/plain", "BADIDX");
			}
		}
		else
		{
			Serial.println("Missing params");
			request->send(200, "text/plain", "MISSINGPARAMS");
		}
	});

	server.on("/setstate", HTTP_POST, [](AsyncWebServerRequest *request) {
		Serial.println("setstate request"); // TODO - this needs to be tested
		if (request->hasParam("isSlave", true))
		{
			int idx = request->getParam("isSlave", true)->value().toInt();
			bool oldislave = isSlave;
			if (idx == 0)
			{
				Serial.println("switched to master");
				isSlave = false;
			}
			else
			{
				Serial.println("switched to slave");
				isSlave = true;
			}
			saveState();
			request->send(200, "text/plain", "OK");
			if (oldislave != isSlave)
			{
				ESP.restart();
			}
		}
		else
		{
			Serial.println("Missing params");
			request->send(200, "text/plain", "MISSINGPARAMS");
		}
	});

	server.on("/httpreq", HTTP_GET, [](AsyncWebServerRequest *request) {
		Serial.println("HTTP test request");
		if (WiFi.status() == WL_CONNECTED)
		{

			// Your IP address with path or Domain name with URL path
			makehttprequest("http://192.168.5.2/setlighting", 1, "{\"r\": 200, \"g\": 40, \"b\": 200, \"time\": 15}");
			request->send(200, "text/plain", "OK");
		}
		else
		{
			request->send(200, "text/plain", "NOCONN");
		}
	});

	server.on("/forward", HTTP_POST, [](AsyncWebServerRequest *request) {
		if (!isSlave)
		{
			if (request->hasParam("boardname", true) && request->hasParam("postjson", true) && request->hasParam("post", true) && request->hasParam("url", true))
			{
				// step 1: get the board that's being referred to

				char ipstr[15] = "bad";
				boardnametoip(request->getParam("boardname", true)->value().c_str(), ipstr);
				Serial.println(ipstr);
				char urlstr[50] = "http://";

				if (ipstr[0] == 'N')
				{
					request->send(200, "text/plain", "MISSINGBNAME");
				}
				else
				{
					if (ipstr[0] == 'M')
					{
						request->send(200, "text/plain", "DEVICENOCONN");
					}
					else
					{
						Serial.println("found device");
						// step 3: send it a message

						// Your IP address with path or Domain name with URL path
						strncat(urlstr, ipstr, strlen(ipstr));
						short oldlen = strlen(urlstr);
						for (int i = 0; i < 20; i++)
						{
							urlstr[oldlen + i] = request->getParam("url", true)->value().c_str()[i];
							if (request->getParam("url", true)->value().c_str()[i] == '\0')
							{
								break;
							}
						}
						Serial.println(urlstr);
						makehttprequest(urlstr, request->getParam("post", true)->value().c_str()[0] == '1', request->getParam("postjson", true)->value().c_str());
						request->send(200, "text/plain", httpresponse.c_str());
						Serial.println("Sent");
						//Serial.println("Sent2");
						//request->send(200, "text/plain", "OK");
					}
				}
			}
			else
			{
				request->send(200, "text/plain", "MISSINGPARAMS");
			}
		}
		else
		{
			request->send(404, "text/plain", "");
		}
	});

	server.on("/sensordata", HTTP_GET, [](AsyncWebServerRequest *request) {
		std::string toreturn = "";
		// line 1 - current moisture reading
		toreturn += String(4095 - analogRead(MOISTURE_PIN)).c_str();
		toreturn += "\n";
		toreturn += (watering ? "watering" : "notwatering");
		toreturn += "\n";
		toreturn += wateringreason;
		toreturn += "\n";
		toreturn += String((double)(cooldownendsat - esp_timer_get_time())).c_str();
		toreturn += "\n";
		request->send(200, "text/plain", (std::string(String(lasttemperatureReading).c_str()) + "\n" + String(lastmoistureReading).c_str() + "\n" + String(lastsunlightReading).c_str() + "\n" + toreturn).c_str());
	});

	server.on("/getwateringinfo", HTTP_GET, [](AsyncWebServerRequest *request) {
		std::string toreturn = "";
		// line 1 - current moisture reading
		toreturn += String(4095 - analogRead(MOISTURE_PIN)).c_str();
		toreturn += "\n";
		toreturn += (watering ? "watering" : "notwatering");
		toreturn += "\n";
		toreturn += wateringreason;
		toreturn += "\n";
		toreturn += String((double)(cooldownendsat - esp_timer_get_time())).c_str();
		toreturn += "\n";
		request->send(200, "text/plain", toreturn.c_str());
	});

	server.on("/givedata", [](AsyncWebServerRequest *request) {
		Serial.println("endpoint hit");
		if (!isSlave)
		{
			if (request->hasParam("boardname", true) && request->hasParam("moisture", true) && request->hasParam("temperature", true) && request->hasParam("sunlight", true))
			{
				HTTPClient https;
				https.begin("https://api.irrigation.smartlylinked.com/givedata", root_ca); //Specify the URL and certificate
				https.setTimeout(2250);
				https.addHeader("Content-Type", "application/x-www-form-urlencoded");
				int httpCode = https.POST((std::string("password=ad9uwxcCkeTBSekNnrCMjj5m&boardname=") + request->getParam("boardname", true)->value().c_str() + "&moisture=" + request->getParam("moisture", true)->value().c_str() + "&temperature=" + request->getParam("temperature", true)->value().c_str() + "&sunlight=" + +request->getParam("sunlight", true)->value().c_str()).c_str()); //Make the request

				if (httpCode > 0)
				{ //Check for the returning code
					String payload = https.getString();
					Serial.println(httpCode);
					Serial.println(payload);
				}

				else
				{
					Serial.println("Error on HTTP request");
				}

				https.end(); //Free the resources
				request->send(200, "text/plain", "OK");
			}
			else
			{
				request->send(200, "text/plain", "BADPARAMS");
			}
		}
		else
		{
			request->send(404, "text/plain", "");
		}
	});

	server.on("/givewateringdata", [](AsyncWebServerRequest *request) {
		if (!isSlave)
		{
			if (request->hasParam("boardname", true) && request->hasParam("wateringstate", true) && request->hasParam("reason", true))
			{
				HTTPClient https;
				https.begin("https://api.irrigation.smartlylinked.com/givewateringdata", root_ca); //Specify the URL and certificate
				https.setTimeout(2250);
				https.addHeader("Content-Type", "application/x-www-form-urlencoded");
				int httpCode = https.POST((std::string("password=ad9uwxcCkeTBSekNnrCMjj5m&boardname=") + request->getParam("boardname", true)->value().c_str() + "&wateringstate=" + request->getParam("wateringstate", true)->value().c_str() + "&reason=" + request->getParam("reason", true)->value().c_str()).c_str()); //Make the request

				if (httpCode > 0)
				{ //Check for the returning code
					String payload = https.getString();
					Serial.println(httpCode);
					Serial.println(payload);
				}

				else
				{
					Serial.println("Error on HTTP request");
				}

				https.end(); //Free the resources
				request->send(200, "text/plain", "OK");
			}
			else
			{
				request->send(200, "text/plain", "BADPARAMS");
			}
		}
		else
		{
			request->send(404, "text/plain", "");
		}
	});

	// server.on("/testhttps", HTTP_GET, [](AsyncWebServerRequest *request) {
	//  HTTPClient https;
	//  https.begin("https://api.irrigation.smartlylinked.com/", root_ca); //Specify the URL and certificate
	//  https.setTimeout(2250);
	//  int httpCode = https.GET();                      //Make the request
	//  if (httpCode > 0)
	//  { //Check for the returning code
	//    String payload = https.getString();
	//    Serial.println(httpCode);
	//    Serial.println(payload);
	//  }
	//  else
	//  {
	//    Serial.println("Error on HTTP request");
	//  }
	//  https.end(); //Free the resources
	//  request->send(200, "text/plain", "OK");
	// });
	// server.on("/forward", HTTP_GET, [](AsyncWebServerRequest *request) {
	//  HTTPClient http;
	//  Serial.println("HTTP test request");
	//  if (!isSlave)
	//  {
	//    // char ipstr[15];
	//    // boardnametoip(request->getParam("boardname", true)->value().c_str(), ipstr);
	//    // char urlstr[50] = "http://";
	//    // strcat(urlstr, ipstr);
	//    // strcat(urlstr, request->getParam("url", true)->value().c_str());
	//    // Your IP address with path or Domain name with URL path
	//    http.begin("http://192.168.5.2/");
	//    // Send HTTP POST request
	//    int httpResponseCode = http.GET();
	//    String payload = "--";
	//    if (httpResponseCode > 0)
	//    {
	//      Serial.print("HTTP Response code: ");
	//      Serial.println(httpResponseCode);
	//      payload = http.getString();
	//    }
	//    else
	//    {
	//      Serial.print("Error code: ");
	//      Serial.println(httpResponseCode);
	//    }
	//    // Free resources
	//    http.end();
	//    Serial.println(payload);
	//    request->send(200, "text/plain", "OK");
	//  }
	//  else
	//  {
	//    request->send(200, "text/plain", "NOCONN/PARAM");
	//  }
	// });

	server.on("/getconnecteddevices", HTTP_GET, [](AsyncWebServerRequest *request) {
		wifi_sta_list_t wifi_sta_list;
		tcpip_adapter_sta_list_t adapter_sta_list;

		memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
		memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

		esp_wifi_ap_get_sta_list(&wifi_sta_list);
		tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);

		for (int i = 0; i < adapter_sta_list.num; i++)
		{

			tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];

			Serial.print("station nr ");
			Serial.println(i);

			Serial.print("MAC: ");

			for (int i = 0; i < 6; i++)
			{

				Serial.printf("%02X", station.mac[i]);
				if (i < 5)
					Serial.print(":");
			}

			Serial.print("\nIP: ");
			Serial.println(ip4addr_ntoa(&(station.ip)));
		}

		Serial.println("-----------");
		request->send(200, "text/plain", "OK");
	});

	// SEMAPHORE CODE
	// server.on("/waterrequest", HTTP_POST, [](AsyncWebServerRequest *request) {
	//  if (!isSlave)
	//  {
	//    if (request->hasParam("boardname", true))
	//    {
	//      Serial.println("Received request to water");
	//      if (semaphoreSet)
	//      {
	//        Serial.println("Semaphore already held by: ");
	//        std::string toreturn = "NO\n";
	//        toreturn += readstrfromarr(semaphore, 23);
	//        Serial.println(toreturn.c_str());
	//        request->send(200, "text/plain", toreturn);
	//      }
	//      else
	//      {
	//        Serial.println("Claiming semaphore");
	//        // claim the semaphore
	//        std::string semaphoreboardname(request->getParam("boardname", true)->value().c_str());
	//        writestrtoarr(semaphoreboardname, semaphore, 23);
	//        semaphoresetat = esp_timer_get_time();
	//        semaphoreSet = true;
	//        request->send(200, "text/plain", "OK");
	//      }
	//    }
	//    else
	//    {
	//      request->send(200, "text/plain", "MISSINGPARAMS");
	//    }
	//  }
	//  else
	//  {
	//    request->send(404, "text/plain", "");
	//  }
	// });
	// server.on("/watercomplete", HTTP_POST, [](AsyncWebServerRequest *request) {
	//  if (!isSlave)
	//  {
	//    if (request->hasParam("boardname", true))
	//    {
	//      Serial.println("Water complete request");
	//      if (semaphoreSet)
	//      {
	//        // check the boardname
	//        std::string semaphoreboardname(request->getParam("boardname", true)->value().c_str());
	//        if (semaphoreboardname == readstrfromarr(semaphore, 23))
	//        {
	//          Serial.println("Unsetting semaphore");
	//          semaphoreSet = false;
	//          semaphoresetat = LLMAX;
	//          request->send(200, "text/plain", "OK");
	//        }
	//        else {
	//          Serial.println("The requester doesn't own the semaphore");
	//          request->send(200, "text/plain", "BADOWNER");
	//        }
	//      }
	//      else
	//      {
	//        Serial.println("No semaphore");
	//        request->send(200, "text/plain", "NOSEMAPHORE");
	//      }
	//    }
	//    else
	//    {
	//      request->send(200, "text/plain", "MISSINGPARAMS");
	//    }
	//  }
	//  else
	//  {
	//    request->send(404, "text/plain", "");
	//  }
	// });

	server.on("/userwater", HTTP_POST, [](AsyncWebServerRequest *request) {
		if (request->hasParam("force", true) && request->hasParam("for", true))
		{
			if (request->getParam("force", true)->value().c_str()[0] == '1')
			{
				beginWatering(true, true);
				request->send(200, "text/plain", "OK");
				// stop watering in for seconds
				endwateringat = esp_timer_get_time() + (request->getParam("for", true)->value().toInt() * 1000 * 1000);
			}
			else
			{
				beginWatering(true, false);
				if (watering)
				{
					request->send(200, "text/plain", "OK");
				}
				else
				{
					request->send(200, "text/plain", "BLOCKED");
				}
			}
		}
		else
		{
			request->send(200, "text/plain", "MISSINGPARAMS");
		}
	});
	server.on("/userstopwater", HTTP_POST, [](AsyncWebServerRequest *request) {
		stopWatering();
		request->send(200, "text/plain", "OK");
	});

	server.on("/updatepage", HTTP_GET, [](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
		response->addHeader("Connection", "close");
		response->addHeader("Access-Control-Allow-Origin", "*");
		request->send(response);
	});

	server.on(
		"/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    // the request handler is triggered after the upload has finished... 
    // create the response, add header, and send response
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", (Update.hasError())?"FAIL":"OK");
    response->addHeader("Connection", "close");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
    restartRequired = true; }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    //Upload handler chunks in data
    
    if(!index){ // if index == 0 then this is the first frame of data
      Serial.printf("UploadStart: %s\n", filename.c_str());
      Serial.setDebugOutput(true);
      
      // calculate sketch space required for the update
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if(!Update.begin(maxSketchSpace)){//start with max available size
        Update.printError(Serial);
      }
      // Update.runAsync(true); // tell the updaterClass to run in async mode
    }
	Serial.println("In update");
	Serial.println(index);
    //Write chunked data to the free sketch space
    if(Update.write(data, len) != len){
        Update.printError(Serial);
    }
    
    if(final){ // if the final flag is set then this is the last frame of data
      if(Update.end(true)){ //true to set the size to the current progress
          Serial.printf("Update Success: %u B\nRebooting...\n", index+len);
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
    } });

	server.onNotFound(notFound);
}

//uint8_t newMACAddress[] = {0xC4, 0x4F, 0x33, 0x75, 0x7A, 0xB1};

void setup()
{
	Serial.begin(115200);
	WiFi.mode(WIFI_AP_STA);

	Serial.println(WiFi.macAddress());
	//esp_wifi_set_mac(ESP_IF_WIFI_STA, &newMACAddress[0]);

	Serial.print("[NEW] ESP32 Board MAC Address:  ");
	Serial.println(WiFi.macAddress());

	delay(100);
	WiFi.disconnect();
	ledcSetup(0, 5000, 8);
	ledcAttachPin(REDPIN, 0);
	ledcSetup(1, 5000, 8);
	ledcAttachPin(GREENPIN, 1);
	ledcSetup(2, 5000, 8);
	ledcAttachPin(BLUEPIN, 2);

	Serial.println("Ojas Gulati 2020 - v4");
	EEPROM.begin(EEPROM_SIZE);
	randomSeed(analogRead(0));
	// if we have stored anything in EEPROM, bit VERSION_BYTE will contain the value
	short preset = EEPROM.read(VERSION_BYTE);
	if (preset != EEPROM_VERSION)
	{
		Serial.println("No state found, saving state...");
		Serial.println("Autogenerating boardname");
		std::string boardnamepool = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		std::string boardnamestr = "AutoGarden-";
		for (short i = 0; i < 8; i++)
		{
			boardnamestr.push_back(boardnamepool[random(boardnamepool.size())]);
		}

		Serial.println(boardnamestr.c_str());
		writestrtoarr(boardnamestr, boardname, 23);

		Serial.println("Autogenerating boardpassword");
		std::string boardpasswordpool = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		std::string boardpasswordstr = "";
		for (short i = 0; i < 8; i++)
		{
			boardpasswordstr.push_back(boardpasswordpool[random(boardpasswordpool.size())]);
		}

		Serial.println(boardpasswordstr.c_str());
		writestrtoarr(boardpasswordstr, boardpassword, 8);
		saveState();
	}
	readState();
	Serial.println("Printing state:");
	printState();
	Serial.println("Starting WIFI work!");
	delay(100);
	WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
	delay(100);
	//WiFi.setHostname((readstrfromarr(boardname, 23) + "").c_str());
	if (!WiFi.setHostname("myFancyESP32"))
	{
		Serial.println("Hostname failed to configure");
	}
	delay(100);
	if (hasWiFi)
	{
		WiFi.begin(ssid, password);
		delay(2000);
		if (WiFi.status() == WL_CONNECTED)
		{
			Serial.print("IP Address: ");
			Serial.println(WiFi.localIP());
		}
	}

	WiFi.softAP(readstrfromarr(boardname, 23).c_str(), readstrfromarr(boardpassword, 8).c_str());
	delay(100);
	Serial.println(WiFi.macAddress());

	if (isSlave)
	{
		WiFi.softAPConfig(IPAddress(192, 168, 6, 1), IPAddress(192, 168, 6, 1), IPAddress(255, 255, 255, 0));
	}
	else
	{
		WiFi.softAPConfig(IPAddress(192, 168, 5, 1), IPAddress(192, 168, 5, 1), IPAddress(255, 255, 255, 0));
	}
	IPAddress myIP = WiFi.softAPIP();
	Serial.print("AP IP address: ");
	Serial.println(myIP);

	DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), F("*"));
	DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Headers"), F("content-type"));

	addServerMethods();

	lastwificonn = esp_timer_get_time() + (5000 * 1000);
	Serial.println("beginning");
	server.begin();
	Serial.println("begun - updated");
	// delay(1000);
	// TODO: rgb sweep here
	pinMode(WATERING_PIN, OUTPUT);
	sensors.begin();
	sensors.setWaitForConversion(false);
	sensors.setResolution(12);
	sensors.requestTemperatures();
	temperatureReadyAt = esp_timer_get_time() + (1000 * 1000);

	// ArduinoOTA
	//  .onStart([]() {
	//    String type;
	//    if (ArduinoOTA.getCommand() == U_FLASH)
	//      type = "sketch";
	//    else // U_SPIFFS
	//      type = "filesystem";
	//    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
	//    Serial.println("Start updating " + type);
	//  })
	//  .onEnd([]() {
	//    Serial.println("\nEnd");
	//  })
	//  .onProgress([](unsigned int progress, unsigned int total) {
	//    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
	//  })
	//  .onError([](ota_error_t error) {
	//    Serial.printf("Error[%u]: ", error);
	//    if (error == OTA_AUTH_ERROR)
	//      Serial.println("Auth Failed");
	//    else if (error == OTA_BEGIN_ERROR)
	//      Serial.println("Begin Failed");
	//    else if (error == OTA_CONNECT_ERROR)
	//      Serial.println("Connect Failed");
	//    else if (error == OTA_RECEIVE_ERROR)
	//      Serial.println("Receive Failed");
	//    else if (error == OTA_END_ERROR)
	//      Serial.println("End Failed");
	//  });

	// ArduinoOTA.begin();

	Serial.println(esp_get_free_heap_size());

	for (short i = 0; i < 60; i++)
	{
		mreadings[i] = 4096;
		treadings[i] = 4096;
		lreadings[i] = 4096;
	}

	for (short i = 0; i < 10; i++)
	{
		smreadings[i] = 4096;
		streadings[i] = 4096;
		slreadings[i] = 4096;
	}

	for (short i = 0; i < SERVER_POST_BLOCKS; i++)
	{
		pmreadings[i] = 4096;
		ptreadings[i] = 4096;
		plreadings[i] = 4096;
	}

	for (short i = 0; i <= 255; i++)
	{
		delay(6);
		color hsvrainbow(i, 255, 255);
		color rgbrainbow = hsvtorgb(hsvrainbow);

		writecolor(&rgbrainbow);
	}
}

void loop()
{
	if (watering)
	{
		digitalWrite(WATERING_PIN, HIGH);
	}
	else
	{
		digitalWrite(WATERING_PIN, LOW);
	}

	if (esp_timer_get_time() > endwateringat)
	{
		stopWatering();
	}

	if (restartRequired)
	{ // check the flag here to determine if a restart is required
		Serial.printf("Restarting ESP\n\r");
		restartRequired = false;
		ESP.restart();
	}
	//ArduinoOTA.handle();
	if (esp_timer_get_time() > temperatureReadyAt)
	{
		moisturelighting = newmoisturelighting;
		// STEP 1: make readings
		float temperatureC = sensors.getTempCByIndex(0);
		short temperatureReading = (short)(temperatureC * 100 + (25 * 100));
		short moistureReading = (4095 - analogRead(MOISTURE_PIN));
		short sunlightReading = analogRead(SUNLIGHT_PIN);
		Serial.print(moistureReading);
		Serial.print("M ");
		Serial.print(sunlightReading);
		Serial.print("L ");
		Serial.print(temperatureC);
		Serial.println("C ");

		lasttemperatureReading = temperatureReading;
		lastmoistureReading = moistureReading;
		lastsunlightReading = sunlightReading;
		sensors.requestTemperatures();

		// calculate newmoisturelighting
		short dynamicsensor;
		if (lightingtype == 'a')
		{
			dynamicsensor = sunlightReading;
		}
		else if (lightingtype == 'b')
		{
			dynamicsensor = moistureReading;
		}
		else
		{
			dynamicsensor = temperatureReading;
		}
		if (dynamicsensor < lowvalue)
		{
			dynamicsensor = lowvalue;
		}
		if (dynamicsensor > highvalue)
		{
			dynamicsensor = highvalue;
		}

		float percent = (float)(dynamicsensor - lowvalue) / (float)(highvalue - lowvalue);
		newmoisturelighting = hsvaverage(lowlighting, highlighting, percent);

		// go and store these readings in the seconds cache
		smreadings[scurridx] = moistureReading;
		streadings[scurridx] = temperatureReading;
		slreadings[scurridx] = sunlightReading;

		if (moistureReading >= moistureEnd && wateringreason == 'm')
		{
			stopWatering();
		}

		if (scurridx == 9)
		{
			if (checkingTime > 10)
			{
				checkingTime = 10;
			}
			if (checkingTime <= 0)
			{
				checkingTime = 1;
			}
			Serial.println("10s average: ");
			int submt = 0;
			int subtt = 0;
			int sublt = 0;
			for (short i = 0; i < 10; i++)
			{
				submt += smreadings[i];
				subtt += streadings[i];
				sublt += slreadings[i];
			}
			submt /= 10;
			subtt /= 10;
			sublt /= 10;
			Serial.println(submt);
			Serial.println(subtt);
			Serial.println(sublt);
			mreadings[maincurridx] = submt;
			treadings[maincurridx] = subtt;
			lreadings[maincurridx] = sublt;
			pmreadings[pcurridx] = submt;
			ptreadings[pcurridx] = subtt;
			plreadings[pcurridx] = sublt;

			if (maincurridx == ((checkingTime * 6) - 1))
			{
				maincurridx = 0;
			}
			else
			{
				maincurridx += 1;
			}

			if (pcurridx == SERVER_POST_BLOCKS - 1)
			{
				pcurridx = 0;
				int subPMt = 0;
				int subPTt = 0;
				int subPLt = 0;
				for (short i = 0; i < SERVER_POST_BLOCKS; i++)
				{
					subPMt += pmreadings[i];
					subPTt += ptreadings[i];
					subPLt += plreadings[i];
				}
				subPMt /= SERVER_POST_BLOCKS;
				subPTt /= SERVER_POST_BLOCKS;
				subPLt /= SERVER_POST_BLOCKS;

				sendDataPacket(readstrfromarr(boardname, 23), subPMt, subPTt, subPLt);
			}
			else
			{
				pcurridx += 1;
			}

			int subMOt = 0;
			for (short i = 0; i < (checkingTime * 6); i++)
			{
				subMOt += mreadings[i];
			}
			subMOt /= (checkingTime * 6);
			if (subMOt < moistureBegin)
			{
				beginWatering(false, false);
			}

			scurridx = 0;
		}
		else
		{
			scurridx += 1;
		}

		temperatureReadyAt = esp_timer_get_time() + (1000 * 1000);
	}

	if (WiFi.status() != WL_CONNECTED && lastwificonn < esp_timer_get_time())
	{
		Serial.println("triggering reconnect");
		if (hasWiFi)
		{
			WiFi.begin(ssid, password);
		}
		lastwificonn = esp_timer_get_time() + (20000 * 1000);
	}
	// set our lighting
	if (userstopafter < esp_timer_get_time())
	{
		useractive = false;
	}
	if (systemstopafter < esp_timer_get_time())
	{
		systemactive = false;
	}
	if (flashtoggleafter < esp_timer_get_time())
	{
		flashison = !flashison;
		flashtoggleafter = esp_timer_get_time() + flashperiod;
	}
	if (useractive)
	{
		writecolor(&userlighting);
	}
	else if (systemactive)
	{
		if (!flashison && isflashing)
		{
			color black(0, 0, 0);
			writecolor(&black);
		}
		else
		{
			writecolor(&systemlighting);
		}
	}
	else
	{
		// actual moisture lighting is time adjusted average from
		float completion = 1.0 - ((float)(temperatureReadyAt - esp_timer_get_time()) / (1000 * 1000));

		color adjustedlighting = hsvaverage(moisturelighting, newmoisturelighting, completion);

		writecolor(&adjustedlighting);
	}

	delay(50);
}
