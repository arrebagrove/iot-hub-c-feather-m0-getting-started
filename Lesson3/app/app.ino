// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <Adafruit_WINC1500.h>
#include <Adafruit_WINC1500Client.h>
#include <Adafruit_WINC1500Server.h>
#include <Adafruit_WINC1500SSLClient.h>
#include <Adafruit_WINC1500Udp.h>
#include <time.h>
#include <sys/time.h>
#include "NTPClient.h"
#include <AzureIoTHub.h>

#include "config.h"

#define WINC_CS   8
#define WINC_IRQ  7
#define WINC_RST  4
#define WINC_EN   2

#define LED_PIN 13

// Setup the WINC1500 connection with the pins above and the default hardware SPI.
Adafruit_WINC1500 WiFi(WINC_CS, WINC_IRQ, WINC_RST);

static Adafruit_WINC1500SSLClient sslClient; // for Adafruit WINC1500

/*
 * The new version of AzureIoTHub library change the AzureIoTHubClient signature.
 * As a temporary solution, we will test the definition of AzureIoTHubVersion, which is only defined 
 *    in the new AzureIoTHub library version. Once we totally deprecate the last version, we can take 
 *    the #ifdef out.
 */
#ifdef AzureIoTHubVersion
static AzureIoTHubClient iotHubClient;
#else
AzureIoTHubClient iotHubClient(sslClient);
#endif

void initSerial()
{
    // Start serial and initialize stdout
    Serial.begin(9600);

    // wait for serial port to connect. Needed for native USB port only
    while (!Serial); 
}

void initWifi()
{
    // Attempt to connect to Wifi network:
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);


    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WiFi.begin(ssid, pass);

    while (WiFi.status() != WL_CONNECTED)
    {
      // Get Mac Address and show it.
      // WiFi.macAddress(mac) save the mac address into a six length array, but the endian may be different. The m0 board should
      // start from mac[5] to mac[0], but some other kinds of board run in the oppsite direction.  
      uint8_t mac[6];
      WiFi.macAddress(mac);
      Serial.print("You device with MAC address ");
      Serial.print(mac[5], HEX);
      Serial.print(":");
      Serial.print(mac[4], HEX);
      Serial.print(":");
      Serial.print(mac[3], HEX);
      Serial.print(":");
      Serial.print(mac[2], HEX);
      Serial.print(":");
      Serial.print(mac[1], HEX);
      Serial.print(":");
      Serial.print(mac[0], HEX);
      Serial.print(" connects to ");
      Serial.print(ssid);
      Serial.println(" failed! Waiting 10 seconds to retry.");
      WiFi.begin(ssid, pass);
      delay(10000);
    }

    Serial.print("Connected to wifi");
    Serial.println(ssid);
}

void initTime()
{
    Adafruit_WINC1500UDP     _udp;

    time_t epochTime = (time_t)-1;

    NTPClient ntpClient;
    ntpClient.begin();

    while (true)
    {
        epochTime = ntpClient.getEpochTime("0.pool.ntp.org");

        if (epochTime == (time_t)-1)
        {
            Serial.println("Fetching NTP epoch time failed! Waiting 5 seconds to retry.");
            delay(5000);
        }
        else
        {
            Serial.print("Fetched NTP epoch time is: ");
            Serial.println(epochTime);
            break;
        }
    }
    
    ntpClient.end();

    struct timeval tv;
    tv.tv_sec = epochTime;
    tv.tv_usec = 0;

    settimeofday(&tv, NULL);
}

static void sendCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
    if (IOTHUB_CLIENT_CONFIRMATION_OK == result)
    {
        LogInfo("Message sent to Azure IoT Hub\r\n");
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
    }
    else
    {
        LogInfo("Failed to send message to Azure IoT Hub\r\n");
    }
}

static void sendMessage(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, const unsigned char* buffer, size_t size)
{
    IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromByteArray(buffer, size);
    if (messageHandle == NULL)
    {
        LogInfo("unable to create a new IoTHubMessage\r\n");
    }
    else
    {
        if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messageHandle, sendCallback, NULL) != IOTHUB_CLIENT_OK)
        {
            LogInfo("Failed to hand over the message to IoTHubClient\r\n");
        }
        else
        {
            LogInfo("IoTHubClient accepted the message for delivery\r\n");
        }

        IoTHubMessage_Destroy(messageHandle);
    }
}

void setup()
{
    // enable red LED GPIO for writing
    pinMode(LED_PIN, OUTPUT);

    // delay to give user time to connect serial terminal, during this time red LED will be on    
    digitalWrite(LED_PIN, HIGH);
    delay(10000);
    digitalWrite(LED_PIN, LOW);

#ifdef WINC_EN
    pinMode(WINC_EN, OUTPUT);
    digitalWrite(WINC_EN, HIGH);
#endif

    initSerial();
    initWifi();
    initTime();

#ifdef AzureIoTHubVersion
    iotHubClient.begin(sslClient);
#else
    iotHubClient.begin();
#endif

    // setting epoch time for Azure IoT Hub
    struct timeval tv;
    gettimeofday(&tv, NULL);
    iotHubClient.setEpochTime(tv.tv_sec);
}

void loop()
{
    IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, HTTP_Protocol);

    
    if (iotHubClientHandle == NULL)
    {
        LogInfo("Failed on IoTHubClient_CreateFromConnectionString\r\n");
    }
    else
    {
        while (1)
        {
            sendMessage(iotHubClientHandle, (const unsigned char*)"message", 7);
            IoTHubClient_LL_DoWork(iotHubClientHandle);
            delay(2000);
        }

        IoTHubClient_LL_Destroy(iotHubClientHandle);
    }
}