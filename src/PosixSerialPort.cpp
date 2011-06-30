#include "PosixSerialPort.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <errno.h>

#include <string>

#ifndef B460800
#define B460800 460800
#endif
#ifndef B921600
#define B921600 921600
#endif

PosixSerialPort::PosixSerialPort(const std::string& name, bool isUsb) :
    SerialPort(name), _devfd(-1), _isUsb(isUsb), _timeout(0)
{
}

PosixSerialPort::~PosixSerialPort()
{
    if (_devfd >= 0)
        ::close(_devfd);
}

bool
PosixSerialPort::open(int baud,
                      int data,
                      SerialPort::Parity parity,
                      SerialPort::StopBit stop)
{
    struct termios options;
    speed_t speed;
    std::string dev("/dev/");

    dev += _name;
    _devfd = ::open(dev.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (_devfd == -1)
        return false;
   
    if (tcgetattr(_devfd, &options) == -1)
    {
        close();
        return false;
    }

    switch (baud)
    {
    case 9600:
        speed = B9600;
        break;
    case 19200:
        speed = B19200;
        break;
    case 38400:
        speed = B38400;
        break;
    case 57600:
        speed = B57600;
        break;
    case 115200:
        speed = B115200;
        break;
    case 230400:
        speed = B230400;
        break;
    case 460800:
        speed = B460800;
        break;
    case 921600:
        speed = B921600;
        break;
    default:
        close();
        return false;
    }

    if (cfsetispeed(&options, speed) || cfsetospeed(&options, speed))
    {
        close();
        return false;
    }
   
    options.c_cflag |= (CLOCAL | CREAD);

    switch (data)
    {
        case 8:
            options.c_cflag &= ~CSIZE;
            options.c_cflag |= CS8;
            break;
        case 7:
            options.c_cflag &= ~CSIZE;
            options.c_cflag |= CS7;
            break;
        default:
            close();
            return false;
    }
        
    switch (parity)
    {
        case SerialPort::ParityNone:
            options.c_cflag &= ~PARENB;
            options.c_cflag &= ~PARODD;
            options.c_iflag &= ~(INPCK | ISTRIP);
            break;
        case SerialPort::ParityOdd:
            options.c_cflag |= PARENB;
            options.c_cflag |= PARODD;
            options.c_iflag |= (INPCK | ISTRIP);
            break;
        case SerialPort::ParityEven:
            options.c_cflag |= PARENB;
            options.c_cflag &= ~PARODD;
            options.c_iflag |= (INPCK | ISTRIP);
            break;
        default:
            close();
            return false;
    }

    switch (stop)
    {
    case StopBitOne:
        options.c_cflag &= ~CSTOPB;
        break;
    case StopBitTwo:
        options.c_cflag |= CSTOPB;
        break;
    default:
        close();
        return false;
    }
    
    // No hardware flow control
    options.c_cflag &= ~CRTSCTS;

    // No software flow control
    options.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Raw input
    options.c_iflag &= ~(BRKINT | ICRNL);
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // Raw output
    options.c_oflag &= ~OPOST;

    // No wait time
    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 0;

    if (tcsetattr(_devfd, TCSANOW, &options))
    {
        close();
        return false;
    }
    
    return true;
}

void
PosixSerialPort::close()
{
    if (_devfd >= 0)
        ::close(_devfd);
    _devfd = -1;
}

int
PosixSerialPort::read(uint8_t* buffer, int len)
{
    fd_set fds;
    struct timeval tv;
    int numread = 0;
    int retval;

    if (_devfd == -1)
        return -1;

    while (numread < len)
    {
        FD_ZERO(&fds);
        FD_SET(_devfd, &fds);
       
        tv.tv_sec  = _timeout / 1000;
        tv.tv_usec = (_timeout % 1000) * 1000;
       
        retval = select(_devfd + 1, &fds, NULL, NULL, &tv);
	
        if (retval < 0)
        {
            return -1;
        }
        else if (retval == 0)
        {
            return numread;
        }
        else if (FD_ISSET(_devfd, &fds))
        {
            retval = ::read(_devfd, (uint8_t*) buffer + numread, len - numread);
            if (retval < 0)
                return -1;
            numread += retval;
        }
    }
    
    return numread;
}

int
PosixSerialPort::write(const uint8_t* buffer, int len)
{
    if (_devfd == -1)
        return -1;

    return ::write(_devfd, buffer, len);
}

int
PosixSerialPort::get()
{
    uint8_t byte;
    
    if (_devfd == -1)
        return -1;

    if (read(&byte, 1) != 1)
        return -1;
    
    return byte;
}

int
PosixSerialPort::put(int c)
{
    uint8_t byte;
    
    byte = c;
    return write(&byte, 1);
}

void
PosixSerialPort::flush()
{
    // There isn't a reliable way to flush on a file descriptor
    // so we just wait it out.  One millisecond is the USB poll
    // interval so that should cover it.
    usleep(1000);
}

bool
PosixSerialPort::timeout(int millisecs)
{
    _timeout = millisecs;
    return true;
}

