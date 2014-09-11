#include "../libponyc/ast/parserapi.h"
#include "../libponyc/pkg/package.h"
#include "../libponyc/pass/pass.h"
#include "../libponyc/ds/stringtab.h"
#include "../libponyc/platform/platform.h"

#include "options.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <sys/ioctl.h>
#  include <unistd.h>
#endif

enum
{
  OPT_AST,
  OPT_LLVM,
  OPT_OPTLEVEL,
  OPT_PATHS,
  OPT_PASSES,
  OPT_TRACE,
  OPT_WIDTH
};

static arg_t args[] =
{
  {"opt", 'O', ARGUMENT_REQUIRED, OPT_OPTLEVEL},
  {"path", 'p', ARGUMENT_REQUIRED, OPT_PATHS},
  {"pass", 'r', ARGUMENT_REQUIRED, OPT_PASSES},
  {"trace", 't', ARGUMENT_NONE, OPT_TRACE},
  {"width", 'w', ARGUMENT_REQUIRED, OPT_WIDTH},
  ARGUMENTS_FINISH
};

void usage()
{
  printf(
    "ponyc [OPTIONS] <package directory>\n"
    "  --opt, -O       optimisation level (0-3)\n"
    "  --path, -p      add additional colon separated search paths\n"
    "\n"
    "  --pass, -r      restrict phases\n"
    "    =parse\n"
    "    =parsefix\n"
    "    =sugar\n"
    "    =scope1\n"
    "    =name\n"
    "    =flatten\n"
    "    =traits\n"
    "    =scope2\n"
    "    =expr\n"
    "    =ast          output an abstract syntax tree\n"
    "    =ir           output LLVM IR\n"
    "    =bitcode      output LLVM bitcode\n"
    "    =asm          output assembly\n"
    "    =obj          output an object file\n"
    "    =all          the default: generate an executable\n"
    "\n"
    "  --trace, -t     enable parse trace\n"
    "  --width, -w     width to target when printing the AST\n"
    "\n"
    );
}

size_t get_width()
{
  size_t width = 80;
#ifdef _WIN64
  CONSOLE_SCREEN_BUFFER_INFO info;

  if(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
  {
    int cols = info.srWindow.Right - info.srWindow.Left + 1;

    if(cols > width)
      width = cols;
  }
#else
  struct winsize ws;

  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws))
  {
    if(ws.ws_col > width)
      width = ws.ws_col;
  }
#endif
  return width;
}

int main(int argc, char* argv[])
{
  package_init(argv[0]);

  int opt = 0;
  size_t width = get_width();

  parse_state_t s;
  opt_init(args, &s, &argc, argv);

  int id;
  while((id = opt_next(&s)) != -1)
  {
    switch(id)
    {
      case OPT_OPTLEVEL: opt = atoi(s.arg_val); break;
      case OPT_PATHS: package_add_paths(s.arg_val); break;

      case OPT_TRACE: parse_trace(true); break;
      case OPT_WIDTH: width = atoi(s.arg_val); break;

      case OPT_PASSES:
        if(!limit_passes(s.arg_val))
        {
          usage();
          return -1;
        }
        break;

      default: usage(); return -1;
    }
  }

  if((opt < 0) || (opt > 3))
  {
    usage();
    return -1;
  }

  const char* path;

  switch(argc)
  {
    case 1: path = "."; break;
    case 2: path = argv[1]; break;
    default: usage(); return -1;
  }

  ast_t* program = program_load(path);
  int ret = 0;

  if(program != NULL)
  {
    if(!program_passes(program, opt))
      ret = -1;

    ast_free(program);
  } else {
    ret = -1;
  }

  print_errors();
  free_errors();

  package_done();
  stringtab_done();

  return ret;
}
