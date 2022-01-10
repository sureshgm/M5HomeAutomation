/*
  Modbus Library for Arduino Example - Modbus RTU Client
  Read Holding Registers from Modbus RTU Server in blocking way
  ESP8266 Example
  
  (c)2020 Alexander Emelianov (a.m.emelianov@gmail.com)
  https://github.com/emelianov/modbus-esp8266
*/

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ModbusRTU.h>
#include <SoftwareSerial.h>

// REPLACE WITH RECEIVER MAC Address
uint8_t broadcastAddress[] = {0x24, 0x0A, 0xC4, 0xF8, 0xB2, 0xE4};

#define SLAVE_ID 1
#define FIRST_REG 3029
#define REG_COUNT 14
#define RE_DE D3  // D3 GPIO0 as _RE DE drive pin 
#define OB_LED D4
#define MAX_MSG_LEN 240

char ReplyBuffer[MAX_MSG_LEN];       // a string to send back
unsigned long Prevmills;
uint32_t MsgCnt;

SoftwareSerial S(D2, D1); // Rx pin - D2/RO, Tx pin -D1/DI
ModbusRTU mb;

const String mbUnits[] = {"    ", " V  ", " A  ", " KW ", "KVAR", " KVA", " PF ", " HZ ", " Nil", " Nil"};

typedef union _TFORM_ {         // union to convert between types
  unsigned long ul;
  long l;
  int i;
  unsigned short us[2];
  short s[2];
  unsigned char uc[4];
  char c[4];
  float f;
  unsigned char *str;
} TF;

float cvtBeFloatToFloat(uint16_t *dt, char swp, uint32_t mask);   // function declaration for type conversion
/*
 * modbus call back function
 */
bool cb(Modbus::ResultCode event, uint16_t transactionId, void* data) { // Callback to monitor errors
  if (event != Modbus::EX_SUCCESS) {
    Serial.print("Request result: 0x");
    Serial.print(event, HEX);
    return false;
  }
  return true;
}
/*
 * data sent callback funtion for ESP now
 */
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) 
{
  Serial.print("Last Packet Send Status: ");
  if (sendStatus == 0){
    Serial.println("Delivery success");
    digitalWrite(OB_LED, HIGH);
  }
  else{
    Serial.println("Delivery fail");
  }
}

/*
 * Callback function that will be executed when data is received 
 */
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len)
{
  //memcpy(&myData, incomingData, sizeof(myData));
}
 
 /*
  * Setup function, called onceat startup
  */
void setup() {
  Serial.begin(115200);
  pinMode(RE_DE, OUTPUT);
  pinMode(OB_LED, OUTPUT);
  digitalWrite(RE_DE, LOW);
  
  S.begin(9600, SWSERIAL_8N1);
  mb.begin(&S);
  mb.master();
  
  WiFi.mode(WIFI_STA);
  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE); // ESP_NOW_ROLE_SLAVE
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  // Register peer
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_CONTROLLER, 3, NULL, 0);
  Prevmills = millis();
  MsgCnt = 0;
}

void loop() {
  uint16_t res[REG_COUNT];
  float mbvar;
  char tempbuff[64];
  if (!mb.slave() && (millis() - Prevmills > 10000)) {    // Check if no transaction in progress and 3 seconds passed
    Prevmills = millis();
    for(int i=0; i<REG_COUNT; i++)
      res[i] = 0;                     // clear the buffer register to 0, before reading MODBUS
    digitalWrite(RE_DE, HIGH);
    mb.readHreg(SLAVE_ID, FIRST_REG, res, REG_COUNT, cb); // Send Read Hreg from Modbus Server
    digitalWrite(RE_DE, LOW);
    while(mb.slave()) {               // Check if transaction is active
      mb.task();
      delay(10);
    }
    ReplyBuffer[0] = NULL;  // empty string 
    tempbuff[0] = NULL;
    sprintf(ReplyBuffer, "#Electric Meter, %d,", MsgCnt++);
    Serial.println();
    for(int rcnt = 1; rcnt < 8 ; rcnt++)  {
      mbvar = cvtBeFloatToFloat(&res[(rcnt-1)*2], 0, 0xffffffff);   // convert 32 bit received value to float
      Serial.print(mbUnits[rcnt]);
      Serial.print(" : ");
      Serial.println(mbvar);
      sprintf(tempbuff, "%s : %f, ", mbUnits[rcnt], mbvar);
      if((strlen(ReplyBuffer) + strlen(tempbuff)) < MAX_MSG_LEN)  
        strcat(ReplyBuffer, tempbuff);  
    }
    strcat(ReplyBuffer, ";\n\r");
    // send a reply, to the IP address and port that sent us the packet we received   
    // Send message via ESP-NOW
    digitalWrite(OB_LED, LOW);
    esp_now_send(broadcastAddress, (uint8_t *) ReplyBuffer, sizeof(ReplyBuffer));
  }
}
/*
 * function for Byte to float convertion
 */
float cvtBeFloatToFloat(uint16_t *dt, char swp, uint32_t mask) {
  TF t;
  if (swp) {
    t.us[0] = *dt++; t.us[0] = *dt++;
  }
  else {
    t.us[1] = *dt++; t.us[0] = *dt++; 
  }
  t.ul &= mask;
  return t.f;
}
