#include <Servo.h>
#include <string.h>

#define IN1  2
#define IN2  4
#define IN3  5
#define IN4  6
#define S0 8
#define S1 9
#define S2 10
#define S3 11
#define SENSOR_OUT 12
#define SERVO_PIN 3

#define ROTATION0 1024
#define ROTATION1 1024
#define ROTATION2 976
#define ROTATION3 1072

#define RED_ANGLE 50
#define GREEN_ANGLE 70
#define BLUE_ANGLE 90

struct RGB {
  int R, G, B;
};

int steps = 0;
boolean direction = false;
unsigned long lastTime;
unsigned long currentMillis;
int stepsLeft = 4096;
long time;
Servo servoMotor;
int servoPosition = GREEN_ANGLE;
bool isRunning = false;
int rotationState = 0;
RGB baseColor[4];
RGB currentColorDiff[4];

void setup()
{
  // Stepper motor
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Color sensor
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(SENSOR_OUT, INPUT);

  // Setting color sensor frequency-scaling to 20%
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);

  // Initialisation
  for (int i = 0; i < 4; i++) {
    currentColorDiff[i].R = 0;
    currentColorDiff[i].G = 0;
    currentColorDiff[i].B = 0;
  }

  // On startup, move to the output positions so that we know where candy is dropped
  servoMotor.attach(SERVO_PIN);  delay(100);
  servoMotor.write(RED_ANGLE);   delay(1500);
  servoMotor.write(BLUE_ANGLE);  delay(1500);
  servoMotor.write(GREEN_ANGLE); delay(100);

  Serial.begin(9600);

  delay(5000); // Wait 5 seconds before beginning
  isRunning = true; // Start running
  checkSerial(); // Check if any input was given before this (like a 'stop' command)
  if (isRunning) autoCalibrate(15); // Calibrate first, before running
}

void loop()
{
  RGB RGBColor;
  checkSerial();
  delay(1000);

  while (isRunning) {
    // When doing rotation zero (starting from slot 0 and getting to slot 1), we will drop candy in slot 3
    setServo((rotationState + 3) % 4);

    // Rotate based on where we are now
    switch (rotationState) {
      case 0:
        doRotation(ROTATION0);
        break;
      case 1:
        doRotation(ROTATION1);
        break;
      case 2:
        doRotation(ROTATION2);
        break;
      case 3:
        doRotation(ROTATION3);
        break;
      default:
        rotationState = 0;
        break;
    }

    rotationState = (rotationState + 1) % 4;
    RGBColor = specialMultipleReadColor(10);
    currentColorDiff[rotationState].R = baseColor[rotationState].R - RGBColor.R;
    currentColorDiff[rotationState].G = baseColor[rotationState].G - RGBColor.G;
    currentColorDiff[rotationState].B = baseColor[rotationState].B - RGBColor.B;
    delay(250);
    checkSerial();
  }
}

// Generally used for debugging
void checkSerial() {
  char string[64] = {0};
  int length = 0;
  while (Serial.available() > 0) {
    string[length] = (char)Serial.read();
    length = length + 1;
  }
  if (length == 0) return;

  // Print the given command
  Serial.print("Serial input: ");
  for (int aux = 0; aux < length; aux++)Serial.print(string[aux]);
  Serial.println(" ");

  if (strcmp(string, "stop") == 0) { //Stop rotating
    isRunning = false;
  }
  else if (strcmp(string, "start") == 0) { //Start rotating
    isRunning = true;
  }
  else if (strcmp(string, "rotate") == 0) { //Do one rotation
    switch (rotationState) {
      case 0:
        Serial.println(" ");
        doRotation(ROTATION0);
        break;
      case 1:
        doRotation(ROTATION1);
        break;
      case 2:
        doRotation(ROTATION2);
        break;
      case 3:
        doRotation(ROTATION3);
        break;
      default:
        break;
    }
    rotationState = (rotationState + 1) % 4;
  }
  else if (string[0] == '+') { //For calibration purposes, move +x steps
    int numberOfSteps = 0;
    for (int i = 1; i < length; i++) {
      numberOfSteps = numberOfSteps * 10 + (string[i] - '0');
    }
    doRotation(numberOfSteps);
  }
  else if (string[0] == '-') { //For calibration purposes, move -x steps
    int numberOfSteps = 0;
    for (int i = 1; i < length; i++) {
      numberOfSteps = numberOfSteps * 10 + (string[i] - '0');
    }
    direction = !direction;
    doRotation(numberOfSteps);
    direction = !direction;
  }
  else if (strcmp(string, "specialRead") == 0) { //Read color and output to serial
    specialReadColor();
  }
  else if (strcmp(string, "read") == 0) { //Read color and output to serial
    readColor();
  }
  else if (strcmp(string, "specialMultiple") == 0) { //Read color and output to serial
    specialMultipleReadColor(10);
  }
  else if (strcmp(string, "extremeMultiple") == 0) { //Read color and output to serial (intended to give extreme precision)
    specialMultipleReadColor(100);
  }
  else if (strcmp(string, "autoCalibrate") == 0) { //Determines the base color of each slot. Needed before running
    autoCalibrate(15);
  }
  else if (strcmp(string, "printBase") == 0) { //Will print to serial the determined base colors of current slot
    Serial.print("Base: [R,G,B]=[");
    Serial.print(baseColor[rotationState].R);
    Serial.print(",");
    Serial.print(baseColor[rotationState].G);
    Serial.print(",");
    Serial.print(baseColor[rotationState].B);
    Serial.println("]; ");
  }
  else if (strcmp(string, "printDiff") == 0) { //Will print to serial the difference between the base color and the current color read by the sensor
    RGB result;
    result = specialMultipleReadColor(10);
    Serial.print("[R,G,B]=[");
    Serial.print(baseColor[rotationState].R - result.R);
    Serial.print(",");
    Serial.print(baseColor[rotationState].G - result.G);
    Serial.print(",");
    Serial.print(baseColor[rotationState].B - result.B);
    Serial.println("]; ");
  }
}

// Determines the base color of each slot. Needed before running
void autoCalibrate(int iterationCount) {
  for (int i = 0; i < 4; i++) {
    RGB color = specialMultipleReadColor(iterationCount);
    baseColor[rotationState].R = color.R;
    baseColor[rotationState].G = color.G;
    baseColor[rotationState].B = color.B;
    switch (rotationState) {
      case 0:
        doRotation(ROTATION0);
        break;
      case 1:
        doRotation(ROTATION1);
        break;
      case 2:
        doRotation(ROTATION2);
        break;
      case 3:
        doRotation(ROTATION3);
        break;
      default:
        rotationState = 0;
        break;
    }
    rotationState = (rotationState + 1) % 4;

  }
}

// Will set the servo to an angle based on the data in currentColorDiff
void setServo(int slot) {
  int targetPosition;
  int i;

  int decision = maxPos(currentColorDiff[slot].R, currentColorDiff[slot].G, currentColorDiff[slot].B);
  if (decision == 0) targetPosition = RED_ANGLE;
  else if (decision == 1) targetPosition = GREEN_ANGLE;
  else if (decision == 2) targetPosition = BLUE_ANGLE;
  else targetPosition = servoPosition;

  // Gradually move the servo to the target position
  if (targetPosition > servoPosition) {
    for (i = servoPosition + 1; i <= targetPosition; i++) {
      servoMotor.write(i);
      delay(10);
    }
  }
  else {
    for (i = servoPosition - 1; i >= targetPosition; i--) {
      servoMotor.write(i);
      delay(10);
    }
  }

  servoPosition = targetPosition;
  currentColorDiff[slot].R = 0;
  currentColorDiff[slot].G = 0;
  currentColorDiff[slot].B = 0;
}

// Returns 0, 1, or 2 depending on which parameter is the max
int maxPos(int a, int b, int c) {
  if (a > b) {
    if (a > c) return 0;
    else return 2;
  }
  else {
    if (b >= c) return 1;
    else return 2;
  }
}

// Will move the stepper
void doRotation(int steps) {
  stepsLeft = steps;
  while (stepsLeft > 0) {
    currentMillis = micros();
    if (currentMillis - lastTime >= 1000) {
      stepper(1);
      time = time + micros() - lastTime;
      lastTime = micros();
      stepsLeft--;
    }
  }
}

// The base function to read the color
void readColor() {
  int frequency = 0;

  // Setting red filtered photodiodes to be read
  digitalWrite(S2, LOW);
  digitalWrite(S3, LOW);

  // Reading the output frequency
  frequency = pulseIn(SENSOR_OUT, LOW);
  // Printing the value on the serial monitor
  Serial.print("R= ");
  Serial.print(frequency);
  Serial.print("  ");
  delay(100);

  // Setting Green filtered photodiodes to be read
  digitalWrite(S2, HIGH);
  digitalWrite(S3, HIGH);

  // Reading the output frequency
  frequency = pulseIn(SENSOR_OUT, LOW);
  // Printing the value on the serial monitor
  Serial.print("G= ");
  Serial.print(frequency);
  Serial.print("  ");
  delay(100);

  // Setting Blue filtered photodiodes to be read
  digitalWrite(S2, LOW);
  digitalWrite(S3, HIGH);

  // Reading the output frequency
  frequency = pulseIn(SENSOR_OUT, LOW);
  // Printing the value on the serial monitor
  Serial.print("B= ");
  Serial.print(frequency);
  Serial.println("  ");
  delay(100);
}

// Reads the color 'iterationCount' times, averaging the result for an increased precision
RGB specialMultipleReadColor(int iterationCount) {
  RGB result;
  result.R = 0;
  result.G = 0;
  result.B = 0;
  int frequency = 0;
  for (int i = 0; i < iterationCount; i++) {
    // Setting red filtered photodiodes to be read
    digitalWrite(S2, LOW);
    digitalWrite(S3, LOW);
    // Reading the output frequency
    frequency = pulseIn(SENSOR_OUT, LOW);
    result.R += frequency;
    delay(100);

    // Setting Green filtered photodiodes to be read
    digitalWrite(S2, HIGH);
    digitalWrite(S3, HIGH);
    // Reading the output frequency
    frequency = pulseIn(SENSOR_OUT, LOW);
    result.G += frequency;
    delay(100);

    // Setting Blue filtered photodiodes to be read
    digitalWrite(S2, LOW);
    digitalWrite(S3, HIGH);
    // Reading the output frequency
    frequency = pulseIn(SENSOR_OUT, LOW);
    result.B += frequency;
    delay(100);
  }
  result.R = result.R / iterationCount;
  result.G = result.G / iterationCount;
  result.B = result.B / iterationCount;

  Serial.print("[R,G,B]=[");
  Serial.print(result.R);
  Serial.print(",");
  Serial.print(result.G);
  Serial.print(",");
  Serial.print(result.B);
  Serial.println("]; ");

  return result;
}

// Reads the sensor 3 times for each color and returns an RGB struct
RGB specialReadColor() {
  int frequency = 0;
  RGB result;

  // Setting red filtered photodiodes to be read
  digitalWrite(S2, LOW);
  digitalWrite(S3, LOW);
  // Reading the output frequency
  frequency = pulseIn(SENSOR_OUT, LOW);
  result.R = frequency;
  delay(100);

  frequency = pulseIn(SENSOR_OUT, LOW);
  result.R = result.R + frequency;
  delay(100);

  frequency = pulseIn(SENSOR_OUT, LOW);
  result.R = result.R + frequency;
  delay(100);
  result.R = result.R / 3;

  // Setting Green filtered photodiodes to be read
  digitalWrite(S2, HIGH);
  digitalWrite(S3, HIGH);
  // Reading the output frequency
  frequency = pulseIn(SENSOR_OUT, LOW);
  result.G = frequency;
  delay(100);

  frequency = pulseIn(SENSOR_OUT, LOW);
  result.G = result.G + frequency;
  delay(100);

  frequency = pulseIn(SENSOR_OUT, LOW);
  result.G = result.G + frequency;
  delay(100);
  result.G = result.G / 3;

  // Setting Blue filtered photodiodes to be read
  digitalWrite(S2, LOW);
  digitalWrite(S3, HIGH);
  // Reading the output frequency
  frequency = pulseIn(SENSOR_OUT, LOW);
  result.B = frequency;
  delay(100);

  frequency = pulseIn(SENSOR_OUT, LOW);
  result.B = result.B + frequency;
  delay(100);

  frequency = pulseIn(SENSOR_OUT, LOW);
  result.B = result.B + frequency;
  delay(100);
  result.B = result.B / 3;

  Serial.print("[R,G,B]=[");
  Serial.print(result.R);
  Serial.print(",");
  Serial.print(result.G);
  Serial.print(",");
  Serial.print(result.B);
  Serial.println("]; ");

  return result;
}

// Sets the pins of the stepper to the next required state
void stepper(int xw) {
  for (int x = 0; x < xw; x++) {
    switch (steps) {
      case 0:
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, HIGH);
        break;
      case 1:
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, HIGH);
        break;
      case 2:
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
        break;
      case 3:
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
        break;
      case 4:
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);
        break;
      case 5:
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, HIGH);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);
        break;
      case 6:
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);
        break;
      case 7:
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, HIGH);
        break;
      default:
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);
        break;
    }
    SetDirection();
  }
}

// Sets the state of the stepper motor
void SetDirection() {
  if (direction == 1) {
    steps++;
  }
  if (direction == 0) {
    steps--;
  }
  if (steps > 7) {
    steps = 0;
  }
  if (steps < 0) {
    steps = 7;
  }
}
