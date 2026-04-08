#include "cli.h"

#include "../application/processor.h"
#include "interactive.h"
#include "../shared/error.h"

int run_cli(int argc, char **argv) {
  if (argc == 1) {
    return run_interactive_mode();
  }

  if (argc != 2) {
    print_error("file open failed");
    return 1;
  }
  return process_sql_file(argv[1]);
}
