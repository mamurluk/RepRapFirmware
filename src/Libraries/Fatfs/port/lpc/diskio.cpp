/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2007        */
/*-----------------------------------------------------------------------*/
/* This is a stub disk I/O module that acts as front end of the existing */
/* disk I/O modules and attach it to FatFs module with common interface. */
/*-----------------------------------------------------------------------*/
//SD :: Modified to work with RRF
//SD :: Updated for RTOS




#include "diskio.h"
#include <stdio.h>
#include <string.h>
#include "SDCard.h"


#include "RepRapFirmware.h"
#include "RepRap.h"
#include "Tasks.h"


extern SDCard *_ffs[_DRIVES]; //Defined in CoreLPC

static unsigned int highestSdRetriesDone = 0;

unsigned int DiskioGetAndClearMaxRetryCount() noexcept
{
    const unsigned int ret = highestSdRetriesDone;
    highestSdRetriesDone = 0;
    return ret;
}


/* drv - Physical drive nmuber (0..) */
DSTATUS disk_initialize (BYTE drv) noexcept
{
    MutexLocker lock(Tasks::GetSpiMutex());
	return (DSTATUS)_ffs[drv]->disk_initialize();
}

/* drv - Physical drive nmuber (0..) */
DSTATUS disk_status (BYTE drv) noexcept
{
    MutexLocker lock(Tasks::GetSpiMutex());
	return (DSTATUS)_ffs[drv]->disk_status();
}

/* drv - Physical drive nmuber (0..) */
/* buff - Data buffer to store read data */
/* sector - Sector address (LBA) */
/* count - Number of sectors to read (1..255) */
DRESULT disk_read (BYTE drv, BYTE *buff, DWORD sector, BYTE count) noexcept
{
    if (reprap.Debug(moduleStorage))
    {
        debugPrintf("Read %u %u %lu\n", drv, count, sector);
    }

    MutexLocker lock(Tasks::GetSpiMutex());
    
    unsigned int retryNumber = 0;
    uint32_t retryDelay = SdCardRetryDelay;
    while(_ffs[drv]->disk_read(buff, sector, count) != RES_OK){
        lock.Release();
        ++retryNumber;
        if (retryNumber == MaxSdCardTries)
        {
            return RES_ERROR;
        }
        delay(retryDelay);
        retryDelay *= 2;
        lock.ReAcquire();
    }
    
    if (retryNumber > highestSdRetriesDone)
    {
        highestSdRetriesDone = retryNumber;
    }

        
    return RES_OK;
}

#if _READONLY == 0
/* drv - Physical drive nmuber (0..) */
/* buff - Data to be written */
/* sector - Sector address (LBA) */
/* count - Number of sectors to write (1..255) */

DRESULT disk_write (BYTE drv, const BYTE *buff, DWORD sector, BYTE count) noexcept
{
    MutexLocker lock(Tasks::GetSpiMutex());
    
    if (reprap.Debug(moduleStorage))
    {
        debugPrintf("Write %u %u %lu\n", drv, count, sector);
    }
    
    /* Write the data */
    unsigned int retryNumber = 0;
    uint32_t retryDelay = SdCardRetryDelay;
    while (_ffs[drv]->disk_write(buff, sector, count) != RES_OK)
    {
        lock.Release();
        ++retryNumber;
        if (retryNumber == MaxSdCardTries)
        {
            return RES_ERROR;
        }
        delay(retryDelay);
        retryDelay *= 2;
        lock.ReAcquire();
    }
    
    if (retryNumber > highestSdRetriesDone)
    {
        highestSdRetriesDone = retryNumber;
    }
    
    return RES_OK;
}
#endif /* _READONLY */

/* drv - Physical drive nmuber (0..) */
/* ctrl - Control code */
/* buff - Buffer to send/receive control data */

DRESULT disk_ioctl (BYTE drv, BYTE ctrl, void *buff) noexcept
{
    MutexLocker lock(Tasks::GetSpiMutex());
    return _ffs[drv]->disk_ioctl(ctrl, buff);
}


