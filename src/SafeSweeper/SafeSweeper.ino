#include <Smartcar.h>
#include <SPI.h>
#include <RFID.h>
#include <SoftwareSerial.h>
#include <TinyGPS.h>    

//GPS     
float lat = 22.2222, lon = 33.3333; // create variable for latitude and longitude object
SoftwareSerial gpsSerial(18,19);//rx,tx 
TinyGPS gps; // create gps object

//Odometer:
const unsigned short LEFT_ODOMETER_PIN = 2;
const unsigned short RIGHT_ODOMETER_PIN = 3;
const int PULSES_PER_METER = 30; //for odometer
DirectionlessOdometer leftOdometer(PULSES_PER_METER);
DirectionlessOdometer rightOdometer(PULSES_PER_METER);

//Ultrasonic sensor:
bool obstacle = false;
const unsigned int MAX_DISTANCE = 60;  //recognizable by sensor
const unsigned int MIN_DISTANCE = 20; //distance to obstacle
const int TRIGGER_PIN = 6; //D6
const int ECHO_PIN = 7; //D7
SR04 frontSensor(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);

//Gyroscope:
const int GYROSCOPE_OFFSET = 37;
GY50 gyroscope(GYROSCOPE_OFFSET);

//RFID reader:
#define SS_PIN 20
#define RST_PIN 21
RFID rfid(SS_PIN, RST_PIN);
int serNum[5];
int cards[][5] = {{129,243,229,47,184}};

//Automatic mode:
const float CAR_SPEED = 40.0;
const int TURN_RIGHT = 160;  //angle
const int ZERO = 0;
const int MIN_B = 5;
const int EVEN = 2;

//Manual mode:
const int fSpeed = 50; //50% of the full speed forward
const int bSpeed = -50; //50% of the full speed backward
const int lDegrees = -55; //degrees to turn left
const int rDegrees = 55; //degrees to turn right

//Smartcar:
const int BAUD_RATE = 9600; //for serial
const int ANGLE_CORRECTION = 13;  //offset
bool automode = false;
BrushedMotor leftMotor(8, 10, 9);
BrushedMotor rightMotor(12, 13, 11);
DifferentialControl control(leftMotor, rightMotor);
SmartCar car(control, gyroscope, leftOdometer, rightOdometer);


void setup() {
  Serial.begin(BAUD_RATE);  //The general serial
  Serial3.begin(BAUD_RATE); //Serial for bluetooth
  SPI.begin();              //Initializes the pins for the RFID reader
  rfid.init();              //Initializes the reader
  car.setAngle(ANGLE_CORRECTION);
  pinMode(14,INPUT);        //Input pin for bluetooth
  pinMode(15,OUTPUT);       //Output pin for bluetooth

  leftOdometer.attach(LEFT_ODOMETER_PIN, []() {
    leftOdometer.update();
  });
  rightOdometer.attach(RIGHT_ODOMETER_PIN, []() {
    rightOdometer.update();
  });

  gpsSerial.begin(9600); // connect gps sensor

  // if analog input pin 0 is unconnected, random analog
  // noise will cause the call to randomSeed() to generate
  // different seed numbers each time the sketch runs.
  // randomSeed() will then shuffle the random function.
  randomSeed(analogRead(0));
}

void loop() {
  if(rfid.isCard()){                            //Detects mine        
    car.setSpeed(ZERO);
    Serial3.write('m');
    sendGPS();
    
    while(!Serial3.available()){}
    char command = Serial3.read();
    while(command != 'm'){
      command = Serial3.read();
    }
  }
  if(automode == true){                         //Automatic mode
      char input = Serial3.read();
    Serial.println(frontSensor.getDistance());
    if(input == '6'){
      automode = false;
      car.setSpeed(ZERO);
    }
    if(!obstacleExists()&& automode){
      car.setSpeed(CAR_SPEED);
    } 
    else {                        
        rotateTillFree();
    }
  }
  else if (automode == false){                  //Manual mode
    char input = Serial3.read();
    if (input == '0'){                          //Makes it go foward
      car.setSpeed(fSpeed);
      car.setAngle(0);
    } 
    else if (input == '1'){                     //Makes it go backwards
      car.setSpeed(bSpeed);
      car.setAngle(0);
    } 
    else if (input == '2'){                     //Makes it turn left
      car.setSpeed(fSpeed);
      car.setAngle(lDegrees);
    }
    else if (input == '3'){                     //Makes it turn right
      car.setSpeed(fSpeed);
      car.setAngle(rDegrees);
    }
    else if (input == '4'){                     //Makes it stop
      car.setSpeed(0);
      car.setAngle(0);
    }
    else if (input == '5'){                     //Auto Mode
      automode = true;
    }
    else if (input == 'c'){                     //Send GPS
    sendGPS();
    }
  }
}



//Automatic mode methods:
/**
   Rotate the car at specified degrees with certain speed untill there is no obstacle
*/
void rotateTillFree() {
  int degrees = random(30, 160);
  
  while(obstacleExists()){
    unsigned int initialHeading = car.getHeading();
    bool hasReachedTargetDegrees = false;
    
    while (!hasReachedTargetDegrees) {
      rotateOnSpot(degrees, CAR_SPEED);
      int currentHeading = car.getHeading();
      
      if ( currentHeading > initialHeading) {
        // If we are turning left and the current heading is larger than the
        // initial one (e.g. started at 10 degrees and now we are at 350), we need to substract 360
        // so to eventually get a signed displacement from the initial heading (-20)
        currentHeading -= 360;
      } else if (degrees > ZERO && currentHeading < initialHeading) {
        // If we are turning right and the heading is smaller than the
        // initial one (e.g. started at 350 degrees and now we are at 20), so to get a signed displacement (+30)
        currentHeading += 360;
      }
      // Degrees turned so far is initial heading minus current (initial heading
      // is at least 0 and at most 360. To handle the "edge" cases we substracted or added 360 to currentHeading)
      int degreesTurnedSoFar = initialHeading - currentHeading;
      hasReachedTargetDegrees = smartcarlib::utils::getAbsolute(degreesTurnedSoFar) >= smartcarlib::utils::getAbsolute(degrees);
    }
  }
}
/**
   Measure and return the distance of obstacle within 100 cm
*/
int getObstacleDistance(){
  return frontSensor.getDistance();
}
/**
   Checks if there is an obstacle within 100cm
*/
bool obstacleExists(){
  int distanceToObstacle = getObstacleDistance();
  
  if(distanceToObstacle < MIN_DISTANCE && distanceToObstacle > MIN_B){
    return true;
  } else {
    return false;
  }
}

/**
   Rotate the car on spot at the specified degrees with the certain speed
   @param degrees   The degrees to rotate on spot. Positive values for clockwise
                    negative for counter-clockwise.
   @param speed     The speed to rotate
*/
void rotateOnSpot(int targetDegrees, int speed) {
  speed = smartcarlib::utils::getAbsolute(speed);
  targetDegrees %= 360; //put it on a (-360,360) scale
  if (!targetDegrees) return; //if the target degrees is 0, don't bother doing anything
  /* Let's set opposite speed on each side of the car, so it rotates on spot */
  if (targetDegrees > 0) { //positive value means we should rotate clockwise
    car.overrideMotorSpeed(speed, -speed); // left motors spin forward, right motors spin backward
  } else { //rotate counter clockwise
    car.overrideMotorSpeed(-speed, speed); // left motors spin backward, right motors spin forward
  }
  unsigned int initialHeading = car.getHeading(); //the initial heading we'll use as offset to calculate the absolute displacement
  int degreesTurnedSoFar = 0; //this variable will hold the absolute displacement from the beginning of the rotation
  while (abs(degreesTurnedSoFar) < abs(targetDegrees)) { //while absolute displacement hasn't reached the (absolute) target, keep turning
    car.update(); //update to integrate the latest heading sensor readings
    int currentHeading = car.getHeading(); //in the scale of 0 to 360
    if ((targetDegrees < 0) && (currentHeading > initialHeading)) { //if we are turning left and the current heading is larger than the
      //initial one (e.g. started at 10 degrees and now we are at 350), we need to substract 360, so to eventually get a signed
      currentHeading -= 360; //displacement from the initial heading (-20)
    } else if ((targetDegrees > 0) && (currentHeading < initialHeading)) { //if we are turning right and the heading is smaller than the
      //initial one (e.g. started at 350 degrees and now we are at 20), so to get a signed displacement (+30)
      currentHeading += 360;
    }
    degreesTurnedSoFar = initialHeading - currentHeading; //degrees turned so far is initial heading minus current (initial heading
    //is at least 0 and at most 360. To handle the "edge" cases we substracted or added 360 to currentHeading)
  }
}

String getGPS(){
  if(gpsSerial.available()){ // check for gps data 
    if(gps.encode(gpsSerial.read())){  // encode gps data   
      //to change the gps default values
      lat = 55.585695;
      lon = 69.25685;
      
      gps.f_get_position(&lat, &lon); // get latitude and longitude 
      } else {
        lat = 0;
        lon = 0;
      }
  } 
  String latitude = String(lat, 6); 
  String longitude = String(lon, 6);
  String gpsToBeSent = "c" + latitude + " " + longitude +"/";
  return gpsToBeSent;
}

void sendGPS(){
  String gpsToBeSent = getGPS();
  int lengthOfChar = gpsToBeSent.length();
  for(int i = 0; i < lengthOfChar; i++){
    char shoot = gpsToBeSent.charAt(i);
    Serial3.write(shoot);
  }
}
