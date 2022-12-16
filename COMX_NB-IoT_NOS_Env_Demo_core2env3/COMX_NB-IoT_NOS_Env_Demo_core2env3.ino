//#include <M5Stack.h>
#include <M5Core2.h>

#include <stdint.h>
#include <vector>
#include "M5_ENV.h"
#include <ArduinoJson.h> //For Creating a Json File

DynamicJsonBuffer jsonBuffer;                                 //Set Buffer size to Dynamic
JsonObject& root = jsonBuffer.createObject();               //Create an object 'root' which is called later to print JSON Buffer

SHT3X sht30;
QMP6988 qmp6988;

int lengthOfJSON, interval = 750000;
float tmp = 0.0, hum = 0.0, pressure = 0.0;
char aux_str[100];
char port[] = "80";                    // HTTP PORT
String getStr = "";
long previousMillis;
bool forceSend = true;

char thingsboard_url[] = "iot-dev.nos.pt";
String AccessToken = "CHAVE";

TFT_eSprite Disbuff = TFT_eSprite(&M5.Lcd);

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

void updateThingsboard()
{
  lengthOfJSON = 0;                  //Set size of JSON text as '0' initially
  Serial.println("Opening TCP");
  snprintf(aux_str, sizeof(aux_str), "AT+CIPSTART=\"TCP\",\"%s\",\"%s\"", thingsboard_url, port); //IP_address
  if (sendATcommand2(aux_str, "CONNECT OK", "CONNECT FAIL", 30000) == 1)
  {
    Serial.println("Connected");
    String json = "";
    root.printTo(json);                       //Store JSON in a String named 'json'
    lengthOfJSON = json.length();             //This gives us total size of our json text
    //TCP packet to send POST Request on https (Thingsboard)
    getStr = "POST /api/v1/" + AccessToken + "/telemetry HTTP/1.1\r\nHost: " + thingsboard_url + "\r\nAccept: */*\r\nContent-Type: application/json\r\nContent-Length:" + lengthOfJSON + "\r\n\r\n" + json;
    //TCP packet to send POST Request on https (Thingsboard)
    String sendcmd = "AT+CIPSEND=" + String(getStr.length());

    if (sendATcommand2(sendcmd, ">", "ERROR", 10000) == 1)
    {
      delay(100);
      if (sendATcommand2(getStr, "SEND OK", "ERROR", 10000)) { //Sending Data Here
        M5.Lcd.setTextSize(1);
        M5.Lcd.drawString("Ultimo envio: OK    ", 20, 170);
      } else {
        M5.Lcd.setTextSize(1);
        M5.Lcd.drawString("Ultimo envio: ERRO!", 20, 170);
      }
    }
    Serial.println("Closing the Socket!");
    sendATcommand2("AT+CIPCLOSE", "CLOSE OK", "ERROR", 10000);
  }
  else
  {
    Serial.println("Error opening the connection");
  }
  Serial.println("Shutting down the connection!");
  sendATcommand2("AT+CIPSHUT", "OK", "ERROR", 10000);
}//updateThingsboard

void makeJson( float val1, float val2, float val3)
{
  Serial.println("\nMaking JSON text meanwhile\n");
  root["temperature"] = val1;
  root["humidity"] = val2;
  root["pressure"] = val3;
    String json1 = "";
      root.printTo(json1); 
      Serial.println(json1);
}

void getData() {

  pressure = qmp6988.calcPressure();
  
  if (sht30.get() == 0) { //Obtain the data of shT30.  获取sht30的数据
    tmp = sht30.cTemp;  //Store the temperature obtained from shT30.  将sht30获取到的温度存储
    hum = sht30.humidity; //Store the humidity obtained from the SHT30.  将sht30获取到的湿度存储
  } else {
    tmp = 0, hum = 0;
  }
}


void setup()
{
  M5.begin();
  M5.Lcd.begin();

  //Serial1.begin(115200, SERIAL_8N1, 5, 13);
  //Serial1.begin(115200, SERIAL_8N1, 16, 17);
  Serial1.begin(115200, SERIAL_8N1, 13, 14);


  Wire.begin(); //Wire init, adding the I2C bus.  Wire初始化, 加入i2c总线

  qmp6988.init();

  Disbuff.createSprite(320, 20);
  Disbuff.fillRect(0, 0, 320, 20, BLACK);
  Disbuff.drawRect(0, 0, 320, 20, Disbuff.color565(36, 36, 36));
  Disbuff.pushSprite(0, 0);

  pinMode(2, OUTPUT);
  digitalWrite(2, 0);

  Disbuff.setTextColor(WHITE);
  Disbuff.setTextSize(1);
  for (int i = 0; i < 100; i++ )
  {
    Disbuff.fillRect(0, 0, 320, 20, Disbuff.color565(36, 36, 36));
    Disbuff.pushSprite(0, 0);
    Disbuff.setCursor(7, 7);
    Disbuff.printf("Reset Module %02d", i);
    Disbuff.pushSprite(0, 0);
    delay(10);
  }
  digitalWrite(2, 1);

  Disbuff.fillRect(0, 0, 320, 20, Disbuff.color565(36, 36, 36));
  Disbuff.setCursor(7, 7);
  Disbuff.printf("NOS HUB 5G - NB-IoT Starter Kit - Environment Demo");
  Disbuff.pushSprite(0, 0);
}

void dataLCD() {
  getData(); //update from sensor
  M5.Lcd.setTextSize(2);
  String tempS = "Temperatura: ";
  tempS += tmp;
  M5.Lcd.drawString(tempS, 20, 30);
  String humS = "Humidade: ";
  humS += hum;
  M5.Lcd.drawString(humS, 20, 50);
  String presS = "Pressao Atm: ";
  presS += pressure;
  M5.Lcd.drawString(presS, 20, 70);
}

void loop()
{
  unsigned long currentMillis = millis();
  int timepassed = currentMillis - previousMillis;

  if (timepassed >= interval or forceSend) {
    previousMillis = currentMillis;
    M5.Lcd.setTextSize(2);
    M5.Lcd.drawString("SENDING DATA!           ", 20, 150);
    dataLCD();
    M5.update();

    getData();
    makeJson(tmp, hum, pressure);                    //Making JSON text here
    updateThingsboard();
    forceSend = false;
  } else {
    M5.Lcd.setTextSize(2);
    String timeS = "Tempo ate enviar: ";
    timeS += (interval - timepassed) / 1000;
    timeS += "    ";
    M5.Lcd.drawString(timeS, 20, 150);
  }

  if (M5.BtnA.wasPressed() or M5.BtnB.wasPressed() or M5.BtnC.wasPressed())  forceSend = true;

  M5.Lcd.setTextSize(1);
  M5.Lcd.drawString("Pressionar qualquer botao para forcar envio...", 15, 225);
  M5.update();

}
