#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// ---------- Wi-Fi settings ----------
const char* ssid = "ESP32_Bulb_Control";
const char* password = "12345678";

// ---------- DHT11 settings ----------
#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// ---------- Relay settings ----------
const int relayPins[4] = {23, 22, 21, 19};

const int RELAY_ON = LOW;
const int RELAY_OFF = HIGH;

bool relayState[4] = {false, false, false, false};

const float TEMP_ON = 30.0;
const float TEMP_OFF = 28.0;

float temperature = NAN;
float humidity = NAN;

unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 2000;

WebServer server(80);

// ---------- Webpage ----------
String makePage() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Smart Bulbs</title>
<style>
body {
  font-family: Arial, sans-serif;
  background: #eef2f7;
  text-align: center;
  padding: 15px;
}
.card {
  background: white;
  max-width: 380px;
  margin: 14px auto;
  padding: 18px;
  border-radius: 14px;
  box-shadow: 0 2px 8px #ccc;
}
button {
  padding: 12px 22px;
  margin: 5px;
  border: 0;
  border-radius: 8px;
  color: white;
  font-size: 16px;
}
.on { background: #16a34a; }
.off { background: #dc2626; }
.status { font-weight: bold; }
</style>
</head>
<body>

<h2>💡 ESP32 Smart Bulbs</h2>

<div class="card">
  <h3>🌡️ Room Temperature</h3>
  <h1 id="temp">-- °C</h1>
  <p>Humidity: <span id="hum">--</span>%</p>
  <p id="autoStatus">Reading sensor...</p>
  <small>ON at 30°C | OFF at 28°C</small>
</div>

<div class="card">
  <h3>Bulb 1</h3>
  <p class="status" id="s1">Loading...</p>
  <button class="on" onclick="control(1,'on')">ON</button>
  <button class="off" onclick="control(1,'off')">OFF</button>
</div>

<div class="card">
  <h3>Bulb 2</h3>
  <p class="status" id="s2">Loading...</p>
  <button class="on" onclick="control(2,'on')">ON</button>
  <button class="off" onclick="control(2,'off')">OFF</button>
</div>

<div class="card">
  <h3>Bulb 3</h3>
  <p class="status" id="s3">Loading...</p>
  <button class="on" onclick="control(3,'on')">ON</button>
  <button class="off" onclick="control(3,'off')">OFF</button>
</div>

<div class="card">
  <h3>Bulb 4 — Automatic</h3>
  <p class="status" id="s4">Loading...</p>
  <p>Controlled by the DHT11 sensor.</p>
</div>

<script>
async function updateStatus() {
  try {
    const response = await fetch('/status');
    const data = await response.json();

    document.getElementById('temp').textContent =
      data.temperature === null ? '-- °C' :
      data.temperature.toFixed(1) + ' °C';

    document.getElementById('hum').textContent =
      data.humidity === null ? '--' :
      data.humidity.toFixed(1);

    for (let i = 1; i <= 4; i++) {
      document.getElementById('s' + i).textContent =
        data['relay' + i] ? 'ON' : 'OFF';
    }

    document.getElementById('autoStatus').textContent =
      data.temperature === null
      ? 'Sensor error: check DHT11 wiring'
      : (data.relay4
          ? 'Temperature control: Relay ON'
          : 'Temperature control: Relay OFF');

  } catch (error) {
    document.getElementById('autoStatus').textContent =
      'Connection lost';
  }
}

async function control(relay, action) {
  try {
    await fetch('/relay/' + relay + '/' + action);
    await updateStatus();
  } catch (error) {
    alert('Could not contact ESP32');
  }
}

updateStatus();
setInterval(updateStatus, 2000);
</script>

</body>
</html>
)rawliteral";

  return page;
}

// ---------- Sensor and automatic relay ----------
void readTemperature() {
  unsigned long now = millis();

  if (now - lastDHTRead < DHT_INTERVAL) {
    return;
  }

  lastDHTRead = now;

  float newTemp = dht.readTemperature();
  float newHumidity = dht.readHumidity();

  if (isnan(newTemp) || isnan(newHumidity)) {
    Serial.println("DHT11 reading failed!");
    temperature = NAN;
    humidity = NAN;

    
    relayState[3] = false;
    digitalWrite(relayPins[3], RELAY_OFF);
    return;
  }

  temperature = newTemp;
  humidity = newHumidity;

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" C | Humidity: ");
  Serial.print(humidity);
  Serial.println("%");

  // Hysteresis control for relay 4
  if (temperature >= TEMP_ON) {
    relayState[3] = true;
  } else if (temperature <= TEMP_OFF) {
    relayState[3] = false;
  }

  digitalWrite(
    relayPins[3],
    relayState[3] ? RELAY_ON : RELAY_OFF
  );
}

// ---------- Web handlers ----------
void handleRoot() {
  server.send(200, "text/html", makePage());
}

void handleStatus() {
  String json = "{";

  json += "\"temperature\":";
  json += isnan(temperature) ? "null" : String(temperature, 1);

  json += ",\"humidity\":";
  json += isnan(humidity) ? "null" : String(humidity, 1);

  for (int i = 0; i < 4; i++) {
    json += ",\"relay";
    json += String(i + 1);
    json += "\":";
    json += relayState[i] ? "true" : "false";
  }

  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  
  for (int i = 0; i < 4; i++) {
    digitalWrite(relayPins[i], RELAY_OFF);
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], RELAY_OFF);
  }

  dht.begin();

  WiFi.mode(WIFI_AP);

  if (!WiFi.softAP(ssid, password)) {
    Serial.println("Failed to start Wi-Fi!");
    return;
  }

  Serial.println();
  Serial.println("ESP32 Wi-Fi started!");
  Serial.print("Wi-Fi name: ");
  Serial.println(ssid);
  Serial.print("Open: http://");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/status", handleStatus);

  
  for (int i = 1; i <= 3; i++) {
    server.on(("/relay/" + String(i) + "/on").c_str(), [i]() {
      relayState[i - 1] = true;
      digitalWrite(relayPins[i - 1], RELAY_ON);
      server.send(200, "text/plain", "ON");
    });

    server.on(("/relay/" + String(i) + "/off").c_str(), [i]() {
      relayState[i - 1] = false;
      digitalWrite(relayPins[i - 1], RELAY_OFF);
      server.send(200, "text/plain", "OFF");
    });
  }

  server.begin();
  Serial.println("Web server started!");
}

void loop() {
  readTemperature();
  server.handleClient();
}