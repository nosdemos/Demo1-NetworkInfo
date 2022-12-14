//#include <M5Stack.h>
#include <M5Core2.h>

#include <stdint.h>
#include <vector>
#include "TFTTerminal.h"
#include "FS.h"
#include "SPIFFS.h"

char thingsboard_url[] = "iot-dev.nos.pt";


char aux_str[100];
char port[] = "80";                    // PORT Connected on
String getStr = "";

TFT_eSprite Disbuff = TFT_eSprite(&M5.Lcd);
TFT_eSprite TerminalBuff = TFT_eSprite(&M5.Lcd);
TFTTerminal terminal(&TerminalBuff);
TaskHandle_t xhandle_lte_event = NULL;
SemaphoreHandle_t command_list_samap;


uint32_t numberbuff[128];
String readstr;
uint8_t restate;

typedef enum
{
  kQUERY_MO = 0,
  KTEST_MO,
  kASSIGN_MO,
  kACTION_MO,
  kQUERY_MT,
  kTEST_MT,
  kASSIGN_MT,
  kACTION_MT,
  kINFORM
} LTEMsg_t;

typedef enum
{
  kErrorSendTimeOUT = 0xe1,
  kErrorReError = 0xe2,
  kErroeSendError = 0xe3,
  kSendReady = 0,
  kSending = 1,
  kWaitforMsg = 2,
  kWaitforRead = 3,
  kReOK
} LTEState_t;


int8_t sendATcommand(char* ATcommand, char* expected_answer, unsigned int timeout)
{
  uint8_t x = 0,  answer = 0;
  char response[3000];         //Response buffer
  unsigned long previous;

  memset(response, '\0', 3000);    // Initialize the string
  delay(100);
  while ( Serial.available() > 0) Serial.read();   // Clean the input buffer

  Serial.println(ATcommand);    // Send the AT command
  Serial1.println(ATcommand); //Response from Serial1

  x = 0;
  previous = millis();

  // this loop waits for the answer
  do
  {
    if (Serial1.available() != 0)
    {
      // if there is data in the UART input buffer, read it and checks for the asnwer
      response[x] = Serial1.read();     //Read Response from Serial1 port
      Serial.print(response[x]);        //Print response on Serial 0 port
      x++;

      // check if the desired answer  is in the response of the module
      if (strstr(response, expected_answer) != NULL)
      {
        answer = 1;
      }
    }
  }//do
  // Waits for the asnwer with time out
  while ((answer == 0) && ((millis() - previous) < timeout));   //Check till answer = 0 and timout period(ms)
  return answer;
}//sendATcommand()


int8_t sendATcommand2(String ATcommand, char* expected_answer1, char* expected_answer2, unsigned int timeout)
{
  uint8_t x = 0,  answer = 0;
  char response[3000];
  unsigned long previous;
  memset(response, '\0', 3000);    // Initialize the string
  delay(100);

  while ( Serial1.available() > 0) Serial1.read();   // Clean the input buffer

  Serial1.println(ATcommand);    // Send the AT command
  Serial.println(ATcommand);
  x = 0;
  previous = millis();

  // this loop waits for the answer
  do
  {
    // if there are data in the UART input buffer, reads it and checks for the asnwer
    if (Serial1.available() != 0)
    {
      response[x] = Serial1.read();
      Serial.print(response[x]);
      x++;
      // check if the desired answer 1  is in the response of the module
      if (strstr(response, expected_answer1) != NULL)
      {
        answer = 1;
      }
      // check if the desired answer 2 is in the response of the module
      else if (strstr(response, expected_answer2) != NULL)
      {
        answer = 2;
      }
    }//if()

  }//do

  // Waits for the asnwer with time out
  while ((answer == 0) && ((millis() - previous) < timeout));
  return answer;
}//sendATcommand2()

struct ATCommand
{
  uint8_t command_type;
  String str_command;
  uint16_t send_max_number;
  uint16_t max_time;
  uint8_t state;
  String read_str;
  uint16_t _send_count;
  uint16_t _send_time_count;

} user;

using namespace std;
vector<ATCommand> serial_at;
String zmmi_str;
void LTEModuleTask(void *arg)
{
  int Number = 0;
  String restr;
  while (1)
  {
    xSemaphoreTake(command_list_samap, portMAX_DELAY);

    if (Serial1.available() != 0)
    {
      String str = Serial1.readString();
      restr += str;

      if (restr.indexOf("\r\n") != -1)
      {

      }

      if ( restr.indexOf("+ZMMI:") != -1)
      {
        zmmi_str = restr;
      }
      else if ((restr.indexOf("OK") != -1) || (restr.indexOf("ERROR") != -1))
      {
        Serial.print(restr);
        if (restr.indexOf("OK") != -1)
        {
          if ((serial_at[0].command_type == kACTION_MO) || (serial_at[0].command_type == kASSIGN_MO))
          {
            serial_at.erase(serial_at.begin());
            Serial.printf("erase now %d\n", serial_at.size());
          }
          else
          {
            serial_at[0].read_str = restr;
            serial_at[0].state = kWaitforRead;
          }
        }
        else if (restr.indexOf("ERROR") != -1)
        {
          serial_at[0].state = kErrorReError;
        }
        else
        {
        }
        restr.clear();
      }
    }

    if (serial_at.empty() != true)
    {
      Number = 0;
      switch (serial_at[0].state)
      {
        case kSendReady:
          Serial.printf(serial_at[0].str_command.c_str());
          Serial1.write(serial_at[0].str_command.c_str());
          serial_at[0].state = kSending;
          break;
        case kSending:

          if (serial_at[0]._send_time_count > 0)
          {
            serial_at[0]._send_time_count--;
          }
          else
          {
            serial_at[0].state = kWaitforMsg;
          }
          /* code */
          break;
        case kWaitforMsg:
          if (serial_at[0]._send_count > 0)
          {
            serial_at[0]._send_count--;
            serial_at[0]._send_time_count = serial_at[0].max_time;
            Serial.printf(serial_at[0].str_command.c_str());
            Serial1.write(serial_at[0].str_command.c_str());
            restr.clear();
            serial_at[0].state = 1;
          }
          else
          {
            serial_at[0].state = kErrorSendTimeOUT;
          }
          /* code */
          break;
        case kWaitforRead:
          /* code */
          break;
        case 4:
          /* code */
          break;
        case kErrorSendTimeOUT:
          /* code */
          break;
        case 0xe2:
          /* code */
          break;
        default:
          break;
      }
    }
    xSemaphoreGive(command_list_samap);
    delay(10);
  }
}

void AddMsg(String str, uint8_t type, uint16_t sendcount, uint16_t sendtime)
{
  struct ATCommand newcommand;
  newcommand.str_command = str;
  newcommand.command_type = type;
  newcommand.max_time = sendtime;
  newcommand.send_max_number = sendcount;
  newcommand.state = 0;
  newcommand._send_count = sendcount;
  newcommand._send_time_count = sendtime;
  xSemaphoreTake(command_list_samap, portMAX_DELAY);
  serial_at.push_back(newcommand);
  xSemaphoreGive(command_list_samap);
}

uint8_t readSendState(uint32_t number)
{
  xSemaphoreTake(command_list_samap, portMAX_DELAY);
  uint8_t restate = serial_at[number].state;
  xSemaphoreGive(command_list_samap);
  return restate;
}

uint32_t getATMsgSize()
{
  xSemaphoreTake(command_list_samap, portMAX_DELAY);
  uint32_t restate = serial_at.size();
  xSemaphoreGive(command_list_samap);
  return restate;
}
String ReadMsgstr(uint32_t number)
{
  xSemaphoreTake(command_list_samap, portMAX_DELAY);
  String restate = serial_at[number].read_str;
  xSemaphoreGive(command_list_samap);
  return restate;
}

bool EraseFirstMsg()
{
  xSemaphoreTake(command_list_samap, portMAX_DELAY);
  serial_at.erase(serial_at.begin());
  xSemaphoreGive(command_list_samap);
  return true;
}

uint16_t GetstrNumber( String Str, uint32_t* ptrbuff )
{
  uint16_t count = 0;
  String Numberstr;
  int	 indexpos = 0;
  while ( Str.length() > 0 )
  {
    indexpos = Str.indexOf(",");
    if ( indexpos != -1 )
    {
      Numberstr = Str.substring(0, Str.indexOf(","));
      Str = Str.substring(Str.indexOf(",") + 1, Str.length());
      ptrbuff[count] = Numberstr.toInt();
      count ++;
    }
    else
    {
      ptrbuff[count] = Str.toInt();
      count ++;
      break;
    }
  }
  return count;
}
vector<String> restr_v;
uint16_t GetstrNumber( String StartStr, String EndStr, String Str )
{
  uint16_t count = 0;
  String Numberstr;
  int	 indexpos = 0;

  Str = Str.substring(Str.indexOf(StartStr) + StartStr.length(), Str.indexOf(EndStr));
  Str.trim();
  restr_v.clear();

  while ( Str.length() > 0 )
  {
    indexpos = Str.indexOf(",");
    if ( indexpos != -1 )
    {
      Numberstr = Str.substring(0, Str.indexOf(","));
      Str = Str.substring(Str.indexOf(",") + 1, Str.length());
      restr_v.push_back(Numberstr);
      count ++;
    }
    else
    {
      restr_v.push_back(Numberstr);;
      count ++;
      break;
    }
  }
  return count;
}

String getReString( uint16_t Number )
{
  if ( restr_v.empty())
  {
    return String("");
  }
  return restr_v.at(Number);
}

uint16_t GetstrNumber( String StartStr, String EndStr, String Str, uint32_t* ptrbuff )
{
  uint16_t count = 0;
  String Numberstr;
  int	 indexpos = 0;

  Str = Str.substring(Str.indexOf(StartStr) + StartStr.length(), Str.indexOf(EndStr));
  Str.trim();

  while ( Str.length() > 0 )
  {
    indexpos = Str.indexOf(",");
    if ( indexpos != -1 )
    {
      Numberstr = Str.substring(0, Str.indexOf(","));
      Str = Str.substring(Str.indexOf(",") + 1, Str.length());
      ptrbuff[count] = Numberstr.toInt();
      count ++;
    }
    else
    {
      ptrbuff[count] = Str.toInt();
      count ++;
      break;
    }
  }
  return count;
}

void pingGoogle() {
  AddMsg("AT+CIPPING=8.8.8.8\r\n", kQUERY_MT, 1000, 1000);
  delay(100);
  while ((readSendState(0) == kSendReady) || (readSendState(0) == kSending) || (readSendState(0) == kWaitforMsg))delay(50);
  restate = readSendState(0);
  readstr = ReadMsgstr(0).c_str();

  if (( readstr.indexOf("+CIPPING") != -1 ) && ( restate == kWaitforRead ))
  {
    int count = GetstrNumber("+CIPPING", "OK", readstr, numberbuff);
    if ( count != 0 )
    {

      terminal.printf("png:%s\r\n", readstr);
      //M5.Lcd.fillRect(200,50,30,30,GREEN);
    }

  }

  EraseFirstMsg();

  Disbuff.print(readstr);
  terminal.print(readstr);
}


void setup()
{
  // put your setup code here, to run once:
  M5.begin();
  //Serial1.begin(115200, SERIAL_8N1, 5, 13);
  //Serial1.begin(115200, SERIAL_8N1, 16, 17);
  Serial1.begin(115200, SERIAL_8N1, 13, 14);

  // M5.Power.begin();
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  M5.Lcd.drawBmpFile(SPIFFS, "/nos_initial.bmp", 0, 0);

  pinMode(2, OUTPUT);
  digitalWrite(2, 0);

  Disbuff.setTextColor(WHITE);
  Disbuff.setTextSize(1);
  for (int i = 0; i < 100; i++ )
  {
    Disbuff.fillRect(220, 0, 320, 2400, Disbuff.color565(36, 36, 36));
    Disbuff.pushSprite(0, 0);
    Disbuff.setCursor(227, 7);
    Disbuff.printf("Reset Module %02d", i);
    Disbuff.pushSprite(0, 0);
    delay(10);
  }
  digitalWrite(2, 1);

  xTaskCreate(LTEModuleTask, "LTEModuleTask", 1024 * 2, (void *)0, 4, &xhandle_lte_event);
  command_list_samap = xSemaphoreCreateMutex();
  xSemaphoreGive(command_list_samap);

  Disbuff.createSprite(320, 20);
  Disbuff.fillRect(0, 0, 320, 20, BLACK);
  Disbuff.drawRect(0, 0, 320, 20, Disbuff.color565(36, 36, 36));
  Disbuff.pushSprite(0, 0);

  TerminalBuff.createSprite(230, 220);
  TerminalBuff.fillRect(0, 0, 230, 220, BLACK);
  TerminalBuff.drawRect(0, 0, 230, 220, Disbuff.color565(36, 36, 36));
  TerminalBuff.pushSprite(0, 20);
  terminal.setFontsize(2);
  terminal.setGeometry(0, 20, 230, 220);

  M5.Lcd.clear(BLACK);

  Disbuff.fillRect(0, 0, 320, 20, Disbuff.color565(36, 36, 36));
  Disbuff.setCursor(7, 7);
  Disbuff.printf("NOS HUB 5G - NB IoT Starter Kit");
  Disbuff.pushSprite(0, 0);

}


void loop()
{

  AddMsg("AT+CSQ\r\n", kQUERY_MT, 1000, 1000);
  while ((readSendState(0) == kSendReady) || (readSendState(0) == kSending) || (readSendState(0) == kWaitforMsg))delay(50);
  restate = readSendState(0);
  readstr = ReadMsgstr(0).c_str();

  if (( readstr.indexOf("+CSQ:") != -1 ) && ( restate == kWaitforRead ))
  {
    int count = GetstrNumber("+CSQ:", "OK", readstr, numberbuff);
    if ( count != 0 )
    {
      M5.Lcd.drawString("RSSI", 240, 200);
      if (( numberbuff[0] >= 2 ) && ( numberbuff[0] <= 31 ) && ( numberbuff[1] != 99 ))
      {
        int rssi = 110 - (( numberbuff[0] - 2 ) * 2 );
        terminal.printf("RSSI:-%d\r\n", rssi);
        M5.Lcd.fillCircle(305, 200, 12, GREEN);
      }
      else
      {
        M5.Lcd.fillCircle(305, 200, 12, RED);
      }
    }
  }
  EraseFirstMsg();

  // Disbuff.print(readstr);
  terminal.print(readstr);

  AddMsg("AT+CREG?\r\n", kQUERY_MT, 1000, 1000);
  while ((readSendState(0) == kSendReady) || (readSendState(0) == kSending) || (readSendState(0) == kWaitforMsg))delay(50);
  restate = readSendState(0);
  readstr = ReadMsgstr(0).c_str();
  if (( readstr.indexOf("+CREG:") != -1 ) && ( restate == kWaitforRead ))
  {
    int count = GetstrNumber("+CREG:", "OK", readstr, numberbuff);
    if ( count != 0 )
    {
      M5.Lcd.drawString("Network", 240, 155);
      if (( numberbuff[1] == 1 ))
      {
        M5.Lcd.fillCircle(305, 160, 12, GREEN);
      }
      else
      {
        M5.Lcd.fillCircle(305, 160, 12, RED);
      }
    }
  }
  EraseFirstMsg();

  terminal.print(readstr);

  AddMsg("AT+CENG?\r\n", kQUERY_MT, 1000, 1000);
  while ((readSendState(0) == kSendReady) || (readSendState(0) == kSending) || (readSendState(0) == kWaitforMsg))delay(50);
  restate = readSendState(0);
  readstr = ReadMsgstr(0).c_str();

  if (( readstr.indexOf("+CENG:") != -1 ) && ( restate == kWaitforRead ))
  {
    int count = GetstrNumber("+CENG:", "OK", readstr, numberbuff);
    Serial.printf("count:%d\r\n", count);
    if ( count != 0 )
    {
      M5.Lcd.drawString("NB-IoT Status:", 236, 30);

      int band = numberbuff[8];
      String bandS = "Band: ";
      bandS += band;
      M5.Lcd.drawString(bandS, 240, 40);

      int earfcn = numberbuff[0];
      String earfcnS = "EARFCN: ";
      earfcnS += earfcn;
      M5.Lcd.drawString(earfcnS, 240, 50);

      int pci = numberbuff[2];
      String pciS = "PCI: ";
      pciS += pci;
      M5.Lcd.drawString(pciS, 240, 60);

      int rsrp = numberbuff[4];
      String rsrpS = "RSRP: ";
      rsrpS += rsrp;
      M5.Lcd.drawString(rsrpS, 240, 70);

      int rsrq = numberbuff[5];
      String rsrqS = "RSRQ: ";
      rsrqS += rsrq;
      M5.Lcd.drawString(rsrqS, 240, 80);
    }
  }
  EraseFirstMsg();

  terminal.print(readstr);

  AddMsg("AT+COPS?\r\n", kQUERY_MT, 1000, 1000);
  while ((readSendState(0) == kSendReady) || (readSendState(0) == kSending) || (readSendState(0) == kWaitforMsg))delay(50);
  restate = readSendState(0);
  readstr = ReadMsgstr(0).c_str();
  if (( readstr.indexOf("+COPS:") != -1 ) && ( restate == kWaitforRead ))
  {
    int count = GetstrNumber("+COPS:", "OK", readstr, numberbuff);
    if ( count != 0 )
    {
      M5.Lcd.drawString("Operator", 240, 115);
      if ( numberbuff[3] == 9 )
      {
        M5.Lcd.fillCircle(305, 120, 12, GREEN);
      }
      else
      {
        M5.Lcd.fillCircle(305, 120, 12, RED);
      }
    }
  }
  EraseFirstMsg();

  //Disbuff.print(readstr);
  terminal.print(readstr);

  //pingGoogle();

  delay(750);
  M5.update();

}
