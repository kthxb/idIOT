#include <TimedAction.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

/*
 * Debugging, OLED-Screen
 */

#include "SSD1306.h"

SSD1306 display(0x3C, D2, D1);

#define MSTR_IP "192.168.178.39"
#define MSTR_PORT 1337

#define DISPLAY_LINES 6
#define DISPLAY_CHARS 30

/*
 * Wifi-Settings
 */
const char* ssid = "R-A-D-A-R-L-O-V-E";
const char* pword = "0544101718199113";

char logMessages[DISPLAY_LINES][DISPLAY_CHARS];
int numberMessages = 0;

WiFiUDP Udp;

/*
 * Measurement-Settings
 */
boolean meas_running = false;
float meas_freq = 10.0; //Hz - Msg. pro Sekunde
float meas_update_freq = 0.5; //Hz - Datensendungen an den Server pro Sekunde
int meas_length = 1000;
long meas_starttime = -1;

//TimedAction recieveServerThread = TimedAction(1000 / meas_freq, measureToBuffer); ?

#define BUFFER_SIZE 1000
#define BUFFERCHECK false
float meas_buffer[BUFFER_SIZE];
long meas_time_buffer[BUFFER_SIZE];
int bufferIndex = 0;
int measures = 1;

void setup() {
  Serial.begin(115200);
  pinMode(D0, OUTPUT);
  display.init();
  
  Serial.printf("\nConnecting to: %s\n", ssid);
  logMessageF("Connecting to: %s", ssid);
  
  WiFi.begin(ssid, pword);
  boolean high = true;
  while(WiFi.status() != WL_CONNECTED){
    digitalWrite(D0, high ? HIGH : LOW);
    high = !high;
    delay(100);
    Serial.print(".");
  }
  Serial.println("\n");
  logMessage("Connected!");
  //                f   upd  anz
  startMeasurement(10000, 0.5, 10); //DEBUG
}

float measure(){
  return 1.0;
}

void measureToBuffer(){
  if(measures > meas_length){
    stopMeasurement();
    sendUpdateToServer();
    return;
  } else {
    if(sizeof(meas_buffer)/sizeof(float) <= bufferIndex){
      logMessageF("E:Buf.Sz:%d", bufferIndex);
    } else {
      int t =  measure();
      meas_buffer[bufferIndex] = t;
      meas_time_buffer[bufferIndex] = millis() - meas_starttime;
      //logMessageF("Logged: %ld -> %d", t, bufferIndex);
    }
  }
  
  measures++;
  bufferIndex++;
}

void sendUpdateToServer(){
  logMessage("sendUpdateToServer");
  Serial.print("[");
  for(int i = 0; i < bufferIndex; i++){
    Serial.print(meas_time_buffer[i]);
    Serial.print(", ");
    Serial.print(meas_buffer[i]);
    if(i < bufferIndex - 1) Serial.print("; ");
  }
  Serial.println("]");
  //TODO
  bufferIndex = 0;
}

/*
 * "Threading"
 */
TimedAction measuringThread = TimedAction(1000 / meas_freq, measureToBuffer);
TimedAction sendServerThread = TimedAction(1000 / meas_update_freq, sendUpdateToServer);

void startMeasurement(float mfreq, float mupd, int mlength){
  meas_running = true;
  meas_freq = mfreq;
  meas_update_freq = mupd;
  meas_length = mlength;
  measuringThread.setInterval(1000 / meas_freq);
  sendServerThread.setInterval(1000 / meas_update_freq);
  measures = 1;
  bufferIndex = 0;

  //meas_buffer = (float*) malloc((meas_freq / meas_update_freq) * sizeof(float));
  
  if(BUFFERCHECK && sizeof(meas_buffer) < ((meas_freq / meas_update_freq) * sizeof(float))){
    logMessage("Error: Not enough RAM for measurement.");
    stopMeasurement();
    return;
  }
  
  logMessage("Starting measurement:");
  logMessageF("meas_freq = %d", (int)meas_freq);
  logMessageF("meas_update_freq = %d/10", (int)(meas_update_freq*10));
  logMessageF("meas_length = %d", meas_length);
  logMessageF("meas_req_buffer = %d", (int)(meas_freq / meas_update_freq) * sizeof(float));
  logMessageF("meas_buffer_size = %d", (int)(sizeof(meas_buffer) / sizeof(float)));

  meas_starttime = millis();
  sendServerThread.reset();
  measuringThread.reset();
  sendServerThread.enable();
  measuringThread.enable();
}

void stopMeasurement(){
  meas_running = false;
  measuringThread.disable();
  sendServerThread.disable();
  
  logMessageF("Stopped msrmt: %d meas.", measures-1);

  //free(meas_buffer);
}

void loop() {
  sendServerThread.check();
  measuringThread.check();
}

/*
 * DISPLAY-REGION
 */

void logMessageF(const char* format, ...)
{
    char msg[30];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    logMessage(msg);
}

void logRuntime(){
  logMessageF("+%lds", millis()/1000L);
}

void logMessage(char* msg){
  if(numberMessages == DISPLAY_LINES){
    for(int i = 0; i < DISPLAY_LINES - 1; i++){
      for(int o = 0; o < DISPLAY_CHARS; o++){
        logMessages[i][o] = logMessages[i+1][o];
      }
    }
    numberMessages = DISPLAY_LINES - 1;
  }
  
  for(int i = 0; i < DISPLAY_CHARS; i++){
    logMessages[numberMessages][i] = msg[i];
  }
  numberMessages++;
  redrawDisplay();

  Serial.println(msg);
}

void redrawDisplay(){
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.clear();
  for(int i = 0; i < DISPLAY_LINES; i++){
    display.drawString(0, i * 10, logMessages[i]);
  }
  display.display();
}

