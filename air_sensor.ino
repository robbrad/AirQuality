#include <SPI.h>
#include <Ethernet.h>
#include "DHT.h"
#include <math.h>
#include <Wire.h>
#include "MutichannelGasSensor.h"
#include "HP20x_dev.h"
#include "KalmanFilter.h"

unsigned char ret = 0;

/* Instance */
KalmanFilter t_filter;    //temperature filter
KalmanFilter p_filter;    //pressure filter
KalmanFilter a_filter;    //altitude filter

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
IPAddress server(192, 168, 0, 30); // numeric IP for Google (no DNS)
//char server[] = "www.google.com";    // name address for Google (using DNS)

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 0, 40);

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;


#define Vc 4.95
//the number of R0 you detected just now
#define R0 35.54
#define DHTPIN A1     // what pin we're connected to

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

DHT dht(DHTPIN, DHTTYPE);

int pin = 8;
unsigned long duration;
unsigned long starttime;
unsigned long sampletime_ms = 2000;//sampe 30s&nbsp;;
unsigned long lowpulseoccupancy = 0;
float ratio = 0;
float concentration = 0;
//String data = "";

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  delay(150);
  HP20x.begin();
  delay(100);

  /* Determine HP20x_dev is available or not */
  ret = HP20x.isAvailable();

  pinMode(8, INPUT);
  starttime = millis();//get the current time;
  //Serial.println("power on!");


  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  //if (Ethernet.begin(mac) == 0) {
  //  Serial.println("Failed to configure Ethernet using DHCP");
  // try to congifure using IP address instead of DHCP:
  Ethernet.begin(mac, ip);

  // give the Ethernet shield a second to initialize:
  delay(10000);
  // Serial.println("connecting...");
  //}
  // give the Ethernet shield a second to initialize:
  //delay(1000);
  //Serial.println("connecting...");



  // Serial.println(Ethernet.localIP());


  gas.begin(0x04);//the default I2C address of the slave is 0x04
  gas.powerOn();
  //Serial.print("Firmware Version = ");
  //Serial.println(gas.getVersion());
  //Serial.println("Particles\tRS\tHCHO (PPM)\tNH3 (PPM)\tCO (PPM)\tNO2 (PPM)\tC3H8 (PPM)\tC4H10 (PPM)\tCH4 (PPM)\tH2 (PPM)\tC2H5OH (PPM)");
  dht.begin();
  delay(10000);

}

void loop() {

  if (client.available()) {
    char c = client.read();
    //  Serial.print(c);
  }

  float HATemp2;
  float HAPres2;
  float HAAlt2;
  float d;

  if (OK_HP20X_DEV == ret)
  {
    unsigned long HATemp = HP20x.ReadTemperature();
    d = HATemp / 100.0;
    HATemp2 = t_filter.Filter(d);

    unsigned long HAPres = HP20x.ReadPressure();
    d = HAPres / 100.0;
    HAPres2 = p_filter.Filter(d);

    unsigned long HAAlt = HP20x.ReadAltitude();
    d = HAAlt / 100.0;
    HAAlt2 = a_filter.Filter(d);

  }

  duration = pulseIn(pin, LOW);
  lowpulseoccupancy = lowpulseoccupancy + duration;

  

  if ((millis() - starttime) >= sampletime_ms) //if the sampel time = = 30s
  {
    ratio = lowpulseoccupancy / (sampletime_ms * 10.0); // Integer percentage 0=&gt;100

    d = ((1.1*ratio - 3.8)*ratio + 520)*ratio + 0.62;
    //d = 1.1 * pow(ratio, 3) - 3.8 * pow(ratio, 2) + 520 * ratio + 0.62; // using spec sheet curve
    //Serial.print("concentration = ");

    lowpulseoccupancy = 0;
    starttime = millis();
  }
  //Serial.print("\t");
  //HCHO

  int sensorValue = analogRead(A0);
  double Rs = (1023.0 / sensorValue) - 1;

  //double ppm = pow(10.0, ((log10(Rs / R0) - 0.0827) / (-0.4807)));
double ppm = 2500.33 * pow(Rs, -2.0803);
  
  //Serial.print("HCHO ppm = ");

  //MultiChannel Gas
  float NH3 = gas.measure_NH3();
  float CO = gas.measure_CO();
  float NO2 = gas.measure_NO2();
  float C3H8 = gas.measure_C3H8();
  float C4H10 = gas.measure_C4H10();
  float CH4 = gas.measure_CH4();
  float H2 = gas.measure_H2();
  float C2H5OH = gas.measure_C2H5OH();


  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  //data = String("dust=") + d + "&rs=" + Rs + "&hcho=" + ppm + "&nh3=" + NH3 + "&co=" + CO + "&no2=" + NO2 + "&c3h8=" + C3H8 + "&c4h10=" + C4H10 + "&ch4=" + CH4 + "&h2=" + H2 + "&c2h5oh=" + C2H5OH + "&temp=" + t + "&hum=" + h + "&HATemp=" + HATemp2 + "&HAPres=" + HAPres2 + "&HAAlt=" + HAAlt2;

  //Serial.println(data);


  if (client.connect(server, 80)) {
    //Serial.println(F("connected"));
    client.println(F("POST /air_add.php HTTP/1.1"));
    client.println(F("Host:  192.168.0.30"));
    client.println(F("User-Agent: Arduino/1.0"));
    client.println(F("Connection: close"));
    client.println(F("Content-Type: application/x-www-form-urlencoded;"));
    client.print(F("Content-Length: "));
    client.println(500);
    client.println();
    client.println(String("dust=") + d + "&rs=" + Rs + "&hcho=" + ppm + "&nh3=" + NH3 + "&co=" + CO + "&no2=" + NO2 + "&c3h8=" + C3H8 + "&c4h10=" + C4H10 + "&ch4=" + CH4 + "&h2=" + H2 + "&c2h5oh=" + C2H5OH + "&temp=" + t + "&hum=" + h + "&HATemp=" + HATemp2 + "&HAPres=" + HAPres2 + "&HAAlt=" + HAAlt2);
  }
  else
  {
    Serial.println(F("could not connect"));
  }

//  Serial.println(data);

  if (client.connected()) {
    client.stop();
  }


  delay(300000);
}
