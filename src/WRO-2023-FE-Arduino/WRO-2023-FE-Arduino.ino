// Imports libraries used
#include <Servo.h>
#include "Adafruit_TCS34725.h"
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Pixy2.h>

// Defines registries for the gyro sensor
#define CTRL_REG1 0x20
#define CTRL_REG2 0x21
#define CTRL_REG3 0x22
#define CTRL_REG4 0x23
#define CTRL_REG5 0x24


// Creates the Chassis class which includes steering and driving
class Chassis
{
public:
  Chassis(int e1, int i1, int i2, int steerPort)
  {
    enA = e1;
    in1 = i1;
    in2 = i2;

    sPort = steerPort;

    pinMode(enA, OUTPUT);
    pinMode(in1, OUTPUT);
    pinMode(in2, OUTPUT);

  }
  void move(int speed)
  {
    speed = speed / abs(speed) * min(255, abs(speed));
    analogWrite(enA, abs(speed));
    if (speed == 0)
    {
      digitalWrite(in1, LOW);
      digitalWrite(in2, LOW);
    } else if (speed > 0)
    {
      digitalWrite(in1, HIGH);
      digitalWrite(in2, LOW);
    } else
    {
      digitalWrite(in1, LOW);
      digitalWrite(in2, HIGH);
    }
  }
  void steer(int angle)
  {
    steering.write(angle / abs(angle) * min(abs(angle), 70) + 94);
  }
  void attachServo()
  {
    steering.attach(sPort);
  }
  Servo steering;

  int enA;
  int in1;
  int in2;
  int sPort;
};

// Creates the rgbSensor class which is used for the rgb sensor
class rgbSensor
{
public:
  int getColor()
  {
    float red, green, blue;

    tcs.setInterrupt(false);

    tcs.getRGB(&red, &green, &blue);

    tcs.setInterrupt(true);

    int col = 0;  //0: White, 1: Orange, 2: Blue
    if (blue > 90 && red < 60) { col = 2; }
    if (red > 80 && blue < 55) { col = 1; }
    return col;
  }

  void setup()
  {
    tcs.begin();
  }

  Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_2_4MS, TCS34725_GAIN_4X);
};

// Creates the IRSensors class which is used to read the IR sensor values
class IRSensors
{
public:
  IRSensors(int ports[])
  {
    for (int i = 0; i < 3; i++)
    {
      irPorts[i] = ports[i];
    }
  }

  int getDistance(int port)
  {
    port = max(min(port, 2), 0);
    if (irPorts[port]>13)
    {
      return (11.63417 + (241.6444 - 11.63417)/pow((1 + (analogRead(irPorts[port])*5.0/1023.0/0.5075672)), 1.868922))*1.5;
    }
    return 1-digitalRead(irPorts[port]);
  }

  void setup()
  {
    for (int i : irPorts)
    {
      if (i<14)
      {
        pinMode(i, INPUT);
      }
    }
  }
  int irPorts[3];
};

// Creates the Gyro class which is used to measure our heading
class Gyro
{
public:

  void setup()
  {
    writeRegister(L3G4200D_Address, CTRL_REG1, 0b00001111);
    writeRegister(L3G4200D_Address, CTRL_REG2, 0b00000000);
    writeRegister(L3G4200D_Address, CTRL_REG3, 0b00001000);
    writeRegister(L3G4200D_Address, CTRL_REG4, 0b00110000);
    writeRegister(L3G4200D_Address, CTRL_REG5, 0b00000000);
    delay(1);
  }

  void writeRegister(int deviceAddress, byte address, byte val)
  {
    Wire.beginTransmission(deviceAddress);
    Wire.write(address);
    Wire.write(val);
    Wire.endTransmission();
  }

  int readRegister(int deviceAddress, byte address)
  {
    int v;
    Wire.beginTransmission(deviceAddress);
    Wire.write(address);
    Wire.endTransmission();
    Wire.requestFrom(deviceAddress, 1);
    delay(1);
    v = Wire.read();
    return v;
  }

  float getGyroChange()
  {
    byte zMSB = readRegister(L3G4200D_Address, 0x2D);
    byte zLSB = readRegister(L3G4200D_Address, 0x2C);
    return ((zMSB << 8) | zLSB);
  }

  float getAngle()
  {
    return angle;
  }

  void calibrate()
  {
    for (int i = 0; i < 4000; i++)
    {
      drift += getGyroChange();
    }
    drift /= 4000;
    prevTime = micros();
  }

  void updateGyro()
  {
    angle += (micros() - prevTime) / 1000000.0 * (getGyroChange() - drift) / -14.286;
    prevTime = micros();
    Serial.println("hello");
  }

  float angle = 0;
  unsigned long int prevTime = micros();
  float drift = 0;
  int L3G4200D_Address = 105;
};

class ColorBlock : Block 
{
  public:
    void getPosition(int &x, int &y)
    {
      x = this->m_x;
      y = this->m_y - (this->m_height / 2);
    }
};

// Creates the Camera class which is used to get certain blocks from the PixyCam 2.1
class Camera
{
public:
  void setup()
  {
    pixy.init();
  }

  Block getClosest()
  {
    pixy.ccc.getBlocks();

    int lowInd = 0;
    int lowVal = pixy.ccc.blocks[0].m_y;
    if (pixy.ccc.numBlocks)
    {
      for (int i = 1; i<pixy.ccc.numBlocks; i++)
      {
        if (pixy.ccc.blocks[i].m_y>lowVal)
        {
          lowVal = pixy.ccc.blocks[i].m_y;
          lowInd = i;
        }
      }
    }

    return pixy.ccc.blocks[lowInd];
  }

  int getObjectNum()
  {
    pixy.ccc.getBlocks();
    return pixy.ccc.numBlocks;
  }

  Pixy2 pixy;
};


// Constructs an instance of the classes
Chassis chassis(5, 6, 7, 9); // For movement
rgbSensor rgbSense; // For sensing lines on the mat
const int irPorts[3] = {2, A1, 4}; // Ports for the IR sensors
IRSensors irSensors(irPorts); // For detecting distance in the front and sides
Gyro gyro; // For detecting the angle of our robot
Camera camera; // For getting the obstacles
const int buttonPort = 8; // Pin for the pushbutton

int speed = 0; // Defines the speed for the robot and is used for acceleration
int cornerCount = 0; // Number of corners passed
unsigned long int endTime = 10000000000; // 
int dir = 0; // Stores the direction of our robot: | 0: Undecided | 1: Clockwise | 2: Counterclockwise |
int cornerScanDelay = 0; // Stores time of last line detected for time between corners
bool cornerDetected = false; // Boolean to store if the line has been detected
float steer = 0.0;
Block prevObj; // Stores the last blcok
Block prevObj2; // Stores the last passed block
Block closeBlock;
int target = 0; // Stores the target position of the blocks
float err = 0; // Stores the error for following the object
const float kP = -0.35; // Stores the kP for following the object

void setup()
{
  Serial.begin(9600); // Starts the serial monitor for debugging

  // Sets the pin mode for the button
  pinMode(buttonPort, INPUT);

  // Sets up the electronic components
  Serial.println("ABC");
  chassis.attachServo();
  chassis.steer(30);
  rgbSense.setup();
  irSensors.setup();
  gyro.setup();
  // gyro.calibrate();
  camera.setup();
  delay(1000);
  chassis.steer(0); // Aligns steering when everything is ready

  // Start the program once the button is pressed
  while (!digitalRead(buttonPort))
  {
    Serial.println(irSensors.getDistance(1));
    delay(5);
  }
}

void open()
{
  speed = 255; // Sets speed to 255

  steer = -1; // Steering is set to straight

  if (dir==0)
  { // Rewrites direction to the color of the sensor if the direction is not defined
    dir = rgbSense.getColor();
  }

  if(cornerDetected && irSensors.getDistance(1)<40)
  { // Starts turning when the corner is detected and the robot is close to the wall
    
    // Turns and sets the bias based on which direction the robot is moving
    if (dir==1)
    {
      chassis.steer(30);
    }else
    {
      chassis.steer(-30);
    }

    // Waits until the front IR sensor does not see anything
    while (irSensors.getDistance(1)<50)
    {
      // gyro.updateGyro();
    }

    cornerDetected = false; // Resets cornerDetected variable until the line is seen again
  }

  if (rgbSense.getColor()==dir && dir!=0 && millis()-cornerScanDelay>2000)
  { // Checks if the line is seen and if the time after the last scan was greater than 2000ms
    delay(60);
    cornerCount++; // Increases the amount of corners we have passed
    if (cornerCount==12)
    { // If the gyro angle says 3 laps are finished, stop the robot after some time
      endTime = millis()+3500;
    }

    cornerDetected = true; // Flags the cornerDetected variable as true
    
    cornerScanDelay = millis(); // Updates cornerScanDelay with the current time
  }

  if (irSensors.getDistance(0))
  {
    steer = 30;
  }else if (irSensors.getDistance(2))
  {
    steer = -30;
  }

  if (millis()<endTime)
  { // If the time is not finished, keep moving the robot
    chassis.steer(steer);
    chassis.move(speed);
  }else
  { // If the time is up, stop the robot
    chassis.move(0);
    chassis.steer(0);
    delay(100000000);
  }
}

void challenge()
{
  speed = 255; // Sets speed to 255

  steer = -1; // Steering is set to straight

  if (dir==0)
  { // Rewrites direction to the color of the sensor if the direction is not defined
    dir = rgbSense.getColor();
  }

  if (rgbSense.getColor()==dir && dir>0)
  {
    cornerCount++;
    if (cornerCount==12)
    {
      endTime = millis()+3500;
    }else if (cornerCount==8 && prevObj2.m_signature==1)
    {
      dir = 3-dir;
      while (irSensors.getDistance(1)>40){}
      chassis.steer(-40);
      delay(1500);
      cornerCount+=1;
      // Insert code for reversing the direction
    }else
    {
      if (camera.getClosest().m_signature==3-dir)
      {
        while (irSensors.getDistance(1)>40)
        {
          closeBlock = camera.getClosest();
          if (closeBlock.m_signature<=2){
            if (closeBlock.m_signature==1)
            { // Sets the target for the block position on the left based on how far it is if it is red
              target = (207-closeBlock.m_y)/2+15;
              // target = 157;
            }else
            { // Sets the target for the block position on the right based on how far it is if it is green
              target = 300.0-(207-closeBlock.m_y)/2;
              // target = 157;
            }
            err = target - (int)closeBlock.m_x; // Sets the error to the difference between the current position and the target
            steer = err*kP; // Gets steering value
            if (steer>30){
              steer = 30;
            }else if (steer<-30){
              steer = -30;
            }
            if (closeBlock.m_signature<=2)
            {
              prevObj2 = closeBlock;
            }
          }else{
            steer = 0;
          }
          chassis.steer(steer);
        }
        chassis.steer((1.5-dir)*60);
        while (irSensors.getDistance(1)<155)
        {
          // Insert Waiting Code Here
        }
      }else if (camera.getClosest().m_signature>2)
      {
        while (irSensors.getDistance(1)>70)
        {
          // Insert Waiting Code Here
        }
        chassis.steer((1.5-dir)*100);
        while (irSensors.getDistance(1)<195)
        {
          // Insert Waiting Code Here
        }
        delay(100);
      }else
      {
        chassis.steer(0);
        delay(100);
        chassis.steer((1.5-dir)*40);
        delay(300);
      }
    }
  }

  closeBlock = camera.getClosest(); // Gets closest block from the camera

  if (closeBlock.m_y>187 && closeBlock.m_signature<=2)
  { // If the current block is different from the old block, set the most recent block to the old block
    prevObj2 = closeBlock;
  }
  
  if (closeBlock.m_signature<=2)
  { 
    if (closeBlock.m_signature==1)
    { // Sets the target for the block position on the left based on how far it is if it is red
      target = (207-closeBlock.m_y)/2+15;
      // target = 157;
    }else
    { // Sets the target for the block position on the right based on how far it is if it is green
      target = 300.0-(207-closeBlock.m_y)/2;
      // target = 157;
    }
    err = target - (int)closeBlock.m_x; // Sets the error to the difference between the current position and the target
    steer = err*kP; // Gets steering value
    if (steer>30){
      steer = 30;
    }else if (steer<-30){
      steer = -30;
    }
  }

  if (irSensors.getDistance(0))
  {
    steer = 60;
  }else if (irSensors.getDistance(2))
  {
    steer = -60;
  }

  if (millis()<endTime)
  { // If the time is not finished, keep moving the robot
    chassis.steer(steer);
    chassis.move(speed);
  }else
  { // If the time is up, stop the robot
    chassis.move(0);
    chassis.steer(0);
    delay(10000000);
  }

}

void loop()
{
  // open();
  challenge();
}