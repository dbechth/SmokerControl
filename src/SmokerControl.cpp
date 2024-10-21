
#include <EEPROM.h>
#include "ThermostatControl.h"
#include "AC2.h"
#include <WiFiClient.h>
#include <SPI.h>
#include <Wire.h>
#include <Servo.h>
#include "max6675.h"
#include "smokerIDX.h"
//#define bluetoothProbe
#ifdef ARDUINO_ARCH_ESP32
	#ifdef bluetoothProbe
		//Inkbird stuff
		#include "InkbirdCom.h"
		BLEScan *pBLEScan;
	#else
		#define CONFIG_LOG_DEFAULT_LEVEL ESP_LOG_NONE
		#define LOG_LOCAL_LEVEL ESP_LOG_NONE
	#endif
#endif

#ifdef ARDUINO_ARCH_ESP32
	constexpr auto ControllerName = "Smoker32";
#else
	constexpr auto ControllerName = "Smoker";
#endif
int thermoDO = D4;
int thermoCS = D5;
int thermoCLK = D6;
int thermoCS2 = D3;
int pinInletDamper = D1;
int pinOutletDamper = D2;

MAX6675 smokechamberthermocouple(thermoCLK, thermoCS, thermoDO);
MAX6675 fireboxthermocouple(thermoCLK, thermoCS2, thermoDO);

Servo InletDamper;  // create servo object to control a servo
Servo OutletDamper;  // create servo object to control a servo

//OS
#define Task100mS  100
#define Task1S  1000
#define Task1m  60000
unsigned long lastTime = 0;
unsigned long timeNow = 0;
unsigned long Time1S = 0;
unsigned long Time1m = 0;
//File file;


#define OutletDameperOpen 100
#define OutletDameperClosed 0

int DamperSetpoint = 100;
int OutletDamperSetpoint = 100;
int ServoCMD;
int OutletServoCMD;
int prevServoCMD;
int servoWriteTimer;
bool servoUpdateRequired;
bool servoAttached;
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
char ssid[] = "Bulldog";
char pass[] = "6412108682";
char APssid[] = "DBBSmoker";

WiFiClient client;

// Replace with your unique Thing Speak WRITE API KEY
const char* tsapiKey = "";//"OXMB6DTDS6D6VJYD";
const char* resource = "/update?api_key=";

// Thing Speak API server 
const char* server = "api.thingspeak.com";

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
			#ifdef bluetoothProbe
				BuildJSON("Probe1", chartdata[i].Probe1) + "," +
				BuildJSON("Probe2", chartdata[i].Probe2) + "," +
				BuildJSON("Probe3", chartdata[i].Probe3) + "," +
				BuildJSON("Probe4", chartdata[i].Probe4) +
			#endif
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
			#ifdef bluetoothProbe
				BuildJSON("Probe1", chartdata[idx].Probe1) + "," +
				BuildJSON("Probe2", chartdata[idx].Probe2) + "," +
				BuildJSON("Probe3", chartdata[idx].Probe3) + "," +
				BuildJSON("Probe4", chartdata[idx].Probe4) +
			#endif
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
			AC2.webserver.send(200, "text/plain", String(smokechamberThermostat.temperature, 2));
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
			AC2.webserver.send(200, "text/plain", String(smokechamberThermostat.setpoint));
		}
		else if (AC2.webserver.hasArg("setPointDeadband"))
		{
			AC2.webserver.send(200, "text/plain", String(smokechamberThermostat.hysteresis));
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
			smokechamberThermostat.setpoint = AC2.webserver.arg("setPointF").toFloat();

			uint8_t tempvar = (uint8_t)(smokechamberThermostat.setpoint / 10.0);
			EEPROM.write(0, tempvar);
			EEPROM.commit();
		}
		else if (AC2.webserver.hasArg("setPointDeadband"))
		{
			smokechamberThermostat.hysteresis = AC2.webserver.arg("setPointDeadband").toFloat();
			uint8_t tempvar = (uint8_t)(smokechamberThermostat.hysteresis * 10.0);
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

	static float smokechamberTemperature = 0;
	static float fireboxTemperature = 0;
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
#ifdef bluetoothProbe
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
#endif
	}
	if (Time1S >= Task1S) {
		smokechamberTemperature = smokechamberthermocouple.readFahrenheit();
		fireboxTemperature = fireboxthermocouple.readFahrenheit();
		Serial.print("SmokeTemp: ");
		Serial.println(smokechamberTemperature);
		Serial.print("FireboxTemp: ");
		Serial.println(fireboxTemperature);

		smokechamberThermostat.task(smokechamberTemperature);

		if (startup)
		{
			ServoCMD = maxPW;
			if (smokechamberThermostat.temperature >= (smokechamberThermostat.setpoint + preheatOffset))
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

				if (smokechamberThermostat.output == smokechamberThermostat.cmdHeat)
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
					OutletDamperSetpoint = OutletDameperClosed;
				}
				else if (smokechamberThermostat.output == smokechamberThermostat.cmdCool)
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
					OutletDamperSetpoint = OutletDameperOpen;
				}
				else
				{
					inIdle = true;
					DamperSetpoint = idlePCT;
					OutletDamperSetpoint = OutletDameperClosed;
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
				OutletDamperSetpoint = manualPCT;
			}
			ServoCMD = map(DamperSetpoint, 0, 100, minPW, maxPW);     // scale it to use it with the servo (value between 0 and 180)
			OutletServoCMD = map(OutletDamperSetpoint, 0, 100, 135, 30);     // scale it to use it with the servo (value between 0 and 180)
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
		else if(servoWriteTimer >= 10)//time in seconds for the servo to stay active
		{
			servoUpdateRequired = true;//false to enable the servo power down feature
		}
		
		if(servoUpdateRequired)
		{
			if (!servoAttached)
			{
				InletDamper.attach(pinInletDamper);
				OutletDamper.attach(pinOutletDamper);

				servoAttached = true;
			}
			
			InletDamper.write(ServoCMD);
			OutletDamper.write(OutletServoCMD);
		}
		else
		{
			if (servoAttached)
			{
				InletDamper.detach();
				OutletDamper.detach();
				servoAttached = false;
			}
			
		}
		prevServoCMD = ServoCMD;
		

		Time1S = 25;
	}
	if (Time1m >= Task1m) {
		chartdata[chartdataIndex].temperature = smokechamberThermostat.temperature;
		chartdata[chartdataIndex].setpoint = smokechamberThermostat.setpoint;
		chartdata[chartdataIndex].damperPCT = DamperSetpoint;
		#ifdef bluetoothProbe
			chartdata[chartdataIndex].Probe1 = Probes[0];
			chartdata[chartdataIndex].Probe2 = Probes[1];
			chartdata[chartdataIndex].Probe3 = Probes[2];
			chartdata[chartdataIndex].Probe4 = Probes[3];
			getBatteryData();
		#endif
		chartdataIndex++;
		Time1m = 50;
	}

}

void setup() {
	//bool success = SPIFFS.begin();
	Serial.begin(115200);
	EEPROM.begin(512);
	Serial.println("Starting");

	WiFi.hostname(ControllerName);
	WiFi.mode(WIFI_AP_STA);
	WiFi.begin(ssid, pass);
	WiFi.softAP(APssid);

	//these three web server calls handle all data within the webpage and can be found in the mainpage, get, and set, functions respectively
	AC2.webserver.on("/", mainPage);
	AC2.webserver.on("/get", get);
	AC2.webserver.on("/set", set);
	delay(5000);
	Serial.println(WiFi.localIP());
	AC2.init(ControllerName, WiFi.localIP(), IPADDR_BROADCAST, 4020, Task100mS);

	float tempSetpoint = 225.0;
	float tempDeadband = 5.0;
	bool resetDefaults = EEPROM.read(10);

	


	if (resetDefaults)
	{
		Serial.println("Loading default settings...");
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
		Serial.println("Loading previous settings...");
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

	smokechamberThermostat.init(tempSetpoint, tempDeadband, smokechamberThermostat.HeatCool, &AC2.webserver);

#ifdef bluetoothProbe
	setLogLevel();

	ESP_LOGD("BBQ", "Scanning");
	BLEDevice::init("");

	// Retrieve a Scanner and set the callback we want to use to be informed when we
	// have detected a new device.  Specify that we want active scanning and start the
	// scan to run for 30 seconds.
	pBLEScan = BLEDevice::getScan();
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
	pBLEScan->setActiveScan(true);
	pBLEScan->start(30);
#endif
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
