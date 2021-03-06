/*This is the code that will go on the front
door to control the deadbolt. It will consist
of three buttons on the deadbolt itself, and
control wires going to the relay on the access
controller. It has specific times the relay
is supposed to be open, to prevent people from
ripping off the controller and unlocking the door.
There will also be controls in openHAB to control
the deadbolt via mqtt. 
VERSION NOTES:
1. As of writing this, it is functioning flawlessly,
and I'm just waiting on my breakout pins to make
this a permanent and not-super-jenk operation.
2. Good comments need to be added as always.
//SIGNED//
JACK W. O'REILLY
23 Oct 2016*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>  //mqtt client library
#include <Servo.h>

// Update these with values suitable for your network.

const char* ssid = "oreilly";  //wifi ssid
const char* password = "9232616cc8";  //wifi password
const char* mqtt_server = "mqtt.oreillyj.com";  //server IP (port later defined)
int mqtt_port = 1883;
const char* mqtt_user = "papaya96";
const char* mqtt_pass = "24518000jJ";

const char* versionNum = "1.45";

const char* door_com = "osh/liv/door/com";
const char* test_com = "osh/all/test/com";
const char* lock_init = "osh/liv/door/persist";
const char* openhab_start = "osh/all/start";

const char* door_stat = "osh/liv/door/stat";
const char* test_stat = "osh/all/test/stat";
const char* openhab_stat = "osh/liv/door/openhab";
const char* allPub = "osh/all/stat";
const char* doorbell_stat = "osh/liv/door/doorbell";
const char* version_stat = "osh/liv/door/version";

bool lockState = HIGH;
bool lockTester = LOW;

int servoPin = 14;
int lockLed = 16;
int unlockLed = 5;
int doorSensor = 15;

#define buttonUnlock 0
#define buttonLock 13
#define buttonWait 12
#define accessRelay 2
#define doorBell 4

bool currentStateButton = LOW;
bool lastStateButton = LOW;

#define numButtons 5
char* buttonArray[numButtons] = {"0", "13", "12", "2", "0"};

int lockDegree = 127;
int counter = 0;
int waitVar = 1;
unsigned long loopTime = 0;
unsigned long referenceTime = 265;

Servo lockServo;

void lockDoor(int);
void dimmer(int);
void setup_wifi();
void callback(char*, byte*, unsigned int);
void buttonPress();

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

void setup()
{
  Serial.begin(115200);
    
  Serial.println();
  Serial.print("Door lock Pap Version: ");
  Serial.println(versionNum);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  pinMode(lockLed, OUTPUT);
  pinMode(unlockLed, OUTPUT);
  pinMode(buttonLock, INPUT_PULLUP);
  pinMode(buttonUnlock, INPUT_PULLUP);
  pinMode(buttonWait, INPUT_PULLUP);
  pinMode(accessRelay, INPUT_PULLUP);
  pinMode(doorSensor, INPUT_PULLUP);
  pinMode(doorBell, INPUT_PULLUP);

  analogWrite(unlockLed, 0);
  analogWrite(lockLed, 1023);
  //lockServo.attach(servoPin);
}

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);  //starts wifi connection

  while (WiFi.status() != WL_CONNECTED) {  //while wifi not connected...
    int i;
    Serial.print(".");
    analogWrite(lockLed, 0);  //turn LED off
    analogWrite(unlockLed, 0);
    for (i = 0; i <= 50; i++)
    {
       buttonPress();
       delay(10);
       yield();
    }
    analogWrite(lockLed, 1023);  //turn LED on
    analogWrite(unlockLed, 1023);
    for (i = 0; i <= 50; i++)
    {
       buttonPress();
       delay(10);
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());  //prints local ip address
}

void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (((char)payload[0] == '1') && !strcmp(topic, door_com))
  {
    delay(100);
    yield();
    lockDoor(1);
  }
  else if (((char)payload[0] == '0') && !strcmp(topic, door_com))
  {
    delay(100);
    yield();
    lockDoor(0);
  }
  else if (((char)payload[0] == '1') && !strcmp(topic, test_com))
  {
    client.publish(test_stat, "Living Room Door Controller is Online!");
    client.publish(openhab_stat, "ON");
    client.publish(version_stat, versionNum);
  }
  else if (((char)payload[0] == '0') && !strcmp(topic, lock_init))
  {
    analogWrite(unlockLed, 1023);
    analogWrite(lockLed, 0);
    lockState = LOW;
  }
  else if (((char)payload[0] == '1') && !strcmp(topic, openhab_start))
  {
    if (lockState)
    {
      client.publish(door_stat, "ON");
    }
    else if (!lockState)
    {
      client.publish(door_stat, "OFF");
    }
  }
}

void reconnect()  //this function is called repeatedly until mqtt is connected to broker
{
  while (!client.connected())  //while not connected...
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    client.connect("ESP8266Door", mqtt_user, mqtt_pass);  //connect funtion, must include ESP8266Client
    if (client.connected())  //if connected...
    {
      Serial.println("Connected!");
      client.subscribe(door_com);  //once connected, subscribe
      client.loop();
      client.subscribe(test_com);
      client.loop();
      client.subscribe(lock_init);
      client.loop();
      client.subscribe(openhab_start);
      client.loop();
      
      client.publish(allPub, "Door Lock just reconnected!");
      client.publish(openhab_stat, "ON");
      analogWrite(lockLed, 1023);
      analogWrite(unlockLed, 0);
    }
    else  //if we're not connected
    {
      Serial.print("failed, rc=");  //if failed, prints out the reason (look up rc codes in api header file)
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      if (WiFi.status() != WL_CONNECTED)  //if wifi is disconnected (could be reason for mqtt not connecting)
      {
        setup_wifi();  //calls wifi setup function
      }
      dimmer(1);  //dim the button LED on and off (around 5 second delay)
    }
  }
}

void loop()
{
  unsigned long int tester = millis();
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
  buttonPress();
  yield();
  if ((millis() - loopTime) >= 300000)
  {
    pinMode(lockLed, OUTPUT);
    pinMode(unlockLed, OUTPUT);
    pinMode(buttonLock, INPUT_PULLUP);
    pinMode(buttonUnlock, INPUT_PULLUP);
    pinMode(buttonWait, INPUT_PULLUP);
    pinMode(accessRelay, INPUT_PULLUP);
    pinMode(doorSensor, INPUT_PULLUP);
    pinMode(doorBell, INPUT_PULLUP);
    loopTime = millis();
    yield();
    client.loop();
  }
  if (lockTester)
  {
    delay(40);
    client.loop();
    lockTester = LOW;
  }
}

void lockDoor(int lockMode)  //door lock function
{
  if (lockMode)  //if parameter is a 1
  {
    client.publish(door_stat, "ON");
    client.publish(allPub, "Living Room Door is Locked");
    analogWrite(unlockLed, 0);
    analogWrite(lockLed, 1023);
    lockServo.attach(servoPin);
    delay(20);
    lockServo.write(0);
    Serial.println(0);
    delay(1250);
    lockServo.detach();
    lockState = HIGH;
    lockTester = HIGH;
  }
  else if (!lockMode)  //if 0 (unlock)
  {
    client.publish(door_stat, "OFF");
    client.publish(allPub, "Living Room Door is Unlocked");
    analogWrite(unlockLed, 1023);
    analogWrite(lockLed, 0);
    lockServo.attach(servoPin);
    delay(20);
    lockServo.write(lockDegree);
    Serial.println(lockDegree);
    delay(1250);
    lockServo.detach();
    lockState = LOW;
    lockTester = HIGH;
  }
}

void dimmer(int tester)  //dimmer function (for recognizing disconnect when in wall)
{
  int i;
  int pos;
  for (i = 0; i < 1023; i++) //loop from dark to bright in ~3.1 seconds
  {
    analogWrite(lockLed, i);  //set PWM rate
    analogWrite(unlockLed, i);
    if (tester)
    {
      buttonPress();
    }
    delay(3);  //ideal timing for dimming rate
    yield();
  }
  for (i = 1023; i > 0; i--) //slowly turn back off
  {
    analogWrite(lockLed, i);
    analogWrite(unlockLed, i);
    if (tester)
    {
      buttonPress();
    }
    delay(3);
    yield();
  }
}

void buttonPress()  //function that
{
  int i;
  for (i = 0; i < numButtons; i++)  //for loop that checks each button
  {
    int currentButton = atoi(buttonArray[i]);
    currentStateButton = digitalRead(currentButton);  //current state is reading the state of the button
    if (!currentStateButton)  //if the button is currently being pressed...
    {
      yield();
      switch (currentButton)  //switch statement where the argument is the pin number that is currently being pushed
      {
        case buttonLock:  //if it's the lock button...
          Serial.println("buttonLock");
          //client.publish(door_stat, "ON");  //publish that the door's locked
          lockDoor(1);  //lock the door
          break;
        case buttonUnlock:  //if unlock button..
          Serial.println("buttonUnlock");
          //client.publish(door_stat, "OFF");
          lockDoor(0);
          break;
        case accessRelay:
          Serial.println("accessRelay");
          delay(5);
          counter = 5;
          while (!digitalRead(accessRelay))
          {
            unsigned long int tester = millis();
            delay(5);
            yield();
            counter += (millis() - tester);
          }
          if ((counter >= 240) && (counter <= 260))
          {
            lockDoor(0);
          }
          else
          {
            delay(5000);
            yield();
          }
          yield();
          break;
        case buttonWait:
          Serial.println("buttonWait");
          /*waitVar = 1;
          delay(5000);
          while (waitVar <= 5)
          {
            if (!digitalRead(doorSensor))
            {
              lockDoor(1);
              waitVar = 6;
            }
            delay(3000);
            yield();
            waitVar++;
          }*/
          while (!digitalRead(buttonWait))
          {
            yield();
          }
          delay(30);
          dimmer(0);
          lockDoor(1);
          break;
        case doorBell:
          Serial.println("doorBell");
          client.publish(doorbell_stat, "ON");
          break;
       }
      while (!digitalRead(currentButton))  //while the current button is still being pressed...
      {
        yield();  //delay so the esp doesn't blow up/crash :)
      }
    }
  yield();
  }
}
