#define _GNU_SOURCE
#include <platform.h>
#include <string.h>
#include <stdio.h>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <sys/mman.h>
#endif

void* virtual_alloc(size_t bytes)
{
#if defined(PLATFORM_IS_WINDOWS)
  return VirtualAlloc(NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#elif defined(PLATFORM_IS_POSIX_BASED)
  void* p = mmap(0, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1,
    0);

  if(p == MAP_FAILED)
  {
    perror("out of memory: ");
    abort();
  }

  return p;
#endif
}

bool virtual_free(void* p, size_t bytes)
{
#if defined(PLATFORM_IS_WINDOWS)
  return VirtualFree(p, 0, MEM_RELEASE) != 0;
#elif defined(PLATFORM_IS_POSIX_BASED)
  return munmap(p, bytes) == 0;
#endif
}
