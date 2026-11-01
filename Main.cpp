#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SI4735.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <AiEsp32RotaryEncoder.h>
#define WIFI_SSID "Galaxy A52 6DE5"
#define WIFI_PASS "Mehdi2440423173"
#define PIN_SDA 18
#define PIN_SCL 17
#define PIN_SI_RST 15
#define PIN_ADC_AUDIO 4
#define PIN_TFT_CS 10
#define PIN_TFT_DC 8
#define PIN_TFT_RST 9
#define PIN_TFT_BL 14
#define PIN_ENC_CLK 38
#define PIN_ENC_DT 39
#define PIN_ENC_SW 40
Adafruit_ST7789 tft = Adafruit_ST7789(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
SI4735 radio;
AiEsp32RotaryEncoder encoder = AiEsp32RotaryEncoder(PIN_ENC_CLK, PIN_ENC_DT, PIN_ENC_SW, -1, 4);
struct City { const char* name; float lat; float lon; };
const City IRAN[] = {
 {"Tehran",35.68,51.38},{"Mashhad",36.26,59.61},{"Isfahan",32.65,51.66},
 {"Shiraz",29.59,52.58},{"Tabriz",38.09,46.27},{"Ahvaz",31.31,48.67},
 {"Rasht",37.28,49.58},{"Kerman",30.28,57.08},{"Yazd",31.89,54.35},
 {"BandarAbbas",27.18,56.26},{"Zahedan",29.45,60.88},{"Urmia",37.55,45.07}
};
const int IRAN_N = 12;
float myLat = 35.68; float myLon = 51.38;
int cityIdx = 0;
uint8_t page = 0;
uint8_t bandMode = 0;
uint8_t vol = 28;
uint16_t presets[20]; uint8_t presetCount = 0;
int encoderLast = 0;
void IRAM_ATTR isrEnc(){ encoder.readEncoder_ISR(); }
void setBand(uint8_t m){
 bandMode = m;
 if(m==0){ radio.setFM(6400,10800,8750,10); radio.setRdsMode(1,2,2,2,2); }
 if(m==1){ radio.setAM(520,1710,1000,9); }
 if(m==2){ radio.setAM(2300,30000,10000,5); radio.setBandwidth(2,1); }
 if(m==3){ radio.setAM(150,520,300,1); }
 if(m==4){ radio.setSSB(3000,30000,10000,5,1); radio.setSSBConfig(1,1,1,1,1,1); }
 radio.setVolume(vol);
}
void drawRadio(){
 tft.fillScreen(ST77XX_BLACK);
 tft.fillRect(0,0,320,28,ST77XX_BLUE);
 tft.setTextSize(2); tft.setTextColor(ST77XX_YELLOW);
 tft.setCursor(8,6);
 const char* bn = bandMode==0?"FM":bandMode==1?"MW":bandMode==2?"SW":bandMode==3?"LW":"SSB";
 tft.print(bn);
 tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
 tft.setCursor(70,12); tft.print(IRAN[cityIdx%IRAN_N].name); tft.print(" V1");
 tft.setTextSize(3); tft.setTextColor(ST77XX_GREEN);
 tft.setCursor(10,40); tft.print(radio.getFrequency());
 tft.setTextSize(2); tft.setTextColor(ST77XX_CYAN);
 tft.setCursor(10,75);
 tft.print("R:"); tft.print(radio.getCurrentRSSI());
 tft.print(" S:"); tft.print(radio.getCurrentSNR());
 tft.setTextColor(ST77XX_WHITE);
 tft.setCursor(10,100); tft.print("VOL:"); tft.print(vol);
 if(bandMode==0){
  radio.getRdsStatus();
  if(radio.getRdsReceived()){ tft.setCursor(10,130); tft.setTextSize(1); tft.print(radio.getRdsText0A()); }
 }
 tft.setTextSize(1); tft.setTextColor(0x8410);
 tft.setCursor(5,158); tft.print("CLK:page DBL:vol LNG:seek");
}
void doScan(){
 tft.fillScreen(ST77XX_BLACK);
 tft.setTextSize(2); tft.setTextColor(ST77XX_YELLOW);
 tft.setCursor(5,5); tft.print("V1 AUTO SCAN...");
 presetCount=0;
 for(int i=0;i<20;i++){
  radio.seekStationUp();
  delay(350);
  uint16_t f=radio.getFrequency();
  bool dup=false;
  for(int k=0;k<presetCount;k++) if(presets[k]==f) dup=true;
  if(!dup && presetCount<20){
   presets[presetCount++]=f;
   tft.setTextSize(1); tft.setTextColor(ST77XX_GREEN);
   tft.setCursor(10,30+presetCount*11); tft.print(f);
  }
  if(presetCount>=20) break;
 }
 delay(1200);
}
void manualEntry(){
 int d[5]={0,8,7,5,0}; int pos=0; int last=encoder.readEncoder();
 tft.fillScreen(ST77XX_BLACK);
 tft.setTextSize(1); tft.setTextColor(ST77XX_YELLOW);
 tft.setCursor(5,10); tft.print("MANUAL rotate+click");
 while(true){
  if(encoder.encoderChanged()){
   int diff=encoder.readEncoder()-last; last=encoder.readEncoder();
   d[pos]+=(diff>0?1:-1);
   if(d[pos]>9) d[pos]=0; if(d[pos]<0) d[pos]=9;
  }
  tft.setTextSize(3); tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(20,70);
  tft.fillRect(0,60,320,50,ST77XX_BLACK);
  tft.print(String(d[0])+String(d[1])+String(d[2])+"."+String(d[3])+String(d[4]));
  if(encoder.isEncoderButtonClicked()){
   pos++; if(pos>=5){
    float f=(d[0]*100+d[1]*10+d[2])+(d[3]*10+d[4])/100.0;
    radio.setFrequency((uint16_t)(f*100)); break;
   }
  }
  if(digitalRead(PIN_ENC_SW)==LOW){ delay(700); break; }
  delay(10);
 }
}
String getWx(){
 if(WiFi.status()!=WL_CONNECTED) return "WiFi OFF";
 HTTPClient http; char url[170];
 snprintf(url,sizeof(url),"https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code,wind_speed_10m&timezone=auto",myLat,myLon);
 http.begin(url);
 if(http.GET()!=200){ http.end(); return "WX ERR"; }
 String pl=http.getString(); http.end();
 JsonDocument doc; if(deserializeJson(doc,pl)) return "JSON ERR";
 float t=doc["current"]["temperature_2m"]; int w=doc["current"]["weather_code"]; float wind=doc["current"]["wind_speed_10m"];
 char b[80]; snprintf(b,sizeof(b),"%.1fC code:%d wind:%.0f",t,w,wind);
 return String(b);
}
void drawMap(){
 tft.fillScreen(ST77XX_BLACK);
 tft.setTextSize(2); tft.setTextColor(ST77XX_YELLOW);
 tft.setCursor(120,5); tft.print("IRAN V1");
 tft.drawRect(60,30,200,100,0x8410);
 for(int i=0;i<IRAN_N;i++){
  int x=60+(int)((IRAN[i].lon-44.0)*200.0/19.0);
  int y=30+(int)((39.7-IRAN[i].lat)*100.0/14.5);
  tft.fillCircle(x,y,(i==cityIdx%IRAN_N)?5:2,(i==cityIdx%IRAN_N)?ST77XX_RED:ST77XX_GREEN);
 }
 tft.setTextSize(2); tft.setTextColor(ST77XX_WHITE);
 tft.setCursor(90,140); tft.print(IRAN[cityIdx%IRAN_N].name);
}
void setup(){
 Serial.begin(115200);
 pinMode(PIN_TFT_BL,OUTPUT); digitalWrite(PIN_TFT_BL,HIGH);
 Wire.begin(PIN_SDA,PIN_SCL);
 tft.init(170,320); tft.setRotation(1); tft.fillScreen(ST77XX_BLACK);
 tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
 tft.setCursor(10,70); tft.print("ATS MINI V1 BOOT...");
 encoder.begin(); encoder.setup(isrEnc);
 encoder.setBoundaries(-100000,100000,false); encoder.setAcceleration(40);
 analogReadResolution(12);
 radio.setup(PIN_SI_RST,0,1,SI473X_ANALOG_AUDIO,XOSCEN_RCLK);
 delay(300); radio.setVolume(vol); setBand(0);
 WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASS);
 delay(500);
 drawRadio();
}
void loop(){
 if(encoder.encoderChanged()){
  int d=encoder.readEncoder()-encoderLast; encoderLast=encoder.readEncoder();
  if(page==0){ if(d>0) radio.frequencyUp(); else radio.frequencyDown(); drawRadio(); }
 }
 if(encoder.isEncoderButtonClicked()){
  page=(page+1)%7;
  if(page==0) drawRadio();
  if(page==1){ doScan(); page=0; drawRadio(); }
  if(page==2){ manualEntry(); page=0; drawRadio(); }
  if(page==3){ drawMap(); }
  if(page==4){ tft.fillScreen(ST77XX_BLACK); tft.setCursor(5,70); tft.setTextSize(1); tft.print(getWx()); }
  if(page==5){ tft.fillScreen(ST77XX_BLACK); tft.setCursor(5,50); tft.print("SSTV Ready Need Wire GPIO4"); }
  if(page==6){ tft.fillScreen(ST77XX_BLACK); tft.setCursor(5,50); tft.print("CW Ready Need Wire GPIO4"); }
 }
 if(encoder.isEncoderButtonDoubleClicked()){
  unsigned long t0=millis(); int last=encoder.readEncoder();
  while(millis()-t0<5000){
   if(encoder.encoderChanged()){ int dd=encoder.readEncoder()-last; last=encoder.readEncoder(); if(dd>0&&vol<63) vol++; if(dd<0&&vol>0) vol--; radio.setVolume(vol); }
   delay(10);
  }
  drawRadio();
 }
 static unsigned long ps=0; static bool ld=false;
 if(digitalRead(PIN_ENC_SW)==LOW){ if(ps==0) ps=millis(); if(!ld&&millis()-ps>900){ ld=true; radio.seekStationUp(); drawRadio(); } }
 else { ps=0; ld=false; }
 delay(10);
}