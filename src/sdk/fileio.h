/* fileio.h - PS2 EE file I/O declarations for the PC syntax pass. */
#pragma once
#include "scetypes.h"
#include "sifdev.h"

#define SCE_RDONLY   0x0001
#define SCE_WRONLY   0x0002
#define SCE_RDWR     0x0003
#define SCE_CREAT    0x0200
#define SCE_TRUNC    0x0400
#define SCE_APPEND   0x0100

#define SCE_SEEK_SET 0
#define SCE_SEEK_CUR 1
#define SCE_SEEK_END 2

int sceOpen(const char *filename, int flag, ...);
int sceClose(int fd);
int sceRead(int fd, void *buf, int size);
int sceWrite(int fd, const void *buf, int size);
int sceLseek(int fd, int offset, int whence);
/* PORT: no host device answers an ioctl; always -1. */
int sceIoctl(int fd, int req, void *arg);

