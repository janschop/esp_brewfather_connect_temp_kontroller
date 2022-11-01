
// https://randomnerdtutorials.com/esp32-ssd1306-oled-display-arduino-ide/

// ideer: har en bryter som skrur av og på om den skal koble seg opp på nette, så man kan bruke den som rent termometer
#include <WiFi.h>
#include <ArduinoHttpClient.h>
#include <SPI.h>
#include <Wire.h> //kan kanskje fjernes
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Esp.h>

#define MAX6675_SCK  18
#define MAX6675_CS   19
#define MAX6675_SO   5

#define BLUE 0x001F

#define fridge_on 32
#define fridge_off 33
#define btn_s_down 26
#define btn_s_up 25


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const char* ssid = "Get-2G-DCCF41";
const char* password = "TDMT8HF3HN";

// Loga
/*
const char* ssid = "Telenor5581par"; fff
const char* password = "Agensleddet7Italienskene8";
*/

WiFiClient wifi;

//pins
/*const int fridge_on = 32; // må endres ut ifra onenote
const int fridge_off = A3;
const int interrupt_id_increase = 0;
const int interrrupt_id_decrease = 1;*/

//variabler
int n = 100; //antall ganger temp skal summeres før avg. Prøver 100, siden temp ikke alltid blir registrert i brewfather 
int read_time = 1000; //tid i ms mellom hver temp måling for 15 min mellom post
String t_state = "";
int state;
float temperature_read;
float temperature_read_avg;
float sum;
int t = 0;

//TEMPERATUR
volatile int set_temperature = 20; //må ha dette som volatile for å kunne bruke variablen i en interrupt ISR funksjon
double dT = 1; //tillat avvik fra set_temp
int temp_ctrl_activity;

void set_temp_increase() {
  if (t+200<millis()){
   set_temperature ++;
  Serial.println("s ++"); 
  t=millis();
  }
}

void set_temp_decrease() {
   if (t+200<millis()){
   set_temperature --;
  Serial.println("s --"); 
  t=millis();
  }
}


void setup(){
  Serial.begin(115200);
  delay(10);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); 
  }
  
  readThermocouple(); //prevents startup error
  connect_to_wifi();

  pinMode(btn_s_down, INPUT_PULLDOWN);
  pinMode(btn_s_up, INPUT_PULLDOWN);
  attachInterrupt(btn_s_down, set_temp_decrease, FALLING);
  attachInterrupt(btn_s_up, set_temp_increase, FALLING);
}

void loop(){

  float s = 0;
  for (int i = 0; i < 10; i++) { //checks and controlls 10 times for each post cycle
    
    s += get_avg_temp_n_controll();
  }  
  Post(s/10); // brewfather can't be posted to more than every 15 minutes
}

float get_avg_temp_n_controll() {
  //får tak i avg temp, og oppdaterer skjermen
  sum = 0;
  
  for (int i = 0; i < n; i ++) {
    temperature_read = readThermocouple();
    sum += temperature_read;
    
    Serial.print("T: ");
    Serial.println(temperature_read,1);
      
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(10, 5);  
    display.clearDisplay();
    display.print("T: ");
    display.print(temperature_read,1);
    display.print("C");
    display.setCursor(10,25);
    display.print("S: ");
    display.print(set_temperature,1);
    display.print("  C");
    display.setCursor(7,45);
    display.print(t_state);
    /*if(state == 1) {
      //display.setTextColor(BLUE);
      display.setCursor(7,45);
      display.print("FRIDGE ON");
    }
    if(state == 0){
      display.setCursor(7,45);
      display.print("FRIDGE OFF");
    }
    else{}
      */
    display.display();
    
    delay(read_time);
  }
  temperature_read_avg = sum / n;

  Serial.print("avg: ");
  Serial.println(temperature_read_avg);

  temp_compare(temperature_read_avg);
  
  return temperature_read_avg;
}

void temp_compare(double temp) {
  //Easy peasy IF-statement for temperature adjust
  if (temp <= set_temperature) {
    activate_sw(fridge_off);
    delay(500);
    Serial.println("FRIDGE OFF");
    t_state = "FRIDGE OFF";
    state = 0;
  } 

  if (temp > (set_temperature /*+ dT*/)) { //for bruk med frys må systemet være mer responsiv, derfor sløfes "dT" i det tilfellet
    activate_sw(fridge_on);
    delay(500);
    Serial.println("FRIDGE ON");
    t_state = "FFRIDGE ON";
    state = 1;
  }
}


void Post(double temp) {
  
  int setT_int = set_temperature;
  Serial.println(String(setT_int));
  
  String json = "{\n \"name\": \"PNS-tempcontroll\",\n \"temp\": ";
    
  json += String (temp, 1);
  json += ",\n \"temp_unit\": \"C\", \n \"comment\": \"";
  json += t_state;
  json += ", SET TEMP: ";
  json += String(setT_int); //får ikke omgjort volatile int til string
  json += "\" \n}";
   
    HttpClient client(wifi, "log.brewfather.net", 80);

    client.post("/stream?id=6j8S71QopX5LFK", "application/json", json);
    //int statusCode = client.responseStatusCode();
    //String resp = client.responseBody(); // if not called the next `.post` will return `-4`
    client.stop();   
}

void activate_sw(int pin) {
  pinMode(pin,OUTPUT);
  digitalWrite(pin,HIGH);
  delay(300); // so the receiver has enough time to get the signal (?)
  digitalWrite(pin,LOW);

  Serial.print("activating: ");
  Serial.println(pin);
}

void connect_to_wifi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 10);  
  display.clearDisplay();
  display.println("Connecting to WiFi");
  display.display();
  display.setCursor(50,30);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (i == 5){ // if the unit is not connected to wifi within 5 seconds, it restards
        ESP.restart();
    }
    else {
      Serial.print(".");
      display.print(".");
      display.display();
      delay(1000);
      i ++;
    }
  }
  display.clearDisplay();
  display.setCursor(10, 10); 
  display.print("WiFi connected!");
  display.display();
  delay(1000);
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

double readThermocouple() {
    uint16_t v;
    pinMode(MAX6675_CS, OUTPUT);
    pinMode(MAX6675_SO, INPUT);
    pinMode(18, OUTPUT); // MAX6675_SCK

    digitalWrite(MAX6675_CS, LOW);
    delay(10);

    // Read in 16 bits,
    //  15    = 0 always
    //  14..2 = 0.25 degree counts MSB First
    //  2     = 1 if thermocouple is open circuit  
    //  1..0  = uninteresting status
    
    v = shiftIn(MAX6675_SO, 18, MSBFIRST);
    v <<= 8;
    v |= shiftIn(MAX6675_SO, 18, MSBFIRST);

    digitalWrite(MAX6675_CS, HIGH);
    if (v & 0x4) 
    {    
    // Bit 2 indicates if the thermocouple is disconnected
    return NAN;     
    }

    // The lower three bits (0,1,2) are discarded status bits
    v >>= 3;

    // The remaining bits are the number of 0.25 degree (C) counts
    return v*0.25-3; // measured boiling water at 102.75 degrees (C), adjusted the outputvalue
}
