#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/ast/builder.h"
#include "../../src/libponyc/ast/parser.h"
#include "../../src/libponyc/ast/source.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "../../src/libponyc/platform/platform.h"

#define PONY_EXTENSION  ".pony"
#define AST_EXTENSION   ".ast"


static bool do_file_succeed(const char* pony_file, const char* ast_file)
{
  printf("Parse %s - expect success\n", pony_file);

  source_t* pony_src = source_open(pony_file);

  if(pony_src == NULL)
  {
    printf("Could not open %s\n", pony_file);
    return false;
  }

  ast_t* actual = parser(pony_src);

  if(actual == NULL)
  {
    printf("Could not parse %s\n", pony_file);
    return false;
  }

  source_t* ast_src = source_open(ast_file);

  if(ast_src == NULL)
  {
    printf("Could not open %s\n", ast_file);
    ast_free(actual);
    return false;
  }

  ast_t* expected = build_ast(ast_src);

  if(expected == NULL)
  {
    printf("Could not parse %s\n", ast_file);
    ast_free(actual);
    return false;
  }

  bool r = build_compare_asts(expected, actual);

  if(!r)
    printf("Generated AST did not match expected\n");
  else
    printf("Success\n");

  ast_free(actual);
  ast_free(expected);

  return r;
}


static bool do_file_fail(const char* pony_file)
{
  printf("Parse %s - expected failure\n", pony_file);

  source_t* pony_src = source_open(pony_file);
  if(pony_src == NULL)
  {
    printf("Could not open %s\n", pony_file);
    return false;
  }

  ast_t* actual = parser(pony_src);

  if(actual == NULL)
    printf("Success - parse failed as expected\n");
  else
    printf("Failure - parse unexpectedly succeeded\n");

  ast_free(actual);

  return (actual == NULL);
}

//TODO FIX: Windows, check error codes. Docus only provide generic error codes
static bool do_path(const char* path, bool expect_success)
{
  PONY_ERRNO err = 0;
  PONY_DIR* dir = pony_opendir(path, &err);

  if(dir == NULL)
  {
    switch(err)
    {
      case PONY_IO_EACCES:
        printf("Cannot open %s permission denied\n", path);
        break;

      case PONY_IO_ENOENT:
        printf("Cannot open %s does not exist\n", path);
        break;

      case PONY_IO_ENOTDIR:
        printf("Cannot open %s not a directory\n", path);
        break;

      case PONY_IO_PATH_TOO_LONG:
        printf("Cannot open %s path too long\n", path);
        break;
      default:
        printf("Cannot open %s unknown error %d\n", path, errno);
        break;
    }

    return false;
  }
  
  PONY_DIRINFO dirent;
  PONY_DIRINFO* d;
  bool success = true;

  while(!pony_dir_entry_next(dir, &dirent, &d) && (d != NULL))
  {
    // handle only files with the specified extension
    char* name = pony_get_dir_name(d);
    char* p = strrchr(name, '.');

    if(p == NULL || strcmp(p, PONY_EXTENSION) != 0)
      continue;

    char pony_fullpath[FILENAME_MAX];
    strcpy(pony_fullpath, path);
    strcat(pony_fullpath, "/");
    strcat(pony_fullpath, name);

    char ast_fullpath[FILENAME_MAX];
    strcpy(ast_fullpath, pony_fullpath);

    p = strrchr(ast_fullpath, '.');
    assert(p != NULL);
    memcpy(p, AST_EXTENSION, strlen(AST_EXTENSION) + 1);

    bool r;

    if(expect_success)
      r = do_file_succeed(pony_fullpath, ast_fullpath);
    else
      r = do_file_fail(pony_fullpath);

    if(!r)
    {
      success = false;
      print_errors();
    }

    free_errors();
  }

  pony_closedir(dir);
  return success;
}


int main(int argc, char** argv)
{
  bool r = true;

  if(!do_path("/home/beast/CDev/ponyc/test/bnf/pass", true))  r = false;
  if(!do_path("/home/beast/CDev/ponyc/test/bnf/fail", false)) r = false;

  if(r)
    printf("\nSuccess\n");
  else
    printf("\nFailure\n");

  return r ? 0 : -1;
}
