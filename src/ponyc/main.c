#include "../libponyc/pkg/package.h"
#include "../libponyc/ds/stringtab.h"
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

static struct option opts[] =
{
  {"ast", no_argument, NULL, 'a'},
  {"llvm", no_argument, NULL, 'l'},
  {"opt", no_argument, NULL, 'O'},
  {"path", required_argument, NULL, 'p'},
  {"parse", no_argument, NULL, 'r'},
  {"width", required_argument, NULL, 'w'},
  {NULL, 0, NULL, 0},
};

void usage()
{
  printf(
    "ponyc [OPTIONS] <file>\n"
    "  --ast, -a       print the AST\n"
    "  --llvm, -l      print the LLVM IR\n"
    "  --opt, -O       optimisation level (0-3)\n"
    "  --path, -p      add additional colon separated search paths\n"
    "  --parse, -r     stop after parse phase\n"
    "  --width, -w     width to target when printing the AST\n"
    );
}

size_t get_width()
{
  struct winsize ws;
  size_t width = 80;

  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1)
  {
    if(ws.ws_col > width)
      width = ws.ws_col;
  }

  return width;
}

int main(int argc, char** argv)
{
  package_init(argv[0]);

  bool ast = false;
  bool llvm = false;
  bool parse_only = false;
  int opt = 0;
  size_t width = get_width();
  char c;

  while((c = getopt_long(argc, argv, "alO:p:rw:", opts, NULL)) != -1)
  {
    switch(c)
    {
      case 'a': ast = true; break;
      case 'l': llvm = true; break;
      case 'p': package_paths(optarg); break;
      case 'O': opt = atoi(optarg); break;
      case 'r': parse_only = true; break;
      case 'w': width = atoi(optarg); break;
      default: usage(); return -1;
    }
  }

  if((opt < 0) || (opt > 3))
  {
    usage();
    return -1;
  }

  argc -= optind;
  argv += optind;

  ast_t* program = program_load((argc > 0) ? argv[0] : ".", parse_only);
  int ret = 0;

  if(program != NULL)
  {
    if(ast)
    {
      ast_print(program, width);
    } else if(!parse_only) {
      if(!program_compile(program, opt, llvm))
        ret = -1;
    }

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
