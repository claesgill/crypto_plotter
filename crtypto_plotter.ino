#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

// Webserver
ESP8266WebServer server(80);

// Credentials & Account ETH
const char* ssid = "<your WiFi name";
const char* password = "<your WiFi password>";
const String accountId = "<your account ID>";
const String host = "https://api.nanopool.org/v1/eth/balance/" + accountId;
const String apiGeneralInfo = "https://api.nanopool.org/v1/eth/user/" + accountId;

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

int counter = 0;

bool s = false;
String a = "";
float b  = 0.0;
float hr = 0.0;
float h  = 0.0;
String i = "";

void setup () {
  Serial.begin(115200);
  delay(100);
  Serial.print("Connecting to wifi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("Server IP: ");
  Serial.println(WiFi.localIP());

  // Webserver
  server.on("/", homePage);
  server.onNotFound(homePageNotFound);
  server.begin();
  Serial.println("HTTP server started");

  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  boot_screen();
  waitingForData();
}

void loop() {
  server.handleClient();
  
  if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
    HTTPClient http;  //Declare an object of class HTTPClient
    WiFiClientSecure client;
    int httpCode = 0;

    if (counter % 5000 == 0){
      Serial.println(counter);

      client.setInsecure();
      client.connect(apiGeneralInfo, 443);
      http.begin(client, apiGeneralInfo);
      httpCode = http.GET();
    }

    if (httpCode > 0 && counter % 5000 == 0) {
      String payload = http.getString();   //Get the request response payload

      // Parsing
      StaticJsonDocument<500> doc;
      deserializeJson(doc, payload);
      // Parameters
      const bool accStatus = doc["status"]; // Status
      const String account = doc["data"]["account"];
      const float balance  = doc["data"]["balance"];
      const float hashrate = doc["data"]["hashrate"];

      JsonObject avgHashrate = doc["data"]["avgHashrate"];
      const float h1  = avgHashrate["h1"];
      const float h3  = avgHashrate["h3"];
      const float h6  = avgHashrate["h6"];
      const float h12 = avgHashrate["h12"];
      const float h24 = avgHashrate["h24"];

      JsonObject workers = doc["data"]["workers"][0];
      const String id       = workers["id"];
      const float lastShare = workers["lastshare"];
      const int rating      = workers["rating"];

      // Setting HTML variables
      s  = accStatus;
      a  = account;
      b  = balance;
      hr = hashrate;
      h  = h1;
      i  = id;
      
      // Output to serial monitor
      Serial.print("Status: ");     Serial.println(String(accStatus));
      Serial.print("Account: ");    Serial.println(account);
      Serial.print("Balance: ");    Serial.println(balance, 8);
      Serial.print("Hashrate: ");   Serial.println(hashrate);
      Serial.print("Id: ");         Serial.println(id);
      Serial.print("Last share: "); Serial.println(lastShare);
      Serial.print("Rating: ");     Serial.println(rating);
      Serial.print("H1: ");         Serial.println(h1);
      Serial.print("H3: ");         Serial.println(h3);
      Serial.print("H6: ");         Serial.println(h6);
      Serial.print("H12: ");        Serial.println(h12);
      Serial.print("H24: ");        Serial.println(h24);

      // Display to OLED
      display.clearDisplay();
      display.setCursor(0, 0);
      display.setTextSize(1);              // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE); // Draw white text
      display.cp437(true);                 // Use full 256 char 'Code Page 437' font
      
      display.print("  IP: ");
      display.println(WiFi.localIP());
      
      display.print("Account:  ");
      display.print(account.charAt(0));
      display.print(account.charAt(1));
      display.print(account.charAt(2));
      display.print(account.charAt(3));
      display.print("...");
      display.print(account.charAt(account.length()-3));
      display.print(account.charAt(account.length()-2));
      display.println(account.charAt(account.length()-1));
      
      display.write("Balance:  ");
      display.print(balance, 5);
      display.println(" ETH");
      
      display.write("HR     :  ");
      display.print(hashrate);
      display.println(" Mh/s");
    
      display.println("  Worker");
      display.print("Status:   ");
      (accStatus ? display.println("Online") : display.println("Offline"));
      display.print("Id    :   ");
      display.println(id);
      display.print("Avg HR:   ");
      display.print(h1);
      display.println(" Mh/s");
      
      display.display();
    } else {
      //Serial.print("HTTP Code: ");
      //Serial.println(httpCode);
      // TODO: Error message to OLED and webserver
    }
    http.end();   //Close connection
  }
  // delay(150000);    //Send a request every 30 seconds
  counter++;
  delay(1000);
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
  server.send(200, "text/html", page(a, b, hr, s, i, h));
}

String page(String account, float balance, float hashrate, bool accStatus, String id, float h1){
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
  content += "<h3>Worker</h3>";
  content += "<h3>Status: ";
  content += String(accStatus);
  content += "</h3>";
  content += "<h3>Id    : ";
  content += id;
  content += "</h3>";
  content += "<h3>Avg HR: ";
  content += h1;
  content += "</h3>";
  
  content += "</body>\n";
  content += "</html>\n";
 
  return content;
}

void boot_screen(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.cp437(true);
  display.println("cjg Engine");
  display.display();
  delay(1000);

  display.print("\nBooting");
  int numDots = 3; 
  for(int i=0; i<=numDots; i++){
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("cjg Engine");
    display.print("\nBooting");
    char str[numDots];
    memset(str, '.', i);
    str[i] = '\0';
    display.print(str);
    display.display();
    delay(1000);
  }
}

void waitingForData(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.cp437(true);
  display.println("Fetching data...");
  display.display();
}
