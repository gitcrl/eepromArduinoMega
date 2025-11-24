//  gravador EEPROM  for Arduino MEGA
//  adaptado do codigo: Dana Peters from code by E. Klaus
//  Version 0.1
// utilizando ES de 8bits (DDR - PORT)

#define DATA_PORT     PORTL   // PORT for D0-D7 PORTL
#define DATA_DDR      DDRL    // Data Direction Register for D0-D7
#define DATA_PIN      PINL    // Data READ Register for D0-D7

#define ADDR_LO_PORT  PORTC   // PORT for A0-A7  PORTC 
#define ADDR_LO_DDR   DDRC    // Data Direction Register for A0-A7
#define ADDR_HI_PORT  PORTA   // PORT for A8-A14  PORTA
#define ADDR_HI_DDR   DDRA    // Data Direction Register for A8-A14

#define PIN_EPROM_OE  39 // EEPROM OE    PORTG 2  
#define PIN_EPROM_WE  40 // EEPROM WE    PORTG 1
#define PIN_EPROM_CE  41 // EEPROM CE    PORTG 0
#define PIN_LED       53 // LED          PORTB 0

#define MAX_READ_COUNT    10
#define MAX_RECORD_SIZE   64

uint8_t _data[MAX_RECORD_SIZE];

void setup()
{
   Serial.begin(115200);
   pinMode(PIN_LED, OUTPUT);
   digitalWrite(PIN_LED, LOW);
   digitalWrite(PIN_EPROM_WE, HIGH);
   digitalWrite(PIN_EPROM_CE, HIGH);
   digitalWrite(PIN_EPROM_OE, HIGH);
   pinMode(PIN_EPROM_WE, OUTPUT);
   pinMode(PIN_EPROM_CE, OUTPUT);
   pinMode(PIN_EPROM_OE, OUTPUT);
   setDataPinsAsInput();
   setAddressPinsAsOutput();
   setAddress(0);
}

void loop()
{
   static bool ledState;
   static int idleCount;
   digitalWrite(PIN_LED, ledState ? HIGH : LOW);
   if (idleCount-- == 0)
   {
      idleCount = 1000;
      ledState = !ledState;
      Serial.println("EPv1");
   }
   if (Serial.available())
   {
      uint8_t length;
      uint16_t address;
      char value;
      char c = Serial.read();
      switch (c)
      {
         case 'W':  // write data
            idleCount = 5000;
            ledState = true;
            if (receiveWriteCommand(&length, &address))
            {
               if (writeData(length, address))
                  Serial.println("\r\nOK");
               else
                  Serial.println("\r\nWrite error");
            }
            else
               Serial.println("\r\nCommand error");
            break;
         case 'R':  // read data
            idleCount = 5000;
            ledState = true;
            if (receiveReadCommand(&length, &address))
               readData(length, address);
            else
               Serial.println("\r\nCommand error");
            break;
          case 'P': // data protection enable/disable
             idleCount = 5000;
             ledState = true;
             while (!Serial.available())
                delay(1);
             value = Serial.read();
             if (value == '0')
             {
                disableDataProtection();
                Serial.println("\r\nOK");
             }
             else if (value == '1')
             {
                enableDataProtection();
                Serial.println("\r\nOK");
             }
             else
                Serial.println("\r\nCommand error");             
             break;
          default:
             Serial.print(c);
             break;
      }
	}
	delay(1);  
}

//  Receive command to write data to EEPROM
//  Request: Wssaaaadddddddd.....cs (ss=data length, aaaa=address, dd=data, cs=checksum)
//  Example: W102000CE2C8ABDFBDC86F79701961484F79714D3

bool receiveWriteCommand(uint8_t *length, uint16_t *address)
{
   int h = receiveHexByte(); // get data length
   if (h == -1) return false;
   *length = (uint8_t)h;
   if (*length > MAX_RECORD_SIZE) return false;
   h = receiveHexByte(); // get address high byte
   if (h == -1) return false;
   uint8_t address1 = (uint8_t)h;
   h = receiveHexByte(); // get address low byte
   if (h == -1) return false;
   uint8_t address2 = (uint8_t)h;
   *address = (address1 << 8) | address2;
   int sum = *length + address1 + address2;
   for (int i = 0; i < *length; i++)
   {
      h = receiveHexByte(); // get data byte
      if (h == -1) return false;
      uint8_t value = (uint8_t)h;
      _data[i] = value;
      sum += value;
   }
   h = receiveHexByte(); // get checksum
   if (h == -1) return false;
   uint8_t checksum1 = (uint8_t)h;
   uint8_t checksum2 = (uint8_t)((-sum) & 0xff);
   return checksum1 == checksum2;
}

//  Send data read from EEPROM
//  Request: Rssaaaa (ss=data length, aaaa=address)
//  Response: dddddddd.....cs (dd=data, cs=checksum)

bool receiveReadCommand(uint8_t *length, uint16_t *address)
{
   int h = receiveHexByte(); // get data length
   if (h == -1) return false;
   *length = (uint8_t)h;
   if (*length > MAX_RECORD_SIZE) return false;
   h = receiveHexByte(); // get address high byte
   if (h == -1) return false;
   uint8_t address1 = (uint8_t)h;
   h = receiveHexByte(); // get address low byte
   if (h == -1) return false;
   uint8_t address2 = (uint8_t)h;
   *address = (address1 << 8) | address2;
   return true;
}

//  Write data bytes to EEPROM at specified address

bool writeData(uint8_t length, uint16_t address)
{
   uint8_t dataByte, outByte;
   digitalWrite(PIN_EPROM_OE, HIGH);
   digitalWrite(PIN_EPROM_CE, HIGH);
   digitalWrite(PIN_EPROM_WE, HIGH);
   for (int i = 0; i < length; i++)
   {
      setAddress(address + i);
      dataByte = _data[i];
      writeByte(dataByte);
   }
   setDataPinsAsInput();
   digitalWrite(PIN_EPROM_CE, LOW);
   digitalWrite(PIN_EPROM_WE, HIGH);
   digitalWrite(PIN_EPROM_OE, LOW);
   // check DATA POLLING feature for end of write cycle
   for (int i = 0; i < MAX_READ_COUNT; i++)
   {
      delay(1); // write may take up to 10ms
      outByte = readDataByte();        
      if (outByte == dataByte) // write cycle complete?
         break;
   }
   //  read data to verify the write operation
   for (int i = 0; i < length; i++)
   {
      setAddress(address + i);
      dataByte = _data[i];
      outByte = readDataByte();
      if (outByte != dataByte) // check for error
         return false;
   }
   return true;
}

//  Read data bytes from EEPROM at specified address

void readData(uint8_t length, uint16_t address)
{
   digitalWrite(PIN_EPROM_CE, LOW);
   digitalWrite(PIN_EPROM_OE, LOW);
   uint8_t address1 = (address >> 8) & 0xff;
   uint8_t address2 = address & 0xff;
   int sum = length + address1 + address2;
   for (uint16_t i = 0; i < length; i++)
   {
      setAddress(address + i);
      uint8_t value = readDataByte();
      sum += value;
      sendHexByte(value);
   }
   uint8_t checksum = (uint8_t)((-sum) & 0xff);
   sendHexByte(checksum);
   Serial.println();
}

//  Write the address to the outputs

void setAddress(uint16_t address)
{
   uint8_t address1 = (address >> 8) & 0xff; 
   uint8_t address2 = address & 0xff;
   ADDR_LO_PORT = address2;
   ADDR_HI_PORT = address1;
}

// Read the EPROM data pins until consecutive reading match

uint8_t readDataByte(void)
{
   uint8_t read, lastRead;
   setDataPinsAsInput();
   lastRead = read;
   for (int i = 0; i < MAX_READ_COUNT; i++) 
   {
      delayMicroseconds(10);                        
      read = DATA_PIN;
      if (read == lastRead)
         break;
      lastRead = read;  
   }
   return read;
}

// Write data byte to EEPROM after address has been set

void writeByte(uint8_t value)
{
   digitalWrite(PIN_EPROM_OE, HIGH);
   digitalWrite(PIN_EPROM_CE, HIGH);
   digitalWrite(PIN_EPROM_WE, HIGH);
   writeDataByte(value);
   sendWritePulse();
}

// Set the EEPROM data pins

void writeDataByte(uint8_t value)
{
   setDataPinsAsOutput();
   DATA_PORT = value;
}

//  Write a byte to EEPROM
//  Ensure EPROM outputs are DISABLED (OE=1 or CE=1)

void sendWritePulse(void)
{
   digitalWrite(PIN_EPROM_CE, LOW);
   digitalWrite(PIN_EPROM_WE, LOW);
   delayMicroseconds(10);
   digitalWrite(PIN_EPROM_WE, HIGH);
   digitalWrite(PIN_EPROM_CE, HIGH);
}

//  Configure data bus pins as INPUTS
//  Arduino MEGA pins 42-49 PORTL0-PORTL8

void setDataPinsAsInput(void)
{
   DATA_DDR = 0x00;   // set pins 42-49 = INPUTS
   DATA_PORT = 0xff;  // engage pull-up
}

//  Configure data bus pins as OUTPUTS
//  Arduino MEGA pins 42-49 PORTL bits 0-7
//  Ensure EPROM outputs are DISABLED (OE=1 or CE=1)

void setDataPinsAsOutput(void)
{
   DATA_DDR = 0xff;    // set pins 42-49 = OUTPUTS
   DATA_PORT = 0xff;   // set all bits HIGH
}

//  Configure address pins A0-A14 as OUTPUTS 
//  Arduino MEGA pins 30-37 (A0-A7) pins 22-28 (A8-A14) 

void setAddressPinsAsOutput(void)
{
   ADDR_LO_DDR = 0xff;    // DDRC = OUTPUTs   C0-C7
   ADDR_LO_PORT = 0xff;   // PORTC = HIGH     C0-C7
   ADDR_HI_DDR = 0x7f;    // DDRA  = OUTPUTS  A0-A6
   ADDR_HI_PORT = 0x7f;   // PORTA = HIGH     A0-A6
}

// Disable software write protection

void disableDataProtection(void)
{
   digitalWrite(PIN_EPROM_OE, HIGH); 
   digitalWrite(PIN_EPROM_CE, HIGH);
   digitalWrite(PIN_EPROM_WE, HIGH);
   delay(10);  
   setAddress(0x5555);
   writeDataByte(0xAA);
   sendWritePulse();
   setAddress(0x2AAA);
   writeDataByte(0x55);
   sendWritePulse();
   setAddress(0x5555);
   writeDataByte(0x80);
   sendWritePulse();
   setAddress(0x5555);
   writeDataByte(0xAA);
   sendWritePulse();
   setAddress(0x2AAA);
   writeDataByte(0x55);
   sendWritePulse();
   setAddress(0x5555);
   writeDataByte(0x20);
   sendWritePulse();
}

// Enable software write protection

void enableDataProtection(void)
{
   digitalWrite(PIN_EPROM_OE, HIGH); 
   digitalWrite(PIN_EPROM_CE, HIGH);
   digitalWrite(PIN_EPROM_WE, HIGH);
   delay(10);  
   setAddress(0x5555);
   writeDataByte(0xAA);
   sendWritePulse();
   setAddress(0x2AAA);
   writeDataByte(0x55);
   sendWritePulse();
   setAddress(0x5555);
   writeDataByte(0xA0);
   sendWritePulse();
}

//  Wait for 1 hex character and return the nibble value

int receiveHexNibble(void)
{
    while (!Serial.available())
       delay(1);
    char value = Serial.read();
    if (value >= '0' && value <= '9')
       return value - '0';           
    if (value >= 'A' && value <= 'F')
       return value - 'A' + 10;
    return -1;
}

//  Wait for 2 hex characters and return the byte value

int receiveHexByte(void)
{
   int k = receiveHexNibble();
   if (k < 0) return -1;
   uint8_t value = (uint8_t)(k << 4);
   k = receiveHexNibble();
   if (k < 0) return -1;
   value |= (uint8_t)k;
   return value;
}  

//  Send value as 2 hex characters 

void sendHexByte(uint8_t value)
{
   char k = (value >> 4) & 0x0f;
   k += k > 9 ? 0x37 : 0x30;
   Serial.print(k);
   k = value & 0x0f;
   k += k > 9 ? 0x37 : 0x30;
   Serial.print(k);
}  
