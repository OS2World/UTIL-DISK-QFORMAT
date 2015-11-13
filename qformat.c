/* qformat.c  version 1.0  July 26, 1992
 * Quick format of diskette drives - 3.5" and 5.25"
 * Compiled with IBM C Set/2 compiler
 * NOTE: This program will not format 2.88MB diskettes
 * Program runs in OS/2 Win or OS/2 FS modes
 * usage: qformat <drive>:
 *
 * Author - John K. Gotwals     (317)494-2564 phone
 *                              (317)496-1212 fax
 *                              gotwals@vm.cc.purdue.edu
 *                              jgotwals (bix)
 *
 * Copyright John K. Gotwals 1992
*/

#define INCL_BASE

#include <os2.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_SECTORS 18
#define SECTOR_SIZE 512
#define DIR_ENTRY_SIZE 32

typedef _Packed struct
    {
    USHORT bytesSector;
    UCHAR  sectorsCluster;
    USHORT reservedSectors;
    UCHAR  numFats;
    USHORT rootDirEntries;
    USHORT totallSectors;
    UCHAR  mediaDescriptor;
    USHORT sectorsFat;
    USHORT sectorsTrack;
    USHORT numHeads;
    ULONG  hiddenSectors;
    ULONG  largeTotalSectors;
    CHAR   reserved[6];
    USHORT numCylinders;
    UCHAR  type;
    USHORT attributes;
    } diskDevice;

typedef _Packed struct
    {
    UCHAR  command;
    USHORT head;
    USHORT cylinder;
    USHORT firstSector;
    USHORT numSectors;
    CHAR   table[MAX_SECTORS * 4];
    } diskTrack;

typedef struct
    {
    UCHAR command;
    UCHAR drive;
    } parmList;


void readTrack(char *buffer, int cyl, int head, int firstSector,
    int sectors);
void writeTrack(char *buffer, int cyl, int head, int firstSector,
    int sectors);
void format(int fatInit);
void mediaError(int errorNum);
void error(char *msg, int error);

diskDevice device;
ULONG deviceLength = sizeof(device);

diskTrack track;
ULONG trackLength  = sizeof(track);

parmList parm;
ULONG parmLength   = sizeof(parm);

char DriveName[3] = "x:";
HFILE FileHandle;
ULONG Action;

char trackBuffer[MAX_SECTORS * SECTOR_SIZE];

int main(int argc, char **argv)
    {
    int i;
    APIRET rc;      /* return code */
/*
 * Check command line for drive letter
*/
    if (argc > 1)
        DriveName[0] = tolower(*argv[1]);
    else
        {
        printf("usage: qformat drive:\n");
        return 0;
        }
/*
 * Do not procede if improper device!
*/
    if ((DriveName[0] != 'a') && (DriveName[0] != 'b'))
        error("Only a: or b: can be formated!", 1);
/*
 * Get access to the device
*/
    rc = DosOpen(
        DriveName,
        &FileHandle,
        &Action,
        0L,                 /* file size     */
        FILE_NORMAL,        /* file attribue */
        OPEN_ACTION_OPEN_IF_EXISTS,
        OPEN_FLAGS_DASD             |
        OPEN_FLAGS_WRITE_THROUGH    |
        OPEN_FLAGS_NOINHERIT        |
        OPEN_SHARE_DENYREADWRITE    |
        OPEN_ACCESS_READWRITE,
        0L);                /* no extended attributes */
    if (rc != NO_ERROR)
        error("DosOpen", rc);
/*
 * Lock the drive
*/
    rc = DosDevIOCtl(
        FileHandle,
        0x08L,                  /* device catagory          */
        0x00L,                  /* function code            */
        0,                      /* parameter packet address */
        0,                      /* parm list length         */
        0,                      /* parm list length address */
        0,                      /* address of extended BPB  */
        0,                      /* length of extended BPB   */
        0);                     /* address of ext BPB len   */
    if (rc != NO_ERROR)
        error("DosDevIOCtl lock", rc);
/*
 * Get the device parameters
 * (BPB = BIOS Parameter Block)
*/
    parm.command = 1;           /* return the BPB for the media
                                   currently in the drive   */
    rc = DosDevIOCtl(
        FileHandle,
        0x08L,                  /* device catagory          */
        0x63L,                  /* function code            */
        &parm,                  /* parameter packet address */
        parmLength,             /* parm list length         */
        &parmLength,
        &device,                /* address of extended BPB  */
        deviceLength,           /* length of extended BPB   */
        &deviceLength);

    if (rc != NO_ERROR)
        error("DosDevIOCtl get BPB", rc);
/*
 * Build the parameter packet
 * (see page 18-154 of Physical Device Driver Reference)
*/
    memset(&track, 0, sizeof(track));
    for (i = 0; i < MAX_SECTORS; i++)
        {
        track.table[4 * i] = i + 1;
        track.table[4 * i + 3] = 2;     /* 512 bytes per sector */
        }
/*
 * get permission before formatting
*/
    printf("\nPress enter to format drive %s\n", DriveName);
    while (getchar() != '\n');
/*
 * Determine type of diskette and carry out format
*/
    memset(trackBuffer, 0, sizeof(trackBuffer));
    switch (device.mediaDescriptor)
        {
        case 0xf9:
            if (device.type == 2  ||  device.type == 7)
                format(0xfffff9);       /* 720KB */
            else if (device.type == 1)
                format(0xfffff9);       /* 1.2MB */
            else
                mediaError(1);
            break;
        case 0xfd:
            if (device.type == 1  ||  device.type == 0)
                format(0xfffffd);       /* 360KB */
            else
                mediaError(2);
            break;
        case 0xf0:
            if (device.type == 7)
                format(0xfffff0);       /* 1.44MB */
            else
                mediaError(3);
            break;
        default:
            mediaError(4);
            break;
        }
/*
 * Redetermine the media
*/
    rc = DosDevIOCtl(
        FileHandle,
        0x08L,                  /* device catagory          */
        0x02L,                  /* function code            */
        0,                      /* parameter packet address */
        0,                      /* parm list length         */
        0,                      /* parm list length address */
        0,                      /* address of extended BPB  */
        0,                      /* length of extended BPB   */
        0);                     /* address of ext BPB len   */
    if (rc != NO_ERROR)
        error("DosDevIOCtl redetermine", rc);
/*
 * Unlock the media
*/
    rc = DosDevIOCtl(
        FileHandle,
        0x08L,                  /* device catagory          */
        0x01L,                  /* function code            */
        0,                      /* parameter packet address */
        0,                      /* parm list length         */
        0,                      /* parm list length address */
        0,                      /* address of extended BPB  */
        0,                      /* length of extended BPB   */
        0);                     /* address of ext BPB len   */
    if (rc != NO_ERROR)
        error("DosDevIOCtl unlock", rc);
/*
 * Close the device
*/
    rc = DosClose(FileHandle);
    if (rc != NO_ERROR)
        error("DosClose", rc);
    return 0;
    }

/*
 * format() zeros FATs and root directory and
 * initializes both FATs
*/
void format(int fatInit)
    {
    int zeroSectorsTrack0;
    int zeroSectorsTrack1;
    int fatSectors;
    int dirSectors;
    int *ptrackBuffer = (int *)trackBuffer;

    fatSectors = device.numFats * device.sectorsFat;
    dirSectors = (device.rootDirEntries * DIR_ENTRY_SIZE) / SECTOR_SIZE;
    zeroSectorsTrack0 = device.sectorsTrack - 1;
    zeroSectorsTrack1 = fatSectors + dirSectors - zeroSectorsTrack0;
    writeTrack(trackBuffer, 0, 0, 1, zeroSectorsTrack0);
    writeTrack(trackBuffer, 0, 1, 0, zeroSectorsTrack1);
    *ptrackBuffer = fatInit;
    writeTrack(trackBuffer, 0, 0, 1, 1);  /* 1st FAT */
    writeTrack(trackBuffer, 0, 0, 1 + device.sectorsFat, 1);
    return;
    }

#ifdef TESTING
/*
 * readTrack() can read up to 1 track (for testing purposes)
*/
void readTrack(char *buffer, int cyl, int head, int firstSector,
    int sectors)
    {
    APIRET rc;
    ULONG bufferLength = SECTOR_SIZE * sectors;
 
    track.command = 1;  /* track layout starts with sector 1 and */
                        /* contains only consecutive sectors     */
    track.head = head;
    track.cylinder = cyl;
    track.firstSector = firstSector;
    track.numSectors = sectors;
    rc = DosDevIOCtl(
        FileHandle,
        0x08L,          /* device catagory          */
        0x64L,          /* function code            */
        &track,         /* parameter packet address */
        trackLength,    /* parameter packet length  */
        &trackLength,
        buffer,         /* data packet address      */
        bufferLength,
        &bufferLength);
    if (rc != NO_ERROR)
        error("DosDevIOCtl during track read", rc);
    return;
    }

#endif

/*
 * writeTrack() can write up to 1 track
*/
void writeTrack(char *buffer, int cyl, int head, int firstSector,
    int sectors)
    {
    APIRET rc;
    ULONG bufferLength = SECTOR_SIZE * sectors;
 
    track.command = 1;  /* track layout starts with sector 1 and */
                        /* contains only consecutive sectors     */
    track.head = head;
    track.cylinder = cyl;
    track.firstSector = firstSector;
    track.numSectors = sectors;
    rc = DosDevIOCtl(
        FileHandle,
        0x08L,          /* device catagory          */
        0x44L,          /* function code            */
        &track,         /* parameter packet address */
        trackLength,    /* parameter packet length  */
        &trackLength,
        buffer,         /* data packet address      */
        bufferLength,
        &bufferLength);
    if (rc != NO_ERROR)
        error("DosDevIOCtl during track write", rc);
    return;
    }

/*
 * mediaError() - Display error message and info about diskette.
 *                Then exit() program.
*/
void mediaError(int errorNum)
    {
    fprintf(stderr, "Error %d: Can not quick format diskette.\n",
        errorNum);
    fprintf(stderr, "drive type = %d  media descriptor = %d\n",
        (int)device.type, (int)device.mediaDescriptor);
    exit(errorNum);
    }

/*
 * error() - Display message, error number and exit program
*/
void error(char *msg, int error)
    {
    fprintf(stderr, "Error %d - %s\n", error, msg);
    exit(error);
    }
