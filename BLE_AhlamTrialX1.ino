// Example sketch for interfacing with the DS1302 timekeeping chip.
//
// Copyright (c) 2009, Matt Sparks
// All rights reserved.
//
// http://quadpoint.org/projects/arduino-ds1302
#include <stdio.h>
#include <DS1302.h>

#include "FS.h"
#include "SD.h"
#include "SPI.h"
//#include "EEPROM.h"


#include <BLESniff.h>
#include "SimpleBLE.h"

namespace {

// Set the appropriate digital I/O pin connections. These are the pin
// assignments for the Arduino as well for as the DS1302 chip. See the DS1302
// datasheet:
//
//   http://datasheets.maximintegrated.com/en/ds/DS1302.pdf
const int kCePin   = 27; //12;  // Chip Enable
const int kIoPin   = 14;  // Input/Output
const int kSclkPin = 12; //27;  // Serial Clock

// Create a DS1302 object.
DS1302 rtc(kCePin, kIoPin, kSclkPin);

String dayAsString(const Time::Day day) {
  switch (day) {
    case Time::kSunday: return "Sunday";
    case Time::kMonday: return "Monday";
    case Time::kTuesday: return "Tuesday";
    case Time::kWednesday: return "Wednesday";
    case Time::kThursday: return "Thursday";
    case Time::kFriday: return "Friday";
    case Time::kSaturday: return "Saturday";
  }
  return "(unknown day)";
}






/******************************************************************** RTC Functinos ***************************************/


Time sys_time(2020, 1, 1, 0, 0, 0, Time::kSunday);

void printTime() {
  // Get the current time and date from the chip.
  sys_time = rtc.time();

  // Name the day of the week.
  //const String day = dayAsString(sys_time.day);

  // Format the time and date and insert into the temporary buffer.
  char buf[50];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d %02d-%02d-%04d",
           sys_time.hr, sys_time.min, sys_time.sec, 
           sys_time.date, sys_time.mon,sys_time.yr );

  // Print the formatted string to serial so we can see the time.
  Serial.print(buf);
}

}  // namespace


BLESniff ble;
void bleMACCallBack(unsigned char *, int, unsigned char *);
String SHA256(String data);


typedef struct{
    unsigned char mac[6];
    int rssi;
    unsigned char info[6];
    unsigned char timedata[6]; 
}scan_hit;

scan_hit scan_results_array[1000];
unsigned int scan_result_num = 0;

//scan_hit1 scan_results[2][1024];


char hex[256];
uint8_t data[256];
int start = 0;
int seconds = 0;
int unit_id;

uint8_t hash[32];
String pin;
#define SHA256_BLOCK_SIZE 32

typedef struct {
  uint8_t data[64];
  uint32_t datalen;
  unsigned long long bitlen;
  uint32_t state[8];
} SHA256_CTX;


void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len);
void sha256_final(SHA256_CTX *ctx, uint8_t hash[]);

#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))
#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))

#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

static const uint32_t k[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

void setup() {
  int val, x;  
  char timestamp[50];
  char *ptr;
  
  pinMode(5, OUTPUT);
  digitalWrite(5,HIGH);
  
  
  Serial.begin(115200);

#if 0
  /*
  ptr = (char *)&unit_id;
  *ptr = EEPROM.read(0);
  ptr++;
  *ptr = EEPROM.read(1);
*/
  Serial.print("Unit ID: ");
  Serial.println(unit_id);

  
  Serial.print("Current Time: ");
   printTime();
   Serial.println(" ");
  Serial.println("Do you wish to set or init clock?");
  Serial.println("Press s to set or i to init (10 seconds to select)");

  for( x=0;x<100;x++){
    val = Serial.read();

    if(val=='i'){
      Serial.println("init clock");
             // Initialize a new chip by turning off write protection and clearing the
        // clock halt flag. These methods needn't always be called. See the DS1302
        // datasheet for details.
        rtc.writeProtect(false);
        rtc.halt(false);
      
        // Make a new time object to set the date and time.
        // Sunday, September 22, 2013 at 01:38:50.
        Time t(2020, 1, 1, 12, 00, 00, Time::kThursday);
      
        // Set the time and date on the chip.
        rtc.time(t);
      break;
    }
    if(val=='s'){
      //Serial.println("set clock");
      set_clock();
      break;
    }
/*
    if(val='u'){
      Serial.println("init unit ID");
      set_unit_id();
    }
    */
    if(x%10==0){
      Serial.print('.');
    }
    
    delay(100);
  }


#endif


#if 1


    if(!SD.begin(5)){
        Serial.println("Card Mount Failed");
        while(1){
            
        }
       
    }
    else{
          Serial.println("Card Mounted");
    
          
          uint8_t cardType = SD.cardType();
      
          if(cardType == CARD_NONE){
              Serial.println("No SD card attached");
              
          }

          else{
      
              Serial.print("SD Card Type: ");
              if(cardType == CARD_MMC){
                  Serial.println("MMC");
              } else if(cardType == CARD_SD){
                  Serial.println("SDSC");
              } else if(cardType == CARD_SDHC){
                  Serial.println("SDHC");
              } else {
                  Serial.println("UNKNOWN");
              }
          
              uint64_t cardSize = SD.cardSize() / (1024 * 1024);
              Serial.printf("SD Card Size: %lluMB\n", cardSize);
          
              String my_mac = "0001"; //WiFi.macAddress();
              char mac_str[16];
              my_mac.toCharArray(mac_str, 16);
  
              sprintf(timestamp, "Unit ID: %s Started at %d-%d-%d %d:%02d:%02d\n",mac_str, sys_time.yr,sys_time.mon,sys_time.date,sys_time.hr, sys_time.min, sys_time.sec);
    
        
              Serial.println(timestamp);
    
            
              appendFile(SD, "/mac_log.csv", timestamp);
             
            }
          }

#endif


#if 0

  SPI.begin();

  Serial.println("SPI Test");
  while(1){
    digitalWrite(5,LOW);
    SPI.transfer(0x22);
    digitalWrite(5,HIGH);
    delay(1000);
  }

#endif



    ble.begin("DIT BLE NODE");
    ble.SetCallBack(bleMACCallBack);
 
}

// Loop and print the time every second.
void loop() {
  Serial.print("\r\n");
  printTime();
  delay(1000);

  if((sys_time.sec % 10)==0){
    Serial.print(scan_result_num);    
  }
  
}

void set_unit_id(void){
 int id = 0;
  char mybuffer[50];
  char *ptr;
  
  Serial.print ("Enter the Unit ID:\r\n");
  Serial.print ("01234\r\n");
  readline (mybuffer, 5);
  sscanf (mybuffer, "%5u",&id);

  ptr = (char *)&id;
  
  //EEPROM.write(*ptr,0);
  ptr++;
 // EEPROM.write(*ptr,1);


  //EEPROM.writeShort(0, id);
  Serial.print ("Unit ID: ");
  Serial.print (id);
  Serial.print ("\r\n");
  
}

// get user input, set clock
void set_clock (void)
{
  int h = 0, m = 0, s = 0;
  int mn = 0, dy = 0, yr = 0;
  char mybuffer[50];
  
  Serial.print ("Enter the time:\r\n");
  Serial.print ("C\r\n");
  readline (mybuffer, 20);
  sscanf (mybuffer, "%2u %2u %2u %2u %2u %4u", &h, &m, &s, &dy, &mn, &yr);

  if (h + m + s + mn + dy + yr) {
    //RTC.writeTime (h, m, s, mn, dy, yr); // "write time" or whatever the function is that sets the RTC
    Time t(yr, mn, dy, h, m, s, Time::kThursday);//
      
        // Set the time and date on the chip.
    rtc.time(t);
    Serial.print ("\r\nClock is set!\r\n");
    printTime();

  } else {
    Serial.print ("\r\nClock has not been changed\r\n");
  }
}

uint16_t readline (char *buffer, uint16_t limit)
{
  char c;
  uint16_t ptr = 0;

  while (1) {

    if (Serial.available()) {

      c = Serial.read();

      if (c == 0x0D) { // cr == end of line
        *(buffer + ptr) = 0; // mark end of line
        break; // return char count
      }

      if (c == 0x08) { // backspace

        if (ptr) { // if not at the beginning

          *(buffer + --ptr) = 0;
          Serial.print ( (char) 0x08); // erase backspaced char
          Serial.print ( (char) 0x20);
          Serial.print ( (char) 0x08);

        } else {
          Serial.print ( (char) 0x07); // ascii bel (beep) if terminal supports it
        }

      } else { // not a backspace, handle it

        if (ptr < (limit - 1)) { // if not at the end of the line

          Serial.print ( (char) c); // echo char
          *(buffer + ptr++) = c; // put char into buffer

        } else { // at end of line, can't add any more
          Serial.print ( (char) 0x07); // ascii bel (beep) if terminal supports it
        }

      }

    }

  }
}


void bleMACCallBack(unsigned char *addr, int rssi, unsigned char *adv){
     char msg[512];
     char fname[100];
     char mac[64];
     char info[64];
     char timestamp[30];
     char toppic[18];
     int i;
     unsigned long period;

     //rtc.refresh();
     //Time t = rtc.time();   
    
 
    info[0]=0;
              
    #if 1
      if(adv[8]==9){
        //human readable, just copy over as is!
        for(i=0;i<adv[7]-1;i++){
          info[i]=adv[9+i];
        }
        info[i]=0;
      }
      else{
        btoh(info,&adv[7],15); 
      }

    #endif
    
   // info[i]=0;
#if 1
    memcpy(scan_results_array[scan_result_num].mac, addr, 6);
    scan_results_array[scan_result_num].rssi = rssi;
    //sys_time.GetData(scan_results_array[0][0].timedata);

    scan_result_num++;
#endif     
  
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    //Serial.print(mac);

#if 0
 

    String mac_str(mac);
    mac_str.toLowerCase();
    String sha_str = SHA256(mac_str); 

    // Length (with one extra character for the null terminator)
    int str_len = sha_str.length() + 1; 
    char sha_array[str_len];
    sha_str.toCharArray(sha_array, str_len);


    //snprintf(buf, sizeof(buf), "%02d:%02d:%02d %02d-%02d-%04d", t.hr, t.min, t.sec, t.date, t.mon,t.yr );

    sprintf(msg, "%d-%02d-%02d %02d:%02d:%02d, %s, %d, %s\r\n",sys_time.yr,sys_time.mon,sys_time.date, sys_time.hr, sys_time.min, sys_time.sec,  sha_array, rssi, info);
     
    sprintf(fname, "/mac_log_%d-%02d-%02d.csv",sys_time.yr,sys_time.mon,sys_time.date);
 
 
      //just record on SD Card
      //printf("\r\n%s \r\n", msg );
      #if 1
      appendFile(SD, fname, msg);
      #else
      
      appendFile(SD, "/mac_log.csv", msg);
      #endif
#endif

    
}


void sha256_transform(SHA256_CTX *ctx, const uint8_t data[]) {
  uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

  for (i = 0, j = 0; i < 16; ++i, j += 4)
    m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16) | ((uint32_t)data[j + 2] << 8) | ((uint32_t)data[j + 3]);
  for ( ; i < 64; ++i)
    m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

  a = ctx->state[0];
  b = ctx->state[1];
  c = ctx->state[2];
  d = ctx->state[3];
  e = ctx->state[4];
  f = ctx->state[5];
  g = ctx->state[6];
  h = ctx->state[7];

  for (i = 0; i < 64; ++i) {
    t1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
    t2 = EP0(a) + MAJ(a,b,c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  ctx->state[0] += a;
  ctx->state[1] += b;
  ctx->state[2] += c;
  ctx->state[3] += d;
  ctx->state[4] += e;
  ctx->state[5] += f;
  ctx->state[6] += g;
  ctx->state[7] += h;
}

void sha256_init(SHA256_CTX *ctx)
{
  ctx->datalen = 0;
  ctx->bitlen = 0;
  ctx->state[0] = 0x6a09e667;
  ctx->state[1] = 0xbb67ae85;
  ctx->state[2] = 0x3c6ef372;
  ctx->state[3] = 0xa54ff53a;
  ctx->state[4] = 0x510e527f;
  ctx->state[5] = 0x9b05688c;
  ctx->state[6] = 0x1f83d9ab;
  ctx->state[7] = 0x5be0cd19;
}

void sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len) {
  uint32_t i;

  for (i = 0; i < len; ++i) {
    ctx->data[ctx->datalen] = data[i];
    ctx->datalen++;
    if (ctx->datalen == 64) {
      sha256_transform(ctx, ctx->data);
      ctx->bitlen += 512;
      ctx->datalen = 0;
    }
  }
}

void sha256_final(SHA256_CTX *ctx, uint8_t hash[]) {
  uint32_t i;

  i = ctx->datalen;

  // Pad whatever data is left in the buffer.
  if (ctx->datalen < 56) {
    ctx->data[i++] = 0x80;
    while (i < 56)
      ctx->data[i++] = 0x00;
  }
  else {
    ctx->data[i++] = 0x80;
    while (i < 64)
      ctx->data[i++] = 0x00;
    sha256_transform(ctx, ctx->data);
    memset(ctx->data, 0, 56);
  }

  // Append to the padding the total message's length in bits and transform.
  ctx->bitlen += ctx->datalen * 8;
  ctx->data[63] = ctx->bitlen;
  ctx->data[62] = ctx->bitlen >> 8;
  ctx->data[61] = ctx->bitlen >> 16;
  ctx->data[60] = ctx->bitlen >> 24;
  ctx->data[59] = ctx->bitlen >> 32;
  ctx->data[58] = ctx->bitlen >> 40;
  ctx->data[57] = ctx->bitlen >> 48;
  ctx->data[56] = ctx->bitlen >> 56;
  sha256_transform(ctx, ctx->data);

  // Since this implementation uses little endian byte ordering and SHA uses big endian,
  // reverse all the bytes when copying the final state to the output hash.
  for (i = 0; i < 4; ++i) {
    hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
    hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
  }
}

char *btoh(char *dest, uint8_t *src, int len) {
  char *d = dest;
  while( len-- ) sprintf(d, "%02x", (unsigned char)*src++), d += 2;
  return dest;
}

String SHA256(String data) 
{
  uint8_t data_buffer[data.length()];
  
  for(int i=0; i<data.length(); i++)
  {
    data_buffer[i] = (uint8_t)data.charAt(i);
  }
  
  SHA256_CTX ctx;
  ctx.datalen = 0;
  ctx.bitlen = 512;
  
  sha256_init(&ctx);
  sha256_update(&ctx, data_buffer, data.length());
  sha256_final(&ctx, hash);
  return(btoh(hex, hash, 32));
}


/******************************************************************** SD Card Functinos ***************************************/

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.print (file.name());
          //  time_t t= file.getLastWrite();
          //  struct tm * tmstruct = localtime(&t);
         //   Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.print(file.size());
          //  time_t t= file.getLastWrite();
        //    struct tm * tmstruct = localtime(&t);
         //   Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
        }
        file = root.openNextFile();
    }
}

void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    //Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.print("*");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}



/*******************************************************************************************************************************/

  
