int ADSK = D1;
int ADDO = D0;
int Gain = 0;
int state = 0;
int loopCount = 0;

// Change these to calibrate
const float readingWithNothing = 8644377; // 'Nothing' means the weight of the apparatus and scooper or whatever.
const float readingWith3SeltzerCans = 9059920; // A highly scientific unit.

// Legit constant and calculated consts
const float weightOf3SeltzerCansInGrams = 1113;
const float scaleUnitsOf3SeltzerCans = readingWith3SeltzerCans - readingWithNothing;
const float gramsPerScaleUnit = weightOf3SeltzerCansInGrams / scaleUnitsOf3SeltzerCans;

// Timing consts
const unsigned int secondsPerLoop = 10;
const unsigned int minutesPerUpdate = 10;
const unsigned int loopsUntilReport = ((60 / secondsPerLoop) * minutesPerUpdate);

TCPClient client;
char message [100];

void resetAD(int gain)
{
	Gain = gain;
	digitalWrite(ADSK, HIGH); 
	delayMicroseconds(1);
	digitalWrite(ADSK, LOW);
	readADOutput(); // Take a junk reading to set the gain for the first reading.
}

unsigned long readADOutput()
{
	noInterrupts();

	unsigned long Count;
	delayWrite(ADSK, LOW); // Start measuring (PD_SCK LOW)
	Count=0;
	while(digitalRead(ADDO) == HIGH); // Wait until ADC is ready
	for (int i=0;i<24;i++)
	{
		delayWrite(ADSK, HIGH); // PD_SCK HIGH (Start) 
		Count=Count<<1; // Shift Count left at falling edge
		if(digitalRead(ADDO) == LOW) // This is funky but since the number comes back 2's compliment, we need to invert the bits. Might as well read it inverted. 
		{
			Count++;
		}
		delayWrite(ADSK, LOW); // PD_SCK LOW
	}

	setGainPulsesForNextReading();

	// The number is in 2's compliment. To Convert it to a regular number, swap the 24th bit and add 1.
	Count = Count^0x800000;
	
	interrupts();
	
	// Delay so the HX-711 doesn't think we are sending another pulse to set the gain.
	delay(20);
	
	// Setting the clock high puts the HX-711 to sleep. 
	delayWrite(ADSK, HIGH);
	
	return Count;
}

void delayWrite(int pin, bool highOrLow)
{
	digitalWrite(pin, highOrLow);
	delayMicroseconds(1);
}

// Pass in a 1, 2, or 3 for Gain level and channel input. 
// With the HX-711 every time you read data you program how the next read will work by sending bonus pulses to the serial clock. 
// 1 Channel A gain of 128
// 2 Channel B gain of 32
// 3 Channel A gain of 64
void setGainPulsesForNextReading()
{
	if(Gain < 1 || Gain > 3)
	{
		// I'm saving you from yourself.
		Gain = 1;	 
	}
	for(int i = 0; i < Gain; i++)
	{
		delayWrite(ADSK, HIGH); 
		delayWrite(ADSK, LOW);
	}
}

float ConvertToGrams(unsigned long adOutput)
{
	float returnVal = (float)abs(adOutput - readingWithNothing) * gramsPerScaleUnit;
	return returnVal;
}

void setup()
{
	state = 0;
	pinMode(ADDO, INPUT);
	pinMode(ADSK, OUTPUT);
	resetAD(1);
	Particle.publish("DEBUG", "setup complete!");
}

void loop()
{
	delay(secondsPerLoop * 1000);
	unsigned long output = readADOutput();
	unsigned long outputMinusOffset = abs(output - readingWithNothing);
	int grams = (int)ConvertToGrams(output);

	if (loopCount >= loopsUntilReport)
	{
		sprintf(message, "%d", grams);
		Particle.publish("grams", message, PUBLIC);
		loopCount = 0;
	}
	else
	{
		sprintf(message, "grams: %d	|	raw with offset: %lu	|	raw without offset: %lu | loopCount: %d", grams, outputMinusOffset, output, loopCount);
		Particle.publish("DEBUG", message);
		loopCount++;
	}
}
