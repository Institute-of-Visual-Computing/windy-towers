//ble imports
#include "BLEDevice.h"
#include "BLE2902.h"
//wifi imports
#include <Arduino.h>
#include "settings.h"

#include <SimpleCLI.h>

#include "BTS7960.h"

//#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#include <WiFi.h>           // Core Wi-Fi functions for ESP32
#include <WiFiClient.h>     // Optional: if you use WiFiClient or HTTP libraries

#include <ArduinoOSCWiFi.h>

#include <ESP_DoubleResetDetector.h>      //https://github.com/khoih-prog/ESP_DoubleResetDetector
// Number of seconds after reset during which a 
// subseqent reset will be considered a double reset.
////////////////////////////////////////////////////////////////////////////////////WIFI/////////////////////////////////////////////////////////////////////////////////////////////
DoubleResetDetector* drd;
bool DRD_Detected = false;

BTS7960 fanController(D_OUT_ENABLE_FAN, PWM_OUT_LPWM, PWM_OUT_RPWM);

SimpleCLI cli;
Command cmdSpeed;
Command cmdDirection;
Command cmdSimulate;
Command cmdstopFans;
int _speed = 0;
int _direction = 0;
int _simulating = true;
int _stopFans = true;
int _autoFans = true;
bool usingwifi = false;
bool usingbt = true;
int counter = 0;
unsigned long WifilastMessage = 0;
unsigned long lastResponse = 0;

unsigned loopDelta = 0;
unsigned long lastMillis = 0;

char ssidBuffer[33] = "TP-Link_41F2";      // Default SSID
char passwordBuffer[65] = "46311467";  // Default password

char* DEFAULT_AP_SSID = ssidBuffer;
char* DEFAULT_AP_PASSWORD = passwordBuffer;

uint16_t connectedClientId = 0;


void turn(BTS7960::Direction direction, uint8_t speed)
{
    fanController.Turn(direction, speed);
    /*Serial.print("Direction: ");
    Serial.print((int) fanController.getDirection());
    Serial.print(" Speed: ");
    Serial.print(fanController.getPwmValue());
    Serial.print(" Simulate: ");
    Serial.println(simulating);*/
}
void statusinfo(){
    
    Serial.print(" Direction: ");
    //Serial.print((int) fanController.getDirection());
    //Serial.print(" : ");
    Serial.print(_direction);
    Serial.print(" Speed: ");
    //Serial.print(fanController.getPwmValue());
    //Serial.print(" : ");
    Serial.print(_speed);
    Serial.print(" Simulate: ");
    Serial.print(_simulating);
    Serial.print(" Stop Fans: ");
    Serial.print(_stopFans);
    Serial.print(" Auto Fans: ");
    Serial.print(_autoFans);
    Serial.print(" Using Wifi?: ");
    Serial.println(usingwifi);
    
}

void wifi_setup() {
    delay(100);

    Serial.println("WiFi Setup");

    // Handle DRD: reset WiFi credentials if a double reset was detected
    if (DRD_Detected) {
        Serial.println("Double reset detected: clearing WiFi settings");
        WiFi.disconnect(true); // Clear saved credentials
        delay(100);
    }

    // Use STA mode only (disable AP mode)
    WiFi.mode(WIFI_STA);

    // Set hostname (optional)
    WiFi.setHostname(hostName.c_str());

    Serial.print("Connecting to SSID: ");
    Serial.println(DEFAULT_AP_SSID);

    // Start connection
    WiFi.begin(DEFAULT_AP_SSID, DEFAULT_AP_PASSWORD);

    // Wait up to 10 seconds for connection
    int attempts = 0;
    const int maxAttempts = 100; // 100 x 100ms = 10 seconds
    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
        delay(100);
        Serial.print(".");
        attempts++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Connected to WiFi. IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        /*
        Serial.println("Failed to connect to WiFi. Starting AP mode...");

        // Start Access Point to allow manual connection or configuration
        WiFi.mode(WIFI_AP);
        WiFi.softAP(DEFAULT_AP_SSID, DEFAULT_AP_PASSWORD);

        Serial.print("AP Mode started. SSID: ");
        Serial.println(DEFAULT_AP_SSID);
        Serial.print("IP Address: ");
        Serial.println(WiFi.softAPIP());*/
    }

    Serial.println();
}

void cliErrorCallback(cmd_error* errorPtr) {
    CommandError e(errorPtr);

    Serial.println("ERROR: " + e.toString());

    if (e.hasCommand()) {
        Serial.println("Did you mean? " + e.getCommand().toString());
    } else {
        Serial.println(cli.toString());
    }
}

void cliSpeedCallback(cmd* cmdPtr) {
    Command cmd(cmdPtr);

    Argument argSpeed = cmd.getArgument("value");
    if(argSpeed.isSet())
    {
        String speedString = argSpeed.getValue();
        int speed = speedString.toInt();
        if(speed >= 0 && speed < 256)
        {
            turn(fanController.getDirection(), speed);
            _speed = speed;
        }
    }
}

void cliDirectionCallback(cmd* cmdPtr) {
    Command cmd(cmdPtr);

    Argument argDirection   = cmd.getArgument("value");
    if(argDirection.isSet())
    {
        String directionString = argDirection.getValue();
        int direction          = directionString.toInt();
        if(direction >= 0 && direction < 3)
        {
            turn((BTS7960::Direction) direction, fanController.getPwmValue());
            _direction = direction;
        }
    }
}

void cliSimulationCallback(cmd* cmdPtr) {
    Command cmd(cmdPtr);

    Argument argSimulation   = cmd.getArgument("value");
    if(argSimulation.isSet())
    {
        String simulateString = argSimulation.getValue();
        int simulate          = simulateString.toInt(); 
        if(simulate >= 0 && simulate < 2)
        {
            _simulating = simulate;
        }
    }
}
void clistopFansCallback(cmd* cmdPtr) {
    Command cmd(cmdPtr);

    Argument argstopFans   = cmd.getArgument("value");
    if(argstopFans.isSet())
    {
        String stopFansString = argstopFans.getValue();
        int fanStop          = stopFansString.toInt(); 
        if(fanStop >= 0 && fanStop < 2)
        {
            _stopFans = fanStop;
        }
    }
}
void cliautoFansCallback(cmd* cmdPtr) {
    Command cmd(cmdPtr);

    Argument argAutoFans   = cmd.getArgument("value");
    if(argAutoFans.isSet())
    {
        String autoFansString = argAutoFans.getValue();
        int autoFan          = autoFansString.toInt(); 
        if(autoFan >= 0 && autoFan < 2)
        {
            _autoFans = autoFan;
        }
    }
}

void cli_loop()
{
    if (Serial.available())
    {
        String input = Serial.readStringUntil('\n');
        Serial.print("# ");
        Serial.println(input);
        cli.parse(input);
    }
}

void cli_setup()
{
    cmdSpeed = cli.addCmd("s/peed", cliSpeedCallback);
    cmdSpeed.addPosArg("value");

    cmdDirection = cli.addCmd("d/irection", cliDirectionCallback);
    cmdDirection.addPosArg("value");

    cmdSimulate = cli.addCmd("s/imulate", cliSimulationCallback);
    cmdSimulate.addPosArg("value");

    cmdstopFans = cli.addCmd("s/topFans", clistopFansCallback);
    cmdstopFans.addPosArg("value");
  
    cli.setOnError(cliErrorCallback);

}

void oscReply(const String &remoteAddress)
{
    Serial.print(remoteAddress);
    Serial.println(" Reply");
    usingwifi = true;
    //statusinfo();
    WifilastMessage = millis();
    //OscWiFi.send(remoteAddress.c_str(), OSC_SEND_PORT, "/fan/speed/state", (int)fanController.getPwmValue());
    //OscWiFi.send(remoteAddress.c_str(), OSC_SEND_PORT, "/fan/direction/state", (int)fanController.getDirection());
    OscWiFi.send(remoteAddress.c_str(), OSC_SEND_PORT, "/fan/connected/state", true);
}

void oscSpeedCallback(const OscMessage& m)
{
    if(m.isInt32(0))
    {
        int val = m.arg<int>(0);
        if(val >= 0 && val < 256)
        {
            //turn(fanController.getDirection(), val);
            _speed = val;
        }
        oscReply(m.remoteIP());   
    }
}

void oscDirectionCallback(const OscMessage& m)
{
    if(m.isInt32(0))
    {
        int val = m.arg<int>(0);
        if(val >= 0 && val < 3)
        {
            //turn((BTS7960::Direction)val, fanController.getPwmValue());
            _direction = val;
        }
        oscReply(m.remoteIP());
    }
}
void oscSimulationCallback(const OscMessage& m)
{
    if(m.isInt32(0))
    {
        int val = m.arg<int>(0);
        if(val >= 0 && val < 2)
        {
            _simulating = val;
        }
        oscReply(m.remoteIP());
    }
}
void oscStopFansCallback(const OscMessage& m)
{
    if(m.isInt32(0))
    {
        int val = m.arg<int>(0);
        if(val >= 0 && val < 2)
        {
            _stopFans = val;
        }
        oscReply(m.remoteIP());
    }
}

void oscAutoFansCallback(const OscMessage& m)
{
    if(m.isInt32(0))
    {
        int val = m.arg<int>(0);
        if(val >= 0 && val < 2)
        {
            _autoFans = val;
        }
        oscReply(m.remoteIP());
    }
}

void osc_setup()
{
    OscWiFi.subscribe(OSC_LISTENER_PORT, "/fan/speed/set", oscSpeedCallback);
    OscWiFi.subscribe(OSC_LISTENER_PORT, "/fan/direction/set", oscDirectionCallback);
    OscWiFi.subscribe(OSC_LISTENER_PORT, "/fan/simulate/set", oscSimulationCallback);
    OscWiFi.subscribe(OSC_LISTENER_PORT, "/fan/stopFans/set", oscStopFansCallback);
    OscWiFi.subscribe(OSC_LISTENER_PORT, "/fan/autoFans/set", oscAutoFansCallback);
}

////////////////////////////////////////////////////////////////////////////////////BT/////////////////////////////////////////////////////////////////////////////////////////////

// pin 2 on the Hiletgo
// (can be turned on/off from the iPhone app)
const uint32_t led = 2;
unsigned long BTlastMessage = 0;
// pin 5 on the RGB shield is button 1
// (button press will be shown on the iPhone app)
const uint32_t button = 13;

static BLEUUID serviceUUID("A9E90000-194C-4523-A473-5FDF36AA4D20");
static BLEUUID ledUUID("A9E90001-194C-4523-A473-5FDF36AA4D20");
static BLEUUID buttonUUID("A9E90002-194C-4523-A473-5FDF36AA4D20");

bool deviceConnected = false;
bool oldDeviceConnected = false;

bool lastButtonState = false;

BLEServer* pServer = 0;
BLECharacteristic* pCharacteristicCommand = 0;
BLECharacteristic* pCharacteristicData = 0;

class BTServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer* pServer)
{
    Serial.println("Connected...");
    deviceConnected = true;
};

void onDisconnect(BLEServer* pServer)
{
    Serial.println("Disconnected...");
    deviceConnected = false;

    // don't leave the led on if they disconnect
    digitalWrite(led, LOW);
}
};


class BTCallbacks : public BLECharacteristicCallbacks
{
    void onRead(BLECharacteristic* pCharacteristic)
{
}

void onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    int len = value.length();
    if (len == 0) {
        Serial.println("Empty BLE message");
        return;
    }

    const uint8_t* data = (const uint8_t*)value.data();
    uint8_t commandType = data[0];

    switch (commandType) {
        case 0x01: { // Control message: speed, direction, etc.
            BTlastMessage = millis();
            //Serial.println("Recieved fancontrol message");
            if (len != 21) { // 1 byte for type + 4 x 4-byte ints = 17
                Serial.println("Invalid control message length.");
                return;
            }
            //Serial.println("Valid control message length.");
            int BTspeed, BTdirection;
            memcpy(&BTspeed, &data[1], 4);
            memcpy(&_stopFans, &data[5], 4);
            memcpy(&BTdirection, &data[9], 4);
            memcpy(&_simulating, &data[13], 4);
            memcpy(&_autoFans, &data[17], 4);
            /*Serial.println("----- Parsed BLE Data -----");
            Serial.print("BTspeed     : "); Serial.println(BTspeed);
            Serial.print("_stopFans   : "); Serial.println(_stopFans);
            Serial.print("BTdirection : "); Serial.println(BTdirection);
            Serial.print("_simulating : "); Serial.println(_simulating);
            Serial.print("_autoFans   : "); Serial.println(_autoFans);
            Serial.println("Raw BLE payload:");*/
            /*for (int i = 0; i < len; ++i) {
                Serial.printf("[%02d] 0x%02X\n", i, data[i]);
            }
            Serial.println("---------------------------");*/
            if (BTspeed >= 0 && BTspeed < 256) {
                //turn(fanController.getDirection(), BTspeed);
                _speed = BTspeed;
                
            }
            if (BTdirection >= 0 && BTdirection < 3) {
                //turn((BTS7960::Direction)BTdirection, fanController.getPwmValue());
                _direction = BTdirection;
            }
            //statusinfo();
            break;
        }

        case 0x02: { // Wi-Fi credentials message
            BTlastMessage = millis();
            Serial.println("Recieved WifiData message");
            if (len < 4) {
                Serial.println("Wi-Fi data too short");
                return;
            }

            int offset = 1;
            uint8_t ssidLen = data[offset++];
            if (ssidLen > 32 || offset + ssidLen >= len) {
                Serial.println("Invalid SSID length");
                return;
            }

            memcpy(ssidBuffer, data + offset, ssidLen);
            ssidBuffer[ssidLen] = '\0';  // Null-terminate
            offset += ssidLen;

            uint8_t passLen = data[offset++];
            if (passLen > 64 || offset + passLen > len) {
                Serial.println("Invalid password length");
                return;
            }

            memcpy(passwordBuffer, data + offset, passLen);
            passwordBuffer[passLen] = '\0';  // Null-terminate
            
            Serial.print("Received SSID: ");
            Serial.println(DEFAULT_AP_SSID);
            Serial.print("Received Password: ");
            Serial.println(DEFAULT_AP_PASSWORD);
            WiFi.disconnect(true); // Clear saved credentials
            wifi_setup();
            break;
        }
        case 0x03: { // Wi-Fi credentials message
            BTlastMessage = millis();
            Serial.println("Recieved Disconnnect message");

            uint8_t dc = data[1];
            if(dc == 0x01){
                WiFi.disconnect(true); // Clear saved credentials
                pServer->disconnect(connectedClientId);
                Serial.println("Disconnected!");
                delay(5000);
                ESP.restart();
            }
            
            
            break;
        }


        default:
            Serial.println("Unknown command type");
            break;
    }
}
};

// debounce time (in ms)
int debounce_time = 10;

// maximum debounce timeout (in ms)
int debounce_timeout = 100;

void BluetoothStartAdvertising()
{
    if (pServer != 0)
    {
        BLEAdvertising* pAdvertising = pServer->getAdvertising();
        pAdvertising->start();
    }
}

void BluetoothStopAdvertising()
{
    if (pServer != 0)
    {
        BLEAdvertising* pAdvertising = pServer->getAdvertising();
        pAdvertising->stop();
    }
}

void BTsetup()
{
    Serial.begin(115200);

    // led turned on/off from the iPhone app
    pinMode(led, OUTPUT);

    // button press will be shown on the iPhone app)
    pinMode(button, INPUT);

    BLEDevice::init("ledbtn");
    Serial.println("init ledbtn");
    // BLEDevice::setCustomGattsHandler(my_gatts_event_handler);
    // BLEDevice::setCustomGattcHandler(my_gattc_event_handler);

    pServer = BLEDevice::createServer();
    BLEService* pService = pServer->createService(serviceUUID);
    pServer->setCallbacks(new BTServerCallbacks());

    pCharacteristicCommand = pService->createCharacteristic(
        buttonUUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY);

    pCharacteristicCommand->setCallbacks(new BTCallbacks());
    pCharacteristicCommand->setValue("");
    pCharacteristicCommand->addDescriptor(new BLE2902());

    pCharacteristicData = pService->createCharacteristic(
        ledUUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY);

    pCharacteristicData->setCallbacks(new BTCallbacks());
    pCharacteristicData->setValue("");
    pCharacteristicData->addDescriptor(new BLE2902());

    pService->start();
    BluetoothStartAdvertising();
}


void BTloop()
{
    if (pServer != 0)
    {
        // disconnecting
        if (!deviceConnected && oldDeviceConnected)
        {
            delay(500);                  // give the bluetooth stack the chance to get things ready
            pServer->startAdvertising(); // restart advertising
            Serial.println("start advertising");
            oldDeviceConnected = deviceConnected;
        }

        // connecting
        if (deviceConnected && !oldDeviceConnected)
        {
            oldDeviceConnected = deviceConnected;
        }

        uint8_t buttonState = digitalRead(button);
        
        if (deviceConnected && pCharacteristicCommand != 0 && buttonState != lastButtonState)
        {
            lastButtonState = buttonState;

            uint8_t packet[1];
            packet[0] = buttonState == HIGH ? 0x00 : 0x01;
            pCharacteristicCommand->setValue(packet, 1);
            pCharacteristicCommand->notify();
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////General/////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
    Serial.begin(115200);

    drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
    if (drd->detectDoubleReset()) 
    {
        Serial.println("Double Reset Detected");
        DRD_Detected = true;
    } 

    // put your setup code here, to run once:
    // pinMode(BTN_WIFI_RESET, INPUT_PULLUP);
    pinMode(LED_BUILTIN, OUTPUT);
    //pinMode(A_IN_RPWM, INPUT_PULLDOWN);
    Serial.println("I am " + hostName);
    cli_setup();
    #ifdef USE_WIFI
      wifi_setup();
      osc_setup();
    #endif
    BTsetup();
    
    Serial.println("Setup done");
    lastMillis = millis();
    lastResponse = millis();
}

float sinusCurveforSimulating(float y){
    float x;
    x = sin(y*20)/16 + sin(y*25+PI)/20 + sin(y*28+PI)/20 + sin(y*39)/20 * sin(y*20)/2 * (sin(y*20)/1) * 5+0.5;
    return x;
}
void checkLastResponse(){
    if(WifilastMessage < (millis()-10000) && BTlastMessage < (millis()-10000)){
        if(_autoFans){
            _simulating = true;
            _stopFans = false;
        }else{
            _speed = 0;
        }
        if(WifilastMessage > BTlastMessage){
            lastResponse = WifilastMessage;
        }else{
            lastResponse = BTlastMessage;
        }
        /*Serial.print(lastResponse);
        Serial.print(" last respones::last message ");
        Serial.print(lastMessage); */
    }
}
void checkstopFans(){
    if(_stopFans){
        turn((BTS7960::Direction)1, 0);
    }
    
}
void checkSimulating(){
    if (_simulating && !_stopFans)
    {
        float result;
        float adjusted;
        float remap;
        ++counter;
        adjusted = counter%((int)(PI*200));
        if (adjusted != 0){
            adjusted = adjusted/100;
        }
        result = sinusCurveforSimulating(adjusted);
        if(result < 0.5){
            result = 0.5;
        }
        result = result*100;
        
        remap = map(result, 50,70,0,255);
        turn((BTS7960::Direction)1, remap);
        //fanController.Turn((BTS7960::Direction)1, 163);
       
        /*Serial.print(" SineCurveCounter: ");
        Serial.print(counter);
        Serial.print(" SineCurveAdjust: ");
        Serial.print(adjusted);
        Serial.print(" SineCurveResult: ");
        Serial.print(result);
        Serial.print(" SineCurveRemap: ");
        Serial.println(remap);*/

        delay(150);                             // plus the 100 delay from loop = 250 aka 0.25s
        //statusinfo();
    }
}
void SendtoFan() {
    if(!_stopFans && !_simulating){
        turn((BTS7960::Direction)_direction, _speed);
    }else if (_simulating && !_stopFans ){
        float result;
        float adjusted;
        float remap;
        ++counter;
        adjusted = counter%((int)(PI*200));
        if (adjusted != 0){
            adjusted = adjusted/100;
        }
        result = sinusCurveforSimulating(adjusted);
        if(result < 0.5){
            result = 0.5;
        }
        result = result*100;
        
        remap = map(result, 50,70,0,255);
        _speed = remap;
        turn((BTS7960::Direction)1, remap);
        //delay(150);
    }else if(_stopFans){
        turn((BTS7960::Direction)1, 0);
        _speed = 0;
    }
    
}

void loop() {
    loopDelta = millis() - lastMillis;
    lastMillis = millis();
    drd->loop();
    cli_loop();
    if(!usingwifi){
        BTloop();
    }
    
    #ifdef USE_WIFI
        OscWiFi.update();
    #endif
    
    checkLastResponse();
    SendtoFan();
    statusinfo();
    //checkSimulating();
    //checkstopFans();
    delay(1000);
}
