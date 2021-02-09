/*  2020.12.09  esp32(d1 mini) 에서 log_time_unit  = 6, time_reference = 60, table_size     = 1442, show = 60  로 설정하면 24시간(2시간 간격) 1분 로깅 그래프를 만들 수 있음.
 *  2020.10.20 주화면의 String(i) 부분을 String(sensor_data[i].ltime) 로 대체하여 X축을 로컬시간으로 구현
 *  2020.10.20 로그화일에 기록되는 주기는 loop 문의 마지막부분의 delay(1000)으로 하여 
 *  timer_cnt 가 1초 단위로 카운팅되도록 하면 log_interval 인 60을 1분으로 인식할 수 있게됨.
 *    
 *  2020.9.2 hardreset 을 위해 gpio15를 EN 과 묶었는데 이러면 OTA loading 이 안됨. 원인분석 필요
 * esp32 d1 mini 모델에서는 문제가 없었음(노출 핀이 없어서 GPIO 16번으로 변경). 
 * Partition Scheme 은 default 값으로 로딩하여도 잘 됨(1.2MB APP, 1.5MB SPIFF)
 *  
 *  2020.9.1 - OTA 적용 성공(그러나 기본모델이 잘 안되어 esp32 d1 mini 모델로 성공)
 *  ----> 원인규명함 : SPIFF 메모리를 지우고 나니 기본모델도 되었음.(<--- 자꾸 리부팅하는 듯. / 모델별로 특성을 타는 듯)
 *  
 *  2020.08.07 - Mysensors 또는 AsyncTcp 관련 컴파일 오류가 나면 esp32 보드 버전을 1.0.3 으로 업/다운하면 됨(2020.08.07)
 *   
 *  2020.7.27 : telegram, modbus web html 추가 성공(참고사이트 : https://randomnerdtutorials.com/)
 *  
    2020.9.1 - OTA 기능 추가 성공(esp32 d1 mini 모델로 성공 / 기본모델은 이전로직만에서도 무한 부팅되어 안되었음)

    2020.7.22 telegram 알림 기능 추가 성공 (참고사이트 :  https://randomnerdtutorials.com/blog/)

    2020.7.13 reset, removeSpiff 메뉴 추가 성공   // uploading 잘 안되면  rst 핀 분리후 업로딩할 것.

    2020.6.23 수정성공
    UTC 시간(setup()에서 한번만 찍어내는 듯)과 UNIX 시간(log파일에 저장하는.. )을 우리나라 시간으로 변환하기 성공

   UTC 시간 사용 :  StartTime() 함수에서 아래 문구 사용.
    setenv("TZ", "UTC-09:00",1); // "UTC-05:30" 이면 UTC(India 기준) 시간에서 5시간 30분을 뺀다는 뜻 // Change for your location

    로그파일에 저장하는 시간(UNIX -> 우리나라 시간)
    calcDateTime(int epoch) 함수에서
    seconds      = epoch + (9*3600);  // UNIX 타임을 우리나라 시간으로 변환하기 위해 (9시간 * 3600초)를 더해주면 됨


   2020.6.15 수정성공
   구글 챠트의 구형 라이브러리(jsapi)를 신형(load.js)로 바꾸니 되었음(참고 사이트 : https://developers.google.com/chart/interactive/docs/basic_load_libs)


   2020.5.27
   SPIFFS 메모리 서버가 작동하지 않으면 SPIFFS.begin() 구문을 찾아서 SPIFFS.begin(true) 로 수정해서 해볼 것.
   그리고 SPIFFS 메모리 작동에 오류가 있으면(자주발생한다고 함) setup() 에서 SPIFFS.remove("/"+DataFile); 구문을 일시활성화하여
   지우기를 시도할 것. 그러면 SPIFFS.begin(true) 를 하지 않아도 될 수 있음

   2020.4.17 성공
   esp8266 또는  esp32 모두에서 쓸 수 있는 data logger 프로그램에 esp32 에만 쓸 수 있는 esp32ModbusTCP 를 사용하였기에 esp32에만 적용 가능함.
*/

/* ESP8266 plus WEMOS SHT30-D Sensor with a Temperature and Weight Web Server
  참고사이트 :  http://www.dsbird.org.uk
*/

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#define TZone 9 // or set you your requirements e.g. -5 for EST
//#include <Server.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
WiFiUDP ntpUDP; //** NTP client class
NTPClient timeClient(ntpUDP);
#include <fs.h>
#else
#include <WiFi.h>
#include <ESP32WebServer.h>  //https://github.com/Pedroalbuquerque/ESP32WebServer download and place in your Libraries folder
#include <WiFiClient.h>
#include "FS.h"
#include "SPIFFS.h"
#endif

///////////  OTA --------------
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

const int ledPin =  2;
int ledState = LOW;            
unsigned long previousMillis = 0;    
const long interval = 500;
///////////  OTA --------------


//////// telegram --------
// #include <WiFi.h>    // 위와 중복되므로
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// Initialize Telegram BOT
#define BOTtoken "1381814078:AAEwrxKyNPrHp3Au5pB4kJVz-u_P6Z3LHks"  // your Bot Token (Get from Botfather)

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
#define CHAT_ID "958241417"

WiFiClientSecure client_telegram;   // client -> client_telegram
UniversalTelegramBot bot(BOTtoken, client_telegram);    // client -> client_telegram

int compare = 0;
//////// telegram --------


////// 모드버스 TCP-----
#include <Arduino.h>
#include <WiFi.h>   // 바로 위와 중복되는 듯.
#include <esp32ModbusTCP.h>
////// 모드버스 TCP-----


#include "credentials.h"
#include <SPI.h>
#include <time.h>


/////////////  mysql_db 설정 //////////////////
char mysql_server[] = "54.180.127.63"; // 접속할 서버 주소 APMSETUP이 실행중인 컴퓨터의 ip(내 노트북)
// 서버 PC가 이더넷으로 연결되어 있으면 이더넷 IP를 사용해야함에 유의
String readString = String(30);  // 용도 아직 모르겠음
///////////////////////////////////////////////


String title = "Master Data Logger";
String version = "v1.0";      // Version of this program
String site_width = "1023";
WiFiClient client;

#ifdef ESP8266
ESP8266WebServer server(80); // Start server on port 80 (default for a web-browser, change to your requirements, e.g. 8080 if your Router uses port 80
// To access server from the outside of a WiFi network e.g. ESP8266WebServer server(8266); and then add a rule on your Router that forwards a
// connection request to http://your_network_ip_address:8266 to port 8266 and view your ESP server from anywhere.
// Example http://g6ejd.uk.to:8266 will be directed to http://192.168.0.40:8266 or whatever IP address your router gives to this server
#else
ESP32WebServer server(80); // Start server on port 80 (default for a web-browser, change to your requirements, e.g. 8080 if your Router uses port 80
#endif

int       log_time_unit  = 6;  // default is 1-minute between readings, 10=15secs 40=1min 200=5mins 400=10mins 2400=1hr
int       time_reference = 60;  // Time reference for calculating /log-time (nearly in secs) to convert to minutes
int const table_size     = 1442; // +2정도 해야 구간이 다보임  //  72;  // 80 is about the maximum for the available memory and Google Charts, based on 3 samples/hour * 24 * 1 day = 72 displayed, but not stored!
int       index_ptr, timer_cnt, log_interval, log_count, max_temp, min_temp;
String    webpage, time_now, log_time, lastcall, time_str, DataFile = "datalog.txt";
bool      AScale, auto_smooth, AUpdate, log_delete_approved;
float     temp, weight, t1;

int HardResetPin = 16;    // 추가
bool HardRsetState = HIGH;    //  추가

typedef signed short sint16;
typedef struct {
  int     lcnt; // Sequential log count
  String ltime; // Time record of when reading was taken
  sint16 temp;  // 메모리 절약을 위해 short 사용 // Temperature values, short unsigned 16-bit integer to reduce memory requirement, saved as x10 more to preserve 0.1 resolution
  sint16 weight;  // 메모리 절약을 위해 short 사용 // Weight values, short unsigned 16-bit integer to reduce memory requirement, saved as x10 more to preserve 0.1 resolution
  sint16 t1;
} record_type;

record_type sensor_data[table_size + 1]; // 위의 테이블 사이즈만큼의 배열을 선언함 // Define the data array


////// 모드버스 TCP-----
bool WiFiConnected = false; // wifi 연결상태를 점검하기 위한 변수
esp32ModbusTCP sunnyboy(3, {192, 168, 10, 123}, 502);  // 오브젝트 생성  // Master PLC
//esp32ModbusTCP sunnyboy(14, {192, 168, 1, 250}, 502);  // 오브젝트 생성  // GA-100
enum smaType {
  ENUM,   // enumeration
  UFIX0,  // unsigned, no decimals
  SFIX0,  // signed, no decimals
};
struct smaData {
  const char* name; // 가져올 데이터의 이름 지정
  uint16_t address; // 가져올 데이터의 시작번지 지정 (slave 인 PLC에서는 +1 번지값임)
  uint16_t length;
  smaType type;   // 위의 열거형 중 하나
  uint16_t packetId;
};
smaData smaRegisters[] = {      // 배열로 읽을 레지스터를 정의, 여러 줄 쓰면 그만큼 늘어남
  // length는 2word 씩 읽어야 정상적으로 읽힘 <-- && 아래쪽 읽어올 데이터 타입을 바꾼 상태임
  // 레지스터 이름, address, word 수, 열거형, 패킷 ID

  //  "Pannel Temp", 0, 2, ENUM, 0,  // PLC D2101  // ENUM -> type -> ENUM -> 0
  //  "Write Test", 8, 2, ENUM, 0   // PLC D2109

  // Master PLC
  //  "D5100", 0, 2, ENUM, 0,  // PLC D5101   // PLC 의 쓰기 주소(readHoldingRegisters 사용시)
  //  "D5101", 1, 2, ENUM, 0   // PLC D5102

  //  "D5120", 20, 2, ENUM, 0,  // PLC D5120   // PLC 의 쓰기 주소(readHoldingRegisters 사용시)
  //  "D5123", 23, 2, ENUM, 0   // PLC D5123

  //  "D5021", 20, 2, ENUM, 0,  // PLC D5021   // PLC 의 읽기주소(readInputRegisters 사용시)
  //  "D5007", 6, 2, ENUM, 0   // PLC D5007

  //  "D4501", 0, 2, ENUM, 0,   // ch1  // PLC 의 읽기주소(readInputRegisters 사용시)
  //  "D4502", 1, 2, ENUM, 0,   // ch2
  //  "D4504", 3, 2, ENUM, 0,   // dT(ch1-ch2)
  //  "D4505", 4, 2, ENUM, 0,   // Average dT
  //  "D4506", 5, 2, ENUM, 0,   // operation time
  //  "D4507", 6, 2, ENUM, 0,   // Average dT slope
  //  "D4508", 7, 2, ENUM, 0,    // moisture intensity
  //  "D4550", 49, 2, ENUM, 0  // Mashine state

  "D5021", 20, 2, ENUM, 0,  // PLC D5021(T-oil 온도)   // Master PLC 의 읽기주소(readInputRegisters 사용시)
  "D5007", 6, 2, ENUM, 0,   // PLC D5007(중량)
  "D5043", 42, 2, ENUM, 0,   // PLC D5043(machine state)
  "D5024", 23, 2, ENUM, 0,   // PLC D5024(safety 온도)  
  "D5038", 37, 2, ENUM, 0,   // PLC D5038(boiler out T-oil temp-PV 온도)  
  "D5009", 8, 2, ENUM, 0,   // PLC D5009(저장조 중량)  
  "D5031", 30, 2, ENUM, 0,   // PLC D5031(수분강도)  
  "D5047", 46, 2, ENUM, 0   // PLC D5047(보일러 운전상태)  
  
};
uint8_t numberSmaRegisters = sizeof(smaRegisters) / sizeof(smaRegisters[0]);    // 읽어올 레지스터의 갯수 =  전체 배열의 크기 나누기 첫줄의 크기
uint8_t currentSmaRegister = 0;   // 이건 용처를 못찾았음
uint16_t modValue[8]; // 모드버스로 통신처리된 값을 담을 변수를 배열로 선언(단, 위의 smaRegisters[] 에서 생성하는 레지스터의 갯수에 맞게 상수로 설정)
//int16_t modValue[4]; // 모드버스로 통신처리된 값을 담을 변수를 배열로 선언(단, 위의 smaRegisters[] 에서 생성하는 레지스터의 갯수에 맞게 상수로 설정)
////// 모드버스 TCP-----


///////////////////////////////////////////
///////////////////////////////////////////
void setup() {

  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  pinMode(HardResetPin, OUTPUT);
//  digitalWrite(HardResetPin, HardRsetState);
  Serial.print("------------------------- HardRestPin : ");
  Serial.println(HardRsetState);


  ////// 모드버스 TCP-----
  //  WiFi.disconnect(true);  // delete old config

  sunnyboy.onData([](uint16_t packet, uint8_t slave, esp32Modbus::FunctionCode fc , uint8_t* data , uint16_t len) {
    for (uint8_t i = 0; i < numberSmaRegisters; ++i) {
      if (smaRegisters[i].packetId == packet) {
        smaRegisters[i].packetId = 0;
        switch (smaRegisters[i].type) {
          case ENUM:    // 내용과 break 가 없으므로 아래 두 경우로 진입하게 됨
          case UFIX0:
            {
              // uint16_t value = 0;   // 원래는 uint32_t 였음.
              int16_t value = 0;   // 원래는 uint16_t 였음.  // 음수값을 표현하려고
              //            value = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);   // 원문인데 이부분도 수정할 필요는 있어보임
              value = (data[2] << 8) | (data[3]);   // 절반으로 줄여도 변함없이 65536 까지 됨   <----- 더 연구 필요
              //              Serial.printf("%s: %u\n", smaRegisters[i].name, value);
              Serial.printf("%s: %d", smaRegisters[i].name, value);     // 줄바꿈 없이, 음수값도 가능하게
              modValue[i] = value;
              Serial.print("  -----  modValue["); Serial.print(i); Serial.print("] : ");   Serial.println(modValue[i]);
              break;
            }
          case SFIX0:
            {
              int32_t value = 0;
              value = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
              Serial.printf("%s: %i\n", smaRegisters[i].name, value);
              break;
            }
        }
        return;
      }
    }
  });
  sunnyboy.onError([](uint16_t packet, esp32Modbus::Error e) {
    Serial.printf("Error packet %u: %02x\n", packet, e);
  });
  delay(1000);
  ////// 모드버스 TCP-----


  StartSPIFFS();
  delay(500);
  //SPIFFS.remove("/"+DataFile); // 필요시 버튼으로 이부분을 활성화 시키자  // In case file in ESP32 version gets corrupted, it happens a lot!!
  //Serial.println(F("------------------ DataFile removed"));
  //delay(500);
  StartWiFi(ssid, password);
  StartTime();
  Serial.println(F("WiFi connected.."));
  server.begin(); Serial.println(F("Webserver started...")); // Start the webserver
  Serial.println("Use this URL to connect: http://" + WiFi.localIP().toString() + "/"); // Print the IP address


  ////// 모드버스 TCP-----
  WiFiConnected = true;
  ////// 모드버스 TCP-----


///////////  OTA --------------
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);
  // Hostname defaults to esp3232-[MAC]
   ArduinoOTA.setHostname("Master(esp32)");
  // No authentication by default
  // ArduinoOTA.setPassword("admin");
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
///////////  OTA --------------


  //----------- 해당 URL에 진입하면 두번째 인자인 함수가 실행됨 ------------------------------
  server.on("/",      display_temp_and_Weight); // The client connected with no arguments e.g. http:192.160.0.40/
  server.on("/TH",    display_temp_and_Weight);
  server.on("/TD",    display_temp_and_dewpoint);
  server.on("/DV",    display_dial);
  server.on("/AS",    auto_scale);
  server.on("/AU",    auto_update);
  server.on("/Setup", systemSetup);
  server.on("/Help",  help);
  server.on("/LgTU",  logtime_up);
  server.on("/LgTD",  logtime_down);
  server.on("/LogV",  LOG_view);
  server.on("/LogE",  LOG_erase);
  server.on("/LogS",  LOG_stats);
  server.on("/SoftReset",  SoftReset);  // 추가
  server.on("/SpiffRemove", SpiffRemove);  // 추가
  server.begin();
  Serial.println(F("Webserver started...")); // Start the webserver // F 는 프로그램 메모리인 Flash 에 저장하여 데이터 메모리인 SRAM 을 절약하기 위해서.

  index_ptr    = 0;     // The array pointer that varies from 0 to table_size
  log_count    = 0;     // Keeps a count of readings taken
  AScale       = false; // Google charts can AScale axis, this switches the function on/off
  max_temp     = 200;    // Maximum displayed temperature as default
  min_temp     = -10;   // Minimum displayed temperature as default
  auto_smooth  = false; // If true, transitions of more than 10% between readings are smoothed out, so a reading followed by another that is 10% higher or lower is averaged
  AUpdate      = true;  // Used to prevent a command from continually auto-updating, for example increase temp-scale would increase every 30-secs if not prevented from doing so.
  lastcall     = "temp_weight";      // To determine what requested the AScale change
  log_interval = log_time_unit * 10; //  inter-log time interval, default is 5-minutes between readings, 10=15secs 40=1min 200=5mins 400=10mins 2400=1hr
  timer_cnt    = log_interval + 1; // To trigger first table update, essential
  update_log_time();           // Update the log_time
  log_delete_approved = false; // Used to prevent accidental deletion of card contents, requires two approvals
  reset_array();               // Clear storage array before use
  prefill_array();             // Load old data from FS back into readings array
  Serial.print("system_get_free_heap_size --------  ");
  Serial.println(system_get_free_heap_size()); // diagnostic print to check for available RAM

  bot.sendMessage(CHAT_ID, "400WTE System Bot started up", "");   // telegram start
  delay(5000);
  bot.sendMessage(CHAT_ID, WiFi.localIP().toString(), "");  

}   // end of setup



void loop() {

  //////// OTA --------
  ArduinoOTA.handle();
  
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }
    // set the LED with the ledState of the variable:
    digitalWrite(ledPin, ledState);
  }
  //////// OTA --------


  //////// telegram --------
  String str = "400WTE machine TEMP : " + (String)((float)(modValue[0]/10));
  String str1 = "400WTE machine WEIGHT : " + (String)modValue[1];
  String str2= "400WTE machine STATE : " + (String)modValue[2];
    
  if (modValue[2] != compare) { // modValue[2] 값이 변하면
    bot.sendMessage(CHAT_ID, str, ""); delay(5000);
    bot.sendMessage(CHAT_ID, str1, ""); delay(5000);
    bot.sendMessage(CHAT_ID, str2, "");
    Serial.print(str);    Serial.println("  ---------   massage sent!");
    Serial.print(str1);   Serial.println("  ---------   massage sent!");
    Serial.print(str2);   Serial.println("  ---------   massage sent!");
    compare = modValue[2];
  }
  //////// telegram --------


  ////// 모드버스 TCP-----
  static uint32_t lastMillis = 0;
  if ((millis() - lastMillis > 5000 && WiFiConnected)) {  // 5초 1번씩 읽어옴.
    lastMillis = millis();
    Serial.print("------------ reading registers\n");
    for (uint8_t i = 0; i < numberSmaRegisters; ++i) {
      //      uint16_t packetId = sunnyboy.readHoldingRegisters(smaRegisters[i].address, smaRegisters[i].length); // 쓰기영역
      uint16_t packetId = sunnyboy.readInputRegisters(smaRegisters[i].address, smaRegisters[i].length);     // 읽기영역
      if (packetId > 0) {
        smaRegisters[i].packetId = packetId;
      } else {
        Serial.print("reading error\n");
      }
    }
  }
  ////// 모드버스 TCP-----


  server.handleClient();    // 클라이언트의 요청이 있는 경우 클라이언트와 연결을 생성하고 클라이언트의 요청을 처리함.

  //    temp = dht.readTemperature() * 10;
  temp = modValue[0];
  //    weight = dht.readWeight() * 10;
  weight = modValue[1] * 10;

  // 변수 추가 테스트용
  t1 = modValue[3];

  if (isnan(weight) || isnan(temp)) {
    //    Serial.println("Failed to read from DHT sensor!");
    Serial.println("Failed to read from Register!");
    return;
  }

  time_t now = time(nullptr);
  time_now = String(ctime(&now)).substring(0, 24); // Remove unwanted characters
  // ctime() 함수는 time이 가리키는 시간 값을 문자 스트링 양식의 로컬 시간으로 변환합니다.
  // time 값은 일반적으로 time() 함수를 호출하여 얻습니다.
  // ctime()이 생성하는 스트링 결과는 정확히 26자를 포함하며 형식은 "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n" 이다

  if (time_now != "Thu Jan 01 00:00:00 1970" and timer_cnt >= log_interval) { // If time is not yet set, returns 'Thu Jan 01 00:00:00 1970') so wait.
    timer_cnt = 0;  // log_interval values are 10=15secs 40=1min 200=5mins 400=10mins 2400=1hr
    log_count += 1; // Increase logging event count
    sensor_data[index_ptr].lcnt  = log_count;  // Record current log number, time, temp and Weight readings
    sensor_data[index_ptr].temp  = temp;
    sensor_data[index_ptr].weight  = weight;
    sensor_data[index_ptr].t1  = t1;
    sensor_data[index_ptr].ltime = calcDateTime(time(&now)); // time stamp of reading 'dd/mm/yy hh:mm:ss'
#ifdef ESP8266
    File datafile = SPIFFS.open(DataFile, "a+");
#else
    File datafile = SPIFFS.open("/" + DataFile, FILE_APPEND);
#endif
    time_t now = time(nullptr);
    if (datafile == true) { // if the file is available, write to it
      datafile.println(((log_count < 10) ? "0" : "") + String(log_count) + char(9) + String(temp / 10, 2) + char(9) + String(weight / 10, 2) + char(9) + String(t1 / 10, 2) + char(9) + String(modValue[4]) + char(9) + String(modValue[6]) + char(9) + String(modValue[7]) + char(9) + calcDateTime(time(&now)) + "."); // TAB delimited
      Serial.print(((log_count < 10) ? "0" : "") + String(log_count) + " New Record Added");
      Serial.print("---- "); Serial.println(calcDateTime(time(&now)));
    }
    datafile.close();
    index_ptr += 1; // Increment data record pointer
    if (index_ptr > table_size) { // if number of readings exceeds max_readings (e.g. 100) shift all data to the left and effectively scroll the display left
      index_ptr = table_size;
      for (int i = 0; i < table_size; i++) { // If data table is full, scroll all readings to the left in graphical terms, then add new reading to the end
        sensor_data[i].lcnt  = sensor_data[i + 1].lcnt;
        sensor_data[i].temp  = sensor_data[i + 1].temp;
        sensor_data[i].weight  = sensor_data[i + 1].weight;
        sensor_data[i].t1  = sensor_data[i + 1].t1;        
        sensor_data[i].ltime = sensor_data[i + 1].ltime;
      }
      sensor_data[table_size].lcnt  = log_count;
      sensor_data[table_size].temp  = temp;
      sensor_data[table_size].weight  = weight;
      sensor_data[table_size].t1  = t1;      
      sensor_data[table_size].ltime = calcDateTime(time(&now));   // calcDateTime(UNIX 타임) -> 우리나라 시간으로 계산하는 함수
    }

    mysql_db(); // 데이터 베이스 삽입 새로 추가 // 2020.01.20
    
  }

  timer_cnt += 1; // Readings set by value of log_interval each 40 = 1min
  //delay(500);     // Delay before next check for a client, adjust for 1-sec repeat interval. Temperature readings take some time to complete.
  delay(1000);     // Delay before next check for a client, adjust for 1-sec repeat interval. Temperature readings take some time to complete.
  //Serial.println(millis()); // Used to measure inter-sample timing

}   // end of loop

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

///////////////// mysql_DB //////////////////
void mysql_db()
{
    if (client.connect(mysql_server, 80)) {  // Http 요청 헤더  //  일단 80 포트에서만 성공함.
     client.print("GET /test1.php?");  // 여기서
     client.print("temp=");      
     client.println(temp);   // 여기까지는 PHP에서 받기로한 데이터들이다
     client.println();
     client.println( "HTTP/1.1");
     client.println( "Host: 54.180.127.63" ); //요청을 보낼 서버의 주소
     client.println( "Content-Type: application/x-www-form-urlencoded" );     
     client.println( "Connection: close" );
     client.println();
     client.println();    
     readString="";    
     client.stop();   
     Serial.println("DB sent OK");
//     delay(1000);  // 그냥 여유
    }
}  // end of mysql_db 
///////////////// mysql_DB //////////////////


void prefill_array() { // After power-down or restart and if the FS has readings, load them back in
#ifdef ESP8266
  File datafile = SPIFFS.open(DataFile, "r");
#else
  File datafile = SPIFFS.open("/" + DataFile, FILE_READ);
#endif
  while (datafile.available()) { // if the file is available, read from it
    int read_ahead = datafile.parseInt(); // Sometimes at the end of file, NULL data is returned, this tests for that
    if (read_ahead != 0) { // Probably wasn't null data to use it, but first data element could have been zero and there is never a record 0!
      sensor_data[index_ptr].lcnt  = read_ahead ;
      sensor_data[index_ptr].temp  = datafile.parseFloat() * 10;
      sensor_data[index_ptr].weight  = datafile.parseFloat() * 10;
      sensor_data[index_ptr].t1  = datafile.parseFloat() * 10;      
      sensor_data[index_ptr].ltime = datafile.readStringUntil('.');   // 한 줄의 끝을 '.'으로 기록하였으므로
      sensor_data[index_ptr].ltime.trim();    // 해당문자의 좌우에 있는 공백을 제거해주는 함수
      index_ptr += 1;
      log_count += 1;
    }
    if (index_ptr > table_size) {
      for (int i = 0; i < table_size; i++) {
        sensor_data[i].lcnt  = sensor_data[i + 1].lcnt;
        sensor_data[i].temp  = sensor_data[i + 1].temp;
        sensor_data[i].weight  = sensor_data[i + 1].weight;
        sensor_data[i].t1  = sensor_data[i + 1].t1;        
        sensor_data[i].ltime = sensor_data[i + 1].ltime;
        sensor_data[i].ltime.trim();
      }
      index_ptr = table_size;
    }
  }
  datafile.close();
  // Diagnostic print to check if data is being recovered from SPIFFS correctly
  for (int i = 0; i <= index_ptr; i++) {
    Serial.println(((i < 10) ? "0" : "") + String(sensor_data[i].lcnt) + " " + String(sensor_data[i].temp) + " " + String(sensor_data[i].weight) + " " + String(sensor_data[i].t1) + " " + String(sensor_data[i].ltime));
  }
  datafile.close();
  if (auto_smooth) { // During restarts there can be a discontinuity in readings, giving a spike in the graph, this smooths that out, off by default though
    // At this point the array holds data from the FS, but sometimes during outage and resume, reading discontinuity occurs, so try to correct those.
    float last_temp, last_weight, last_t1;;
    for (int i = 1; i < table_size; i++) {
      last_temp = sensor_data[i].temp;
      last_weight = sensor_data[i].weight;
      last_t1 = sensor_data[i].t1;      
      // Correct next reading if it is more than 10% different from last values
      if ((sensor_data[i + 1].temp > (last_temp * 1.1)) || (sensor_data[i + 1].temp < (last_temp * 1.1))) sensor_data[i + 1].temp = (sensor_data[i + 1].temp + last_temp) / 2; // +/-1% different then use last value
      if ((sensor_data[i + 1].weight > (last_weight * 1.1)) || (sensor_data[i + 1].weight < (last_weight * 1.1))) sensor_data[i + 1].weight = (sensor_data[i + 1].weight + last_weight) / 2;
      if ((sensor_data[i + 1].t1 > (last_t1 * 1.1)) || (sensor_data[i + 1].t1 < (last_t1 * 1.1))) sensor_data[i + 1].t1 = (sensor_data[i + 1].t1 + last_t1) / 2;
    }
  }
  Serial.println("Restored data from SPIFFS");
}   // end of prefill_array()
////////////////////////////////////////////////////////


void display_temp_and_Weight() { // Processes a clients request for a graph of the data
  // See google charts api for more details. To load the APIs, include the following script in the header of your web page.
  // <script type="text/javascript" src="https://www.google.com/jsapi"></script>
  // To autoload APIs manually, you need to specify the list of APIs to load in the initial <script> tag, rather than in a separate google.load call for each API. For instance, the object declaration to auto-load version 1.0 of the Search API (English language) and the local search element, would look like: {
  // This would be compressed to: {"modules":[{"name":"search","version":"1.0","language":"en"},{"name":"elements","version":"1.0","packages":["
  // See https://developers.google.com/chart/interactive/docs/basic_load_libs

  log_delete_approved = false; // Prevent accidental SD-Card deletion
  webpage = ""; // don't delete this command, it ensures the server works reliably!
  append_page_header();

  // https://developers.google.com/loader/ // https://developers.google.com/chart/interactive/docs/basic_load_libs
  // https://developers.google.com/chart/interactive/docs/basic_preparing_data
  // https://developers.google.com/chart/interactive/docs/reference#google.visualization.arraytodatatable and See appendix-A
  // data format is: [field-name,field-name,field-name] then [data,data,data], e.g. [12, 20.5, 70.3]

  //webpage += F("<script type=\"text/javascript\" src=\"https://www.google.com/jsapi?autoload={'modules':[{'name':'visualization','version':'1','packages':['corechart']}]}/\"></script>");
  //webpage += F("<script type=\"text/javascript\"> google.setOnLoadCallback(drawChart);");

  webpage += F("<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script>");
  webpage += F("<script type=\"text/javascript\">google.charts.load('current', {packages: ['corechart']});");
  webpage += F("google.charts.setOnLoadCallback(drawChart);");

  webpage += F("function drawChart() {");
  webpage += F("var data = google.visualization.arrayToDataTable([");
  webpage += F("['Reading','Temperature','Weight', 'T1'],\n");
  // webpage += F("['Reading','Temperature','Weight','t1'],\n");  
  // for (int i = 0; i < index_ptr; i = i + 2) { // Can't display all data points!!!
  for (int i = 0; i < index_ptr; i = i + 2) { // Can't display all data points!!!  
    //webpage += "['" + String(i) + "'," + String(sensor_data[i].temp / 10.00F, 1) + "," + String(sensor_data[i].weight / 10.00F, 2) + "," + String(sensor_data[i].t1 / 10.00F, 2) + "],"; // 원래 : sensor_data[i].weight/1000.00F
    webpage += "['" + String(sensor_data[i].ltime) + "'," + String(sensor_data[i].temp / 10.00F, 1) + "," + String(sensor_data[i].weight / 10.00F, 2) + "," + String(sensor_data[i].t1 / 10.00F, 2) + "],"; // 원래 : sensor_data[i].weight/1000.00F
  }
  webpage += "]);\n";
  //-----------------------------------
  webpage += F("var options = {");
  webpage += F("title:'Temperature & Weight & T1',titleTextStyle:{fontName:'Arial', fontSize:20, color: 'Maroon'},");
  webpage += F("legend:{position:'bottom'},colors:['red','blue','green'],backgroundColor:'#F3F3F3',chartArea:{width:'85%',height:'55%'},");
  webpage += F("hAxis:{titleTextStyle:{color:'Purple',bold:true,fontSize:16},gridlines:{color:'#333'},showTextEvery:60,title:'");
  webpage += log_time + "'},"; 
  // Uncomment next line to display x-axis in time units, but Google Charts can't cope with much data, may be just a few readings!
  //webpage += "minorGridlines:{fontSize:8,format:'d/M/YY',units:{hours:{format:['hh:mm a','ha']},minutes:{format:['HH:mm a Z', ':mm']}}},";
  //minorGridlines:{units:{hours:{format:['hh:mm a','ha']},minutes:{format:['HH:mm a Z', ':mm']}}  to display  x-axis in count units
  webpage += F("vAxes:");
  if (AScale) {
    webpage += F("{0:{viewWindowMode:'explicit',gridlines:{color:'black'}, title:'Temp Deg-C',format:'##.##'},");
    webpage += F(" 1:{gridlines:{color:'transparent'},viewWindow:{min:0,max:400},title:'Weight kg',format:'##kg'},},");
  }
  else {
    webpage += F("{0:{viewWindowMode:'explicit',gridlines:{color:'black'},title:'Temp Deg-C',format:'##.##',viewWindow:{min:");
    webpage += String(min_temp) + ",max:" + String(max_temp) + "},},";
    webpage += F(" 1:{gridlines:{color:'transparent'},viewWindow:{min:0,max:400},title:'Weight kg',format:'##kg'},},");
  }
  webpage += F("series:{0:{targetAxisIndex:0},1:{targetAxisIndex:1},curveType:'none'},};");
  webpage += F("var chart = new google.visualization.LineChart(document.getElementById('line_chart'));chart.draw(data, options);");
  webpage += F("}");
  webpage += F("</script>");
  webpage += F("<div id='line_chart' style='width:1020px; height:500px'></div>");
  append_page_footer();
  server.send(200, "text/html", webpage);
  webpage = "";
  lastcall = "temp_weight";
}


void display_temp_and_dewpoint() { // Processes a clients request for a graph of the data
  float dew_point;
  // See google charts api for more details. To load the APIs, include the following script in the header of your web page.
  // <script type="text/javascript" src="https://www.google.com/jsapi"></script>
  // To autoload APIs manually, you need to specify the list of APIs to load in the initial <script> tag, rather than in a separate google.load call for each API.
  // For instance, the object declaration to auto-load version 1.0 of the Search API (English language) and the local search element, would look like: {
  // This would be compressed to: {"modules":[{"name":"search","version":"1.0","language":"en"},{"name":"elements","version":"1.0","packages":["
  // See https://developers.google.com/chart/interactive/docs/basic_load_libs
  log_delete_approved = false; // Prevent accidental file deletion
  webpage = ""; // don't delete this, it ensures the server works reliably!
  append_page_header();
  // https://developers.google.com/loader/ // https://developers.google.com/chart/interactive/docs/basic_load_libs
  // https://developers.google.com/chart/interactive/docs/basic_preparing_data
  // https://developers.google.com/chart/interactive/docs/reference#google.visualization.arraytodatatable and See appendix-A
  // data format is: [field-name,field-name,field-name] then [data,data,data], e.g. [12, 20.5, 70.3]
  webpage += F("<script type=\"text/javascript\" src=\"https://www.google.com/jsapi?autoload={'modules':[{'name':'visualization','version':'1','packages':['corechart']}]}/\"></script>");
  webpage += F("<script type=\"text/javascript\"> google.setOnLoadCallback(drawChart);");
  webpage += F("function drawChart() {");
  webpage += F("var data = google.visualization.arrayToDataTable(");
  webpage += F("[['Reading','Temperature','Dew Point'],");
  for (int i = 0; i < index_ptr; i = i + 2) { // Can't display all data points!!!
    if (isnan(Calc_DewPoint(sensor_data[i].temp / 10, sensor_data[i].weight / 10))) dew_point = 0; else dew_point = Calc_DewPoint(sensor_data[i].temp / 10, sensor_data[i].weight / 10);
    webpage += "['" + String(i) + "'," + String(float(sensor_data[i].temp) / 10, 1) + "," + String(dew_point, 1) + "],";
  }
  webpage += "]);";
  //-----------------------------------
  webpage += F("var options = {");
  webpage += F("title:'Temperature & Dew Point',titleTextStyle:{fontName:'Arial', fontSize:20, color: 'Maroon'},");
  webpage += F("legend:{position:'bottom'},colors:['red','orange'],backgroundColor:'#F3F3F3',chartArea:{width:'85%',height:'55%'},");
  webpage += "hAxis:{gridlines:{color:'black'},titleTextStyle:{color:'Purple',bold:true,fontSize:16},showTextEvery:5,title:'" + log_time + "'},";
  //webpage += "minorGridlines:{fontSize:8,format:'d/M/YY',units:{hours:{format:['hh:mm a','ha']},minutes:{format:['HH:mm a Z', ':mm']}}},";//  to display  x-axis in time units
  //minorGridlines:{units:{hours:{format:['hh:mm:ss a','ha']},minutes:{format:['HH:mm a Z', ':mm']}}  to display  x-axis in time units
  webpage += F("vAxes:");
  if (AScale)
    webpage += F("{0:{viewWindowMode:'explicit',gridlines:{color:'black'}, title:'Temp Deg-C',format:'##.##'},},");
  else
    webpage += "{0:{viewWindowMode:'explicit',viewWindow:{min:" + String(min_temp) + ",max:" + String(max_temp) + "},gridlines:{color:'black'},title:'Temp Deg-C',format:'##.##'},},";
  webpage += F("series:{0:{targetAxisIndex:0},1:{targetAxisIndex:0},},curveType:'none'};");
  webpage += F("var chart = new google.visualization.LineChart(document.getElementById('line_chart'));chart.draw(data, options);");
  webpage += F("}");
  webpage += F("</script>");
  webpage += F("<div id='line_chart' style='width:1020px; height:500px'></div>");
  append_page_footer();
  server.send(200, "text/html", webpage);
  webpage  = "";
  lastcall = "temp_dewp";
}   // end of display_temp_and_dewpoint()
////////////////////////////////////////////////////////


void display_dial () { // Processes a clients request for a dial-view of the data
  log_delete_approved = false; // PRevent accidental SD-Card deletion
  webpage = ""; // don't delete this command, it ensures the server works reliably!
  append_page_header();
  webpage += F("<script type=\"text/javascript\" src=\"https://www.google.com/jsapi?autoload={'modules':[{'name':'visualization','version':'1','packages':['gauge']}]}\"></script>");
  webpage += F("<script type=\"text/javascript\">");
  webpage += "var temp=" + String(temp / 10, 2) + ",weight=" + String(weight, 1) + ";";
  // https://developers.google.com/chart/interactive/docs/gallery/gauge
  webpage += F("google.load('visualization','1',{packages: ['gauge']});");
  webpage += F("google.setOnLoadCallback(drawgaugetemp);");
  webpage += F("google.setOnLoadCallback(drawgaugeweight);");
  webpage += F("var gaugetempOptions={min:-20,max:200,yellowFrom:-20,yellowTo:0,greenFrom:0,greenTo:100,redFrom:100,redTo:200,minorTicks:10,majorTicks:['-20','0','100','200']};");
  webpage += F("var gaugeweightOptions={min:0,max:600,yellowFrom:0,yellowTo:200,greenFrom:200,greenTo:400,redFrom:400,redTo:600,minorTicks:10,majorTicks:['0','100','200','300','400','500', '600']};");
  webpage += F("var gaugetemp,gaugeweight;");
  webpage += F("function drawgaugetemp() {gaugetempData = new google.visualization.DataTable();");
  webpage += F("gaugetempData.addColumn('number','deg-C');"); // 176 is Deg-symbol, there are problems displaying the deg-symbol in google charts
  webpage += F("gaugetempData.addRows(1);gaugetempData.setCell(0,0,temp);");
  webpage += F("gaugetemp = new google.visualization.Gauge(document.getElementById('gaugetemp_div'));");
  webpage += F("gaugetemp.draw(gaugetempData, gaugetempOptions);}");
  webpage += F("function drawgaugeweight() {gaugeweightData = new google.visualization.DataTable();gaugeweightData.addColumn('number','kg');");
  webpage += F("gaugeweightData.addRows(1);gaugeweightData.setCell(0,0,weight/10);");
  webpage += F("gaugeweight = new google.visualization.Gauge(document.getElementById('gaugeweight_div'));");
  webpage += F("gaugeweight.draw(gaugeweightData,gaugeweightOptions);};");
  webpage += F("</script>");
  webpage += F("<div id='gaugetemp_div' style='width:300px;height:300px;display:block;margin:0 auto;margin-left:auto;margin-right:auto;'></div>");
  webpage += F("<div id='gaugeweight_div' style='width:300px;height:300px;display:block;margin:0 auto;margin-left:auto;margin-right:auto;'></div>");
  append_page_footer();
  server.send(200, "text/html", webpage);
  webpage = "";
  lastcall = "dial";
}

float Calc_DewPoint(float temp, float weight) {
  return 243.04 * (log(weight / 100.00) + ((17.625 * temp) / (243.04 + temp))) / (17.625 - log(weight / 100.00) - ((17.625 * temp) / (243.04 + temp)));
}

void reset_array() {
  for (int i = 0; i <= table_size; i++) {
    sensor_data[i].lcnt  = 0;
    sensor_data[i].temp  = 0;
    sensor_data[i].weight  = 0;
    sensor_data[i].t1  = 0;    
    sensor_data[i].ltime = "";
  }
}

// After the data has been displayed, select and copy it, then open Excel and Paste-Special and choose Text, then select, then insert graph to view
void LOG_view() {
#ifdef ESP8266
  File datafile = SPIFFS.open(DataFile, "r"); // Now read data from SPIFFS
#else
  File datafile = SPIFFS.open("/" + DataFile, FILE_READ); // Now read data from FS
#endif
  if (datafile) {
    if (datafile.available()) { // If data is available and present
      String dataType = "application/octet-stream";
      if (server.streamFile(datafile, dataType) != datafile.size()) {
        Serial.print(F("Sent less data than expected!"));
      }
    }
  }
  datafile.close(); // close the file:
  webpage = "";
}

void LOG_erase() { // Erase the datalog file
  webpage = ""; // don't delete this command, it ensures the server works reliably!
  append_page_header();
  if (AUpdate) webpage += "<meta http-equiv='refresh' content='30'>"; // 30-sec refresh time and test is needed to stop auto updates repeating some commands
  if (log_delete_approved) {
#ifdef ESP8266
    if (SPIFFS.exists(DataFile)) {
      SPIFFS.remove(DataFile);
      Serial.println(F("File deleted successfully"));
    }
#else
    if (SPIFFS.exists("/" + DataFile)) {
      SPIFFS.remove("/" + DataFile);
      Serial.println(F("File deleted successfully"));
    }
#endif
    webpage += "<h3 style=\"color:orange;font-size:24px\">Log file '" + DataFile + "' has been erased</h3>";
    log_count = 0;
    index_ptr = 0;
    timer_cnt = 2000; // To trigger first table update, essential
    log_delete_approved = false; // Re-enable FS deletion
  }
  else {
    log_delete_approved = true;
    webpage += "<h3 style=\"color:orange;font-size:24px\">Log file erasing is now enabled, repeat this option to erase the log. Graph or Dial Views disable erasing again</h3>";
  }
  append_page_footer();
  server.send(200, "text/html", webpage);
  webpage = "";
}

void LOG_stats() { // Display file size of the datalog file
  webpage = ""; // don't delete this command, it ensures the server works reliably!
  append_page_header();
#ifdef ESP8266
  File datafile = SPIFFS.open(DataFile, "r"); // Now read data from SPIFFS
#else
  File datafile = SPIFFS.open("/" + DataFile, FILE_READ); // Now read data from FS
#endif
  webpage += "<h3 style=\"color:orange;font-size:24px\">Data Log file size = " + String(datafile.size()) + "-Bytes</h3>";
  webpage += "<h3 style=\"color:orange;font-size:24px\">Number of readings = " + String(log_count) + "</h3>";
  datafile.close();
  append_page_footer();
  server.send(200, "text/html", webpage);
  webpage = "";
}

void auto_scale () { // Google Charts can auto-scale graph axis, this turns it on/off
  if (AScale) AScale = false; else AScale = true;
  if (lastcall == "temp_weight") display_temp_and_Weight();
  if (lastcall == "temp_dewp") display_temp_and_dewpoint();
  if (lastcall == "dial")      display_dial();
}

void auto_update () { // Google Charts can auto-scale graph axis, this turns it on/off
  if (AUpdate) AUpdate = false; else AUpdate = true;
  if (lastcall == "temp_weight") display_temp_and_Weight();
  if (lastcall == "temp_dewp") display_temp_and_dewpoint();
  if (lastcall == "dial")      display_dial();
}

void logtime_down () {  // Timer_cnt delay values 1=15secs 4=1min 20=5mins 40=10mins 240=1hr, increase the values with this function
  log_interval -= log_time_unit;
  if (log_interval < log_time_unit) log_interval = log_time_unit;
  update_log_time();
  if (lastcall == "temp_weight") display_temp_and_Weight();
  if (lastcall == "temp_dewp") display_temp_and_dewpoint();
  if (lastcall == "dial")      display_dial();
}

void logtime_up () {  // Timer_cnt delay values 1=15secs 4=1min 20=5mins 40=10mins 240=1hr, increase the values with this function
  log_interval += log_time_unit;
  update_log_time();
  if (lastcall == "temp_weight") display_temp_and_Weight();
  if (lastcall == "temp_dewp") display_temp_and_dewpoint();
  if (lastcall == "dial")      display_dial();
}

void update_log_time() {
  float log_hrs;
  log_hrs = table_size * log_interval / time_reference;
  log_hrs = log_hrs / 60.0; // Should not be needed, but compiler can't calculate the result in-line!
  float log_mins = (log_hrs - int(log_hrs)) * 60;
  log_time = String(int(log_hrs)) + ":" + ((log_mins < 10) ? "0" + String(int(log_mins)) : String(int(log_mins))) + " Hrs  of readings, (" + String(log_interval / 1) + ")secs per reading"; // 기존 /2 없음
  //log_time += ", Free-mem:("+String(system_get_free_heap_size())+")";
}

void systemSetup() {
  webpage = ""; // don't delete this command, it ensures the server works reliably!
  append_page_header();
  String IPaddress = WiFi.localIP().toString();
  webpage += F("<h3 style=\"color:orange;font-size:24px\">System Setup, Enter Values Required</h3>");
  webpage += F("<meta http-equiv='refresh' content='200'/ URL=http://");
  webpage += IPaddress + "/Setup>";
  webpage += "<form action='http://" + IPaddress + "/Setup' method='POST'>";
  webpage += "Maximum Temperature on Graph axis (currently = " + String(max_temp) + "&deg;C<br>";
  webpage += F("<input type='text' name='max_temp_in' value='30'><br>");
  webpage += "Minimum Temperature on Graph axis (currently = " + String(min_temp) + "&deg;C<br>";
  webpage += F("<input type='text' name='min_temp_in' value='-10'><br>");
  webpage += "Logging Interval (currently = " + String(log_interval / 1) + "-Secs) (1=15secs)<br>";  // 기존 /2 없음
  webpage += F("<input type='text' name='log_interval_in' value=''><br>");
  webpage += "Auto-scale Graph (currently = " + String(AScale ? "ON" : "OFF") + "<br>";
  webpage += F("<input type='text' name='auto_scale' value=''><br>");
  webpage += "Auto-update Graph (currently = " + String(AUpdate ? "ON" : "OFF") + "<br>";
  webpage += F("<input type='text' name='auto_update' value=''><br>");
  webpage += F("<input type='submit' value='Enter'><br><br>");
  webpage += F("</form>");
  append_page_footer();
  server.send(200, "text/html", webpage); // Send a response to the client asking for input
  if (server.args() > 0 ) { // Arguments were received
    for ( uint8_t i = 0; i < server.args(); i++ ) {
      String Argument_Name   = server.argName(i);
      String client_response = server.arg(i);
      if (Argument_Name == "max_temp_in") {
        if (client_response.toInt()) max_temp = client_response.toInt(); else max_temp = 30;
      }
      if (Argument_Name == "min_temp_in") {
        if (client_response.toInt() == 0) min_temp = 0; else min_temp = client_response.toInt();
      }
      if (Argument_Name == "log_interval_in") {
        if (client_response.toInt()) log_interval = client_response.toInt(); else log_interval = 300;
        log_interval = client_response.toInt() * log_time_unit;
      }
      if (Argument_Name == "auto_scale") {
        if (client_response == "ON" || client_response == "on") AScale = !AScale;
      }
      if (Argument_Name == "auto_update") {
        if (client_response == "ON" || client_response == "on") AUpdate = !AUpdate;
      }
    }
  }
  webpage = "";
  update_log_time();
}

void append_page_header() {   // 웹페이지 상단 폼
  webpage  = "<!DOCTYPE html><head>";
  if (AUpdate) webpage += F("<meta http-equiv='refresh' content='30' charset='UTF-8'>"); // 30-sec refresh time, test needed to prevent auto updates repeating some commands
  webpage += F("<title>");
  webpage += title;
  webpage += F("</title>");
  webpage += F("<style>ul{list-style-type:none;margin:0;padding:0;overflow:hidden;background-color:#31c1f9;font-size:12px;}"); // 원래는 14px // 하단메뉴가 가로를 넘어가면 px를 줄일 것
  webpage += F("li{float:left;}");
  webpage += F("li a{display:block;text-align:center;padding:5px 25px;text-decoration:none;}");
  webpage += F("li a:hover{background-color:#FFFFFF;}");
  webpage += F("h1{background-color:#31c1f9;}");
  webpage += F("body{width:");
  webpage += site_width;
  webpage += F("px;margin:0 auto;font-family:arial;font-size:14px;text-align:center;color:#ed6495;background-color:#F7F2Fd;}");
  webpage += F("</style></head><body><h1>");
  webpage += title;  
  webpage += version + "</h1>";

  etc_data();
}


// 기타 데이터 추가
void etc_data() {

  // webpage += F("<h3>");
  // webpage += F("Ch1 : ");                webpage += String(float(modValue[0])/10);  webpage += F("</br>");
  // webpage += F("Ch2 : ");                webpage += String(float(modValue[1])/10);  webpage += F("</br>");
  // webpage += F("dT(ch1-ch2) : ");        webpage += String(float(modValue[2])/10);  webpage += F("</br>");
  // webpage += F("Average dT : ");         webpage += String(float(modValue[3])/10);  webpage += F("</br>");
  // webpage += F("operation time : ");     webpage += String(modValue[4]);            webpage += F("</br>");
  // webpage += F("Average dT slope : ");   webpage += String(float(modValue[5])/10);  webpage += F("</br>");
  // webpage += F("moisture intensity : "); webpage += String(modValue[6]);            webpage += F("</br>");
  // webpage += F("Mashine state : ");      webpage += String(modValue[7]);            webpage += F("</h3>");

  webpage += F("<ul>");
  webpage += F("<h3 color = 'black'>");
  webpage += F("<li>T-oil Temperature : ");  webpage += String(float(modValue[0]) / 10);  webpage += F("</li></br>");
  webpage += F("<li>Weight : ");             webpage += String(modValue[1]);              webpage += F("</li></br>");
  webpage += F("<li>Mashine state : ");      webpage += String(modValue[2]);              webpage += F("</li></h3>");
  webpage += F("</ul>");
} // end of 기타 데이터

//  "D4501", 0, 2, ENUM, 0,   // ch1  // PLC 의 읽기주소(readInputRegisters 사용시)
//  "D4502", 1, 2, ENUM, 0,   // ch2
//  "D4504", 3, 2, ENUM, 0,   // dT(ch1-ch2)
//  "D4505", 4, 2, ENUM, 0,   // Average dT
//  "D4506", 5, 2, ENUM, 0,   // operation time
//  "D4507", 6, 2, ENUM, 0,   // Average dT slope
//  "D4508", 7, 2, ENUM, 0    // moisture intensity
//  "D4550", 49, 2, ENUM, 0,  // Mashine state



void append_page_footer() { // Saves repeating many lines of code for HTML page footers   // 웹 페이지 하단 폼
  webpage += F("<ul>");
  webpage += F("<li><a href='/TH'>Temp&deg;/weight</a></li>");
//  webpage += F("<li><a href='/TD'>Temp&deg;/DewP</a></li>");
//  webpage += F("<li><a href='/DV'>Dial</a></li>");
  webpage += F("<li><a href='/LgTU'>Records&dArr;</a></li>");
  webpage += F("<li><a href='/LgTD'>Records&uArr;</a></li>");
  webpage += F("<li><a href='/AS'>AutoScale(");
  webpage += String((AScale ? "ON" : "OFF")) + ")</a></li>";
  webpage += F("<li><a href='/AU'>Refresh(");
  webpage += String((AUpdate ? "ON" : "OFF")) + ")</a></li>";
  webpage += F("<li><a href='/Setup'>Setup</a></li>");
  webpage += F("<li><a href='/Help'>Help</a></li>");
  webpage += F("<li><a href='/LogS'>Log Size</a></li>");
  webpage += F("<li><a href='/LogV'>Log View</a></li>");
  webpage += F("<li><a href='/LogE'>Log Erase</a></li>");
  webpage += F("<li><a href='/SoftReset'>SoftReset</a></li>"); // 추가
  webpage += F("<li><a href='/SpiffRemove'>SpiffRemove</a></li>"); // 추가
  webpage += F("</ul><footer><p ");
  webpage += F("style='background-color:#a3b2f7'>&copy;");
  char HTML[15] = {0x40, 0x88, 0x5c, 0x98, 0x5C, 0x84, 0xD2, 0xe4, 0xC8, 0x40, 0x64, 0x60, 0x62, 0x70, 0x00}; for (byte c = 0; c < 15; c++) {
    HTML[c] >>= 1;
  }
  webpage += String(HTML) + F("</p>\n");
  webpage += F("</footer></body></html>");
}

// 추가
void SoftReset() {

  webpage = ""; // don't delete this command, it ensures the server works reliably!
  append_page_header();
  webpage += F("<section style=\"font-size:14px\">");
  webpage += F("<h1>소프트리셋 완료. 웹페이지를 닫은 후 다시 열어주세요</h1><br><br>");
  webpage += F("</section>");
  append_page_footer();
  server.send(200, "text/html", webpage);
  webpage = "";

  delay(3000);

//  HardRsetState = LOW;  // hard reset
  Serial.print("------------------------- HardRestPin : ");
  Serial.println(HardRsetState);
//  digitalWrite(HardResetPin, HardRsetState);
  delay(1000);
  Serial.print("------------------------- HardRestPin : ");
  Serial.println(HardRsetState);
//  digitalWrite(HardResetPin, HardRsetState);

  ESP.restart();
}


// 추가
void SpiffRemove() {
  webpage = ""; // don't delete this command, it ensures the server works reliably!
  append_page_header();
  webpage += F("<section style=\"font-size:14px\">");
  webpage += F("<h1>SPIFF 삭제완료. 웹페이지를 닫은 후 다시 열어주세요</h1><br><br>");
  webpage += F("</section>");
  append_page_footer();
  server.send(200, "text/html", webpage);
  webpage = "";

  SPIFFS.remove("/" + DataFile);
  Serial.print("------------------------- SPIFF Data Deleted! ");
  delay(3000);

//  HardRsetState = LOW;  // hard reset
  Serial.print("------------------------- HardRestPin : ");
  Serial.println(HardRsetState);
//  digitalWrite(HardResetPin, HardRsetState);
  delay(1000);
  Serial.print("------------------------- HardRestPin : ");
  Serial.println(HardRsetState);
//  digitalWrite(HardResetPin, HardRsetState);
}



String calcDateTime(int epoch) { // From UNIX time becuase google charts can use UNIX time
  int seconds, minutes, hours, dayOfWeek, current_day, current_month, current_year;
  seconds      = epoch + (9 * 3600); // UNIX 타임을 우리나라 시간으로 변환하기 위해 (9시간 * 3600초)를 더해주면 됨
  minutes      = seconds / 60; // calculate minutes
  seconds     -= minutes * 60; // calculate seconds
  hours        = minutes / 60; // calculate hours
  minutes     -= hours   * 60;
  current_day  = hours   / 24; // calculate days
  hours       -= current_day * 24;
  current_year = 1970;         // Unix time starts in 1970
  dayOfWeek    = 4;            // on a Thursday
  while (1) {
    bool     leapYear   = (current_year % 4 == 0 && (current_year % 100 != 0 || current_year % 400 == 0));
    uint16_t daysInYear = leapYear ? 366 : 365;
    if (current_day >= daysInYear) {
      dayOfWeek += leapYear ? 2 : 1;
      current_day   -= daysInYear;
      if (dayOfWeek >= 7) dayOfWeek -= 7;
      ++current_year;
    }
    else
    {
      dayOfWeek  += current_day;
      dayOfWeek  %= 7;
      /* calculate the month and day */
      static const uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      for (current_month = 0; current_month < 12; ++current_month) {
        uint8_t dim = daysInMonth[current_month];
        if (current_month == 1 && leapYear) ++dim; // add a day to February if a leap year
        if (current_day >= dim) current_day -= dim;
        else break;
      }
      break;
    }
  }
  current_month += 1; // Months are 0..11 and returned format is dd/mm/ccyy hh:mm:ss
  current_day   += 1;
  String date_time = (current_day < 10 ? "0" + String(current_day) : String(current_day)) + "/" + (current_month < 10 ? "0" + String(current_month) : String(current_month)) + "/" + String(current_year).substring(2) + " ";
  date_time += ((hours   < 10) ? "0" + String(hours) : String(hours)) + ":";
  date_time += ((minutes < 10) ? "0" + String(minutes) : String(minutes)) + ":";
  date_time += ((seconds < 10) ? "0" + String(seconds) : String(seconds));
  return date_time;
}

void help() {
  webpage = ""; // don't delete this command, it ensures the server works reliably!
  append_page_header();
  webpage += F("<section style=\"font-size:14px\">");
  webpage += F("Temperature&Weight - display temperature and Weight>");
  webpage += F("Temperature&Dewpoint - display temperature and dewpoint<br>");
  webpage += F("Dial - display temperature and Weight values<br>");
  webpage += F("Max&deg;C&uArr; - increase maximum y-axis by 1&deg;C;<br>");
  webpage += F("Max&deg;C&dArr; - decrease maximum y-axis by 1&deg;C;<br>");
  webpage += F("Min&deg;C&uArr; - increase minimum y-axis by 1&deg;C;<br>");
  webpage += F("Min&deg;C&dArr; - decrease minimum y-axis by 1&deg;C;<br>");
  webpage += F("Logging&dArr; - reduce logging rate with more time between log entries<br>");
  webpage += F("Logging&uArr; - increase logging rate with less time between log entries<br>");
  webpage += F("Auto-scale(ON/OFF) - toggle graph Auto-scale ON/OFF<br>");
  webpage += F("Auto-update(ON/OFF) - toggle screen Auto-refresh ON/OFF<br>");
  webpage += F("Setup - allows some settings to be adjusted<br><br>");
  webpage += F("Log Size - display log file size in bytes<br>");
  webpage += F("View Log - stream log file contents to the screen, copy and paste into a spreadsheet using paste special, text<br>");
  webpage += F("Erase Log - erase log file, needs two approvals using this function. Graph display functions reset the initial erase approval<br><br>");
  webpage += F("</section>");
  append_page_footer();
  server.send(200, "text/html", webpage);
  webpage = "";
}

////////////// SPIFFS Support ////////////////////////////////
// For ESP8266 See: http://esp8266.github.io/Arduino/versions/2.0.0/doc/filesystem.html
void StartSPIFFS() {
  boolean SPIFFS_Status;
  SPIFFS_Status = SPIFFS.begin(true);
  if (SPIFFS_Status == false)
  { // Most likely SPIFFS has not yet been formated, so do so
#ifdef ESP8266
    Serial.println("Formatting SPIFFS Please wait .... ");
    if (SPIFFS.format() == true) Serial.println("SPIFFS formatted successfully");
    if (SPIFFS.begin(true) == false) Serial.println("SPIFFS failed to start...");
#else
    SPIFFS.begin(true);
    File datafile = SPIFFS.open("/" + DataFile, FILE_READ);
    if (!datafile || !datafile.isDirectory()) {
      Serial.println("SPIFFS failed to start..."); // If ESP32 nothing more can be done, so delete and then create another file
      SPIFFS.remove("/" + DataFile); // The file is corrupted!!
      datafile.close();
    }
#endif
  } else Serial.println("SPIFFS Started successfully...");
}

////////////// WiFi, Time and Date Functions /////////////////
int StartWiFi(const char* ssid, const char* password) {
  int connAttempts = 0;
  Serial.print(F("\r\nConnecting to: ")); Serial.println(String(ssid));
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500); Serial.print(".");
    if (connAttempts > 20) {
      Serial.println("\nFailed to connect to a Wi-Fi network");
      return -5;
    }
    connAttempts++;
  }
  Serial.print(F("WiFi connected at: "));
  Serial.println(WiFi.localIP());
  return 1;
}

#ifdef ESP8266  // ESP8266 이 정의되어 있으면 바로 아래를 실행
void StartTime() {
  // Note: The ESP8266 Time Zone does not function e.g. ,0,"time.nist.gov"
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  // Change this line to suit your time zone, e.g. USA EST configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  // Change this line to suit your time zone, e.g. AUS configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println(F("\nWaiting for time"));
  while (!time(nullptr)) {
    delay(500);
  }
  Serial.println("Time set");
  timeClient.begin();
}

String GetTime() {
  time_t now = time(nullptr);
  struct tm *now_tm;
  int hour, min, second, day, month, year, dow;
  now = time(NULL);
  now_tm = localtime(&now);
  hour   = now_tm->tm_hour;
  min    = now_tm->tm_min;
  second = now_tm->tm_sec;
  day    = now_tm->tm_mday;
  month  = now_tm->tm_mon + 1;
  year   = now_tm->tm_year - 100; // To get just YY information
  String days[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  dow    = ((timeClient.getEpochTime() / 86400L) + 4) % 7;
  time_str = (day < 10 ? "0" + String(day) : String(day)) + "/" +
             (month < 10 ? "0" + String(month) : String(month)) + "/" +
             (year < 10 ? "0" + String(year) : String(year)) + " ";
  time_str = (hour < 10 ? "0" + String(hour) : String(hour)) + ":" + (min < 10 ? "0" + String(min) : String(min)) + ":" + (second < 10 ? "0" + String(second) : String(second));
  Serial.println(time_str);
  return time_str; // returns date-time formatted as "11/12/17 22:01:00"
}

byte calc_dow(int y, int m, int d) {
  static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  y -= m < 3;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}
#else   // ESP8266 이 정의되어 있지 않으면(ESP32 가 사용되면)  아래를 실행.
void StartTime() {
  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  //setenv("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02", 1); // Change for your location
  //setenv("TZ", "EET-2EEST,M3.5.5/0,M10.5.5/0",1); // Change for your location
  setenv("TZ", "UTC-09:00", 1); // "UTC-05:30" 이면 UTC(India 기준) 시간에서 5시간 30분을 뺀다는 뜻 // Change for your location
  //"EET-2EEST,M3.5.5/0,M10.5.5/0";     // Asia

  UpdateLocalTime();
}

void UpdateLocalTime() {
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  }
  //See http://www.cplusplus.com/reference/ctime/strftime/
  Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S"); // Displays: Saturday, June 24 2017 14:05:49
  char output[50];
  strftime(output, 50, "%a %d-%b-%y  (%H:%M:%S)", &timeinfo);
  time_str = output;
}

String GetTime() {
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time - trying again");
  }
  //See http://www.cplusplus.com/reference/ctime/strftime/
  Serial.println(&timeinfo, "%a %b %d %Y %H:%M:%S"); // Displays: Saturday, June 24 2017 14:05:49
  char output[50];
  strftime(output, 50, "%d/%m/%y %H:%M:%S", &timeinfo); //Use %m/%d/%y for USA format
  time_str = output;
  Serial.println(time_str);
  return time_str; // returns date-time formatted like this "11/12/17 22:01:00"
}
#endif
