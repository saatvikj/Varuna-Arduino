#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h> 
#include <String.h>
#define DEBUG true

int ledPin1 = 8;
int ledPin2 = 9;
int ledPin3 = 10;
int val;
float calibrationFactor=4.5;
volatile byte pulseCount;  
float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;
unsigned long oldTime;
int waterOrNot;
int plantNumber;
void setup()
{
  Serial.begin(9600);
  Serial3.begin(115200); //Serial port for the ESP8266
  //Initializing the LED pins as OUTPUT ports
  pinMode(ledPin1 , OUTPUT); 
  pinMode(ledPin2 , OUTPUT);
  pinMode(ledPin3 , OUTPUT);
  //Initiallizing the pins for moisture sensors as INPUT ports
  pinMode(A13 , INPUT);
  pinMode(A14 , INPUT);
  pinMode(A15 , INPUT);
  //Initializing the pins for relays as OUTPUT ports
  pinMode(22,OUTPUT); //pump
  pinMode(11,OUTPUT); //valve
  digitalWrite(11,LOW);
  pinMode(12,OUTPUT); //valve
  digitalWrite(12,LOW);
  pinMode(13,OUTPUT); //valve
  digitalWrite(13,LOW);
  //Initializing the pins for waterflow sensors as INPUT portss
  pinMode(18, INPUT);
  digitalWrite(18, HIGH);
  pinMode(19, INPUT);
  digitalWrite(19, HIGH);
  pinMode(20, INPUT);
  digitalWrite(20, HIGH);
  sendCommand("AT+RST\r\n",2000,DEBUG); // reset module
  sendCommand("AT+CWMODE=3\r\n",1000,DEBUG); // configure as access point
  sendCommand("AT+CWJAP=\"Moto G (4) 8350\",\"shravi27\"\r\n",3000,DEBUG); //connect to the specified wifi network
  delay(10000);
  sendCommand("AT+CIFSR\r\n",1000,DEBUG); // get ip address
  sendCommand("AT+CIPMUX=1\r\n",1000,DEBUG); // configure for multiple connections
  sendCommand("AT+CIPSERVER=1,80\r\n",1000,DEBUG); // turn on TCP server on port 80
  Serial.println("Server ready");
  
}

void loop() {
  // put your main code here, to run repeatedly:
  String val="";
     while(Serial3.available()>0) //read the value on ESP8266 
     {
      char c= Serial3.read();
      val+=c;
      delay(10);
     }
     int findNumber=val.indexOf("="); //find = in the string, if found that means relevant data has been read
     
     if(findNumber!=-1) //perform operations if relevant data is read
      {

         int connectionId=0;
         //assign the plant number and check if user has to water or just see details according to incoming string
         if(val.substring(findNumber+1,findNumber+2)=="0") //plant 1
          plantNumber=1;
         else if(val.substring(findNumber+1,findNumber+2)=="5") //plant 2
          plantNumber=2; 
         else if(val.substring(findNumber+1,findNumber+2)=="9") //plant 3
          plantNumber=3;
         if(val.substring(findNumber+2,findNumber+3)=="Y") //water
          waterOrNot=1;
         else if(val.substring(findNumber+2,findNumber+3)=="N") //details
          waterOrNot=0;
         Serial.println(plantNumber);
         Serial.println(waterOrNot); 
         String content;
         boolean flag=true;
         if(flag)
         {     
         int valvePin=plantNumber+10;
         int soilMoisturePin=plantNumber+12;
         int waterFlowPin=plantNumber+17;
         int temperaturePin=2*(plantNumber+11);
    
        //Initialization for waterflow sensor
         pulseCount=0;
         flowRate=0.0;
         flowMilliLitres=0;
         totalMilliLitres=0;
         oldTime=0;  
         int moistureVal; //variable storing moisture content
         double temperatureVal; //variable storing soil temperature
         attachInterrupt(digitalPinToInterrupt(temperaturePin), pulseCounter, FALLING); //Interrupt for waterflow sensor  
         moistureVal=soilMoisture(soilMoisturePin); //extracting moisture content
         temperatureVal=temperatureSensor(temperaturePin); //extracting temperature values
         if(moistureVal>=800 && waterOrNot==1) //checking if plant needs water
         {
           digitalWrite(valvePin,HIGH); //opening valve 
           delay(1000); 
           digitalWrite(22,HIGH); //opening pump    
           int val1=moistureVal;
           Serial.println(val1);
           while(val1>800) //watering till value reaches normal state
           {
             val1=soilMoisture(soilMoisturePin);
             waterFlowSensor(waterFlowPin); //keeping track of water content
           }
           digitalWrite(22,LOW); //closing the pump
           digitalWrite(valvePin,LOW); //closing the valve
         }
    
         moistureVal=map(moistureVal,300,1023,100,0);
         content += moistureVal;
         content += "% moisture content ";
         content += temperatureVal;
         content += "°C soil temperature ";
         if(waterOrNot==1) //if plant is watered, tell how much water was given
          {
            content+="Amount of water given (in mL) ";
            content+=totalMilliLitres;
          }
         }
    
         sendHTTPResponse(connectionId,content); //send the response
         
         // make close command
         String closeCommand = "AT+CIPCLOSE="; 
         closeCommand+=connectionId; // append connection id
         closeCommand+="\r\n";
         
         sendCommand(closeCommand,1000,DEBUG); // close connection
         sendCommand("AT+RST\r\n",2000,DEBUG); // reset module so that the data gets reset
         sendCommand("AT+CWMODE=3\r\n",1000,DEBUG); // configure as access point
         sendCommand("AT+CIPMUX=1\r\n",1000,DEBUG); // configure for multiple connections
         sendCommand("AT+CIPSERVER=1,80\r\n",1000,DEBUG); // turn on TCP server again on port 80
         Serial.println("Server ready");        
      }
}

int soilMoisture(int analogPin)
{
  val = analogRead(analogPin); //read value from moisture sensor
  Serial.println(val);
  //check what condition the soil is in right now and accordingly light up corresponding alert LED
  if (val<300) //overflow condition
  {
    digitalWrite(ledPin1, HIGH);
    Serial.println("Overflow");
  }
  if (val>300 && val<=800) //normal condition
  {
    digitalWrite(ledPin2, HIGH);
    Serial.println("Normal Condition");
  }
  if (val>=800) //dry condition
  {
    digitalWrite(ledPin3, HIGH);
    Serial.println("Dry Condition");
  }
  delay(1000);
  digitalWrite(ledPin1, LOW);
  digitalWrite(ledPin2, LOW);
  digitalWrite(ledPin3, LOW);
  return val;
}



double temperatureSensor(int analogPin)
{
  OneWire oneWire(analogPin);
  DallasTemperature sensors(&oneWire);
  sensors.begin();
  sensors.requestTemperatures();
  delay(1000);
  return sensors.getTempCByIndex(0); 
}



void waterFlowSensor(int pin)
{
   
   if((millis() - oldTime) > 1000)   
    {
    detachInterrupt(digitalPinToInterrupt(pin));   
    flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
    oldTime = millis();
    flowMilliLitres = (flowRate / 60) * 1000;
    totalMilliLitres += flowMilliLitres;
    unsigned int frac;
    Serial.print("Flow rate: ");
    frac = (flowRate - int(flowRate)) * 10;
    Serial.print("  Current Liquid Flowing: ");             // Output separator
    Serial.print(flowMilliLitres);
    Serial.print("mL/Sec");
    pulseCount = 0;
    attachInterrupt(digitalPinToInterrupt(pin), pulseCounter, FALLING);
  }

}


void pulseCounter()
{
  pulseCount++;
}


String sendData(String command, const int timeout, boolean debug)
{
    String response = "";
    
    int dataSize = command.length();
    char data[dataSize];
    command.toCharArray(data,dataSize);
             
    Serial3.write(data,dataSize); // send the read character to the esp8266
    if(debug)
    {
      Serial.println("\r\n====== HTTP Response From Arduino ======");
      Serial.write(data,dataSize);
      Serial.println("\r\n========================================");
    }
    
    long int time = millis();
    
    while( (time+timeout) > millis())
    {
      while(Serial3.available())
      {
        
        // The esp has data so display its output to the serial window 
        char c = Serial3.read(); // read the next character.
        response+=c;
      }  
    }
    
    if(debug)
    {
      Serial.print(response);
    }
    
    return response;
}
 
/*
* Name: sendHTTPResponse
* Description: Function that sends HTTP 200, HTML UTF-8 response
*/
void sendHTTPResponse(int connectionId, String content)
{
     
     // build HTTP response
     String httpResponse;
     String httpHeader;
     // HTTP Header
     httpHeader = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n"; 
     httpHeader += "Content-Length: ";
     httpHeader += content.length();
     httpHeader += "\r\n";
     httpHeader +="Connection: close\r\n\r\n";
     httpResponse = httpHeader + content + " "; // There is a bug in this code: the last character of "content" is not sent, I cheated by adding this extra space
     sendCIPData(connectionId,httpResponse);

} 
/*
* Name: sendCIPDATA
* Description: sends a CIPSEND=<connectionId>,<data> command
*
*/
void sendCIPData(int connectionId, String data)
{
   String cipSend = "AT+CIPSEND=";
   cipSend += connectionId;
   cipSend += ",";
   cipSend +=data.length();
   cipSend +="\r\n";
   sendCommand(cipSend,1000,DEBUG);
   sendData(data,1000,DEBUG);
}
 
/*
* Name: sendCommand
* Description: Function used to send data to ESP8266.
* Params: command - the data/command to send; timeout - the time to wait for a response; debug - print to Serial window?(true = yes, false = no)
* Returns: The response from the esp8266 (if there is a reponse)
*/

String sendCommand(String command, const int timeout, boolean debug){
    String response = "";
           
    Serial3.print(command); // send the read character to the esp8266
    
    long int time = millis();
    
    while( (time+timeout) > millis())
    {
      while(Serial3.available())
      {
        
        // The esp has data so display its output to the serial window 
        char c =  Serial3.read(); // read the next character.
        response+=c;
      }  
    }
    
    if(debug)
    {
      Serial.print(response);
    }
    
    return response;
}
