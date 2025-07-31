#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>


//################   DEFINES   ################
#define STEP_PIN        2
#define DIR_PIN         3
#define ENABLE_PIN      4
#define SWITCH_TOP      6
#define SWITCH_BOTTOM   7
#define TRIG_PIN        12
const char* ssid = "Testing Facility";
const char* password = "testingfacility";
const char* server_ip = "192.168.1.228";
const uint16_t server_port = 4851;
WiFiClient client;
int b_Homing_O = 0, w_Main_OV = 0, b_SingleStep_O = 0;
int previous_b_Homing_O = 0, previous_b_SingleStep_O = 0; // Use for detecting state changes
long pos_top = 0, pos_bottom = 0, current_position = 0, target_position = 0;
bool is_calibrated = false;
bool SingleStepDirection = LOW;
long ramp_steps = 150;
double speed = 0.0;
const int CALIBRATION_DELAY_US = 3500; // ~20 RPM
const int MIN_DELAY_US = 600;          // fast speed
const int MAX_DELAY_US = 3000;         // slow speed for start (not used)
const bool closing_direction = HIGH;
const bool opening_direction = LOW;


//################   FUNCTIONS PROTOTYPES   ################
void restart();
void runCalibration();
void singleStep(int delay_us);
void sendStatus(int value);
int current_positionAsPercent();
void moveToTarget();
void read_data(bool serial_on);


//################   SETUP   ################
void setup()
{
    Serial.begin(115200);
    delay(1500);
    Serial.print("Starting WiFi connection");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500); Serial.print(".");
    }
    Serial.println("\nWiFi connected!");
    Serial.print("SSID: "); Serial.println(ssid);
    delay(1000);
    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(ENABLE_PIN, OUTPUT);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(SWITCH_TOP, INPUT_PULLUP);
    pinMode(SWITCH_BOTTOM, INPUT_PULLUP);
    digitalWrite(ENABLE_PIN, LOW);
    digitalWrite(TRIG_PIN, LOW);
    if (client.connect(server_ip, server_port))
    {
        Serial.println("Connected to TCP server !");
    }
    else
    {
        Serial.println("TCP connection failed.");
        restart();
    }
}


//################   LOOP   ################
void loop()
{
    // Check WiFi connection
    if (!client.connected())
    {
        Serial.println("Reconnecting TCP in loop...");
        client.connect(server_ip, server_port);
        delay(500);
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500); Serial.print(".");
        }
        return;
    }
    // Check for incoming data
    read_data(false);
    // Calibration
    if (b_Homing_O == 1 and previous_b_Homing_O == 0)
    {
        runCalibration();
        delay(1000);
        return;
    }
    // Single step
    if (b_SingleStep_O == 1 and previous_b_SingleStep_O == 0)
    {
        SingleStepDirection = !SingleStepDirection; // Toggle direction
    }
    if (b_SingleStep_O == 1)
    {
        target_position += (SingleStepDirection ? 5 : -5);
        return;
    }
    // Position control
    if(is_calibrated)
    {
        target_position = map(w_Main_OV, 0, 100, pos_bottom, pos_top);
        if (target_position != current_position)
        {
            moveToTarget();
            digitalWrite(TRIG_PIN, HIGH);   // Trig for Kistler
        }
        else
        {
            //Serial.print("Current Position: ");
            //Serial.println(current_position);
            digitalWrite(TRIG_PIN, LOW);    // Trig for Kistler
        }
    }
}


//################   FUNCTIONS   ################
void restart()
{
    Serial.println("Restarting...");
    delay(1000);
    setup();
}
void runCalibration()
{
    Serial.println("Calibrating...");
    is_calibrated = false;
    current_position = 0;
    // Homing to top position
    digitalWrite(DIR_PIN, opening_direction);
    while (digitalRead(SWITCH_TOP))
    {
        read_data(true);
        singleStep(CALIBRATION_DELAY_US);
        current_position++;
    }
    pos_top = current_position;
    delay(100);
    // Homing to bottom position
    digitalWrite(DIR_PIN, closing_direction);
    while (digitalRead(SWITCH_BOTTOM))
    {
        read_data(true);
        singleStep(CALIBRATION_DELAY_US);
        current_position--;
    }
    pos_bottom = current_position;
    delay(300);
    Serial.println("Calibration done");
    Serial.print("pos_top: ");
    Serial.println(pos_top);
    Serial.print("pos_bottom: ");
    Serial.println(pos_bottom);
    is_calibrated = true;
    delay(500);
}
void singleStep(int delay_us = 20)
{
    if (delay_us < 20) delay_us = 20; // Ensure minimum delay
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(delay_us/2);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(delay_us/2);
}
void sendStatus(int value)
{
  if (!client.connected()) return;
  StaticJsonDocument<64> doc;
  doc["status_epos_e"] = value;
  serializeJson(doc, client);
  client.print('\n');
}
void moveToTarget()
{
    // direction
    bool dir = (target_position > current_position) ? opening_direction : closing_direction;
    digitalWrite(DIR_PIN, dir);

    // One step at correct speed
    long remaining_steps = abs(target_position - current_position);
    double desired_speed;
    if( remaining_steps < ramp_steps)
    {
        desired_speed = remaining_steps / ramp_steps ;
    }
    else
    {
        desired_speed = 1.0; // Full speed
    }

    if( desired_speed < speed )
    {
        speed -= 0.01; // Ramp down
    }
    else if ( desired_speed > speed )
    {
        speed += 0.01; // Ramp up
    }

    int delay_us = map(speed, 0, 1, MAX_DELAY_US, MIN_DELAY_US);
    singleStep(delay_us);
    current_position += (dir == opening_direction) ? 1 : -1;
}
int current_positionAsPercent()
{
  if (pos_top == pos_bottom) return 0;
  return map(current_position, pos_bottom, pos_top, 0, 100);
}
void read_data(bool serial_on = true)
{
    if (client.available())
    {
        String line = client.readStringUntil('\n');
        StaticJsonDocument<400> doc;
        if (!deserializeJson(doc, line))
        {
            previous_b_Homing_O = b_Homing_O;
            b_Homing_O      = doc["b_Homing_O"].as<int>();
            previous_b_SingleStep_O = b_SingleStep_O;
            b_SingleStep_O  = doc["b_SingleStep_O"].as<int>();
            w_Main_OV       = doc["w_Main_OV"].as<int>();
        }
        if(serial_on)
        {
            Serial.print("b_Homing_O: ");
            Serial.print(b_Homing_O);
            Serial.print(", b_SingleStep_O: ");
            Serial.print(b_SingleStep_O);
            Serial.print(", w_Main_OV: ");
            Serial.println(w_Main_OV);
        }
    }
    else
    {
        if (!client.connected())
        {
            reconnectTCP();
        }
        else
        {
            if(serial_on)
            {
                Serial.println("read data -> client not available");
            }
        }
    }
}
void reconnectTCP()
{
  client.stop();
  Serial.println("Reconnecting TCP…");
  if (client.connect(server_ip, server_port))
  {
    Serial.println("TCP reconnected");
  } else
  {
    Serial.println("TCP reconnect failed");
    delay(500);
  }
}