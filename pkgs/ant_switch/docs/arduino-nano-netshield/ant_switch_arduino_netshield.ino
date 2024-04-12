#include <UIPEthernet.h>
/*
 *  
 * Arduino nano + ENC29J60 ethernet shield antenna switch
 * 
 * 8 selecteable antennas in digital outputs d2,d3,d4,d5,d6,d7,d8,d9
 * 
 * curl http://192.168.0.19/?cmd=1 set antenna 1 (only)
 * curl http://192.168.0.19/?cmd=+1 add antenna 1
 * curl http://192.168.0.19/?cmd=-1 remove antenna 1
 * curl http://192.168.0.19/?cmd=t1 toggle antenna 1
 * curl http://192.168.0.19/?cmd=g ground all altennas
 * curl http://192.168.0.19/?cmd=s show selected antennas
 * 
 * This is part of KiwiSDR antenna switch extension
 * https://github.com/OH1KK/KiwiSDR-antenna-switch-extension
 * 
 * UIPEthernet library can be found from https://github.com/ntruchsess/arduino_uip
 */

EthernetServer server = EthernetServer(80);
boolean alreadyConnected = false; // whether or not the client was connected previously

void setup()
{
  for (int dio = 2; dio < 10; dio++) {
    pinMode(dio, OUTPUT);
    digitalWrite(dio, LOW);
  }

  Serial.begin(9600);

  uint8_t mac[6] = {0x00, 0x02, 0x2d, 0x01, 0x02, 0x03};

  // the dns server ip
  IPAddress dnServer(8, 8, 8, 8);
  // the router's gateway address:
  IPAddress gateway(192, 168, 0, 1);
  // the subnet:
  IPAddress subnet(255, 255, 255, 0);
  //the IP address is dependent on your network
  IPAddress ip(192, 168, 0, 19);

  Ethernet.begin(mac, ip, dnServer, gateway, subnet);

  server.begin();
}

void dio_groundall() {
  for (int tmp=1;tmp<9; tmp++) {
    dio_unset(tmp);
  }
}

void dio_selectonly(int dio) {
  dio_groundall();
  dio_set(dio);
}

void dio_set(int dio) {
  digitalWrite(dio+1, LOW);
}

void dio_unset(int dio) {
  digitalWrite(dio+1, HIGH);
}

void dio_toggle(int dio) {
  int dioReading = digitalRead(dio+1);
  if (dioReading == 0) {
    dio_unset(dio);
  } else {
    dio_set(dio);
  }
}

void loop() {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    //Serial.println("new client");
    String textline=""; 
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        //Serial.write(c);
        if (c == '\n') {
          if (textline.length()>11) {
            if (textline.substring(6, 10) == "cmd=") {
              String cmdtxt=textline.substring(10, 12);
              Serial.print("Command detected: ");
              Serial.println(cmdtxt);
              if (cmdtxt == "1 ") dio_selectonly(1);
              if (cmdtxt == "2 ") dio_selectonly(2);
              if (cmdtxt == "3 ") dio_selectonly(3);
              if (cmdtxt == "4 ") dio_selectonly(4);
              if (cmdtxt == "5 ") dio_selectonly(5);
              if (cmdtxt == "6 ") dio_selectonly(6);
              if (cmdtxt == "7 ") dio_selectonly(7);
              if (cmdtxt == "8 ") dio_selectonly(8);
              if (cmdtxt == "t1") dio_toggle(1);
              if (cmdtxt == "t2") dio_toggle(2);
              if (cmdtxt == "t3") dio_toggle(3);
              if (cmdtxt == "t4") dio_toggle(4);
              if (cmdtxt == "t5") dio_toggle(5);
              if (cmdtxt == "t6") dio_toggle(6);
              if (cmdtxt == "t7") dio_toggle(7);
              if (cmdtxt == "t8") dio_toggle(8);
              if (cmdtxt == "+1") dio_set(1);
              if (cmdtxt == "+2") dio_set(2);
              if (cmdtxt == "+3") dio_set(3);
              if (cmdtxt == "+4") dio_set(4);
              if (cmdtxt == "+5") dio_set(5);
              if (cmdtxt == "+6") dio_set(6);
              if (cmdtxt == "+7") dio_set(7);
              if (cmdtxt == "+8") dio_set(8);
              if (cmdtxt == "-1") dio_unset(1);
              if (cmdtxt == "-2") dio_unset(2);
              if (cmdtxt == "-3") dio_unset(3);
              if (cmdtxt == "-4") dio_unset(4);
              if (cmdtxt == "-5") dio_unset(5);
              if (cmdtxt == "-6") dio_unset(6);
              if (cmdtxt == "-7") dio_unset(7);
              if (cmdtxt == "-8") dio_unset(8);
              if (cmdtxt == "g ") dio_groundall();
              if (cmdtxt == "tg") dio_groundall();
            }
          }
          textline="";
        } else {
          textline+=c;
        }
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/plain");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println();
          // output the value of each analog input pin
          String selected_antennas;
          for (int dio = 2; dio < 10; dio++) {
            int dioReading = digitalRead(dio);
            if (dioReading == 0) {
              selected_antennas+=",";
              selected_antennas+=(dio-1);
            }
          }
          if (selected_antennas != "") {
            client.print(selected_antennas.substring(1));
          } else {
            client.print("g");
          }
          client.println("");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    //Serial.println("client disconnected");
  }
}
