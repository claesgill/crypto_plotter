#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

// Webserver
ESP8266WebServer server(80);

// Credentials & Account ETH
const char* ssid            = "<your WiFi name";
const char* password        = "<your WiFi password>";
const String accountId      = "<your account ID>";
const String apiGeneralInfo = "https://api.ethermine.org/miner/" + accountId + "/currentstats";

// OLED
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     3
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String accStatus     = "";
String account       = accountId;
float  balance       = 0.0;
float  hashrate      = 0.0;
float  avgHashrate   = 0.0;
float  h1            = 0.0;
String id            = "None";
int    activeWorkers = 0;
float  constPerMin   = 0.0;

int oled_mode    = 0;
bool makeRequest = true;

unsigned long previousMillis = 0;
const long interval = 3600000; // ~1 hour

void ICACHE_RAM_ATTR makeAPIRequest();
void ICACHE_RAM_ATTR changeOledMode();

void setup () {
  Serial.begin(115200);
  delay(100);

  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.cp437(false);
  display.print("WiFi: ");
  display.display();
  
  Serial.print("WiFi ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    display.print(".");
    display.display();
  }
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("Server IP: ");
  Serial.println(WiFi.localIP());

  display.println("OK!");
  display.display();

  // Webserver
  server.on("/", homePage);
  server.onNotFound(homePageNotFound);
  server.begin();
  Serial.println("HTTP server started");
  delay(1000);
  
  // Setting interrupt pin
  attachInterrupt(digitalPinToInterrupt(D7), makeAPIRequest, RISING);
  attachInterrupt(digitalPinToInterrupt(D8), changeOledMode, RISING);  
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();
  if( makeRequest || (currentMillis - previousMillis >= interval) ){
    (makeRequest ? Serial.println("Manually triggered request.") : Serial.println("1 hour has passed."));
    makeRequest = false;
    previousMillis = currentMillis;
    fetchDataFromAPI();
  }

  // Output to serial monitor
  switch(oled_mode){
    case 1:
      oledDisplayBalance();
      break;
    case 2:
      oledDisplayHashrate();
      break;
    case 3:
      oledDisplayWorkers();
      break;
    case 4:
      oledDisplayIp();
      break;
    case 98:
      break;
    case 99:
      break;
    default:
      oledDisplayStats();
      break;
  }
}

void fetchDataFromAPI(){
  oledFetchingData();
  
  if (WiFi.status() == WL_CONNECTED) {  //Check WiFi connection status
    HTTPClient http;                    //Declare an object of class HTTPClient
    WiFiClientSecure client;            //Declare an object of class WiFiClientSecure
    client.setInsecure();               // Insecure because of HTTPS connection
    client.connect(apiGeneralInfo, 443);
    http.begin(client, apiGeneralInfo);
    
    if (http.GET() > 0) { 
      String payload = http.getString(); //Get the request response payload

      // Parsing
      StaticJsonDocument<500> doc;
      deserializeJson(doc, payload);

      const String _accStatus  = doc["status"];
      const float _balance     = doc["data"]["unpaid"];
      const float _hashrate    = doc["data"]["reportedHashrate"];
      const float _avgHashrate = doc["data"]["averageHashrate"];
      const int _activeWorkers = doc["data"]["activeWorkers"];
      const float _consPerMin  = doc["data"]["coinsPerMin"];
      
      // Setting to global variables
      accStatus     = _accStatus;
      balance       = _balance / 1000000000000000000;
      hashrate      = _hashrate / 1000000;
      avgHashrate   = _avgHashrate / 1000000;
      activeWorkers = _activeWorkers;
      constPerMin   = _consPerMin;      
    } else {
      Serial.print("HTTP Code: ");
      Serial.println(http.GET());
      oledErrorMessage("Fetching data", http.GET());
      oled_mode = 98;
    }
    http.end(); //Close connection
  } else{
    Serial.println("WiFi is not connected!");
    oledErrorMessage("No WiFi...", WiFi.status());
    oled_mode = 99;
  }
}

void homePageNotFound(){
  String message = "<h3>Page not found. Try <a href='http://";
  message += WiFi.localIP().toString();
  message += "/'>http://";
  message += WiFi.localIP().toString();
  message += "/</a></h3>";
  server.send(404, "text/html", message);
}

void homePage(){
  server.send(200, "text/html", page(account, balance, hashrate, accStatus, activeWorkers, avgHashrate));
}

String page(String account, float balance, float hashrate, String accStatus, int activeWorkers, float avgHashrate){
  String content = "<!DOCTYPE html> <html>\n";
  content += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  content += "<title>LED Control</title>\n";
  content += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  content += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  content += "</style>\n";
  content += "</head>\n";
  content += "<body>\n";

  content += "<h1>IP: ";
  content += WiFi.localIP().toString();
  content += "</h1>";
  content += "<h3>Account:  ";
  content += account;
  content += "</h3>";
  content += "<h3>Balance:  ";
  content += String(balance);
  content += " ETH</h3>";
  content += "<h3>Hashrate: ";
  content += String(hashrate);
  content += " Mh/s</h3>";
  content += "<hr>";
  content += "<h3>Workers</h3>";
  content += "<h3>Status: ";
  content += accStatus;
  content += "</h3>";
  content += "<h3>Active workers: ";
  content += String(activeWorkers);
  content += "</h3>";
  content += "<h3>Avg HR: ";
  content += avgHashrate;
  content += "</h3>";
  
  content += "</body>\n";
  content += "</html>\n";
 
  return content;
}

void oledDisplayStats(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.cp437(true);
  display.print("$ ");
  display.println(balance, 5);
  display.println("      ETH");
  display.print("# ");
  display.print(hashrate, 2);
  display.println("         MH/s");
  display.display();
}

void oledDisplayBalance(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.cp437(true);

  display.println("Balance");
  display.setTextSize(1);
  display.println();
  display.setTextSize(3);
  display.println(balance, 5);
  display.setTextSize(2);
  display.println("ETH");
  display.display();
}

void oledDisplayHashrate(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.cp437(true);

  display.println("Hashrate");
  display.setTextSize(1);
  display.println();
  display.setTextSize(3);
  display.println(hashrate, 2);
  display.setTextSize(2);
  display.println("MH/s");
  display.display();
}

void oledDisplayWorkers(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.cp437(true);
  display.println("Workers");
  ( (activeWorkers > 0) ? display.println("Online") : display.println("Off"));
  display.println(activeWorkers);
  display.print(avgHashrate, 2);
  display.println(" Mhs");
  
  display.display();
}

void oledDisplayIp(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.cp437(true);

  display.println("IP");
  display.setTextSize(1);
  display.println();
  display.setTextSize(2);
  display.println(WiFi.localIP());
  display.display();
}

void oledErrorMessage(String str, int errorCode){
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);              // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.cp437(true);                 // Use full 256 char 'Code Page 437' font
  
  display.print("ERROR: ");
  display.println(errorCode);
  display.println(str);
  display.display();
}

void oledFetchingData(){
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(115, 0);
  display.cp437(true);
  display.write("^");
  display.display();
}

void ICACHE_RAM_ATTR changeOledMode(){
  Serial.println("Interrupting - right button!");
  if (oled_mode > 4){
    oled_mode = 0;
  } else {
    oled_mode++;
  }
}

void ICACHE_RAM_ATTR makeAPIRequest(){
  Serial.println("Interrupting - left button!");
  ( (oled_mode > 4) ? oled_mode = 0 : oled_mode);
  makeRequest = true;
}
