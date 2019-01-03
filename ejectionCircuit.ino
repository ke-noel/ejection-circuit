#include <SFE_BMP180.h>
#include <ArduinoLog.h>

#define MAIN_CHUTE_ALT 300  // in feet
#define IN_FLIGHT_ALT 60  // in feet
#define relayPin 3
#define numInFlightMeas 10
#define numBiasMeas 40
#define numTally 30

// Log levels are LOG_LEVEL_ERROR, LOG_LEVEL_WARNING and LOG_LEVEL_VERBOSE
#define LOG_LEVEL LOG_LEVEL_ERROR

SFE_BMP180 bmp;  // the sensor
double bias = 0.0;  // baseline sensor reading

void setup() {
  Serial.begin(9600);
  Log.begin(LOG_LEVEL, &Serial);

  Log.notice("BOOTING UP..." CR);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  pinMode(LED_BUILTIN, OUTPUT);

  // initialize BMP180 sensor here
  while (!bmp.begin()) {
    Log.error("BMP180 didn't init" CR);
  }
  Log.notice("BMP180 initialized." CR);
  digitalWrite(LED_BUILTIN, HIGH);  // indicate the sensor is working
}

// Altitude in feet
double getAltitude() {
  double p = getPressure();
  double a_metres = bmp.altitude(p, bias);  // in metres
  return a_metres * 3.281;  // convert to feet
}

// Pressure in millibars, temperature in degrees Celsius
double getPressure() {
  char status;
  double dummyT, dummyP;  // temporary measurement values

  status = bmp.startTemperature();
  if (status != 0) {
    delay(status);  // wait for measurement

    // function will return 0 if failed, 1 if successful
    status = bmp.getTemperature(dummyT);
    if (status != 0) {
      delay(status);  // wait for measurement

      // Parameter is oversampling function (0-3) where 3 is the highest resolution
      status = bmp.startPressure(3);
      if (status != 0) {
        delay(status); // wait for measurement

        status = bmp.getPressure(dummyP, dummyT);
        if (status != 0) {
          return dummyP;
        } else Log.error("Error retrieving pressure." CR);
      } else Log.error("Error starting pressure measurement." CR);
    } else Log.error("Error retrieving temperature." CR);
  } else Log.error("Error starting temperature measurement." CR);
}

void deployDrogue() {
  digitalWrite(relayPin, HIGH);
}

void deployMainChute() {
  digitalWrite(relayPin, LOW);
}

void loop() {
  // Calculate the bias
  for (int i = 0; i < numBiasMeas; i++) {
    bias += getPressure();
  }
  bias = bias / numBiasMeas;
  Serial.print("Bias: ");
  Serial.println(bias);

  // Check if the rocket is in flight
  float a = 0.0;  // altitude
  int count = 0;
  // Take the average altitude over a set number of readings
  while (true) {
    for (int i = 0; i < numInFlightMeas; i++) {
      a += getAltitude();  // sum of altitudes in current cycle
    }
    a = a / numInFlightMeas;  // average altitude in current cycle
    if (a > IN_FLIGHT_ALT) {
      Serial.println("Rocket in flight.");
      break;
    }
    /*if (count % 10 == 0) {
      Serial.print("Altitude: ");
      Serial.print(a);
      Serial.println("ft ");
    }*/
    Log.notice("Rocket has not reached flight altitude." CR);
    count++;
  }

  // Check if the rocket is descending
  int tally = 0;
  float lastA = getAltitude();

  // Rocket is descending when a given number of altitude measurements are less than the previous ones
  count = 0;
  while (tally < numTally) {
    a = getAltitude();
    if (a < lastA) {
      tally++;
      /*Serial.print("Altitude: ");
      Serial.print(a);
      Serial.println("ft ");*/
    }
    lastA = a;
    delay(100);
  }
  Log.notice("Rocket is descending." CR);
  Serial.println("Deploying drogue now at altitude: ");
  Serial.print(a);
  Serial.println(" ft");
  deployDrogue();

  // Check if the rocket is at the correct altitude to deploy the main chute
  count = 0;
  while (a > MAIN_CHUTE_ALT) {
    if (count % 10 == 0) {
      Serial.print("Altitude: ");
      Serial.print(a);
      Serial.println("ft ");
    }
    Log.notice("Too high to deploy main chute." CR);
    a = getAltitude();
    count++;
  }
  Log.notice("At correct altitude to deploy main chute." CR);
  Serial.println("Deploying main chute now at altitude: ");
  Serial.print(a);
  Serial.println(" ft");
  deployMainChute();

  // stop the program from repeating
  while (true);
}
