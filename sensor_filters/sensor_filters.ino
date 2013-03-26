#include <FreeSixIMU.h>
#include <FIMU_ADXL345.h>
#include <FIMU_ITG3200.h>

#include <Wire.h>
#include <PID_v1.h>


#define ADDRESS1            0x58                    // Address of MD03 for motor 1
#define ADDRESS2            0x5B                    // Address of MD03 for motor 2
#define SOFTREG             0x07                    // Byte to read software
#define CMDBYTE             0x00                    // Command byte
#define SPEEDBYTE           0x02                    // Byte to write to speed register
#define TEMPREG             0x04                    // Byte to read temprature
#define CURRENTREG          0x05                    // Byte to read motor current

// *** PID ***

//PID variabelen
double Input, Output;
 // PID waarden initialiseren.
double Kp = 100;
double Ki = 0;
double Kd = 500;
double Setpoint = 0;

//PID: Specify the links and initial tuning parameters
PID myPID(&Input, &Output, &Setpoint,Kp,Ki,Kd, DIRECT);

//PID frontend
//Specify the links and initial tuning parameters
//PID myPID(&Input, &Output, &Setpoint,2,5,1, DIRECT);

unsigned long serialTime; //this will help us know when to talk with processing

/// *** Sensor ***

float angles[3];
float angle;
double offset;

FreeSixIMU sensor = FreeSixIMU();        // aanmaken van FreeSixIMU object

//I2C
byte motorDir = 1;

void setup(){
  Serial.begin(9600);                    // activeer serial communicatie
  Wire.begin();                         // initialiseer I2C communicatiemet sensor 
  delay(5);
  sensor.init();                         // begin the IMU
  delay(5);
  
//  Serial.print("reset");
  sendData(CMDBYTE, motorDir, SPEEDBYTE, 0);
  
  // calibreren van de sensor neemt de gemiddelde sensor waarde van 3 seconden, 
  // vervolgens meet hij de hoke nog een keer, als de error groter is dan 1 graden begint de calibratie opnieuw
//  double offset = 0;
//  double check = 1000;
//  while (abs(offset - check) > 1){
//    for(int i = 0; i < 30; i++){
//    sensor.getEuler(angles);
//    offset += angles[2];
//    delay(10);
//    }
//    offset = offset / 30;
//    sensor.getEuler(angles);
//    check = angles[2];
//  }  
//  sensor.getEuler(angles);
//  Input = angles[2] - offset;
  
  //turn the PID on
  myPID.SetMode(AUTOMATIC);  
}

void loop(){
  sensor.getEuler(angles);
  angle = angles[2]-offset;
  
  if(angle <= 2 && angle >= -2){
    angle = 0;
  }
  else if(angle > 0){
    angle = angle * -1;
    motorDir = 1;
  } 
  else{
    motorDir = 2;
  }
 // output berekenen met PID, input is de hoekin radialen.
  Input = (angle * (3.14159/180));
//  Serial.print(Input);
//  Serial.println(" voor PID");
  myPID.Compute();
  
    //send-receive with processing if it's time
  if(millis()>serialTime)
  {
    SerialReceive();
    SerialSend();
    serialTime+=20;
  }
    
  byte b = (byte)Output;
//  Serial.print(b);
//  Serial.println(" na PID"); 
//  Serial.println();
  sendData(CMDBYTE, motorDir, SPEEDBYTE, b); 
  
//  for(int i = 0; i < 10000; i++){
//      Serial.println(i % 100);
//      sendData(CMDBYTE, motorDir, SPEEDBYTE, (byte)(i % 100)); 
//  }
  

  //serial read voor PID waardes
  //if (Serial.available() > 0 {
    //read incoming byte
    
  
  //kijk of de wire.endtransmission een fout geeft
//  int return_value = Wire.endTransmission ();
//Serial.print ("end returns:");
//Serial.println (return_value);

  // sending I2C data

}

// *** PID serial communication functions ***

union {                // This Data structure lets
  byte asBytes[24];    // us take the byte array
  float asFloat[6];    // sent from processing and
}                      // easily convert it to a
foo;                   // float array



// getting float values from processing into the arduino
// was no small task.  the way this program does it is
// as follows:
//  * a float takes up 4 bytes.  in processing, convert
//    the array of floats we want to send, into an array
//    of bytes.
//  * send the bytes to the arduino
//  * use a data structure known as a union to convert
//    the array of bytes back into an array of floats

//  the bytes coming from the arduino follow the following
//  format:
//  0: 0=Manual, 1=Auto, else = ? error ?
//  1: 0=Direct, 1=Reverse, else = ? error ?
//  2-5: float setpoint
//  6-9: float input
//  10-13: float output  
//  14-17: float P_Param
//  18-21: float I_Param
//  22-245: float D_Param
void SerialReceive()
{

  // read the bytes sent from Processing
  int index=0;
  byte Auto_Man = -1;
  byte Direct_Reverse = -1;
  while(Serial.available()&&index<26)
  {
    if(index==0) Auto_Man = Serial.read();
    else if(index==1) Direct_Reverse = Serial.read();
    else foo.asBytes[index-2] = Serial.read();
    index++;
  } 
  
  // if the information we got was in the correct format, 
  // read it into the system
  if(index==26  && (Auto_Man==0 || Auto_Man==1)&& (Direct_Reverse==0 || Direct_Reverse==1))
  {
    Setpoint=double(foo.asFloat[0]);
    //Input=double(foo.asFloat[1]);       // * the user has the ability to send the 
                                          //   value of "Input"  in most cases (as 
                                          //   in this one) this is not needed.
    if(Auto_Man==0)                       // * only change the output if we are in 
    {                                     //   manual mode.  otherwise we'll get an
      Output=double(foo.asFloat[2]);      //   output blip, then the controller will 
    }                                     //   overwrite.
    
    double p, i, d;                       // * read in and set the controller tunings
    p = double(foo.asFloat[3]);           //
    i = double(foo.asFloat[4]);           //
    d = double(foo.asFloat[5]);           //
    myPID.SetTunings(p, i, d);            //
    
    if(Auto_Man==0) myPID.SetMode(MANUAL);// * set the controller mode
    else myPID.SetMode(AUTOMATIC);             //
    
    if(Direct_Reverse==0) myPID.SetControllerDirection(DIRECT);// * set the controller Direction
    else myPID.SetControllerDirection(REVERSE);          //
  }
  Serial.flush();                         // * clear any random data from the serial buffer
}

// unlike our tiny microprocessor, the processing ap
// has no problem converting strings into floats, so
// we can just send strings.  much easier than getting
// floats from processing to here no?
void SerialSend()
{
  Serial.print("PID ");
  Serial.print(Setpoint);   
  Serial.print(" ");
  Serial.print(Input);   
  Serial.print(" ");
  Serial.print(Output);   
  Serial.print(" ");
  Serial.print(myPID.GetKp());   
  Serial.print(" ");
  Serial.print(myPID.GetKi());   
  Serial.print(" ");
  Serial.print(myPID.GetKd());   
  Serial.print(" ");
  if(myPID.GetMode()==AUTOMATIC) Serial.print("Automatic");
  else Serial.print("Manual");  
  Serial.print(" ");
  if(myPID.GetDirection()==DIRECT) Serial.println("Direct");
  else Serial.println("Reverse");
}


// *** motor functions ***

void sendData(byte dirReg, byte dirVal, byte speedReg, byte speedVal){         // Function for sending data to MD03
  Wire.beginTransmission(ADDRESS1);         // Send data to MD03
    Wire.write(dirReg);
    Wire.write(dirVal);
    Wire.write(speedReg);
    Wire.write(speedVal);
  Wire.endTransmission(true);
//  Serial.print("Wire return: ");
//  Serial.println(b);
  
  Wire.beginTransmission(ADDRESS2);
    Wire.write(dirReg);
    Wire.write(dirVal);
    Wire.write(speedReg);
    Wire.write(speedVal);
  Wire.endTransmission();
//  Serial.println(b);  
}

//void sendPlotData(String seriesName, float data){
//    Serial.print("{");
//    Serial.print(seriesName);
//    Serial.print(",T,");
//    Serial.print(data);
//    Serial.println("}");
//} 


  

