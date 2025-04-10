#include <Arduino.h>
#include "settings.h"

#include <SimpleCLI.h>

#include "BTS7960.h"

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#include <ArduinoOSCWiFi.h>

#include <ESP_DoubleResetDetector.h>      //https://github.com/khoih-prog/ESP_DoubleResetDetector
// Number of seconds after reset during which a 
// subseqent reset will be considered a double reset.

DoubleResetDetector* drd;
bool DRD_Detected = false;

BTS7960 fanController(D_OUT_ENABLE_FAN, PWM_OUT_LPWM, PWM_OUT_RPWM);

SimpleCLI cli;
Command cmdSpeed;
Command cmdDirection;
Command cmdSimulate;
bool simulating;

unsigned loopDelta = 0;
unsigned long lastMillis = 0;

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
    Serial.print((int) fanController.getDirection());
    Serial.print(" Speed: ");
    Serial.print(fanController.getPwmValue());
    Serial.print(" Simulate: ");
    Serial.println(simulating);
}

void wifi_setup()
{
    // WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.

    // put your setup code here, to run once:
    
    // pinMode(BTN_WIFI_RESET, INPUT_PULLUP);
    delay(100);
    int countDown = 10 * 1000;
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;
    wm.setHostname(hostName.c_str());
    if(DRD_Detected)
    {
        wm.resetSettings();
        delay(100);
    }

    bool res;
    Serial.println("AutoConnect");
    res = wm.autoConnect(configApSSID.c_str(), configApPW.c_str()); // password protected ap
    //res = wm.autoConnect("RATATATATA", "12345678");
    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    // wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result


    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
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
            simulating = simulate;
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
  
    cli.setOnError(cliErrorCallback);

}

void oscReply(const String &remoteAddress)
{
    Serial.print(remoteAddress);
    statusinfo();
    OscWiFi.send(remoteAddress.c_str(), OSC_SEND_PORT, "/fan/speed/state", (int)fanController.getPwmValue());
    OscWiFi.send(remoteAddress.c_str(), OSC_SEND_PORT, "/fan/direction/state", (int)fanController.getDirection());
}

void oscSpeedCallback(const OscMessage& m)
{
    if(m.isInt32(0))
    {
        int val = m.arg<int>(0);
        if(val >= 0 && val < 256)
        {
            turn(fanController.getDirection(), val);
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
            turn((BTS7960::Direction)val, fanController.getPwmValue());
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
            simulating = val;
        }
        oscReply(m.remoteIP());
    }
}

void osc_setup()
{
    OscWiFi.subscribe(OSC_LISTENER_PORT, "/fan/speed/set", oscSpeedCallback);
    OscWiFi.subscribe(OSC_LISTENER_PORT, "/fan/direction/set", oscDirectionCallback);
    OscWiFi.subscribe(OSC_LISTENER_PORT, "/fan/simulate/set", oscSimulationCallback);
}

void setup() {
    Serial.begin(9600);

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
    Serial.println("Setup done");
    lastMillis = millis();
}

void loop() {
    loopDelta = millis() - lastMillis;
    lastMillis = millis();
    drd->loop();
    cli_loop();
    #ifdef USE_WIFI
        OscWiFi.update();
    #endif
    delay(100);
}