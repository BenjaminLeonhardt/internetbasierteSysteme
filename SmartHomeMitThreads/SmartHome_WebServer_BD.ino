#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <DHT.h>
#include "pitches.h"
#include <WiFi.h>
#include "time.h"
#include <TimeLib.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <WebServer.h>   


const char *ssid     = "BensGalaxy";
const char *password = "bla12345";


#define EPD_SS   5
#define EPD_DC   17
#define EPD_RST  16
#define EPD_BUSY 4
#define MAX_DISPLAY_BUFFER_SIZE 800
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))
GxEPD2_3C<GxEPD2_290_C90c, MAX_HEIGHT(GxEPD2_290_C90c)>
    display(GxEPD2_290_C90c(EPD_SS, EPD_DC, EPD_RST, EPD_BUSY));
volatile int page = 0;


#define DHTPIN  32
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);


#define ENABLE 27
#define DIRA   25
#define DIRB   26


#define LDRPIN 34
Servo myservo;


volatile float temperature = 0;
volatile float humidity    = 0;
volatile int   ldrValue    = 0;
volatile bool  fanOn       = false;


int melody[] = { NOTE_C5, NOTE_D5, NOTE_E5, NOTE_F5,
                 NOTE_G5, NOTE_A5, NOTE_B5, NOTE_C6 };
int noteDuration = 500;
#define SPEAKERPIN 12


const char *ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = 3600;
const int   daylightOffset_sec = 3600;


volatile int  weckStunde  = 16;
volatile int  weckMinute  = 24;
volatile bool alarmOff    = false;
volatile bool alarmActive = true;
#define ALARM_OFF_BUTTON 33


volatile float maxTemp     = 0.0;
volatile float minTemp     = 0.0;
volatile float rainSum     = 0.0;
volatile float snowfallSum = 0.0;
String urlWeather = "https://api.open-meteo.com/v1/forecast"
    "?latitude=48.8378&longitude=10.0933"
    "&daily=temperature_2m_max,temperature_2m_min,rain_sum,snowfall_sum"
    "&timezone=Europe%2FBerlin&forecast_days=3";


WebServer server(80);


TaskHandle_t displayHandle     = NULL;
TaskHandle_t forcastHandle     = NULL;
TaskHandle_t readSensorsHandle = NULL;
TaskHandle_t fanHandle         = NULL;
TaskHandle_t servoHandle       = NULL;
TaskHandle_t alarmHandle       = NULL;
TaskHandle_t webHandle         = NULL;

const int DISPLAY_DELAY      = 8000;
const int FORCAST_DELAY      = 86400000;
const int READ_SENSORS_DELAY = 1000;
const int FAN_DELAY          = 1000;
const int SERVO_DELAY        = 1000;
const int ALARM_DELAY        = 1000;
const int WEB_DELAY          = 10;


const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Home</title>
<style>
:root{--bg:#f5f4f0;--s:#fff;--b:rgba(0,0,0,.1);--t:#1a1a18;--m:#6b6a65;--ac:#378ADD;--gr:#3B6D11;--gb:#EAF3DE;--r:12px}
@media(prefers-color-scheme:dark){:root{--bg:#1a1a18;--s:#252522;--b:rgba(255,255,255,.1);--t:#e8e6de;--m:#888780;--gb:#1a2d0a}}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:var(--bg);color:var(--t);min-height:100vh}
.app{max-width:860px;margin:0 auto;padding:1.5rem 1rem 3rem}
h1{font-size:1.25rem;font-weight:500}
.dot{width:8px;height:8px;border-radius:50%;background:#639922;display:inline-block;margin-right:6px}
.dot.off{background:#888}
.tabs{display:flex;gap:4px;margin:1rem 0;flex-wrap:wrap}
.tab{font-size:13px;padding:5px 14px;border-radius:999px;border:1px solid var(--b);background:transparent;cursor:pointer;color:var(--m);font-family:inherit;transition:all .15s}
.tab.active{background:var(--t);color:var(--bg);border-color:transparent}
.page{display:none}.page.active{display:block}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(148px,1fr));gap:.75rem;margin-bottom:.75rem}
.card{background:var(--s);border:1px solid var(--b);border-radius:var(--r);padding:1rem 1.25rem}
.lbl{font-size:12px;color:var(--m);margin-bottom:4px}
.val{font-size:26px;font-weight:500}
.unit{font-size:14px;color:var(--m)}
.badge{display:inline-block;font-size:12px;padding:2px 10px;border-radius:999px;font-weight:500}
.on{background:var(--gb);color:var(--gr)}
.off{background:var(--bg);color:var(--m);border:1px solid var(--b)}
hr{border:none;border-top:1px solid var(--b);margin:.75rem 0}
.sec{font-size:11px;text-transform:uppercase;letter-spacing:.08em;color:var(--m);margin-bottom:.5rem;font-weight:500}
.clk{font-size:2.2rem;font-weight:500;letter-spacing:-1px}
.jt{height:8px;background:var(--bg);border-radius:4px;margin:8px 0;overflow:hidden}
.jf{height:100%;border-radius:4px;background:var(--ac);transition:width .4s}
.jwin{width:44px;height:72px;border:1px solid var(--b);border-radius:4px;background:var(--bg);overflow:hidden;position:relative;flex-shrink:0}
.jbl{background:var(--ac);position:absolute;bottom:0;width:100%;transition:height .4s}
.btn{background:transparent;border:1px solid var(--b);border-radius:8px;padding:6px 14px;font-size:13px;cursor:pointer;color:var(--t);font-family:inherit;transition:background .15s}
.btn:hover{background:var(--bg)}
.brow{display:flex;gap:8px;flex-wrap:wrap;margin-top:.75rem}
.arow{display:flex;align-items:center;gap:8px;margin-top:.75rem}
.arow input{width:58px;padding:5px 8px;font-size:15px;border:1px solid var(--b);border-radius:8px;background:var(--s);color:var(--t);font-family:inherit;text-align:center}
.wt{width:100%;border-collapse:collapse;font-size:13px;margin-top:.5rem}
.wt td{padding:6px 8px;border-bottom:1px solid var(--b)}
.wt tr:last-child td{border-bottom:none}
.wt td:first-child{color:var(--m)}
.wt td:last-child{font-weight:500;text-align:right}
.row{display:flex;align-items:center;gap:12px}
.toast{position:fixed;bottom:1.5rem;left:50%;transform:translateX(-50%);background:var(--t);color:var(--bg);padding:8px 20px;border-radius:999px;font-size:13px;opacity:0;transition:opacity .3s;pointer-events:none}
.toast.show{opacity:1}
</style>
</head>
<body>
<div class="app">
<header style="display:flex;align-items:center;justify-content:space-between;margin-bottom:1rem">
  <div><h1><span class="dot" id="dot"></span>Smart Home</h1>
  <div style="font-size:12px;color:var(--m)" id="espip">Verbinde...</div></div>
  <div id="hclock" style="font-size:13px;color:var(--m)"></div>
</header>
<div class="tabs">
  <button class="tab active" onclick="tab('ov',this)">Uebersicht</button>
  <button class="tab" onclick="tab('jal',this)">Jalousie</button>
  <button class="tab" onclick="tab('alm',this)">Wecker</button>
  <button class="tab" onclick="tab('wthr',this)">Wetter</button>
</div>
<div id="tab-ov" class="page active">
  <div class="grid">
    <div class="card"><div class="lbl">Temperatur</div><div class="val" id="temp">-<span class="unit"> C</span></div><div style="font-size:12px;color:var(--m);margin-top:4px">DHT11 Innen</div></div>
    <div class="card"><div class="lbl">Luftfeuchtigkeit</div><div class="val" id="hum">-<span class="unit"> %</span></div><div style="font-size:12px;color:var(--m);margin-top:4px">DHT11 Innen</div></div>
    <div class="card"><div class="lbl">Ventilator</div><div style="margin-top:6px"><span class="badge off" id="fan">Aus</span></div><div style="font-size:12px;color:var(--m);margin-top:6px">Auto ab 28 C</div></div>
    <div class="card"><div class="lbl">Jalousie</div><div style="font-size:16px;font-weight:500;margin-top:4px" id="jlbl">-</div><div class="jt"><div class="jf" id="jfill" style="width:0%"></div></div><div style="font-size:12px;color:var(--m)">LDR Auto</div></div>
  </div>
  <div class="card" style="margin-bottom:.75rem">
    <div style="display:flex;justify-content:space-between;align-items:flex-start;flex-wrap:wrap;gap:1rem">
      <div><div class="lbl">Uhrzeit</div><div class="clk" id="clk">-</div><div style="font-size:13px;color:var(--m);margin-top:2px" id="dstr"></div></div>
      <div style="text-align:right"><div class="lbl">Wecker</div><div style="font-size:22px;font-weight:500" id="adsp">-</div><div style="font-size:12px;color:var(--m);margin-top:2px" id="ast">-</div></div>
    </div>
  </div>
  <div class="card">
    <div class="sec">Wetter morgen Aalen</div>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:6px;font-size:13px">
      <span style="color:var(--m)">Max</span><span id="omax" style="text-align:right;font-weight:500">-</span>
      <span style="color:var(--m)">Min</span><span id="omin" style="text-align:right;font-weight:500">-</span>
      <span style="color:var(--m)">Regen</span><span id="orain" style="text-align:right;font-weight:500">-</span>
      <span style="color:var(--m)">Schnee</span><span id="osnow" style="text-align:right;font-weight:500">-</span>
    </div>
  </div>
</div>
<div id="tab-jal" class="page">
  <div class="card">
    <div class="sec">Jalousie Manuell</div>
    <div class="row" style="margin:12px 0">
      <div class="jwin"><div class="jbl" id="jvis" style="height:0%"></div></div>
      <div><div style="font-size:18px;font-weight:500" id="jlbl2">-</div>
      <div style="font-size:13px;color:var(--m);margin-top:4px">LDR: <span id="ldrv">-</span></div>
      <div style="font-size:13px;color:var(--m)">Winkel: <span id="sang">-</span> Grad</div></div>
    </div>
    <div class="brow">
      <button class="btn" onclick="setJal(0)">Offen (0)</button>
      <button class="btn" onclick="setJal(90)">Halb (90)</button>
      <button class="btn" onclick="setJal(180)">Zu (180)</button>
    </div>
    <hr>
    <div class="sec">LDR Schwellwerte</div>
    <div style="font-size:13px;color:var(--m)">unter 1200: zu / 1200-2000: halb / ueber 2000: offen</div>
  </div>
</div>
<div id="tab-alm" class="page">
  <div class="card">
    <div class="sec">Wecker einstellen</div>
    <div class="arow">
      <input type="number" id="ah" value="16" min="0" max="23">
      <span style="font-size:20px;color:var(--m)">:</span>
      <input type="number" id="am" value="24" min="0" max="59">
      <span style="font-size:13px;color:var(--m)">Uhr</span>
    </div>
    <div class="brow">
      <button class="btn" onclick="saveAlarm(1)">Speichern und Aktivieren</button>
      <button class="btn" onclick="saveAlarm(0)">Deaktivieren</button>
    </div>
    <hr>
    <div class="sec">Melodie Buzzer Pin 12</div>
    <div style="font-size:13px;color:var(--m)">C5 D5 E5 F5 G5 A5 B5 C6 je 500ms</div>
  </div>
</div>
<div id="tab-wthr" class="page">
  <div class="card">
    <div class="sec">Morgen Aalen</div>
    <table class="wt">
      <tr><td>Max. Temperatur</td><td id="wmax">-</td></tr>
      <tr><td>Min. Temperatur</td><td id="wmin">-</td></tr>
      <tr><td>Regensum</td><td id="wrain">-</td></tr>
      <tr><td>Schneefall</td><td id="wsnow">-</td></tr>
    </table>
    <div style="font-size:12px;color:var(--m);margin-top:.75rem">Wird taeglich vom ESP32 aktualisiert.</div>
  </div>
</div>
</div>
<div class="toast" id="toast"></div>
<script>
const DAYS=['Sonntag','Montag','Dienstag','Mittwoch','Donnerstag','Freitag','Samstag'];
const MON=['Januar','Februar','Maerz','April','Mai','Juni','Juli','August','September','Oktober','November','Dezember'];
const p=n=>n<10?'0'+n:''+n;
function tab(name,btn){document.querySelectorAll('.page').forEach(x=>x.classList.remove('active'));document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active'));document.getElementById('tab-'+name).classList.add('active');btn.classList.add('active');}
function toast(msg){const t=document.getElementById('toast');t.textContent=msg;t.classList.add('show');setTimeout(()=>t.classList.remove('show'),2000);}
function jApply(angle){const pct=(angle/180)*100;const lbl=angle<5?'Offen':angle>175?'Geschlossen':angle>85&&angle<95?'Halb':angle+' Grad';['jfill'].forEach(id=>{const e=document.getElementById(id);if(e)e.style.width=pct+'%';});['jvis'].forEach(id=>{const e=document.getElementById(id);if(e)e.style.height=pct+'%';});['jlbl','jlbl2'].forEach(id=>{const e=document.getElementById(id);if(e)e.textContent=lbl;});const sa=document.getElementById('sang');if(sa)sa.textContent=angle;}
function tickClock(){const d=new Date();document.getElementById('clk').textContent=p(d.getHours())+':'+p(d.getMinutes())+':'+p(d.getSeconds());document.getElementById('dstr').textContent=DAYS[d.getDay()]+', '+d.getDate()+'. '+MON[d.getMonth()]+' '+d.getFullYear();document.getElementById('hclock').textContent=p(d.getHours())+':'+p(d.getMinutes());}
setInterval(tickClock,1000);tickClock();
async function poll(){try{const r=await fetch('/api/status');if(!r.ok)throw'';const s=await r.json();document.getElementById('dot').className='dot';document.getElementById('espip').textContent='ESP32 verbunden';document.getElementById('temp').innerHTML=(s.temperature>0?s.temperature.toFixed(1):'-')+'<span class="unit"> C</span>';document.getElementById('hum').innerHTML=(s.humidity>0?s.humidity.toFixed(1):'-')+'<span class="unit"> %</span>';const fb=document.getElementById('fan');fb.textContent=s.fan_on?'An':'Aus';fb.className='badge '+(s.fan_on?'on':'off');jApply(s.servo_angle||0);const ldr=document.getElementById('ldrv');if(ldr)ldr.textContent=s.ldr_value;document.getElementById('adsp').textContent=p(s.alarm_hour)+':'+p(s.alarm_minute);document.getElementById('ast').textContent=s.alarm_active?'Aktiv':'Deaktiviert';document.getElementById('ah').value=s.alarm_hour;document.getElementById('am').value=s.alarm_minute;if(s.max_temp>0){const fmt=v=>v.toFixed(1);['omax','wmax'].forEach(id=>document.getElementById(id).textContent=fmt(s.max_temp)+' C');['omin','wmin'].forEach(id=>document.getElementById(id).textContent=fmt(s.min_temp)+' C');['orain','wrain'].forEach(id=>document.getElementById(id).textContent=fmt(s.rain_sum)+' mm');['osnow','wsnow'].forEach(id=>document.getElementById(id).textContent=fmt(s.snow_sum)+' cm');}}catch(e){document.getElementById('dot').className='dot off';document.getElementById('espip').textContent='Nicht verbunden';}}
setInterval(poll,3000);poll();
async function setJal(angle){jApply(angle);await fetch('/api/jalousie?angle='+angle);toast('Jalousie: '+angle+' Grad');}
async function saveAlarm(active){const h=parseInt(document.getElementById('ah').value)||0;const m=parseInt(document.getElementById('am').value)||0;await fetch('/api/alarm?hour='+h+'&minute='+m+'&active='+active);document.getElementById('adsp').textContent=p(h)+':'+p(m);document.getElementById('ast').textContent=active?'Aktiv':'Deaktiviert';toast(active?'Wecker: '+p(h)+':'+p(m):'Wecker deaktiviert');}
</script>
</body>
</html>
)rawhtml";

// ═══════════════════════════════════════════════════════════════
//  WEBSERVER ROUTEN
// ═══════════════════════════════════════════════════════════════
void handleRoot() { server.send_P(200, "text/html", DASHBOARD_HTML); }

void handleStatus() {
  JsonDocument doc;
  doc["temperature"]  = temperature;
  doc["humidity"]     = humidity;
  doc["fan_on"]       = fanOn;
  doc["servo_angle"]  = myservo.read();
  doc["ldr_value"]    = ldrValue;
  doc["alarm_hour"]   = weckStunde;
  doc["alarm_minute"] = weckMinute;
  doc["alarm_active"] = alarmActive;
  doc["max_temp"]     = maxTemp;
  doc["min_temp"]     = minTemp;
  doc["rain_sum"]     = rainSum;
  doc["snow_sum"]     = snowfallSum;
  String out; serializeJson(doc, out);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", out);
}

void handleJalousie() {
  if (server.hasArg("angle")) myservo.write(server.arg("angle").toInt());
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleAlarmSet() {
  if (server.hasArg("hour"))   weckStunde  = server.arg("hour").toInt();
  if (server.hasArg("minute")) weckMinute  = server.arg("minute").toInt();
  if (server.hasArg("active")) alarmActive = server.arg("active").toInt() == 1;
  if (!alarmActive) alarmOff = true;
  server.send(200, "application/json", "{\"ok\":true}");
}


void readSensor(void *p) {
  while (true) {
    float t = dht.readTemperature(), h = dht.readHumidity();
    if (!isnan(t)) temperature = t;
    if (!isnan(h)) humidity    = h;
    vTaskDelay(pdMS_TO_TICKS(READ_SENSORS_DELAY));
  }
}

void controlFan(void *p) {
  while (true) {
    if (temperature > 28.0) { digitalWrite(ENABLE,HIGH); digitalWrite(DIRA,HIGH); digitalWrite(DIRB,LOW); fanOn=true; }
    else                    { digitalWrite(ENABLE,LOW); fanOn=false; }
    vTaskDelay(pdMS_TO_TICKS(FAN_DELAY));
  }
}

void controlServo(void *p) {
  while (true) {
    int v = analogRead(LDRPIN); vTaskDelay(pdMS_TO_TICKS(SERVO_DELAY));
    v += analogRead(LDRPIN);    vTaskDelay(pdMS_TO_TICKS(SERVO_DELAY));
    v += analogRead(LDRPIN);    ldrValue = v;
    int s = myservo.read();
    if      (v < 1200 && s < 175)                          myservo.write(180);
    else if (v > 1200 && v < 2000 && (s < 85 || s > 95))  myservo.write(90);
    else if (v > 2000 && s > 5)                            myservo.write(0);
    vTaskDelay(pdMS_TO_TICKS(SERVO_DELAY));
  }
}

void updateDisplay(void *p) {
  while (true) {
    display.setFullWindow(); display.firstPage();
    struct tm timeinfo; getLocalTime(&timeinfo);
    do {
      if (page==0) {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(20,40);  display.print("Temp: ");  display.print(temperature,1); display.print("C");
        display.setCursor(20,80);  display.print("Luftf: "); display.print(humidity,1);    display.print("%");
        display.setCursor(20,120); display.print(fanOn?"Ventilator an":"Ventilator aus");
      } else if (page==1) {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(20,40); display.println("Heute ist der:");
        display.setCursor(20,80); display.println(&timeinfo,"%A, %d.%m.%Y");
        display.setCursor(20,120);display.println(&timeinfo,"%H:%M:%S Uhr");
      } else if (page==2) {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(20,40); display.println("Alarm um:");
        display.setCursor(20,80);
        if(weckStunde<10)display.print("0"); display.print(weckStunde); display.print(":");
        if(weckMinute<10)display.print("0"); display.print(weckMinute); display.println(" Uhr");
        display.setCursor(20,120); display.println(alarmActive?"Aktiv":"Deaktiviert");
      } else if (page==3) {
        display.fillScreen(GxEPD_WHITE);
        time_t tomorrow=time(nullptr)+86400; struct tm *ti=localtime(&tomorrow);
        display.setCursor(20,40); display.print("Wetter "); display.println(ti,"%d.%m.");
        display.setCursor(20,80);
        display.print("Max:"); display.print(maxTemp,1); display.print(" Min:"); display.print(minTemp,1);
        display.setCursor(20,120);
        display.print("R:"); display.print(rainSum,1); display.print(" S:"); display.print(snowfallSum,1);
      } else if (page==4) {
        display.fillScreen(GxEPD_WHITE); display.setCursor(20,40);
        int s=myservo.read();
        if(s<5)           display.println("Jalousie: offen");
        else if(s>85&&s<95){display.println("Jalousie:"); display.setCursor(20,80); display.println("halb zu");}
        else if(s>175)    {display.println("Jalousie:"); display.setCursor(20,80); display.println("geschlossen");}
        else               display.println("Jalousie: ?");
      }
    } while (display.nextPage());
    page=(page+1)%5;
    vTaskDelay(pdMS_TO_TICKS(DISPLAY_DELAY));
  }
}

void handleAlarm(void *p) {
  while (true) {
    struct tm timeinfo; getLocalTime(&timeinfo);
    if (alarmActive && timeinfo.tm_hour==weckStunde && timeinfo.tm_min==weckMinute && !alarmOff) {
      for (int n=0; n<8 && !alarmOff; n++) {
        alarmOff=(digitalRead(ALARM_OFF_BUTTON)==0);
        tone(SPEAKERPIN,melody[n],noteDuration);
        vTaskDelay(pdMS_TO_TICKS(500));
      }
    }
    if (alarmOff && timeinfo.tm_hour==weckStunde && timeinfo.tm_min==weckMinute+1) alarmOff=false;
    vTaskDelay(pdMS_TO_TICKS(ALARM_DELAY));
  }
}

void readWeatherForcast(void *p) {
  while (true) {
    HTTPClient client; client.begin(urlWeather);
    if (client.GET()==HTTP_CODE_OK) {
      String resp=client.getString(); JsonDocument doc; deserializeJson(doc,resp.c_str());
      maxTemp=doc["daily"]["temperature_2m_max"][1]; minTemp=doc["daily"]["temperature_2m_min"][1];
      rainSum=doc["daily"]["rain_sum"][1];           snowfallSum=doc["daily"]["snowfall_sum"][1];
    }
    client.end();
    vTaskDelay(pdMS_TO_TICKS(FORCAST_DELAY));
  }
}

void webServerTask(void *p) {
  while (true) { server.handleClient(); vTaskDelay(pdMS_TO_TICKS(WEB_DELAY)); }
}


void setup() {
  Serial.begin(115200);
  Serial.print("Verbinde mit "); Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status()!=WL_CONNECTED){delay(500);Serial.print(".");}
  Serial.println("\nWiFi verbunden!");
  Serial.print(">>> Dashboard oeffnen: http://");
  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  pinMode(ENABLE,OUTPUT); pinMode(DIRA,OUTPUT); pinMode(DIRB,OUTPUT);
  pinMode(ALARM_OFF_BUTTON,INPUT_PULLUP);
  myservo.attach(13); myservo.write(0);
  dht.begin();

  display.init(); display.setRotation(3);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);

  server.on("/",             handleRoot);
  server.on("/api/status",   handleStatus);
  server.on("/api/jalousie", handleJalousie);
  server.on("/api/alarm",    handleAlarmSet);
  server.begin();

  xTaskCreatePinnedToCore(webServerTask,     "WebTask",    8192,  NULL, 3, &webHandle,         0);
  xTaskCreatePinnedToCore(readWeatherForcast,"WeatherTask",16384, NULL, 1, &forcastHandle,     1);
  xTaskCreatePinnedToCore(updateDisplay,     "DisplayTask",16384, NULL, 2, &displayHandle,     1);
  xTaskCreatePinnedToCore(readSensor,        "SensorTask", 16384, NULL, 1, &readSensorsHandle, 1);
  xTaskCreatePinnedToCore(controlFan,        "FanTask",    16384, NULL, 1, &fanHandle,         1);
  xTaskCreatePinnedToCore(controlServo,      "ServoTask",  16384, NULL, 1, &servoHandle,       1);
  xTaskCreatePinnedToCore(handleAlarm,       "AlarmTask",  16384, NULL, 1, &alarmHandle,       1);

  Serial.println("System gestartet.");
}

void loop() {}
