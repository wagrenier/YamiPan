// sdk/fileio.cpp -- the EE's file calls on yamipan::io::HostFiles.
#include "fileio.h"

#include "yamipan/io/host_files.h"

using yamipan::io::HostFiles;

int sceOpen(const char *filename, int flag, ...)
{
    const bool write = (flag & SCE_WRONLY) != 0 || (flag & SCE_RDWR) == SCE_RDWR;
    return HostFiles::instance().open(filename, write, (flag & SCE_CREAT) != 0, (flag & SCE_TRUNC) != 0);
}

int sceClose(int fd)
{
    return HostFiles::instance().close(fd);
}

int sceRead(int fd, void *buf, int size)
{
    return HostFiles::instance().read(fd, buf, static_cast<std::size_t>(size));
}

int sceWrite(int fd, const void *buf, int size)
{
    return HostFiles::instance().write(fd, buf, static_cast<std::size_t>(size));
}

int sceLseek(int fd, int offset, int whence)
{
    const auto seek = whence == SCE_SEEK_CUR   ? HostFiles::Seek::Current
                      : whence == SCE_SEEK_END ? HostFiles::Seek::End
                                               : HostFiles::Seek::Set;
    return HostFiles::instance().seek(fd, offset, seek);
}

int sceIoctl(int, int, void *)
{
    return -1; // no host device answers one
}
