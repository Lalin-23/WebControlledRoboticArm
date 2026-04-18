#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// --- 1. PIN & HARDWARE DEFINITIONS ---
#define SERVO_1   0  // Base
#define SERVO_2   1  // Arm 1
#define SERVO_3   2  // Arm 2
#define SERVO_4   3  // Arm 3
#define SERVO_5a  4  // Arm 4 Rotation a  <-- Servo 4
#define SERVO_5b  5  // Arm 4 Rotation b  <-- Servo 5
#define SERVO_6   6  // Gripper

#define SERVOMIN  150
#define SERVOMAX  600
#define SERVO_FREQ 50
const int NUM_SERVOS = 7;
const int ALL_CHANNELS[] = {SERVO_1, SERVO_2, SERVO_3, SERVO_4, SERVO_5a, SERVO_5b, SERVO_6};

// --- 2. SERVO RANGE LIMITS ---
// Motors 0-5 are editable. Motor 6 is locked to 40(min) and 120(max).
int SERVO_LIMITS[NUM_SERVOS][2] = {
  {0, 180},   // 0: Base 
  {50, 140},  // 1: Arm 1 
  {40, 140},  // 2: Arm 2
  {40, 140},   // 3: Arm 3
  {60, 180},   // 4: Arm 4a 
  {60, 180},   // 5: Arm 4b 
  {60, 120}   // 6: Gripper (LOCKED - Only uses angles from poses)
};

// Array to track where the servos currently are
float currentAngles[NUM_SERVOS] = {90, 90, 90, 90, 90, 90, 90}; 

// --- 3. ANIMATION STRUCTURE ---
struct Pose {
  float angles[NUM_SERVOS]; 
  int durationMs;     
  int delayAfterMs;
};

// --- POSES ---
Pose animationSequence[] = {
  // Base, A1,  A2,  A3,  A4a, A4b, Grip | Time | Pause
  {{  90,  90,  90,  90,  90,  90,  60},   1000,  0}, // Pose 1: Home
  {{  45,  120, 45,  90,  90,  90,  60},   2000,  0}, // Pose 2: Reach
  {{  45,  120, 45,  90,  90,  90,  120},  500,   0}, // Pose 3: Close Gripper
  {{  45,  90,  90,  90,  90,  90,  120},  1500,  0}, // Pose 4: Lift Arm
  {{  135, 90,  90,  90,  90,  90,  120},  1000,  0}, // Pose 5: Swivel Base
  {{  135, 140, 60,  120, 45,  135, 120},  2500,  0}, // Pose 6: Dynamic 
  {{  135, 140, 60,  120, 45,  135, 60},   500,   0}  // Pose 7: Drop
};

const int NUM_POSES = sizeof(animationSequence) / sizeof(animationSequence[0]);

int angleToPulse(int angle) {
  return map(angle, 0, 180, SERVOMIN, SERVOMAX);
}

// Write to servo enforcing safety limits
void safeWriteServo(int servoIndex, float angle) {
  angle = constrain(angle, SERVO_LIMITS[servoIndex][0], SERVO_LIMITS[servoIndex][1]);
  pwm.setPWM(ALL_CHANNELS[servoIndex], 0, angleToPulse((int)angle));
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ);
  delay(10);

  Serial.println("Moving to initial starting positions...");
  for (int i = 0; i < NUM_SERVOS; i++) {
    safeWriteServo(i, currentAngles[i]);
  }
  delay(1000);
  
  Serial.println("\n=== COMMAND CENTER READY ===");
  Serial.println(" 1-7 : Play a specific pose");
  Serial.println(" e   : Extreme Sweep (0 to 5 Min->Max. Skips Gripper)");
  Serial.println(" m [Motor] [MaxAngle]      : Update max limit");
  Serial.println(" u [Pose] [Motor] [Angle]  : Update pose target");
  Serial.println(" S[Motor]:[Angle]          : Direct Web Control (e.g. S1:90)");
}

// --- 4. THE INTERPOLATOR ---
void moveToPose(Pose targetPose) {
  int steps = 50; 
  int delayPerStep = targetPose.durationMs / steps;

  float stepSizes[NUM_SERVOS];
  for (int i = 0; i < NUM_SERVOS; i++) {
    // ENFORCE SYNC: Make sure index 5 always mirrors index 4 for all animations!
    if (i == 5) {
      targetPose.angles[5] = targetPose.angles[4];
    }
    float targetAngle = constrain(targetPose.angles[i], SERVO_LIMITS[i][0], SERVO_LIMITS[i][1]);
    float totalDistance = targetAngle - currentAngles[i];
    stepSizes[i] = totalDistance / steps;
  }

  for (int currentStep = 1; currentStep <= steps; currentStep++) {
    for (int i = 0; i < NUM_SERVOS; i++) {
      currentAngles[i] += stepSizes[i]; 
      safeWriteServo(i, currentAngles[i]); 
    }
    delay(delayPerStep);
  }

  for (int i = 0; i < NUM_SERVOS; i++) {
    currentAngles[i] = constrain(targetPose.angles[i], SERVO_LIMITS[i][0], SERVO_LIMITS[i][1]);
    safeWriteServo(i, currentAngles[i]);
  }

  if (targetPose.delayAfterMs > 0) {
    delay(targetPose.delayAfterMs);
  }
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;

    char cmd = input[0];

    // COMMAND: Direct Web Servo Control ('S1:90')
    if (cmd == 'S' || cmd == 's') {
      int webServoIdx, angleTarget;
      if (sscanf(input.c_str(), "%*c%d:%d", &webServoIdx, &angleTarget) == 2) {
        int cIdx = webServoIdx - 1; // Map 1-7 to 0-6 array index
        if (cIdx >= 0 && cIdx < NUM_SERVOS) {
          
          // SAFETY: Check if we are trying to control Motor 6
          if (cIdx == 6) {
            angleTarget = constrain(angleTarget, SERVO_LIMITS[6][0], SERVO_LIMITS[6][1]);
          }

          currentAngles[cIdx] = angleTarget;
          safeWriteServo(cIdx, currentAngles[cIdx]);

          // ENFORCE SYNC: If we move index 4, automatically mirror it to index 5
          if (cIdx == 4) {
            currentAngles[5] = angleTarget;
            safeWriteServo(5, currentAngles[5]);
          }
        }
      }
    }

    // COMMAND: Play a Pose (if input starts with a number)
    else if (isDigit(cmd) && input.length() <= 2) {
      int poseNumber = input.toInt();
      if (poseNumber >= 1 && poseNumber <= NUM_POSES) {
        Serial.print("Moving to Pose "); Serial.println(poseNumber);
        moveToPose(animationSequence[poseNumber - 1]);
        Serial.println("Done.");
      }
    }
    
    // COMMAND: Extreme Sweep (Min to Max for Motors 0-5 ONLY, Skips 6 completely)
    else if (cmd == 'e' || cmd == 'E') {
      Serial.println("Running Extreme Sweep for Motors 0-5. Gripper is locked.");

      Pose minPose, maxPose;
      minPose.durationMs = 2000; minPose.delayAfterMs = 500;
      maxPose.durationMs = 2000; maxPose.delayAfterMs = 500;

      for(int i = 0; i < NUM_SERVOS; i++) {
        if(i == 6) {
          minPose.angles[i] = currentAngles[i];
          maxPose.angles[i] = currentAngles[i];
        } else {
          minPose.angles[i] = SERVO_LIMITS[i][0];
          maxPose.angles[i] = SERVO_LIMITS[i][1];
        }
      }

      moveToPose(minPose); // Go to MIN
      moveToPose(maxPose); // Go to MAX
      moveToPose(minPose); // Return to MIN
      Serial.println("Sweep complete.");
    }

    // COMMAND: Change Max Limit ('m 4 180')
    else if (cmd == 'm' || cmd == 'M') {
      int motorIdx, newMax;
      if (sscanf(input.c_str(), "%*c %d %d", &motorIdx, &newMax) == 2) {
        if (motorIdx == 6) {
          Serial.println("DENIED: Motor 6 (Gripper) limits are locked for safety.");
        } 
        else if (motorIdx >= 0 && motorIdx < NUM_SERVOS) {
          SERVO_LIMITS[motorIdx][1] = newMax;
          Serial.print("SUCCESS: Motor "); Serial.print(motorIdx); 
          Serial.print(" max limit is now "); Serial.println(newMax);
        }
      }
    }
    
    // COMMAND: Update Pose ('u 1 4 180')
    else if (cmd == 'u' || cmd == 'U') {
      int poseIdx, motorIdx, newAngle;
      if (sscanf(input.c_str(), "%*c %d %d %d", &poseIdx, &motorIdx, &newAngle) == 3) {
        if (poseIdx >= 1 && poseIdx <= NUM_POSES && motorIdx >= 0 && motorIdx < NUM_SERVOS) {
          animationSequence[poseIdx - 1].angles[motorIdx] = newAngle; 
          Serial.print("SUCCESS: Pose "); Serial.print(poseIdx); 
          Serial.print(", Motor "); Serial.print(motorIdx);
          Serial.print(" target updated to "); Serial.println(newAngle);
        }
      }
    } 
    
    else {
      Serial.println("Invalid command.");
    }
  }
}