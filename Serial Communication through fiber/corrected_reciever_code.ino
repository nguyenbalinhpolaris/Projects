int photoresistor = 3;
int recievedArray[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile unsigned long timeBegin;
volatile unsigned long timeEnd;
bool availability = false; 
int i = 0; 
void timing(){
int k = digitalRead(photoresistor);
if(k==1){
  timeBegin = millis();
  availability = false;
}
if(k==0){
  timeEnd = millis();
  availability = true;
}
}
void write(){//Write the recieved binary signal to the array
unsigned long signalLength = timeEnd - timeBegin;
if(signalLength < 2100 && signalLength > 1900)
{
  recievedArray[i] = 1;
}
if(signalLength < 1100 && signalLength > 900)
{
  recievedArray[i] = 0;
}
}

void setup() {
  // put your setup code here, to run once:
Serial.begin(19200);
attachInterrupt(digitalPinToInterrupt(photoresistor), timing ,CHANGE );
}

void loop() {
  // put your main code here, to run repeatedly:

if(availability == true && i <= 15)//the use of availability is to allow the code to run only when signalLength is available
{
  write();
  i++;
  availability = false;
}
if(i > 15)
{
  //Perform calculating & extracting the number, reset the i count to recieve new numbers, print on LCD
double totalSum = 0;
for(int a = 15; a >= 0; a--)
{
double k = recievedArray[a]*pow(2, 15-a);
totalSum = totalSum + k;
}
Serial.println(totalSum);
i = 0;
for(int a = 0; a <= 15; a++)
  {
    recievedArray[a] = 0; //resetting the array so it can be written on again
  }
}
}