#include "OV7670.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library

#include <WiFi.h>
#include <WiFiClient.h>
#include "BMP.h"

#define SIOD 21 //SDA
#define SIOC 22 //SCL

#define VSYNC 34
#define HREF 35

#define XCLK 32
#define PCLK 33

#define D0 27
#define D1 17
#define D2 16
#define D3 15
#define D4 14
#define D5 13
#define D6 12
#define D7 4

const int TFT_DC = 2;
const int TFT_CS = 5;
//DIN <- MOSI 23
//CLK <- SCK 18

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, 0/*no reset*/);
OV7670 *camera;

WiFiServer server(80);

unsigned char bmpHeader[BMP::headerSize];

// переменная для хранения HTTP-запроса:
String header;


// мотор 1:
int motor1Pin1 = 23; 
int motor1Pin2 = 19; 

// мотор 2:
int motor2Pin1 = 18; 
int motor2Pin2 = 25; 

// переменные для свойств широтно-импульсной модуляции (ШИМ):
const int freq = 30000;

const int pwmChannel0 = 0;
const int pwmChannel1 = 1;
const int pwmChannel2 = 2;
const int pwmChannel3 = 3;

const int resolution = 8;
int dutyCycle = 0;

// переменные для расшифровки HTTP-запроса GET:
String valueString = String(5);
int pos1 = 0;
int pos2 = 0;

void setup() {
  Serial.begin(115200);
  // задаем настройки ШИМ-канала:
  ledcSetup(pwmChannel0, freq, resolution);
  ledcSetup(pwmChannel1, freq, resolution);
  ledcSetup(pwmChannel2, freq, resolution);
  ledcSetup(pwmChannel3, freq, resolution);
  
  // подключаем ШИМ-канал 0 к контактам ENA и ENB,
  // т.е. к GPIO-контактам для управления скоростью вращения моторов:
  ledcAttachPin(motor1Pin1, pwmChannel0);
  ledcAttachPin(motor1Pin2, pwmChannel1);
  ledcAttachPin(motor2Pin1, pwmChannel2);
  ledcAttachPin(motor2Pin2, pwmChannel3);
  // подаем на контакты ENA и ENB 
  // ШИМ-сигнал с коэффициентом заполнения «0»:
  ledcWrite(pwmChannel0, dutyCycle);
  ledcWrite(pwmChannel1, dutyCycle);
  ledcWrite(pwmChannel2, dutyCycle);
  ledcWrite(pwmChannel3, dutyCycle);
  
  WiFi.softAP("ESP32-Robot");

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");  
  Serial.println(IP);
  
  camera = new OV7670(OV7670::Mode::QQQVGA_RGB565, SIOD, SIOC, VSYNC, HREF, XCLK, PCLK, D0, D1, D2, D3, D4, D5, D6, D7);
  BMP::construct16BitHeader(bmpHeader, camera->xres, camera->yres);
  
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(0);
  server.begin();
}

void displayRGB565(unsigned char * frame, int xres, int yres){
  tft.setAddrWindow(0, 0, yres - 1, xres - 1);
  int i = 0;
  for(int x = 0; x < xres; x++)
    for(int y = 0; y < yres; y++){
      i = (y * xres + x) << 1;
      tft.pushColor((frame[i] | (frame[i+1] << 8)));
    }  
}

void motorDrive(WiFiClient client){
         if (header.indexOf("GET /forward") >= 0) {
              ledcWrite(pwmChannel0, 0);
              ledcWrite(pwmChannel1, dutyCycle); 
              ledcWrite(pwmChannel2, 0);
              ledcWrite(pwmChannel3, dutyCycle);
              Serial.println("Forward");  //  "Вперед"
            }  else if (header.indexOf("GET /left") >= 0) {
              ledcWrite(pwmChannel0, 0);
              ledcWrite(pwmChannel1, 0);
              ledcWrite(pwmChannel2, 0);
              ledcWrite(pwmChannel3, dutyCycle); 
              Serial.println("Left");  //  "Влево"
            }  else if (header.indexOf("GET /stop") >= 0) {
              ledcWrite(pwmChannel0, 0);
              ledcWrite(pwmChannel1, 0);
              ledcWrite(pwmChannel2, 0);
              ledcWrite(pwmChannel3, 0);
              Serial.println("Stop");  //  "Стоп"            
            } else if (header.indexOf("GET /right") >= 0) {
              ledcWrite(pwmChannel0, 0);
              ledcWrite(pwmChannel1, dutyCycle);
              ledcWrite(pwmChannel2, 0);
              ledcWrite(pwmChannel3, 0); 
              Serial.println("Right");  //  "Вправо" 
            } else if (header.indexOf("GET /reverse") >= 0) {
              ledcWrite(pwmChannel0, dutyCycle);
              ledcWrite(pwmChannel1, 0);
              ledcWrite(pwmChannel2, dutyCycle);
              ledcWrite(pwmChannel3, 0);
              Serial.println("Reverse");  //  "Назад"         
            }
            //обновление кадра
            else if(header.indexOf("GET /camera") >= 0)
        {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:image/bmp");
            client.println();
            
            client.write(bmpHeader, BMP::headerSize);
            client.write(camera->frame, camera->xres * camera->yres * 2);
        }

        if(header.indexOf("GET /?value=")>=0) {
              pos1 = header.indexOf('=');
              pos2 = header.indexOf('&');
              valueString = header.substring(pos1+1, pos2);
              // Задаем скорость мотора:
              if (valueString == "0") {
                ledcWrite(pwmChannel0, dutyCycle);
                ledcWrite(pwmChannel1, dutyCycle);
                ledcWrite(pwmChannel2, dutyCycle);
                ledcWrite(pwmChannel3, dutyCycle);  
              }
              else { 
                dutyCycle = map(valueString.toInt(), 25, 100, 200, 255);
                ledcWrite(pwmChannel0, dutyCycle);
                ledcWrite(pwmChannel1, dutyCycle);
                ledcWrite(pwmChannel2, dutyCycle);
                ledcWrite(pwmChannel3, dutyCycle);
                Serial.println(valueString);
              } 
            }         
}

void htmlShow(){
    // Очищаем переменную «header»:
    header = "";
  WiFiClient client = server.available();  // Запускаем прослушку 
                                           // входящих клиентов.
  if (client) {                            // Если подключился 
                                           // новый клиент,
    Serial.println("New Client.");         // печатаем в монитор порта  
                                           // сообщение об этом.
    String currentLine = "";               // Создаем строку
                                           // для хранения данных,
                                           // пришедших от клиента.
    while (client.connected()) {           // Запускаем цикл while(), 
                                           // который будет работать,
                                           // пока клиент подключен.
      if (client.available()) {            // Если у клиента
                                           // есть байты, которые
                                           // можно прочесть,
        char c = client.read();            // считываем байт 
        Serial.write(c);                   // и печатаем его 
                                           // в мониторе порта.
        header += c;
        if (c == '\n') {                   // Если полученный байт – 
                                           // это символ новой строки.
          // Если мы получили два символа новой строки подряд,
          // то это значит, что текущая строка пуста.
          // Это конец HTTP-запроса клиента, поэтому отправляем ответ:
          if (currentLine.length() == 0) {
            // HTTP-заголовки всегда начинаются с кода ответа
            // (например, с «HTTP/1.1 200 OK»),
            // а также с информации о типе контента,
            // чтобы клиент знал, что получает.
            // После этого пишем пустую строчку:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
                       //  "Соединение: отключено"
            client.println();
            
            // Этот код отвечает за управление контактами моторов
            // согласно тому, какие нажаты кнопки на веб-странице:
              motorDrive(client);
            // Показываем веб-страницу:

            client.println("<!DOCTYPE HTML><html>");
            //изображение с камеры
            client.println("<style>body{margin: 0}\nimg{height: 100%; width: auto}</style>"
              "<img id='a' src='/camera' onload='this.style.display=\"initial\"; var b = document.getElementById(\"b\"); b.style.display=\"none\"; b.src=\"camera?\"+Date.now(); '>"
              "<img id='b' style='display: none' src='/camera' onload='this.style.display=\"initial\"; var a = document.getElementById(\"a\"); a.style.display=\"none\"; a.src=\"camera?\"+Date.now(); '>");
            
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // При помощи CSS задаем стиль кнопок.
            // Попробуйте поэкспериментировать
            // с атрибутами «background-color» и «font-size»,
            // чтобы стилизовать кнопки согласно своим предпочтениям: 
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { -webkit-user-select: none; -moz-user-select: none; -ms-user-select: none; user-select: none; background-color: #4CAF50;");
            client.println("border: none; color: white; padding: 12px 28px; text-decoration: none; font-size: 26px; margin: 1px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style>");
            client.println("<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js\"></script></head>");
            
            // веб-страница:        
            client.println("<p><button class=\"button\" onclick=\"moveForward()\">FORWARD</button></p>");
            client.println("<div style=\"clear: both;\"><p><button class=\"button\" onclick=\"moveLeft()\">LEFT </button>");
            client.println("<button class=\"button button2\" onclick=\"stopRobot()\">STOP</button>");
            client.println("<button class=\"button\" onclick=\"moveRight()\">RIGHT</button></p></div>");
            client.println("<p><button class=\"button\" onclick=\"moveReverse()\">REVERSE</button></p>");
            client.println("<p>Motor Speed: <span id=\"motorSpeed\"></span></p>");          
            client.println("<input type=\"range\" min=\"0\" max=\"100\" step=\"25\" id=\"motorSlider\" onchange=\"motorSpeed(this.value)\" value=\"" + valueString + "\"/>");
            
            client.println("<script>$.ajaxSetup({timeout:1000});");
            client.println("function moveForward() { $.get(\"/forward\"); {Connection: close};}");
            client.println("function moveLeft() { $.get(\"/left\"); {Connection: close};}");
            client.println("function stopRobot() {$.get(\"/stop\"); {Connection: close};}");
            client.println("function moveRight() { $.get(\"/right\"); {Connection: close};}");
            client.println("function moveReverse() { $.get(\"/reverse\"); {Connection: close};}");
            client.println("var slider = document.getElementById(\"motorSlider\");");
            client.println("var motorP = document.getElementById(\"motorSpeed\"); motorP.innerHTML = slider.value;");
            client.println("slider.oninput = function() { slider.value = this.value; motorP.innerHTML = this.value; }");
            client.println("function motorSpeed(pos) { $.get(\"/?value=\" + pos + \"&\"); {Connection: close};}</script>");
           
            client.println("</html>");
            
            // Пример HTTP-запроса: «GET /?value=100& HTTP/1.1»;
            // Он задает коэффициент заполнения ШИМ на 100% (255):



             
            // HTTP-ответ заканчивается еще одной пустой строкой:
            client.println();
            // Выходим из цикла while():
            break;
          } else {  // Если получили символ новой строки,
                    // то очищаем переменную «currentLine»:
            currentLine = "";
          }
        } else if (c != '\r') {  // Если получили что-либо,
                                 // кроме символа возврата каретки...
          currentLine += c;      // ...добавляем эти данные
                                 // в конец переменной «currentLine»
        }
      }
    }

    // Отключаем соединение:
    client.stop();
    Serial.println("Client disconnected.");  // "Клиент отключен."
    Serial.println("");
  }
}


void loop(){
  //с ними перестает норм работать....
  camera->oneFrame();
  displayRGB565(camera->frame, camera->xres, camera->yres);
    htmlShow();
}
