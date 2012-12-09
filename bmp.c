#include <linux/i2c-dev.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "TSL2561.h"
#include <math.h>

const unsigned char OSS = 0;  // Oversampling Setting

int16_t ac1, ac2, ac3, b1, b2, mb, mc, md;
uint16_t ac4, ac5, ac6;
void calibrate(int dev);
int16_t read16(int dev, int reg);
uint16_t uread16(int dev, int reg);
int read24(int dev, int reg);
void convertut(unsigned int temp);
void convertup(unsigned int temp);
long rawpressure(int dev);
long rawtemp(int dev);
unsigned int temp, pressure;

long b5;

int main(){

  int dev;
  char *filename = "/dev/i2c-0";
  int addr = 0x77;

  if ((dev = open(filename, O_RDWR)) < 0) {
    perror("Failed to open the i2c bus");
    exit(1);
  }
  if (ioctl(dev, I2C_SLAVE, addr) < 0) {
    perror("Failed to acquire bus access and/or talk to slave");
    exit(1);
  }
  if( ioctl( dev, I2C_PEC, 1) < 0) {
    perror("Failed to enable PEC\n");
    exit(1);
  }

  calibrate(dev);

  long ut = rawtemp(dev);
  long up = rawpressure(dev);

  //  ut = 27898;
  //up = 23843;
  printf("Raw Temp: %li\n", ut);  
  printf("Raw Pressure: %li\n", up);

  convertut(ut);
  convertup(up);
  printf("Temp: %f C\n", (double)temp*0.1);
  printf("Pressure: %f PA\n", (double)pressure);
 
  float alt = 44330 * (1-((powf(((double)pressure/101325),0.190263096))));
  printf("Altitude: %f\n", alt);
  return 0;

}

long rawpressure(int dev){
  char buf[10];
  buf[0] = 0xf4;
  buf[1] = (0x34+(OSS<<6));
  
  if (write(dev, buf, 2) != 2) {
    perror("write UP data");
  }
  
  usleep(2+(3<<OSS));

  long pressure = read24(dev, 0x34+(OSS<<6));
  return pressure;
}

long rawtemp(int dev){
  char buf[10];
  buf[0] = 0xf4;
  buf[1] = 0x2e;
  
  if (write(dev, buf, 2) != 2) {
    perror("write UT data");
  }
  
  usleep(4.5);
  return (long)read16(dev, 0xf6);
}

void convertup(unsigned int up){


  long x1, x2, x3, b3, b6, p;
  unsigned long b4, b7;

  b6 = b5 - 4000;
  x1 = (b2 * (b6 * b6 >> 12)) >> 11;
  x2 = ac2 * b6 >> 11;
  x3 = x1 + x2;
  b3 = (((int32_t) ac1 * 4 + x3) + 2)/4;
  x1 = ac3 * b6 >> 13;
  x2 = (b1 * (b6 * b6 >> 12)) >> 16;
  x3 = ((x1 + x2) + 2) >> 2;
  b4 = (ac4 * (unsigned long) (x3 + 32768)) >> 15;
  b7 = ((unsigned long) up - b3) * (50000 >> OSS);
  p = b7 < 0x80000000 ? (b7 * 2) / b4 : (b7 / b4) * 2;
  x1 = (p >> 8) * (p >> 8);
  x1 = (x1 * 3038) >> 16;
  x2 = (-7357 * p) >> 16;

  pressure = p + ((x1 + x2 + 3791) >> 4);
}

void convertut(unsigned int ut){
  long x1,x2;
  x1 = ((long)ut - (long)ac6) * (long) ac5 >> 15;
  x2 = (float)(mc << 11 ) / (long int)(x1 + md);
  b5 = x1 + x2;
  temp = ( b5 + 8 ) >> 4;
}


int16_t read16(int dev, int reg){
  char buf[2];
 
  buf[0] = reg;
  if (write(dev, buf, 1) != 1) {
    perror("Read16");
  }
  
  if (read(dev, buf, 2) != 2) {
    perror("Read16");
  }

  return buf[0] << 8 | buf[1];
}
uint16_t uread16(int dev, int reg){
  char buf[2];
 
  buf[0] = reg;
  if (write(dev, buf, 1) != 1) {
    perror("Read16");
  }
  
  if (read(dev, buf, 2) != 2) {
    perror("Read16");
  }

  return buf[0] << 8 | buf[1];
}

int32_t read24(int dev, int reg){
  char buf[3];
 
  buf[0] = reg;
  if (write(dev, buf, 1) != 1) {
    perror("Read24");
  }
  if (read(dev, buf, 3) != 3) {
    perror("Read24");
  }

  int pres = buf[0];
  pres <<= 8; 
  pres |= buf[1];
  pres <<= 8; 
  pres |= buf[2];
  pres >>= (8-OSS);
  

  return pres;
}

void calibrate(int dev){
  ac1 = read16(dev, 0xaa); 
  ac2 = read16(dev, 0xac); 
  ac3 = read16(dev, 0xae); 
  ac4 = uread16(dev, 0xb0); 
  ac5 = uread16(dev, 0xb2); 
  ac6 = uread16(dev, 0xb4); 

  b1 = read16(dev, 0xb6); 
  b2 = read16(dev, 0xb8); 

  mb = read16(dev, 0xba); 
  mc = read16(dev, 0xbc); 
  md = read16(dev, 0xbe); 
  /*  
  ac1 = 408;
  ac2 = -72; 
  ac3 = -14383;
  ac4 = 32741;
  ac5 = 32757;
  ac6 = 23153;

  b1 = 6190;
  b2 = 4;

  mb = -32768;
  mc = -8711;
  md = 2868;
  
  printf("ac1 = %d\n",ac1);
  printf("ac2 = %d\n",ac2); 
  printf("ac3 = %d\n",ac3);
  printf("ac4 = %d\n",ac4); 
  printf("ac5 = %d\n",ac5); 
  printf("ac6 = %d\n",ac6); 

  printf("b1 = %d\n",b1); 
  printf("b2 = %d\n",b2); 

  printf("mb = %d\n",mb); 
  printf("mc = %d\n",mc); 
  printf("md = %d\n",md); 
*/
}
