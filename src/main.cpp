#include <Arduino.h>
#include <M5Unified.h>
#include <M5GFX.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>
#include <math.h>
M5Canvas canvas(&M5.Display);
Adafruit_BMP280 bmp(&Wire);
Adafruit_AHTX0 aht;
float baselineP0=1013.25f,P=0,T=0,RH=0,h=0,vz=0,rho=0,previous=0;
bool sensors=false,valid=false,havePrevious=false,dirty=true;
uint32_t sampleAt=0,lastSample=0,screenAt=0;
unsigned updates=0;
String lastView;
void sample(){
 sensors_event_t humidity,temperature;
 bool ok=aht.getEvent(&humidity,&temperature);
 float p=bmp.readPressure()/100.0f;
 uint32_t now=millis();
 valid=ok && isfinite(p) && p>=300 && p<=1100 && isfinite(temperature.temperature) && isfinite(humidity.relative_humidity);
 if(!valid){havePrevious=false;return;}
 P=p;T=temperature.temperature;RH=humidity.relative_humidity;
 h=44330.0f*(1.0f-powf(P/baselineP0,0.1903f));
 vz=havePrevious ? (h-previous)/((now-lastSample)/1000.0f):0;
 rho=P*100.0f/(287.05f*(T+273.15f));
 previous=h;lastSample=now;havePrevious=true;
 Serial.printf("P=%.2f T=%.2f RH=%.1f h=%.2f Vz=%.2f rho=%.3f P0=%.2f\n",P,T,RH,h,vz,rho,baselineP0);
}
void text(const String &s,int x,int y,float size=1){
 canvas.setTextSize(size);canvas.setCursor(x,y);canvas.print(s);
}
void render(){
 char altitude[24],pressure[24],density[24],strip[80],battery[16];
 int percent=M5.Power.getBatteryLevel();
 snprintf(battery,sizeof battery,"~%d%%",percent);
 if(valid){
 snprintf(altitude,sizeof altitude,"%+.1f m",h);
 snprintf(pressure,sizeof pressure,"%.1f hPa",P);
 snprintf(density,sizeof density,"%.2f kg/m3",rho);
 snprintf(strip,sizeof strip,"TEMP:%.1fC | RH:%.0f%% | Vz:%+.1fm/s",T,RH,vz);
 }else{
 strcpy(altitude,"NO DATA");strcpy(pressure,"-- hPa");strcpy(density,"-- kg/m3");strcpy(strip,"CHECK SENSOR / I2C WIRING");
 }
 String view=String(altitude)+pressure+density+strip+battery;
 if(view==lastView && !dirty)return;
 canvas.fillSprite(TFT_WHITE);canvas.setTextColor(TFT_BLACK,TFT_WHITE);
 canvas.fillRect(0,0,200,24,TFT_BLACK);
 canvas.setTextColor(TFT_WHITE,TFT_BLACK);
 text("BARO-OPTICS LAB",4,8);text(battery,164,8);
 canvas.setTextColor(TFT_BLACK,TFT_WHITE);
 canvas.drawRect(4,28,192,74,TFT_BLACK);text("ALTITUDE (REL)",12,34);
 canvas.setTextSize(3); int w=canvas.textWidth(altitude);
 if(w>184)canvas.setTextSize(2);
 text(altitude,(200-canvas.textWidth(altitude))/2,56,w>184?2:3);
 canvas.drawRect(4,106,94,52,TFT_BLACK);canvas.drawRect(102,106,94,52,TFT_BLACK);
 text("PRESSURE",10,112);text(pressure,10,132,1.25);
 text("AIR DENSITY",108,112);text(density,108,132,1.25);
 canvas.drawRect(4,162,192,34,TFT_BLACK);
 // Split strip into two lines: the requested single line cannot fit at readable size.
 if(valid){char line[48];snprintf(line,sizeof line,"TEMP:%.1fC | RH:%.0f%%",T,RH);text(line,10,168);snprintf(line,sizeof line,"Vz:%+.1fm/s",vz);text(line,10,182);}else text(strip,10,172);
 if(updates && updates%60==0){
 M5.Display.setEpdMode(epd_mode_t::epd_quality);
 M5.Display.fillScreen(TFT_WHITE);M5.Display.display();M5.Display.waitDisplay();
 }
 M5.Display.setEpdMode(epd_mode_t::epd_fastest);
 canvas.pushSprite(0,0);M5.Display.waitDisplay();
 ++updates;lastView=view;dirty=false;screenAt=millis();
}
void setup(){
 Serial.begin(115200);
 auto cfg=M5.config();cfg.internal_spk=false;cfg.clear_display=true;
 M5.begin(cfg);
 canvas.setColorDepth(1);canvas.createSprite(200,200);canvas.setTextWrap(false);
 Wire.begin(25,26,100000);
 bool bp=bmp.begin(0x76);bool ah=aht.begin(&Wire);sensors=bp&&ah;
 Serial.printf("BMP280@0x76:%d AHT20@0x38:%d\n",bp,ah);
 if(sensors)sample();render();sampleAt=millis();
}
void loop(){
 M5.update();uint32_t now=millis();
 if(now-sampleAt>=1000){sampleAt+=1000;if(now-sampleAt>=1000)sampleAt=now;if(sensors)sample();}
 bool changed=false;
 // M5Unified uses BtnEXT for top button; BtnPWR is the side power button.
 if(valid && (M5.BtnEXT.wasPressed() || M5.BtnPWR.wasPressed())){baselineP0=P;changed=true;}
 if(M5.BtnA.wasPressed()){baselineP0+=0.5f;changed=true;}
 if(M5.BtnC.wasPressed()){baselineP0-=0.5f;changed=true;}
 if(changed){baselineP0=constrain(baselineP0,300.0f,1200.0f);if(valid){h=44330*(1-powf(P/baselineP0,0.1903f));previous=h;vz=0;}dirty=true;}
 // Queue button redraws to preserve the manufacturer's minimum interval.
 if(millis()-screenAt>=15000)render();
 delay(5);
}
