#include <platform.h>
#include <pony.h>
#include "lang.h"

typedef struct pony_stat_t
{
  pony_type_t* desc;
  void* path;

  uint32_t mode;
  uint32_t hard_links;
  uint32_t uid;
  uint32_t gid;
  uint64_t size;
  uint64_t access_time;
  uint64_t modified_time;
  uint64_t change_time;

  bool file;
  bool directory;
  bool pipe;
  bool symlink;
} pony_stat_t;

void os_stat(const char* path, pony_stat_t* p)
{
#if defined(PLATFORM_IS_WINDOWS)
  struct __stat64 st;

  if(_stat64(path, &st) != 0)
    pony_throw();

  p->file = (st.st_mode & _S_IFREG) == _S_IFREG;
  p->directory = (st.st_mode & _S_IFDIR) == _S_IFDIR;
  p->pipe = (st.st_mode & _S_IFIFO) == _S_IFIFO;
  p->symlink = false;
#elif defined(PLATFORM_IS_POSIX_BASED)
  struct stat st;

  if(lstat(path, &st) != 0)
    pony_throw();

  p->symlink = S_ISLNK(st.st_mode) != 0;

  if(p->symlink)
  {
    if(stat(path, &st) != 0)
      pony_throw();
  }

  p->file = S_ISREG(st.st_mode) != 0;
  p->directory = S_ISDIR(st.st_mode) != 0;
  p->pipe = S_ISFIFO(st.st_mode) != 0;
#endif

  p->mode = st.st_mode & 0777;
  p->hard_links = (uint32_t)st.st_nlink;
  p->uid = st.st_uid;
  p->gid = st.st_gid;
  p->size = st.st_size;
  p->access_time = st.st_atime;
  p->modified_time = st.st_mtime;
  p->change_time = st.st_ctime;
}
