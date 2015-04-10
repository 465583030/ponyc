#include <platform.h>
#include <stdio.h>
#include "../asio/asio.h"

#ifndef PLATFORM_IS_WINDOWS
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#endif

PONY_EXTERN_C_BEGIN

FILE* os_stdout()
{
  return stdout;
}

FILE* os_stderr()
{
  return stderr;
}

#ifdef PLATFORM_IS_WINDOWS

static HANDLE stdinHandle;
static bool is_stdin_tty = false;

#else
static struct termios orig_termios;

typedef enum
{
  FD_TYPE_NONE = 0,
  FD_TYPE_DEVICE,
  FD_TYPE_TTY,
  FD_TYPE_PIPE,
  FD_TYPE_FILE
} fd_type_t;

static fd_type_t fd_type(int fd)
{
  fd_type_t type = FD_TYPE_NONE;
  struct stat st;

  if(fstat(fd, &st) != -1)
  {
    switch(st.st_mode & S_IFMT)
    {
      case S_IFIFO:
      case S_IFSOCK:
        // A pipe or a socket.
        type = FD_TYPE_PIPE;
        break;

      case S_IFCHR:
        // A tty or a character device.
        if(isatty(fd))
          type = FD_TYPE_TTY;
        else
          type = FD_TYPE_DEVICE;
        break;

      case S_IFREG:
        // A redirected file.
        type = FD_TYPE_FILE;
        break;

      default:
        // A directory or a block device.
        break;
    }
  }

  return type;
}

static void stdin_tty_restore()
{
  tcsetattr(0, TCSAFLUSH, &orig_termios);
}

static void fd_tty(int fd)
{
  // Turn off canonical mode if we're reading from a tty.
  if(tcgetattr(fd, &orig_termios) != -1)
  {
    struct termios io = orig_termios;

    io.c_iflag &= ~(BRKINT | INPCK | ISTRIP | IXON);
    io.c_cflag |= (CS8);
    io.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    io.c_cc[VMIN] = 1;
    io.c_cc[VTIME] = 0;

    tcsetattr(fd, TCSAFLUSH, &io);

    if(fd == 0)
      atexit(stdin_tty_restore);
  }
}

static void fd_nonblocking(int fd)
{
  // Set to non-blocking.
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
#endif

void os_stdout_setup()
{
#ifdef PLATFORM_IS_WINDOWS
  // TODO
#else
  fd_type_t type = fd_type(STDOUT_FILENO);

  // Use unbuffered output if we're writing to a tty.
  if(type == FD_TYPE_TTY)
    setvbuf(stdout, NULL, _IONBF, 0);
#endif
}

bool os_stdin_setup()
{
  // Return true if reading stdin should be event based.
#ifdef PLATFORM_IS_WINDOWS
  stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
  DWORD type = GetFileType(stdinHandle);

  if(type == FILE_TYPE_CHAR)
  {
    // TTY
    DWORD mode;
    GetConsoleMode(stdinHandle, &mode);
    SetConsoleMode(stdinHandle, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
    is_stdin_tty = true;
  }

  // Always use events
  return true;
#else
  int fd = STDIN_FILENO;
  fd_type_t type = fd_type(fd);

  switch(type)
  {
    case FD_TYPE_TTY:
      fd_nonblocking(fd);
      fd_tty(fd);
      return true;

    case FD_TYPE_PIPE:
    case FD_TYPE_DEVICE:
      fd_nonblocking(fd);
      return true;

    default: {}
  }

  // For a file, directory, or block device, do nothing.
  return false;
#endif
}

uint64_t os_stdin_read(void* buffer, uint64_t space)
{
#ifdef PLATFORM_IS_WINDOWS
  uint64_t len = 0;

  if(is_stdin_tty)
  {
    // TTY. Read from console input.

    /* Note:
     * We can only call ReadConsoleInput() once as the second time it might
     * block. We only get useful data from key downs, which are less than half
     * the events we get. However, due to copy and paste we may get many key
     * down events in a row. Furthermore, <Enter> key downs have to expand to
     * 2 characters in the buffer (an 0xD and an 0xA). This means we can only
     * read (space / 2) events and guarantee that the data they produce will
     * fit in the provided buffer. In general this means the buffer will only
     * be a quarter full, even if there are more events waiting.
     * AMc, 10/4/15
     */
    INPUT_RECORD record[64];
    DWORD readCount = 32;
    char* buf = (char*)buffer;
    uint64_t max_events = space / 2;

    if(max_events < readCount)
      // Limit events read to buffer size, in case they're all key down events
      readCount = (DWORD)max_events;

    BOOL r = ReadConsoleInput(stdinHandle, record, readCount, &readCount);

    if(r == TRUE)
    {
      for(DWORD i = 0; i < readCount; i++)
      {
        INPUT_RECORD* rec = &record[i];

        if(rec->EventType == KEY_EVENT &&
          rec->Event.KeyEvent.bKeyDown == TRUE &&
          rec->Event.KeyEvent.uChar.AsciiChar != 0)
        {
          // This is a key down event
          buf[len++] = rec->Event.KeyEvent.uChar.AsciiChar;

          if(rec->Event.KeyEvent.uChar.AsciiChar == 0xD)
            buf[len++] = 0xA;
        }
      }
    }

    if(len == 0)
      // We have no data, but 0 means EOF, so we return -1 which is try again
      len = -1;
  }
  else
  {
    // Not TTY, ie file or pipe. Just use ReadFile.
    DWORD buf_size = (space <= 0xFFFFFFFF) ? (DWORD)space : 0xFFFFFFFF;
    DWORD actual_len;

    BOOL r = ReadFile(stdinHandle, buffer, buf_size, &actual_len, NULL);

    len = actual_len;

    if(r == FALSE && GetLastError() == ERROR_BROKEN_PIPE)  // Broken pipe
      len = 0;
  }

  // Start listening to stdin notifications again
  iocp_resume_stdin();
  return len;
#else
  return read(0, buffer, space);
#endif
}

PONY_EXTERN_C_END
