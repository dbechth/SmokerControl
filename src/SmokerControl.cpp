
#include <EEPROM.h>
#include "ThermostatControl.h"
#include "AC2.h"
#include <WiFiClient.h>
#include <SPI.h>
#include <Wire.h>
#include <Servo.h>
#include "max6675.h"
#include "smokerIDX.h"

//Inkbird stuff
#include "InkbirdCom.h"

int thermoDO = 12;//D4;
int thermoCS = 13;//D5;
int thermoCLK = 14;//D6;

MAX6675 smokechamberthermocouple(thermoCLK, thermoCS, thermoDO);
MAX6675 fireboxthermocouple(thermoCLK, thermoCS, thermoDO);

Servo InletDamper;  // create servo object to control a servo

//OS
#define Task100mS  100
#define Task1S  1000
#define Task1m  60000
unsigned long lastTime = 0;
unsigned long timeNow = 0;
unsigned long Time1S = 0;
unsigned long Time1m = 0;
//File file;

constexpr auto ControllerName = "Smoker";

int DamperSetpoint = 100;
int ServoCMD;
int prevServoCMD;
int servoWriteTimer;
bool servoUpdateRequired;
bool autoMode = true;
bool startup = true;
bool shutdown = false;
bool inIdle = false;
int transIdleToHeat = 0;
int transIdleToCool = 0;

int hotPCT = 0;
int idlePCT = 20;
int coldPCT = 100;
int manualPCT = 50;
int minPW = 40;
int maxPW = 120;
int preheatOffset = 50;
int autoIdleTuneThreshold = 2;

struct stChartData
{
	int temperature;
	int damperPCT;
	int setpoint;
	int Probe1;
	int Probe2;
	int Probe3;
	int Probe4;
};
stChartData chartdata[1024];
int chartdataIndex = 0;

enum RunMode
{
	Setup,
	Init,
	Run
};
RunMode runMode = Setup;

//WiFi
char ssid[] = "";
char pass[] = "";
WiFiClient client;

String BuildJSON(String paramName, int Value)
{
	return "\"" + paramName + "\":" + Value;
}

void packChartData()
{
	String tmpBuffer = "{\"SmokerData\":[";
	for (int i = 0; i < chartdataIndex; i++)
	{
		if (i>0)
		{
			tmpBuffer += ",";
		}

		tmpBuffer += "{" +
			BuildJSON("index", i) + "," +
			BuildJSON("temperature", chartdata[i].temperature) + "," +
			BuildJSON("damperPCT", chartdata[i].damperPCT) + "," +
			BuildJSON("setpoint", chartdata[i].setpoint) + "," +
			BuildJSON("Probe1", chartdata[i].Probe1) + "," +
			BuildJSON("Probe2", chartdata[i].Probe2) + "," +
			BuildJSON("Probe3", chartdata[i].Probe3) + "," +
			BuildJSON("Probe4", chartdata[i].Probe4) +
			"}" ;
	}
	//tmpBuffer += file.read();
	tmpBuffer += "]}";

	AC2.webserver.send(200, "text/plain", tmpBuffer);
}
void packLastChartData()
{
	int idx = chartdataIndex-1;
	if (idx < 0)
	{
		idx = 0;
	}
	String tmpBuffer = "{\"SmokerData\":[";
		tmpBuffer += "{" +
			BuildJSON("index", idx) + "," +
			BuildJSON("temperature", chartdata[idx].temperature) + "," +
			BuildJSON("damperPCT", chartdata[idx].damperPCT) + "," +
			BuildJSON("setpoint", chartdata[idx].setpoint) + "," +
			BuildJSON("Probe1", chartdata[idx].Probe1) + "," +
			BuildJSON("Probe2", chartdata[idx].Probe2) + "," +
			BuildJSON("Probe3", chartdata[idx].Probe3) + "," +
			BuildJSON("Probe4", chartdata[idx].Probe4) +
			"}" ;
	tmpBuffer += "]}";

	AC2.webserver.send(200, "text/plain", tmpBuffer);
}

void mainPage()
{
	AC2.webserver.send(200, "text/html", smokerIDX);
}

void get()
{
	//read all local values and send them to the webpage
	if (AC2.webserver.args() > 0)
	{
		if (AC2.webserver.hasArg("smokerTemp"))
		{
			AC2.webserver.send(200, "text/plain", String(ThermostatControl.temperature, 2));
		}
		else if (AC2.webserver.hasArg("damperPCT"))
		{
			AC2.webserver.send(200, "text/plain", String(map(ServoCMD, minPW, maxPW, 0, 100)));
		}
		else if (AC2.webserver.hasArg("autoON"))
		{
			AC2.webserver.send(200, "text/plain", String(autoMode));
		}
		else if (AC2.webserver.hasArg("manualON"))
		{
			AC2.webserver.send(200, "text/plain", String(!autoMode));
		}
		else if (AC2.webserver.hasArg("startup"))
		{
			AC2.webserver.send(200, "text/plain", String(startup));
		}
		else if (AC2.webserver.hasArg("shutdown"))
		{
			AC2.webserver.send(200, "text/plain", String(shutdown));
		}
		else if (AC2.webserver.hasArg("setPointF"))
		{
			AC2.webserver.send(200, "text/plain", String(ThermostatControl.setpoint));
		}
		else if (AC2.webserver.hasArg("setPointDeadband"))
		{
			AC2.webserver.send(200, "text/plain", String(ThermostatControl.hysteresis));
		}
		else if (AC2.webserver.hasArg("hotPCT"))
		{
			AC2.webserver.send(200, "text/plain", String(hotPCT));
		}
		else if (AC2.webserver.hasArg("idlePCT"))
		{
			AC2.webserver.send(200, "text/plain", String(idlePCT));
		}
		else if (AC2.webserver.hasArg("coldPCT"))
		{
			AC2.webserver.send(200, "text/plain", String(coldPCT));
		}
		else if (AC2.webserver.hasArg("manualPCT"))
		{
			AC2.webserver.send(200, "text/plain", String(manualPCT));
		}
		else if (AC2.webserver.hasArg("minPW"))
		{
			AC2.webserver.send(200, "text/plain", String(minPW));
		}
		else if (AC2.webserver.hasArg("maxPW"))
		{
			AC2.webserver.send(200, "text/plain", String(maxPW));
		}
		else if (AC2.webserver.hasArg("preheatOffset"))
		{
			AC2.webserver.send(200, "text/plain", String(preheatOffset));
		}
		else if (AC2.webserver.hasArg("autoIdleTuneThreshold"))
		{
			AC2.webserver.send(200, "text/plain", String(autoIdleTuneThreshold));
		}
		else if (AC2.webserver.hasArg("transIdleToCool"))
		{
			AC2.webserver.send(200, "text/plain", String(transIdleToCool));
		}
		else if (AC2.webserver.hasArg("transIdleToHeat"))
		{
			AC2.webserver.send(200, "text/plain", String(transIdleToHeat));
		}
		else if (AC2.webserver.hasArg("chartdata"))
		{
			packChartData();
		}
		else if (AC2.webserver.hasArg("lastchartdata"))
		{
			packLastChartData();
		}

	}
}

void set()
{
	if (AC2.webserver.args() > 0)
	{
		if (AC2.webserver.hasArg("autoON"))
		{
			autoMode = true;
		}
		else if (AC2.webserver.hasArg("manualON"))
		{
			autoMode = false;
			startup = false;
			shutdown = false;
		}
		else if (AC2.webserver.hasArg("startup"))
		{
			startup = !startup;
			if (startup)
			{
				autoMode = true;
				shutdown = false;
			}

		}
		else if (AC2.webserver.hasArg("shutdown"))
		{
			shutdown = !shutdown;
			if (shutdown)
			{
				autoMode = true;
				startup = false;
			}
		}
		else if (AC2.webserver.hasArg("setPointF"))
		{
			ThermostatControl.setpoint = AC2.webserver.arg("setPointF").toFloat();

			uint8_t tempvar = (uint8_t)(ThermostatControl.setpoint / 10.0);
			EEPROM.write(0, tempvar);
			EEPROM.commit();
		}
		else if (AC2.webserver.hasArg("setPointDeadband"))
		{
			ThermostatControl.hysteresis = AC2.webserver.arg("setPointDeadband").toFloat();
			uint8_t tempvar = (uint8_t)(ThermostatControl.hysteresis * 10.0);
			EEPROM.write(1, tempvar);
			EEPROM.commit();
		}
		else if (AC2.webserver.hasArg("hotPCT"))
		{
			hotPCT = AC2.webserver.arg("hotPCT").toInt();
			EEPROM.write(2, hotPCT);
			EEPROM.commit();
		}
		else if (AC2.webserver.hasArg("idlePCT"))
		{
			idlePCT = AC2.webserver.arg("idlePCT").toInt();
			EEPROM.write(3, idlePCT);
			EEPROM.commit();
		}
		else if (AC2.webserver.hasArg("coldPCT"))
		{
			coldPCT = AC2.webserver.arg("coldPCT").toInt();
			EEPROM.write(4, coldPCT);
			EEPROM.commit();
		}
		else if (AC2.webserver.hasArg("manualPCT"))
		{
			manualPCT = AC2.webserver.arg("manualPCT").toInt();
			EEPROM.write(5, manualPCT);
			EEPROM.commit();
		}
		else if (AC2.webserver.hasArg("minPW"))
		{
			minPW = AC2.webserver.arg("minPW").toInt();
			EEPROM.write(6, minPW);
			EEPROM.commit();
		}
		else if (AC2.webserver.hasArg("maxPW"))
		{
			maxPW = AC2.webserver.arg("maxPW").toInt();
			EEPROM.write(7, maxPW);
			EEPROM.commit();
		}
		else if (AC2.webserver.hasArg("preheatOffset"))
		{
			preheatOffset = AC2.webserver.arg("preheatOffset").toInt();
			EEPROM.write(8, preheatOffset);
			EEPROM.commit();
		}
		else if (AC2.webserver.hasArg("autoIdleTuneThreshold"))
		{
			autoIdleTuneThreshold = AC2.webserver.arg("autoIdleTuneThreshold").toInt();
			EEPROM.write(9, autoIdleTuneThreshold);
			EEPROM.commit();
		}
		else if (AC2.webserver.hasArg("resetDefaults"))
		{
			AC2.webserver.arg("resetDefaults") == "reset";
			EEPROM.write(10, true);
			EEPROM.commit();
			ESP.restart();
		}
	}
	AC2.webserver.send(200, "text/html", "ok");
}

boolean RunSetup()
{
	return true;
}

boolean RunInit()
{
	return true;
}

void RunTasks()
{

	static float temperature = 0;
	AC2.task(); //This application manages its own taskrate.

	timeNow = millis();
	unsigned long elapsedTime = timeNow - lastTime;
	if (elapsedTime >= Task100mS) {
		lastTime = timeNow;
		Time1S += elapsedTime;
		Time1m += elapsedTime;
			//ESP_LOGI("BBQ", "Doing a loop.");
			// If the flag "doConnect" is true then we have scanned for and found the desired
			// BLE Server with which we wish to connect.  Now we connect to it.  Once we are
			// connected we set the connected flag to be true.

			if (doConnect == true)
			{
				if (connectToBLEServer(*pServerAddress))
				{
				ESP_LOGI("BBQ", "We are now connected to the BLE Server.");
				connected = true;
				doConnect = false;// added to prevent searching a second time just to fail
				}
				else
				{
				ESP_LOGI("BBQ", "We have failed to connect to the server; there is nothin more we will do.");
				}
				doConnect = false;
			}
	}
	if (Time1S >= Task1S) {
		temperature = smokechamberthermocouple.readFahrenheit();

		ThermostatControl.temperature = temperature;
		ThermostatControl.task();

		if (startup)
		{
			ServoCMD = maxPW;
			if (ThermostatControl.temperature >= (ThermostatControl.setpoint + preheatOffset))
			{
				startup = false;
				shutdown = false;
			}
		}
		else if (shutdown)
		{
			ServoCMD = minPW;
		}
		else
		{
			if (autoMode)
			{

				if (ThermostatControl.output == ThermostatControl.cmdHeat)
				{
					if (inIdle)
					{
						inIdle = false;
						transIdleToHeat++;
					}
					if (DamperSetpoint < coldPCT)
					{
						DamperSetpoint = DamperSetpoint + 5;
					}
				}
				else if (ThermostatControl.output == ThermostatControl.cmdCool)
				{
					if (inIdle)
					{
						inIdle = false;
						transIdleToCool++;
					}
					if (DamperSetpoint > hotPCT)
					{
						DamperSetpoint = DamperSetpoint - 5;
					}
				}
				else
				{
					inIdle = true;
					DamperSetpoint = idlePCT;
				}
				if ((transIdleToHeat - transIdleToCool) >= autoIdleTuneThreshold)
				{
					idlePCT++;
					transIdleToHeat = 0;
					transIdleToCool = 0;
				}
				else if ((transIdleToHeat - transIdleToCool) <= (-autoIdleTuneThreshold))
				{
					idlePCT--;
					transIdleToHeat = 0;
					transIdleToCool = 0;
				}
			}
			else
			{
				DamperSetpoint = manualPCT;
			}
			ServoCMD = map(DamperSetpoint, 0, 100, minPW, maxPW);     // scale it to use it with the servo (value between 0 and 180)
		}

		if (ServoCMD == prevServoCMD)
		{
			servoWriteTimer++; //the servo command is the same as it was on the last timestep so we can increment the timer
		}
		else
		{
			servoWriteTimer = 0; //reset the timer because the command changed
		}

		if(servoWriteTimer == 0)
		{
			servoUpdateRequired = true;
		}
		else if(servoWriteTimer >= 5)//time in seconds for the servo to stay active
		{
			servoUpdateRequired = false;
		}
		
		if(servoUpdateRequired)
		{
			InletDamper.attach(1);//D1);
			InletDamper.write(ServoCMD);
		}
		else
		{
			InletDamper.detach();
		}
		prevServoCMD = ServoCMD;

		Time1S = 25;
		//Serial.println(ThermostatControl.temperature);
	}
	if (Time1m >= Task1m) {
		chartdata[chartdataIndex].temperature = ThermostatControl.temperature;
		chartdata[chartdataIndex].setpoint = ThermostatControl.setpoint;
		chartdata[chartdataIndex].damperPCT = DamperSetpoint;
		chartdata[chartdataIndex].Probe1 = Probes[0];
		chartdata[chartdataIndex].Probe2 = Probes[1];
		chartdata[chartdataIndex].Probe3 = Probes[2];
		chartdata[chartdataIndex].Probe4 = Probes[3];
		chartdataIndex++;
		
		getBatteryData();

		Time1m = 50;
	}

}

void setup() {
	//bool success = SPIFFS.begin();
	Serial.begin(115200);
	//EEPROM.begin(512);

	WiFi.hostname(ControllerName);
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, pass);

	while (WiFi.waitForConnectResult() != WL_CONNECTED) {
		Serial.println("Connection Failed! Rebooting...");
		delay(5000);
		ESP.restart();
	}

	//these three web server calls handle all data within the webpage and can be found in the mainpage, get, and set, functions respectively
	AC2.webserver.on("/", mainPage);
	AC2.webserver.on("/get", get);
	AC2.webserver.on("/set", set);

	AC2.init(ControllerName, WiFi.localIP(), IPADDR_BROADCAST, 4020, Task100mS);

	float tempSetpoint = 225.0;
	float tempDeadband = 5.0;
	bool resetDefaults = EEPROM.read(10);


	if (resetDefaults)
	{
		uint8_t tempvar = (uint8_t)(tempSetpoint / 10.0);
		EEPROM.write(0, tempvar);
		tempvar = (uint8_t)(tempDeadband * 10.0);
		EEPROM.write(1, tempvar);
		EEPROM.write(2, hotPCT);
		EEPROM.write(3, idlePCT);
		EEPROM.write(4, coldPCT);
		EEPROM.write(5, manualPCT);
		EEPROM.write(6, minPW);
		EEPROM.write(7, maxPW);
		EEPROM.write(8, preheatOffset);
		EEPROM.write(9, autoIdleTuneThreshold);
		EEPROM.write(10, false);
		EEPROM.commit();
	}
	else
	{
		//get all defaults from EE
		tempSetpoint = EEPROM.read(0) * 10.0;
		tempDeadband = EEPROM.read(1) / 10.0;
		hotPCT = EEPROM.read(2);
		idlePCT = EEPROM.read(3);
		coldPCT = EEPROM.read(4);
		manualPCT = EEPROM.read(5);
		minPW = EEPROM.read(6);
		maxPW = EEPROM.read(7);
		preheatOffset = EEPROM.read(8);
		autoIdleTuneThreshold = EEPROM.read(9);
	}

	ThermostatControl.init(tempSetpoint, tempDeadband, ThermostatControl.HeatCool, &AC2.webserver);

	setLogLevel();

	ESP_LOGD("BBQ", "Scanning");
	BLEDevice::init("");

	// Retrieve a Scanner and set the callback we want to use to be informed when we
	// have detected a new device.  Specify that we want active scanning and start the
	// scan to run for 30 seconds.
	BLEScan *pBLEScan = BLEDevice::getScan();
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
	pBLEScan->setActiveScan(true);
	pBLEScan->start(30);

	//get started off right
	timeNow = millis();
	lastTime = timeNow;
}

void loop() {

	//this pretty much just defaults to run, add fancy init or setup logic if desired
	switch (runMode)
	{
	case Setup:
		if (RunSetup())
		{
			runMode = Init;
		}
		break;
	case Init:
		if (RunInit())
		{
			runMode = Run;
		}
		break;
	case Run:
		RunTasks();
		break;
	}
}
