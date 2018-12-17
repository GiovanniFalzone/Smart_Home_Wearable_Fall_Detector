#include <ESP8266WiFi.h>

//#define DEBUG_SERIAL

#ifdef DEBUG_SERIAL
	#define DEBUG_PRINT(x)  Serial.print (x)
	#define DEBUG_PRINTLN(x)  Serial.println (x)
#else
	#define DEBUG_PRINT(x)
	#define DEBUG_PRINTLN(x)
#endif

#define MAX_SRV_CLIENTS 1
#define SERVER_PORT 2048

#define RECONNECTION_ITERATIONS 10

const int server_port = 2048;
//const char * server_host = "192.168.43.245";
//const char *ssid = "wifidelvicino";
//const char *password = "b4bb4c4cc4&";
const char * server_host = "192.168.137.1";
const char *ssid = "mynetwork";
const char *password = "passwordsupersegreta";

WiFiClient Client;


void toggleBuiltinLed(uint8_t n){
  uint8_t led_status = LOW;
  for(uint8_t i=0; i<n; i++){
    digitalWrite(LED_BUILTIN, led_status);
    delay(1000/n);
    led_status = !led_status;
  }
}

void connectToWiFi(){
	if(WiFi.status() != WL_CONNECTED){
		DEBUG_PRINT("\nConnecting to "); DEBUG_PRINT(ssid); DEBUG_PRINT(" "); DEBUG_PRINTLN(password);
		WiFi.begin(ssid, password);
		while(WiFi.status() != WL_CONNECTED){
			toggleBuiltinLed(4);
			DEBUG_PRINT(".");
		}
	}
}

void ConnectToMiddleNode(){
	while (!Client.connect(server_host, server_port) || !Client.connected()) {
		toggleBuiltinLed(4);
		DEBUG_PRINTLN("connection failed");
	}
	delay(1000);
}

void InitHandshake(){
	while(Serial.available()) Serial.read();
	uint8_t count = 0;
	while((count < 4) || (Serial.available()==0)){
		Serial.write('G');
		if(Serial.available()){
			if(Serial.read()=='G') count++;
			else {
				count = 0;
				while(Serial.available()) Serial.read();
			}
		}
		toggleBuiltinLed(2);
	}
	for(count=0;count<4;count++)
		Serial.write('G');
	Serial.flush();
	while(Serial.available()) Serial.read();
}

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	toggleBuiltinLed(8);
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	connectToWiFi();
	ConnectToMiddleNode();
	InitHandshake();
	DEBUG_PRINT(WiFi.localIP()); DEBUG_PRINT(" on port "); DEBUG_PRINTLN(SERVER_PORT);
}

void loop() {
	uint32_t ser_ava;
	uint8_t Init = 0;

	while(!Client || !Client.connected()){
		Init=1;
		ConnectToMiddleNode();
	}
	if(Init==1){
		Init=0;
		InitHandshake();
	}

	ser_ava = Serial.available();	//check UART for data
	if(ser_ava){
		uint8_t sbuf[ser_ava];
		Serial.readBytes(sbuf, ser_ava);
		if (Client && Client.connected()){
			Client.write(sbuf, ser_ava);
			DEBUG_PRINTLN((char*)sbuf);
			delay(1);
		}
	}

	if(ser_ava == 128){	//buffer overflow
		DEBUG_PRINTLN("Received Data or Serial Buffer problem");
		toggleBuiltinLed(8);
	}
	// Read all the lines of the reply from server and print them to Serial
	while(Client.available()){
		Serial.write(Client.read());
	}
	delay(1);
}
