#include <WiFi.h>
#include <HTTPClient.h>
#include "HX711.h"

// Add at the top with other global variables
float lastWeight = 0;                      // Store the last sent weight
unsigned long lastSendTime = 0;            // Store the last send time
const unsigned long SEND_INTERVAL = 1000; // 1 second in milliseconds
const float WEIGHT_CHANGE_THRESHOLD = 2; // Minimum weight change to trigger sending (in grams)

// Network credentials
const char *ssid = "PlayMita"; // Nama WIFI
const char *password = "MITAT0BRUTB41K"; // Password WIFI

// HTTP server details
const char *serverUrl = "http://192.168.1.12:8080/data";

// Define the pins for HX711
const uint8_t DATA_PIN = 17;  // HX711 Data pin
const uint8_t CLOCK_PIN = 18; // HX711 Clock pin

HX711 scale;

// Calibration factor for grams (update after calibration)
float calibrationFactor = -99.68; // Added negative sign to invert readings

// Function to connect to Wi-Fi
void connectWiFi()
{
  Serial.println("Attempting to connect to Wi-Fi...");
  WiFi.begin(ssid, password);

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    attempt++;
    if (attempt > 20)
    { // Timeout after 10 seconds
      Serial.println("\nFailed to connect to Wi-Fi. Please check your credentials.");
      return;
    }
  }

  Serial.println("\nWi-Fi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Function to send weight data to HTTP server
void sendData(float weight)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;

    // Begin HTTP connection
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    // Create JSON payload
    String payload = "{";
    payload += "\"galon\": \"galon A\", ";
    payload += "\"value\": " + String(weight, 6); // Send as a proper float with more precision
    payload += "}";

    // Send POST request
    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0)
    {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String response = http.getString();
      Serial.println("Response: " + response);
    }
    else
    {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  }
  else
  {
    Serial.println("WiFi Disconnected");
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000); // Wait for Serial Monitor to open

  // Connect to Wi-Fi
  connectWiFi();

  Serial.println("HX711 Weight Measurement in GRAMS");

  // Initialize HX711
  scale.begin(DATA_PIN, CLOCK_PIN);

  // Set scale
  scale.set_scale(calibrationFactor);

  // Tare the scale
  Serial.println("Taring... Remove all weight.");
  delay(2000);
  scale.tare(); // Reset scale to zero
  Serial.println("Tare completed");
}

void loop()
{
  unsigned long currentTime = millis();

  // Check if it's time to read and potentially send data (every minute)
  if (currentTime - lastSendTime >= SEND_INTERVAL)
  {
    if (scale.is_ready())
    {
      // Get weight in grams
      float currentWeight = scale.get_units();

      Serial.print("Weight: ");
      Serial.print(currentWeight, 2);
      Serial.println(" g");

      // Check if weight has changed significantly
      if (abs(currentWeight - lastWeight) >= WEIGHT_CHANGE_THRESHOLD)
      {
        // Send weight data to HTTP server
        sendData(currentWeight);
        // Update last sent weight
        lastWeight = currentWeight;
      }
      else
      {
        Serial.println("Weight hasn't changed significantly. Not sending data.");
      }
    }
    else
    {
      Serial.println("HX711 not found.");
    }

    // Update last send time
    lastSendTime = currentTime;
  }
}
