#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <SoftwareSerial.h>

// --- SETĂRI WI-FI ---
const char* ssid = "your ssid";
const char* password = "password";

const char* numeZile[] = {"Dum", "Lun", "Mar", "Mie", "Joi", "Vin", "Sam"};
const char* numeLuni[] = {"Ian", "Feb", "Mar", "Apr", "Mai", "Iun", "Iul", "Aug", "Sep", "Oct", "Nov", "Dec"};
// --- SETĂRI NEXTION ---
SoftwareSerial nextion(4, 0); // RX=D2, TX=D3 (GPIO 4 și GPIO 0)

// --- SETĂRI OPEN-METEO (Iași: lat=47.16, lon=27.58) ---
//String weatherURL = "http://api.open-meteo.com/v1/forecast?latitude=47.16&longitude=27.58&current_weather=true";
// --- SETĂRI OPEN-METEO (Iași: lat=47.16, lon=27.58) ---
String weatherURL = "http://api.open-meteo.com/v1/forecast?latitude=47.16&longitude=27.58&current=temperature_2m,relative_humidity_2m,weather_code";

unsigned long lastWeatherUpdate = 0;
const unsigned long weatherInterval = 10 * 60 * 1000; // Actualizare meteo la fiecare 10 minute
unsigned long lastClockUpdate = 0;
int lastSecond = -1;
int lastDay = -1;

void setup() {
  Serial.begin(115200);
  nextion.begin(9600); 

  WiFi.begin(ssid, password);
  Serial.print("Se conectează la WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectat!");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
   setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1); // Fus orar România
  tzset();

  updateWeather();
}

void loop() {
  // Verificăm ceasul mai des de o dată pe secundă, fără să blocăm programul
  if (millis() - lastClockUpdate >= 200) { 
    updateClock();
    lastClockUpdate = millis();
  }

  // Actualizăm vremea la intervalul stabilit
  if (millis() - lastWeatherUpdate >= weatherInterval) {
    updateWeather();
    lastWeatherUpdate = millis();
  }
}

void updateClock() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  if (timeinfo->tm_year > 100) {
    // Trimitem Ora către Nextion DOAR când s-a schimbat secunda
    if (timeinfo->tm_sec != lastSecond) {
      lastSecond = timeinfo->tm_sec;
      
      char timeStr[10];
      sprintf(timeStr, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      sendToNextionText("t0.txt", String(timeStr));
    }

    // Trimitem Data către Nextion DOAR când s-a schimbat ziua
if (timeinfo->tm_mday != lastDay) {
  lastDay = timeinfo->tm_mday;
  
  char dateStr[30]; // Am mărit memoria de la 15 la 30 caractere
  
  // Formatăm data (ex: "Mie, 09 Sep 2026")
  sprintf(dateStr, "%s, %02d-%s-%04d", 
          numeZile[timeinfo->tm_wday], 
          timeinfo->tm_mday, 
          numeLuni[timeinfo->tm_mon], 
          timeinfo->tm_year + 1900);
          
  sendToNextionText("t1.txt", String(dateStr));
}
  }
}

void updateWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    
    Serial.println("Se descarcă datele meteo...");
    http.begin(client, weatherURL);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("Date primite!");
      
      DynamicJsonDocument doc(2048); 
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        // Citim noile campuri din obiectul "current"
        float temp = doc["current"]["temperature_2m"];
        int humidity = doc["current"]["relative_humidity_2m"];
        int weatherCode = doc["current"]["weather_code"];

        //String tempStr = String(temp, 1) + " C"; 
        String tempStr = String(temp, 1) + " \xb0" + "C"; // \xb0 este simbolul pentru grade

        String humStr = String(humidity) + " %"; // Formatăm umiditatea (ex: "45 %")
        String conditionStr = getWeatherDescription(weatherCode);
        int iconId = getWeatherIconId(weatherCode);

        // Trimitem textele și pictogramele către Nextion pe rând
        
        
        sendToNextionText("t4.txt", conditionStr);
        delay(50);
        
        sendToNextionText("t5.txt", humStr);
        delay(50);
        
        sendToNextionText("t2.txt", tempStr);
        //sendToNextionText("t2.txt", "Test");
        delay(50); // Pauză pentru procesare

        sendToNextionPicture("p0", 0);      
        delay(50);
        
        sendToNextionPicture("p1", iconId); 
        
        Serial.println("Trimis Temp: " + tempStr + ", Umid: " + humStr + ", Vreme: " + conditionStr);
      } else {
        Serial.print("Eroare JSON: ");
        Serial.println(error.c_str());
      }
    } else {
      Serial.print("Eroare HTTP: ");
      Serial.println(httpCode);
    }
    http.end();
  }
}

// --- FUNCȚII DE COMUNICARE CU NEXTION ---

void sendToNextionText(String objName, String value) {
  nextion.print(objName + "=\"" + value + "\"");
  sendEnd();
}

void sendToNextionPicture(String objName, int picId) {
  nextion.print(objName + ".pic=" + String(picId));
  sendEnd();
}

void sendEnd() {
  nextion.write(0xFF);
  nextion.write(0xFF);
  nextion.write(0xFF);
  delay(30); // Acorda timp display-ului sa proceseze comanda anterioara
}

// --- FUNCȚIE MAPARE COD METEO -> DESCRIPERE TEXT ---
String getWeatherDescription(int code) {
  switch (code) {
    case 0: return "Senin";
    case 1: case 2: case 3: return "Innorat";
    case 45: case 48: return "Ceata";
    case 51: case 53: case 55: return "Burnita";
    case 61: case 63: case 65: return "Ploaie";
    case 71: case 73: case 75: return "Ninsoare";
    case 95: return "Furtuna";
    default: return "Necunoscut";
  }
}

// --- FUNCȚIE NOUĂ: MAPARE COD METEO -> ID IMAGINE NEXTION ---
int getWeatherIconId(int code) {
  switch (code) {
    case 0: return 1;              // Senin -> Imaginea 1
    case 1: case 2: case 3: return 2; // Innorat -> Imaginea 2
    case 45: case 48: return 3;    // Ceata -> Imaginea 3
    case 51: case 53: case 55: return 4; // Burnita -> Imaginea 4
    case 61: case 63: case 65: return 5; // Ploaie -> Imaginea 5
    case 71: case 73: case 75: return 6; // Ninsoare -> Imaginea 6
    case 95: return 7;              // Furtuna -> Imaginea 7
    default: return 9;              // Implicit Senin (pentru orice altceva)
  }
}