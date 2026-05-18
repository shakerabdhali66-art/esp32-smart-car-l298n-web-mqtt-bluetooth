#include <WiFi.h>
#include <WebServer.h>
#include <BluetoothSerial.h>
#include <PubSubClient.h>

// ================= WIFI =================
const char* WIFI_SSID = "Shaker..";
const char* WIFI_PASSWORD = "123456789";

// ================= AP for Web =================
const char* AP_SSID = "shaker";
const char* AP_PASSWORD = "123456789";

// ================= MQTT =================
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* topicControl = "shaker/car/control";
const char* topicSpeedA  = "shaker/car/speedA";
const char* topicSpeedB  = "shaker/car/speedB";
const char* topicStatus  = "shaker/car/status";

// ================= PINS =================
#define ENA 21
#define ENB 22
#define IN1 4
#define IN2 23
#define IN3 18
#define IN4 19
#define WLED 27
#define RLED 32

// ================= INPUT BUTTONS =================
const int button1 = 13;  // Forward
const int button2 = 12;  // Backward
const int button3 = 14;  // Left
const int button4 = 26;  // Right
const int button5 = 25;  // Stop

// ================= PWM =================
const int PWM_CHANNEL_A = 0;
const int PWM_CHANNEL_B = 1;
const int PWM_FREQUENCY = 5000;
const int PWM_RESOLUTION = 8;

// ================= OBJECTS =================
BluetoothSerial BTSerial;
WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

// ================= STATE =================
int motorSpeedA = 200;
int motorSpeedB = 200;
String currentDirection = "STOP";
bool whiteLedOn = false;
bool redLedOn = false;
bool webButtonActive = false;

unsigned long lastDebounce[5] = {0, 0, 0, 0, 0};
const unsigned long DEBOUNCE_DELAY = 50;
int prevButtonState[5] = {HIGH, HIGH, HIGH, HIGH, HIGH};

enum ControlSource {
  SRC_NONE,
  SRC_WEB,
  SRC_BT,
  SRC_MQTT,
  SRC_PHYSICAL
};

ControlSource lastSource = SRC_NONE;

// ================= PWM APPLY =================
void applyPWM() {
  if (currentDirection == "STOP") {
    ledcWrite(PWM_CHANNEL_A, 0);
    ledcWrite(PWM_CHANNEL_B, 0);
  } else {
    ledcWrite(PWM_CHANNEL_A, motorSpeedA);
    ledcWrite(PWM_CHANNEL_B, motorSpeedB);
  }
}

// ================= MOTOR CONTROL =================
void carForward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  digitalWrite(WLED, HIGH);
  digitalWrite(RLED, LOW);
  currentDirection = "FORWARD";
  whiteLedOn = true;
  redLedOn = false;
  applyPWM();
  Serial.println("[MOTOR] FORWARD");
}

void carBackward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  digitalWrite(WLED, LOW);
  digitalWrite(RLED, HIGH);
  currentDirection = "BACKWARD";
  whiteLedOn = false;
  redLedOn = true;
  applyPWM();
  Serial.println("[MOTOR] BACKWARD");
}

void carLeft() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  digitalWrite(WLED, HIGH);
  digitalWrite(RLED, LOW);
  currentDirection = "LEFT";
  whiteLedOn = true;
  redLedOn = false;
  applyPWM();
  Serial.println("[MOTOR] LEFT");
}

void carRight() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  digitalWrite(WLED, HIGH);
  digitalWrite(RLED, LOW);
  currentDirection = "RIGHT";
  whiteLedOn = true;
  redLedOn = false;
  applyPWM();
  Serial.println("[MOTOR] RIGHT");
}

void carStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  digitalWrite(WLED, LOW);
  digitalWrite(RLED, LOW);
  currentDirection = "STOP";
  whiteLedOn = false;
  redLedOn = false;
  applyPWM();
  Serial.println("[MOTOR] STOP");
}

// ================= STATUS JSON =================
String makeStatusJson() {
  String json = "{";
  json += "\"dir\":\"" + currentDirection + "\",";
  json += "\"spdA\":" + String(motorSpeedA) + ",";
  json += "\"spdB\":" + String(motorSpeedB) + ",";
  json += "\"wled\":" + String(whiteLedOn ? "true" : "false") + ",";
  json += "\"rled\":" + String(redLedOn ? "true" : "false");
  json += "}";
  return json;
}

void publishStatus() {
  if (!client.connected()) return;
  String payload = makeStatusJson();
  client.publish(topicStatus, payload.c_str(), true);
}

// ================= MQTT CALLBACK =================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();

  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(msg);

  String t = String(topic);

  if (t == topicControl) {
    lastSource = SRC_MQTT;
    webButtonActive = false;

    if (msg == "F") carForward();
    else if (msg == "B") carBackward();
    else if (msg == "L") carLeft();
    else if (msg == "R") carRight();
    else if (msg == "S") carStop();
  }
  else if (t == topicSpeedA) {
    motorSpeedA = constrain(msg.toInt(), 0, 255);
    lastSource = SRC_MQTT;
    applyPWM();
  }
  else if (t == topicSpeedB) {
    motorSpeedB = constrain(msg.toInt(), 0, 255);
    lastSource = SRC_MQTT;
    applyPWM();
  }
}

// ================= WIFI STA =================
void setup_wifi_STA() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    if (millis() - start > 20000) {
      Serial.println();
      Serial.println("[WIFI] Retry...");
      start = millis();
      WiFi.disconnect(true);
      delay(1000);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());
}

// ================= MQTT RECONNECT =================
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32CAR-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(topicControl);
      client.subscribe(topicSpeedA);
      client.subscribe(topicSpeedB);
      publishStatus();
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

// ================= WEB PAGE =================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Smart Car</title>
<style>
  *{margin:0;padding:0;box-sizing:border-box}
  body{font-family:Arial,Helvetica,sans-serif;background:#1a1a2e;color:#eee;display:flex;flex-direction:column;align-items:center;padding:15px;min-height:100vh}
  h1{color:#00b4d8;margin-bottom:4px;font-size:1.6em}
  .sub{color:#777;font-size:0.85em;margin-bottom:20px}
  .status{background:rgba(255,255,255,0.06);border:1px solid rgba(255,255,255,0.1);border-radius:10px;padding:18px;width:100%;max-width:460px;text-align:center;margin-bottom:22px}
  .status h2{font-size:1em;color:#999;margin-bottom:10px}
  #dirText{font-size:2em;font-weight:bold;padding:8px;border-radius:6px;margin-bottom:8px}
  .dir-stop{color:#888;background:rgba(100,100,100,0.2)}
  .dir-forward{color:#0f0;background:rgba(0,255,0,0.08)}
  .dir-backward{color:#f44;background:rgba(255,0,0,0.08)}
  .dir-left{color:#fa0;background:rgba(255,170,0,0.08)}
  .dir-right{color:#a6f;background:rgba(170,100,255,0.08)}
  .led-row{display:flex;justify-content:center;gap:25px;margin-top:8px;font-size:0.9em}
  .led-item{display:flex;align-items:center;gap:6px}
  .dot{width:12px;height:12px;border-radius:50%;border:2px solid #555}
  .dot.w-on{background:#fff;border-color:#fff;box-shadow:0 0 8px #fff}
  .dot.r-on{background:#f33;border-color:#f33;box-shadow:0 0 8px #f33}
  .speed-row{display:flex;justify-content:center;gap:20px;margin-top:10px;font-size:0.85em;color:#00b4d8}
  .ctrl{width:100%;max-width:460px;margin-bottom:22px;text-align:center}
  .ctrl h2{font-size:1em;color:#999;margin-bottom:14px}
  .cross{position:relative;width:210px;height:210px;margin:0 auto}
  .arrow{position:absolute;width:70px;height:70px;border:none;border-radius:12px;background:rgba(255,255,255,0.06);cursor:pointer;user-select:none;-webkit-user-select:none;touch-action:none;transition:all 0.12s;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:2px;color:inherit}
  .arrow .tri{font-size:1.6em;line-height:1}
  .arrow .lbl{font-size:0.65em;font-weight:bold;letter-spacing:1px}
  .a-up{top:0;left:50%;transform:translateX(-50%);border:2px solid #0f0;color:#0f0}
  .a-up:active,.a-up.pressed{background:rgba(0,255,0,0.2);box-shadow:0 0 20px rgba(0,255,0,0.3)}
  .a-down{bottom:0;left:50%;transform:translateX(-50%);border:2px solid #f44;color:#f44}
  .a-down:active,.a-down.pressed{background:rgba(255,68,68,0.2);box-shadow:0 0 20px rgba(255,68,68,0.3)}
  .a-left{left:0;top:50%;transform:translateY(-50%);border:2px solid #fa0;color:#fa0}
  .a-left:active,.a-left.pressed{background:rgba(255,170,0,0.2);box-shadow:0 0 20px rgba(255,170,0,0.3)}
  .a-right{right:0;top:50%;transform:translateY(-50%);border:2px solid #a6f;color:#a6f}
  .a-right:active,.a-right.pressed{background:rgba(170,100,255,0.2);box-shadow:0 0 20px rgba(170,100,255,0.3)}
  .a-stop{top:50%;left:50%;transform:translate(-50%,-50%);width:72px;height:72px;border:2px solid #f44;color:#f44;border-radius:50%;background:rgba(255,68,68,0.08)}
  .a-stop:active,.a-stop.pressed{background:rgba(255,68,68,0.25);box-shadow:0 0 20px rgba(255,68,68,0.3)}
  .sliders{width:100%;max-width:460px;margin-bottom:22px}
  .sliders h2{font-size:1em;color:#999;margin-bottom:12px;text-align:center}
  .sld-group{background:rgba(255,255,255,0.04);border:1px solid rgba(255,255,255,0.08);border-radius:8px;padding:12px 16px;margin-bottom:10px}
  .sld-label{display:flex;justify-content:space-between;margin-bottom:6px;font-size:0.9em}
  .sld-label .val{color:#00b4d8;font-weight:bold;min-width:30px;text-align:right}
  input[type=range]{width:100%;-webkit-appearance:none;appearance:none;height:6px;border-radius:3px;background:rgba(255,255,255,0.12);outline:none}
  input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:20px;height:20px;border-radius:50%;background:#00b4d8;cursor:pointer}
  input[type=range]::-moz-range-thumb{width:20px;height:20px;border-radius:50%;background:#00b4d8;cursor:pointer;border:none}
</style>
</head>
<body>
<h1>ESP32 Smart Car</h1>
<p class="sub">Manual Control Dashboard</p>
<div class="status">
  <h2>STATUS</h2>
  <div id="dirText" class="dir-stop">STOP</div>
  <div class="led-row">
    <div class="led-item"><div id="dotW" class="dot"></div> Front LED</div>
    <div class="led-item"><div id="dotR" class="dot"></div> Rear LED</div>
  </div>
  <div class="speed-row">
    <span>ENA: <b id="spdAText">200</b></span>
    <span>ENB: <b id="spdBText">200</b></span>
  </div>
</div>
<div class="ctrl">
  <h2>DIRECTION (hold to move, release to stop)</h2>
  <div class="cross">
    <button class="arrow a-up" data-cmd="forward"><span class="tri">&#9650;</span><span class="lbl">FRONT</span></button>
    <button class="arrow a-left" data-cmd="left"><span class="tri">&#9664;</span><span class="lbl">LEFT</span></button>
    <button class="arrow a-stop" data-cmd="stop"><span class="tri">&#9632;</span><span class="lbl">STOP</span></button>
    <button class="arrow a-right" data-cmd="right"><span class="tri">&#9654;</span><span class="lbl">RIGHT</span></button>
    <button class="arrow a-down" data-cmd="backward"><span class="tri">&#9660;</span><span class="lbl">BACK</span></button>
  </div>
</div>
<div class="sliders">
  <h2>MOTOR SPEED CONTROL</h2>
  <div class="sld-group">
    <div class="sld-label"><span>Motor A (ENA)</span><span class="val" id="valA">200</span></div>
    <input type="range" id="sldA" min="0" max="255" value="200">
  </div>
  <div class="sld-group">
    <div class="sld-label"><span>Motor B (ENB)</span><span class="val" id="valB">200</span></div>
    <input type="range" id="sldB" min="0" max="255" value="200">
  </div>
</div>
<script>
function sendCmd(c){ var x=new XMLHttpRequest(); x.open('GET','/cmd?action='+c,true); x.send(); }
function refreshStatus(){
  var x=new XMLHttpRequest();
  x.onreadystatechange=function(){
    if(this.readyState==4&&this.status==200){
      var d=JSON.parse(this.responseText);
      var el=document.getElementById('dirText');
      el.textContent=d.dir;
      el.className='dir-'+d.dir.toLowerCase();
      document.getElementById('dotW').className='dot'+(d.wled?' w-on':'');
      document.getElementById('dotR').className='dot'+(d.rled?' r-on':'');
      document.getElementById('spdAText').textContent=d.spdA;
      document.getElementById('spdBText').textContent=d.spdB;
    }
  };
  x.open('GET','/status',true); x.send();
}
var dirs=['forward','backward','left','right'];
dirs.forEach(function(c){
  var btn=document.querySelector('[data-cmd="'+c+'"]');
  btn.addEventListener('mousedown',function(e){e.preventDefault();sendCmd(c);btn.classList.add('pressed')});
  btn.addEventListener('mouseup',function(e){e.preventDefault();sendCmd('stop');btn.classList.remove('pressed')});
  btn.addEventListener('mouseleave',function(){ if(btn.classList.contains('pressed')){sendCmd('stop');btn.classList.remove('pressed')} });
  btn.addEventListener('touchstart',function(e){e.preventDefault();sendCmd(c);btn.classList.add('pressed')});
  btn.addEventListener('touchend',function(e){e.preventDefault();sendCmd('stop');btn.classList.remove('pressed')});
  btn.addEventListener('touchcancel',function(){sendCmd('stop');btn.classList.remove('pressed')});
});
document.querySelector('[data-cmd="stop"]').addEventListener('click',function(){sendCmd('stop')});
var sldA=document.getElementById('sldA'),valA=document.getElementById('valA');
var sldB=document.getElementById('sldB'),valB=document.getElementById('valB');
sldA.addEventListener('input',function(){valA.textContent=this.value});
sldA.addEventListener('change',function(){ var x=new XMLHttpRequest(); x.open('GET','/speed?a='+this.value,true); x.send(); });
sldB.addEventListener('input',function(){valB.textContent=this.value});
sldB.addEventListener('change',function(){ var x=new XMLHttpRequest(); x.open('GET','/speed?b='+this.value,true); x.send(); });
setInterval(refreshStatus,500); refreshStatus();
</script>
</body>
</html>
)rawliteral";

// ================= WEB HANDLERS =================
void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleCommand() {
  if (server.hasArg("action")) {
    String action = server.arg("action");
    lastSource = SRC_WEB;

    if (action == "forward") {
      webButtonActive = true;
      carForward();
    }
    else if (action == "backward") {
      webButtonActive = true;
      carBackward();
    }
    else if (action == "left") {
      webButtonActive = true;
      carLeft();
    }
    else if (action == "right") {
      webButtonActive = true;
      carRight();
    }
    else if (action == "stop") {
      webButtonActive = false;
      carStop();
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleSpeed() {
  if (server.hasArg("a")) motorSpeedA = constrain(server.arg("a").toInt(), 0, 255);
  if (server.hasArg("b")) motorSpeedB = constrain(server.arg("b").toInt(), 0, 255);

  if (currentDirection == "FORWARD") carForward();
  else if (currentDirection == "BACKWARD") carBackward();
  else if (currentDirection == "LEFT") carLeft();
  else if (currentDirection == "RIGHT") carRight();

  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  server.send(200, "application/json", makeStatusJson());
}

// ================= PHYSICAL BUTTONS =================
void readPhysicalButtons() {
  int pins[5] = {button1, button2, button3, button4, button5};
  int states[5];

  for (int i = 0; i < 5; i++) {
    int reading = digitalRead(pins[i]);
    if (reading != prevButtonState[i]) {
      lastDebounce[i] = millis();
    }

    if ((millis() - lastDebounce[i]) > DEBOUNCE_DELAY) {
      states[i] = reading;
    } else {
      states[i] = prevButtonState[i];
    }

    prevButtonState[i] = reading;
  }

  if (states[0] == LOW) {
    lastSource = SRC_PHYSICAL;
    carForward();
  }
  else if (states[1] == LOW) {
    lastSource = SRC_PHYSICAL;
    carBackward();
  }
  else if (states[2] == LOW) {
    lastSource = SRC_PHYSICAL;
    carLeft();
  }
  else if (states[3] == LOW) {
    lastSource = SRC_PHYSICAL;
    carRight();
  }
  else if (states[4] == LOW) {
    lastSource = SRC_PHYSICAL;
    carStop();
  }
  else {
    // الإيقاف التلقائي فقط للأزرار الفيزيائية
    if (lastSource == SRC_PHYSICAL && currentDirection != "STOP") {
      carStop();
      lastSource = SRC_NONE;
    }
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(WLED, OUTPUT);
  pinMode(RLED, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(button3, INPUT_PULLUP);
  pinMode(button4, INPUT_PULLUP);
  pinMode(button5, INPUT_PULLUP);

  digitalWrite(WLED, LOW);
  digitalWrite(RLED, LOW);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  ledcSetup(PWM_CHANNEL_A, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_B, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(ENA, PWM_CHANNEL_A);
  ledcAttachPin(ENB, PWM_CHANNEL_B);

  ledcWrite(PWM_CHANNEL_A, 0);
  ledcWrite(PWM_CHANNEL_B, 0);

  carStop();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("[AP] IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/cmd", handleCommand);
  server.on("/speed", handleSpeed);
  server.on("/status", handleStatus);
  server.begin();

  setup_wifi_STA();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  reconnectMQTT();

  BTSerial.begin("shaker");
  Serial.println("[BT] Bluetooth started as 'shaker'");
}

// ================= LOOP =================
void loop() {
  server.handleClient();
  readPhysicalButtons();

  while (BTSerial.available()) {
    char c = BTSerial.read();
    lastSource = SRC_BT;
    webButtonActive = false;

    if (c == 'F' || c == 'f') carForward();
    else if (c == 'B' || c == 'b') carBackward();
    else if (c == 'L' || c == 'l') carLeft();
    else if (c == 'R' || c == 'r') carRight();
    else if (c == 'S' || c == 's') carStop();
  }

  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 1000) {
    lastStatus = millis();
    publishStatus();
  }

  delay(10);
}
