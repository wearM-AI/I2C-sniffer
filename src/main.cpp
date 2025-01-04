/**
 * @AUTHOR Ákos Szabó (Whitehawk Tailor) - aaszabo@gmail.com
 * 
 * This is an I2C sniffer that logs traffic on I2C BUS.
 * 
 * It is not part of the I2C BUS. It is neither a Master, nor a Slave and puts no data to the lines.
 * It just listens and logs the communication.
 * 
 * Two pins as imput are attached to SDC and SDA lines.
 * Since the I2C communications runs on 400kHz so,
 * the tool that runs this program should be fast.
 * This was tested on an ESP32 bord Heltec WiFi Lora32 v2
 * ESP32 core runs on 240MHz.
 * It means there are 600 ESP32 cycles during one I2C clock tick.
 *
 * 
 * The program uses interrupts to detect
 * the raise edge of the SCL - bit transfer 
 * the falling edge of SDA if SCL is HIGH- START
 * the raise edge of SDA if SCL id HIGH - STOP 
 * 
 * In the interrupt routines there is just a few line of code
 * that mainly sets the status and stores the incoming bits.
 * Otherwise the program gets timeout panic in interrupt handler and
 * restart the CPU.
 * 
 */


#include <Arduino.h>
#include <Wire.h>
#include <vector>

//#define I2CTEST  //use it to  run a blinking LED test on SDA and SCL pins

#define PIN_SDA 5  //white
#define PIN_SCL 6  //yellow

#define I2C_IDLE 0
//#define I2C_START 1
#define I2C_TRX 2
//#define I2C_RESP 3
//#define I2C_STOP 4
#define SLAVE_ADDR 119
#define I2C_CLK 400000
#define I2C_BUFFER_SIZE 64



static volatile byte i2cStatus = I2C_IDLE;//Status of the I2C BUS
static uint32_t lastStartMillis = 0;//stoe the last time
static volatile byte dataBuffer[9600];//Array for storing data of the I2C communication
static volatile uint16_t bufferPoiW=0;//points to the first empty position in the dataBufer to write
static uint16_t bufferPoiR=0;//points to the position where to start read from
static volatile byte bitCount = 0;//counter of bit appeared on the BUS
static volatile uint16_t byteCount =0;//counter of bytes were writen in one communication.
static volatile byte i2cBitD =0;//Container of the actual SDA bit
static volatile byte i2cBitD2 =0;//Container of the actual SDA bit
static volatile byte i2cBitC =0;//Container of the actual SDA bit
static volatile byte i2cClk =0;//Container of the actual SCL bit
static volatile byte i2cAck =0;//Container of the last ACK value
static volatile byte i2cCase =0;//Container of the last ACK value
static volatile uint16_t falseStart = 0;//Counter of false start events
//static volatile byte respCount =0;//Auxiliary variable to help detect next byte instead of STOP
//these variables just for statistic reasons
static volatile uint16_t sclUpCnt = 0;//Auxiliary variable to count rising SCL
static volatile uint16_t sdaUpCnt = 0;//Auxiliary variable to count rising SDA
static volatile uint16_t sdaDownCnt = 0;//Auxiliary variable to count falling SDA

void IRAM_ATTR i2cTriggerOnRaisingSCL();
void IRAM_ATTR i2cTriggerOnChangeSDA();


#pragma pack(push, 1)

enum i2c_msg_type{
  TYPE_UNKNOWN = 0,
  LOCAL_TIMESTAMP,
  RECORDING_NO,
  I2C_TYPE_LAST,
};

struct i2c_packet{
  uint8_t msg_type;
  uint8_t msg_id;
  uint8_t dummy[2];
  uint32_t data[7];
}; // Minimum buffer size is 32 bytes, so no reason to make it more efficient

#pragma pack(pop)

std::vector<i2c_packet> pkt_buffer;

////////////////////////////
//// Interrupt handlers
/////////////////////////////

/**
 * Rising SCL makes reading the SDA
 * 
 */
void IRAM_ATTR i2cTriggerOnRaisingSCL() 
{
	// detachInterrupt(PIN_SCL); //trigger for reading data from SDA
	// detachInterrupt(PIN_SDA); //for I2C START and STOP
	// uint8_t i2cbitC1 =0, i2cbitC2 =0;
	// do
	// {
	// 	i2cbitC1 =  digitalRead(PIN_SCL);
	// 	i2cbitC2 =  digitalRead(PIN_SCL);
	// } while (i2cbitC1!=i2cbitC2);
	
	sclUpCnt++;
	
	//is it a false trigger?
	if(i2cStatus==I2C_IDLE)
	{
		falseStart++;
		//return;//this is not clear why do we have so many false START
	}

	do
	{
		i2cBitD =  digitalRead(PIN_SDA);
		i2cBitD2 =  digitalRead(PIN_SDA);
	} while (i2cBitD!=i2cBitD2);

	//get the value from SDA
	i2cBitC =  i2cBitD;

	//decide wherewe are and what to do with incoming data
	i2cCase = 0;//normal case

	if(bitCount==8)//ACK case
		i2cCase = 1;

	if(bitCount==7 && byteCount==0 )// R/W if the first address byte
		i2cCase = 2;

	bitCount++;

	switch (i2cCase)
	{
		case 0: //normal case
			dataBuffer[bufferPoiW++] = '0' + i2cBitC;//48
		break;//end of case 0 general
		case 1://ACK
			if(i2cBitC)//1 NACK SDA HIGH
				{
					dataBuffer[bufferPoiW++] = '-';//45
				}
				else//0 ACK SDA LOW
				{
					dataBuffer[bufferPoiW++] = '+';//43
				}	
			byteCount++;
			bitCount=0;
		break;//end of case 1 ACK
		case 2:
			if(i2cBitC)
			{
				dataBuffer[bufferPoiW++] = 'R';//82
			}
			else
			{
				dataBuffer[bufferPoiW++] = 'W';//87
			}
		break;//end of case 2 R/W

	}//end of switch
    // attachInterrupt(PIN_SCL, i2cTriggerOnRaisingSCL, RISING); //trigger for reading data from SDA
	// attachInterrupt(PIN_SDA, i2cTriggerOnChangeSDA, CHANGE); //for I2C START and STOP
}//END of i2cTriggerOnRaisingSCL() 

/**
 * This is for recognizing I2C START and STOP
 * This is called when the SDA line is changing
 * It is decided inside the function wheather it is a rising or falling change.
 * If SCL is on High then the falling change is a START and the rising is a STOP.
 * If SCL is LOW, then this is the action to set a data bit, so nothing to do.
 */
void IRAM_ATTR i2cTriggerOnChangeSDA()
{
	// detachInterrupt(PIN_SCL); //trigger for reading data from SDA
	// detachInterrupt(PIN_SDA); //for I2C START and STOP
	//make sure that the SDA is in stable state
	do
	{
		i2cBitD =  digitalRead(PIN_SDA);
		i2cBitD2 =  digitalRead(PIN_SDA);
	} while (i2cBitD!=i2cBitD2);

	uint8_t i2cbitC1 =0, i2cbitC2 =0;
	do
	{
		i2cbitC1 =  digitalRead(PIN_SCL);
		i2cbitC2 =  digitalRead(PIN_SCL);
	} while (i2cbitC1!=i2cbitC2);

	i2cClk = digitalRead(PIN_SCL);

	i2cBitD =  digitalRead(PIN_SDA);

	if(i2cBitD)//RISING if SDA is HIGH (1)
	{
		
		// i2cClk = digitalRead(PIN_SCL);
		if(i2cStatus!=I2C_IDLE && i2cClk==1)//If SCL still HIGH then it is a STOP sign
		{			
			//i2cStatus = I2C_STOP;
			i2cStatus = I2C_IDLE;
			bitCount = 0;
			byteCount = 0;
			bufferPoiW--;
			dataBuffer[bufferPoiW++] = 's';//115
			dataBuffer[bufferPoiW++] = '\n'; //10
		}
		sdaUpCnt++;
	}
	else //FALLING if SDA is LOW
	{
		
		// i2cClk = digitalRead(PIN_SCL);
		if(i2cStatus==I2C_IDLE && i2cClk)//If SCL still HIGH than this is a START
		{
			i2cStatus = I2C_TRX;
			//lastStartMillis = millis();//takes too long in an interrupt handler and caused timeout panic and CPU restart
			bitCount = 0;
			byteCount =0;
			dataBuffer[bufferPoiW++] = 'S';//83 STOP
			//i2cStatus = START;		
		}
		sdaDownCnt++;
	}
	// attachInterrupt(PIN_SCL, i2cTriggerOnRaisingSCL, RISING); //trigger for reading data from SDA
	// attachInterrupt(PIN_SDA, i2cTriggerOnChangeSDA, CHANGE); //for I2C START and STOP
}//END of i2cTriggerOnChangeSDA()


////////////////////////////////
//// Functions
////////////////////////////////

/**
 * Reset all important variable
 */
void resetI2cVariable()
{
	i2cStatus = I2C_IDLE;
	bufferPoiW=0;
	bufferPoiR=0;
	bitCount =0;
	falseStart = 0;
}//END of resetI2cVariable()


/**
* @DESC Write out the buffer to the serial console
*
*/
void processDataBuffer()
{
	if(pkt_buffer.size() > 0){
		Serial.print("t" + String(pkt_buffer.front().msg_type) + "i" + String(pkt_buffer.front().msg_id));
		for(int idx = 0; idx < 7; idx++){
			Serial.print("d" + String(idx) + ":" + String(pkt_buffer[0].data[idx]));
		}
		Serial.print("\n");
		pkt_buffer.erase(pkt_buffer.begin());
	}
}//END of processDataBuffer()


void i2c_isr(int param){
  #define I2C_RCV_BUFFER_LEN 32
  #define TIME_DISCREPANCY 50000
  uint8_t buffer[I2C_RCV_BUFFER_LEN];
  Wire.readBytes(buffer, I2C_RCV_BUFFER_LEN);
  i2c_packet* pkt = (i2c_packet*)&buffer;

  pkt_buffer.push_back(*pkt);

  if(pkt_buffer.size() >= 10)
	pkt_buffer.erase(pkt_buffer.begin());

	// Serial.print("got something");
}


/////////////////////////////////
////  MAIN entry point of the program
/////////////////////////////////
void setup() 
{
    Wire.begin(SLAVE_ADDR, PIN_SDA, PIN_SCL, I2C_CLK);
    Wire.onReceive(i2c_isr);
    Wire.setBufferSize(I2C_BUFFER_SIZE);
	Serial.begin(115200);
}//END of setup

/**
 * LOOP
 */
void loop() 
{
	processDataBuffer();
	sleep(0.1);
}//END of loop
