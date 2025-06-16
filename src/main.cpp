#include <WiFi.h>
#include <HTTPClient.h>
#include "HX711.h"

// Add at the top with other global variables
float lastWeight = 0;                     // Store the last sent weight
unsigned long lastSendTime = 0;           // Store the last send time
const unsigned long SEND_INTERVAL = 1000; // 1 second in milliseconds
const float WEIGHT_CHANGE_THRESHOLD = 2;  // Minimum weight change to trigger sending (in grams)

// Network credentials
const char *ssid = "YANTO_WIFI";           // Nama WIFI
const char *password = "martiarganteng"; // Password WIFI (leave empty "" for open networks)

// HTTP server details
const char *serverUrl = "http://192.168.1.19:8080/data"; // IP nya diganti -> ifconfig trus cari ip yang depannya 192.168.1.x

// Define the pins for HX711
const uint8_t DATA_PIN = 17;  // HX711 Data pin
const uint8_t CLOCK_PIN = 18; // HX711 Clock pin

const float full_galon_weight = 20000; // Berat penuh galon dalam gram

HX711 scale;

// Calibration factor for grams (update after calibration)
float calibrationFactor = 28.6; // Fine-tuned: 26.5 * (19000/17606.94) = ~28.6
float zeroOffset = 0; // Offset to correct zero reading

// Function to connect to Wi-Fi
void connectWiFi()
{
  Serial.println("=== WiFi Connection Debug ===");
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  Serial.print("Password length: ");
  Serial.println(strlen(password));
  
  // Disconnect any previous connection
  WiFi.disconnect();
  delay(1000);
  
  // Set WiFi mode to station
  WiFi.mode(WIFI_STA);
  delay(1000);
  
  // Scan for networks first to verify SSID exists
  Serial.println("Scanning for available networks...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("No networks found!");
    return;
  }
  
  bool ssidFound = false;
  for (int i = 0; i < n; ++i) {
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" (");
    Serial.print(WiFi.RSSI(i));
    Serial.print(" dBm) ");
    Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Encrypted");
    
    if (WiFi.SSID(i) == ssid) {
      ssidFound = true;
      Serial.print("*** Target network found with signal strength: ");
      Serial.print(WiFi.RSSI(i));
      Serial.println(" dBm ***");
    }
  }
  
  if (!ssidFound) {
    Serial.println("ERROR: Target SSID not found in scan results!");
    Serial.println("Please check the SSID name and make sure you're in range.");
    return;
  }
  
  // Clear scan results
  WiFi.scanDelete();
  
  Serial.println("Starting connection attempt...");
  
  // Check if password is provided (for open networks, use empty string)
  if (strlen(password) == 0) {
    WiFi.begin(ssid); // Connect to open network
    Serial.println("Connecting to open WiFi network...");
  } else {
    WiFi.begin(ssid, password); // Connect to password-protected network
    Serial.println("Connecting to password-protected WiFi network...");
  }

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 30)
  {
    delay(500);
    Serial.print(".");
    Serial.print("(");
    Serial.print(attempt + 1);
    Serial.print(")");
    
    // Print WiFi status every 5 attempts
    if ((attempt + 1) % 5 == 0) {
      Serial.print(" Status: ");
      switch(WiFi.status()) {
        case WL_IDLE_STATUS:
          Serial.print("IDLE");
          break;
        case WL_NO_SSID_AVAIL:
          Serial.print("NO_SSID");
          break;
        case WL_SCAN_COMPLETED:
          Serial.print("SCAN_COMPLETED");
          break;
        case WL_CONNECTED:
          Serial.print("CONNECTED");
          break;
        case WL_CONNECT_FAILED:
          Serial.print("CONNECT_FAILED");
          break;
        case WL_CONNECTION_LOST:
          Serial.print("CONNECTION_LOST");
          break;
        case WL_DISCONNECTED:
          Serial.print("DISCONNECTED");
          break;
        default:
          Serial.print("UNKNOWN");
      }
      Serial.println();
    }
    
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n=== WiFi Connected Successfully! ===");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("Subnet mask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());
    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("\n=== WiFi Connection Failed! ===");
    Serial.print("Final status: ");
    switch(WiFi.status()) {
      case WL_NO_SSID_AVAIL:
        Serial.println("Network not found");
        break;
      case WL_CONNECT_FAILED:
        Serial.println("Wrong password or authentication failed");
        break;
      case WL_CONNECTION_LOST:
        Serial.println("Connection lost");
        break;
      case WL_DISCONNECTED:
        Serial.println("Disconnected");
        break;
      default:
        Serial.println("Unknown error");
    }
    Serial.println("Troubleshooting tips:");
    Serial.println("1. Double-check SSID and password");
    Serial.println("2. Make sure you're in range of the router");
    Serial.println("3. Try restarting the ESP32");
    Serial.println("4. Check if the network is 2.4GHz (ESP32 doesn't support 5GHz)");
  }
}

// Function to send weight data to HTTP server
void sendData(float weight)
{
  // Check WiFi connection status
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi Disconnected! Attempting to reconnect...");
    connectWiFi();
    
    // If still not connected after reconnection attempt, return
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconnection failed. Skipping data send.");
      return;
    }
  }
  
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;

    // Begin HTTP connection
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    // Calculate percentage
    float percentage = (weight / full_galon_weight) * 100.0;

    // Create JSON payload
    String payload = "{";
    payload += "\"galon\": \"galon A\", ";            // Ganti dengan identitas galon
    payload += "\"value\": " + String(percentage, 2); // Send as percentage with 2 decimal places
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

  // Set scale with initial calibration factor
  scale.set_scale(calibrationFactor);

  // Improved taring process
  Serial.println("Taring... Remove all weight and wait.");
  delay(3000);
  scale.tare(); // Reset scale to zero
  delay(1000);
  
  // Take a few readings to establish zero offset
  float zeroSum = 0;
  for(int i = 0; i < 10; i++) {
    if(scale.is_ready()) {
      zeroSum += scale.get_units();
      delay(100);
    }
  }
  zeroOffset = zeroSum / 10.0;
  
  Serial.println("Tare completed");
  Serial.print("Zero offset: ");
  Serial.println(zeroOffset);
}

void loop()
{
  unsigned long currentTime = millis();

  // Check if it's time to read and potentially send data
  if (currentTime - lastSendTime >= SEND_INTERVAL)
  {
    if (scale.is_ready())
    {
      // Take multiple readings for stability
      float weightSum = 0;
      int validReadings = 0;
      
      for(int i = 0; i < 5; i++) {
        if(scale.is_ready()) {
          float reading = scale.get_units();
          weightSum += reading;
          validReadings++;
          delay(20); // Small delay between readings
        }
      }
      
      if(validReadings > 0) {
        // Get average weight and apply zero offset correction
        float rawWeight = weightSum / validReadings;
        float currentWeight = rawWeight - zeroOffset;
        
        // Ensure weight doesn't go negative (set minimum to 0)
        if (currentWeight < 0) {
          currentWeight = 0;
        }
        
        // Round to nearest gram for stability
        currentWeight = round(currentWeight);

        // Calculate percentage
        float percentage = (currentWeight / full_galon_weight) * 100.0;

        Serial.print("Weight: ");
        Serial.print(currentWeight, 2);
        Serial.println(" g");

        // Output the percentage value as well
        Serial.print("Percentage: ");
        Serial.print(percentage, 2);
        Serial.println(" %");

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
    }
    else
    {
      Serial.println("HX711 not found.");
    }

    // Update last send time
    lastSendTime = currentTime;
  }
}
