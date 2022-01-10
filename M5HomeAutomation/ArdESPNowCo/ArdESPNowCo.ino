/*
*******************************************************************************
  Date: 26 Dec 2021
  Author: Suresh G M
  Adapted program : bulit in library - display in M5 stack and Multi master in ESP Now
  Visit the website for more information：

  Description：Smart emergy monitor - ESP8266 interfaced to EM09 MECO emergy meter with RS485
  Slave ESP8266 reads data from the energy meter Over RS485 and sends to this sketch for processing
  This program receives the text (csv) data and process + display on the TFT screen

*******************************************************************************
*/
#include <M5Core2.h>
#include <esp_now.h>
#include <WiFi.h>

// Global copy of slave
#define NUMSLAVES 20
#define CHANNEL 3
#define PRINTSCANRESULTS 0
#define DISP_LINE_LEN 30

#define DISP_XHEAD_OFFSET 40
#define DISP_XLINE_OFFSET 10
#define DISP_Y_OFFSET 2

// Function prototypes
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void sendData();
void manageSlave();
void ScanForSlave();
void InitESPNow();
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len);

// variables
char myData[240];       // received data buffer
esp_now_peer_info_t slaves[NUMSLAVES] = {};
int SlaveCnt = 0;
bool S1_DataAvailable;

RTC_TimeTypeDef RTCtime;
RTC_DateTypeDef RTCDate;
/* After M5Core2 is started or reset
  the program in the setUp () function will be run, and this part will only be run once.
*/
void setup() {
  Serial.begin(115200);
  S1_DataAvailable = false;
  M5.begin(); //Init M5Core2.
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.fillRect(1, 1, M5.Lcd.width() - 1, 20, BLUE); //Draw a blue header rectangle 20x320 at (x,y)
  M5.Lcd.setCursor(50, 2);              //Move the cursor position to (x,y)to print the tiltle
  M5.Lcd.setTextColor(GREEN);           //Set the font color to Green.
  M5.Lcd.setTextSize(2);                //Set the font size.
  M5.Lcd.printf("Smart Energy System"); //Serial output format string.

  M5.Lcd.setTextSize(2);                //Set the font size.
  //  M5.Lcd.setCursor(2, 30);  M5.Lcd.printf("Volt (V): ");
  //  M5.Lcd.setCursor(2, 50);  M5.Lcd.printf("Amps (A): ");
  //  M5.Lcd.setCursor(2, 70);  M5.Lcd.printf("Pwr (kW): ");
  //  M5.Lcd.setCursor(2, 90);  M5.Lcd.printf("KVar    : ");
  //  M5.Lcd.setCursor(2, 110);  M5.Lcd.printf("kVA     : ");
  //  M5.Lcd.setCursor(2, 130);  M5.Lcd.printf("PF      : ");
  //  M5.Lcd.setCursor(2, 150);  M5.Lcd.printf("Freq(Hz): ");
  M5.Lcd.setCursor(2, 200);
  M5.Lcd.printf("MAC ID: ");
  M5.Lcd.printf(WiFi.macAddress().c_str());

  WiFi.mode(WIFI_STA);
  Serial.println("ESPNow/Multi-Slave/Master Example");
  // This is the mac address of the Master in Station Mode
  Serial.print("STA MAC: "); Serial.println(WiFi.macAddress());
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
}

/* After the program in setup() runs, it runs the program in loop()
*/
void loop() {
  M5.update();  //Read the press state of the key A, B, C
  if (M5.BtnA.wasReleased())  {

  }
  if (M5.BtnB.wasReleased())  {

  }
  if (M5.BtnC.wasReleased())  {

  }
  ScanForSlave();
  if (SlaveCnt > 0) { // check if slave channel is defined
    manageSlave();
    sendData();
  }
  else {
    // No slave found to process
  }
  if (S1_DataAvailable)  {
    S1_DataAvailable = false;
    int msglen = strlen(myData);
    int chr_cnt = 0;
    int line_cnt = 0;
    int x_pos;
    char linedata[DISP_LINE_LEN];
    M5.Lcd.clear(BLACK);  // Clear the screen and set BLACK to the background color.
    //M5.Lcd.fillScreen(BLACK);
    M5.Lcd.fillRect(1, 1, M5.Lcd.width() - 1, 20, BLUE); //Draw a blue header rectangle 20x320 at (x,y)
    //M5.Lcd.setCursor(50, 2);              //Move the cursor position to (x,y)to print the tiltle
    M5.Lcd.setTextColor(GREEN);           //Set the font color to Green.
    M5.Lcd.setTextSize(2);                //Set the font size.

    for (int c_cnt = 1; c_cnt < msglen; c_cnt++) {
      if (myData[c_cnt] == ',')  {
        linedata[chr_cnt] = '\0';
        if (line_cnt == 0)
          x_pos = DISP_XHEAD_OFFSET;
        else
          x_pos = DISP_XLINE_OFFSET;
        M5.Lcd.setCursor(x_pos, (DISP_Y_OFFSET + line_cnt * 25));  M5.Lcd.printf(linedata);
        chr_cnt = 0;
        if (line_cnt++ > 10)
          break;
      }
      else  {
        if (chr_cnt < DISP_LINE_LEN)
          linedata[chr_cnt++] = myData[c_cnt];
      }
    }
    sprintf(linedata, "Updt: %s", GetTimeStr());
    M5.Lcd.setCursor(x_pos, (DISP_Y_OFFSET + line_cnt * 25));  M5.Lcd.printf(linedata);
  }
  delay(200);
}

/*
   Function: Init ESP Now with fallback
*/
void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    InitESPNow();       // Retry InitESPNow, add a counte and then restart?
    //ESP.restart();    // or Simply Restart
  }
}
/*
   Function: Scan for slaves in AP mode
*/
void ScanForSlave() {
  int8_t scanResults = WiFi.scanNetworks();
  memset(slaves, 0, sizeof(slaves));    //reset slaves
  SlaveCnt = 0;
  //  Serial.println("");
  if (scanResults == 0) {
    Serial.println("No WiFi devices in AP Mode found");
  }
  else {
    // gms //Serial.print("Found "); Serial.print(scanResults); Serial.println(" devices ");
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);
      if (PRINTSCANRESULTS) {
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
      }
      delay(10);
      if (SSID.indexOf("Slave") == 0) { // Check if the current device starts with `Slave`
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");

        int mac[6];
        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
          for (int ii = 0; ii < 6; ++ii ) {
            slaves[SlaveCnt].peer_addr[ii] = (uint8_t) mac[ii];
          }
        }
        slaves[SlaveCnt].channel = CHANNEL;   // pick a channel
        slaves[SlaveCnt].encrypt = 0;         // no encryption
        SlaveCnt++;
      }
    }
  }

  if (SlaveCnt > 0) {
    Serial.print(SlaveCnt); Serial.println(" Slave(s) found, processing..");
  } else {
    //gms//Serial.println("No Slave Found, trying again.");
  }
  WiFi.scanDelete();    // clean up ram
}

/*
   Function: manageSlave()
   Description: Check if the slave is already paired with the master.
   If not, pair the slave with master.
*/
void manageSlave() {
  if (SlaveCnt > 0) {
    for (int i = 0; i < SlaveCnt; i++) {
      Serial.print("Processing: ");
      for (int ii = 0; ii < 6; ++ii ) {
        Serial.print((uint8_t) slaves[i].peer_addr[ii], HEX);
        if (ii != 5) Serial.print(":");
      }
      Serial.print(" Status: ");
      bool exists = esp_now_is_peer_exist(slaves[i].peer_addr); // check if the peer exists
      if (exists) {
        Serial.println("Already Paired");           // Slave already paired.
      }
      else {
        esp_err_t addStatus = esp_now_add_peer(&slaves[i]); // Slave not paired, attempt pair
        if (addStatus == ESP_OK) {                // Pair success
          Serial.println("Pair success");
        }
        else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {  // How did we get so far!!
          Serial.println("ESPNOW Not Init");
        }
        else if (addStatus == ESP_ERR_ESPNOW_ARG) {
          Serial.println("Add Peer - Invalid Argument");
        }
        else if (addStatus == ESP_ERR_ESPNOW_FULL) {
          Serial.println("Peer list full");
        }
        else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
          Serial.println("Out of memory");
        }
        else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
          Serial.println("Peer Exists");
        }
        else {
          Serial.println("Not sure what happened");
        }
        delay(100);
      }
    }
  } else {      // No slave found to process
    Serial.println("No Slave found to process");
  }
}
/*
   Function sendData
*/
uint8_t data = 0;
void sendData() {
  data++;
  for (int i = 0; i < SlaveCnt; i++) {
    const uint8_t *peer_addr = slaves[i].peer_addr;
    if (i == 0) { // print only for first slave
      Serial.print("Sending: ");
      Serial.println(data);
    }
    esp_err_t result = esp_now_send(peer_addr, &data, sizeof(data));
    Serial.print("Send Status: ");
    if (result == ESP_OK) {
      Serial.println("Success");
    }
    else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
      Serial.println("ESPNOW not Init.");
    }
    else if (result == ESP_ERR_ESPNOW_ARG) {
      Serial.println("Invalid Argument");
    }
    else if (result == ESP_ERR_ESPNOW_INTERNAL) {
      Serial.println("Internal Error");
    }
    else if (result == ESP_ERR_ESPNOW_NO_MEM) {
      Serial.println("ESP_ERR_ESPNOW_NO_MEM");
    }
    else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
      Serial.println("Peer not found.");
    }
    else {
      Serial.println("Not sure what happened");
    }
    delay(100);
  }
}
/*
   Function: OnDataSent
   callback when data is sent from Master to Slave
*/
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: "); Serial.println(macStr);
  Serial.print("Last Packet Send Status: "); Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

/*
   Function: OnDataRecv
   Description: Callback function that will be executed when data is received
*/
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *indata, int data_len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  memcpy(myData, indata, data_len);
  myData[data_len + 1] = '\0';
  Serial.print("From: "); Serial.print(macStr);
  Serial.print("  Data: "); Serial.println(myData);
  Serial.println("");
  S1_DataAvailable = true;
}

char* GetDateStr() {
  static char DateStr[10];
  M5.Rtc.GetDate(&RTCDate);
  sprintf(DateStr, "%d/%02d/%02d", RTCDate.Year, RTCDate.Month, RTCDate.Date);
  return DateStr;
}
char* GetTimeStr() {
  static char TimeStr[10];
  M5.Rtc.GetTime(&RTCtime); //Gets the time in the RTC.
  sprintf(TimeStr, "%02d:%02d:%02d", RTCtime.Hours, RTCtime.Minutes, RTCtime.Seconds);
  return TimeStr;
}
