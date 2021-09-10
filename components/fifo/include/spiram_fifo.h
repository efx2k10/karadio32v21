#ifndef _SPIRAM_FIFO_H_
#define _SPIRAM_FIFO_H_

int  spiRamFifoInit();
void  spiRamFifoRead(char *buff, unsigned len);
void  spiRamFifoWrite(const char *buff, unsigned len);
unsigned  spiRamFifoFill();
unsigned  spiRamFifoFree();
long  spiRamGetOverrunCt();
long  spiRamGetUnderrunCt();
void setSPIRAMSIZE(unsigned size);
unsigned getSPIRAMSIZE();

void spiRamFifoReset();
void spiRamFifoDestroy();
unsigned spiRamFifoLen();

#endif
