/** @file cmdline.h
 *  @brief The header file for the command line option parser
 *  generated by GNU Gengetopt version 2.22.1
 *  http://www.gnu.org/software/gengetopt.
 *  DO NOT modify this file, since it can be overwritten
 *  @author GNU Gengetopt by Lorenzo Bettini */

#ifndef CMDLINE_H
#define CMDLINE_H

/* If we use autoconf.  */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h> /* for FILE */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef CMDLINE_PARSER_PACKAGE
/** @brief the program name */
#define CMDLINE_PARSER_PACKAGE "ingen"
#endif

#ifndef CMDLINE_PARSER_VERSION
/** @brief the program version */
#define CMDLINE_PARSER_VERSION VERSION
#endif

/** @brief Where the command line options are stored */
struct gengetopt_args_info
{
  const char *help_help; /**< @brief Print help and exit help description.  */
  const char *version_help; /**< @brief Print version and exit help description.  */
  int client_port_arg;	/**< @brief Client OSC port.  */
  char * client_port_orig;	/**< @brief Client OSC port original value given at command line.  */
  const char *client_port_help; /**< @brief Client OSC port help description.  */
  char * connect_arg;	/**< @brief Connect to existing engine at OSC URI (default='osc.udp://localhost:16180').  */
  char * connect_orig;	/**< @brief Connect to existing engine at OSC URI original value given at command line.  */
  const char *connect_help; /**< @brief Connect to existing engine at OSC URI help description.  */
  int engine_flag;	/**< @brief Run (JACK) engine (default=off).  */
  const char *engine_help; /**< @brief Run (JACK) engine help description.  */
  int engine_port_arg;	/**< @brief Engine OSC port (default='16180').  */
  char * engine_port_orig;	/**< @brief Engine OSC port original value given at command line.  */
  const char *engine_port_help; /**< @brief Engine OSC port help description.  */
  int gui_flag;	/**< @brief Launch the GTK graphical interface (default=off).  */
  const char *gui_help; /**< @brief Launch the GTK graphical interface help description.  */
  char * jack_name_arg;	/**< @brief JACK client name (default='ingen').  */
  char * jack_name_orig;	/**< @brief JACK client name original value given at command line.  */
  const char *jack_name_help; /**< @brief JACK client name help description.  */
  char * load_arg;	/**< @brief Load patch.  */
  char * load_orig;	/**< @brief Load patch original value given at command line.  */
  const char *load_help; /**< @brief Load patch help description.  */
  int parallelism_arg;	/**< @brief Number of concurrent process threads (default='1').  */
  char * parallelism_orig;	/**< @brief Number of concurrent process threads original value given at command line.  */
  const char *parallelism_help; /**< @brief Number of concurrent process threads help description.  */
  char * path_arg;	/**< @brief Target path for loaded patch.  */
  char * path_orig;	/**< @brief Target path for loaded patch original value given at command line.  */
  const char *path_help; /**< @brief Target path for loaded patch help description.  */
  char * run_arg;	/**< @brief Run script.  */
  char * run_orig;	/**< @brief Run script original value given at command line.  */
  const char *run_help; /**< @brief Run script help description.  */
  
  unsigned int help_given ;	/**< @brief Whether help was given.  */
  unsigned int version_given ;	/**< @brief Whether version was given.  */
  unsigned int client_port_given ;	/**< @brief Whether client-port was given.  */
  unsigned int connect_given ;	/**< @brief Whether connect was given.  */
  unsigned int engine_given ;	/**< @brief Whether engine was given.  */
  unsigned int engine_port_given ;	/**< @brief Whether engine-port was given.  */
  unsigned int gui_given ;	/**< @brief Whether gui was given.  */
  unsigned int jack_name_given ;	/**< @brief Whether jack-name was given.  */
  unsigned int load_given ;	/**< @brief Whether load was given.  */
  unsigned int parallelism_given ;	/**< @brief Whether parallelism was given.  */
  unsigned int path_given ;	/**< @brief Whether path was given.  */
  unsigned int run_given ;	/**< @brief Whether run was given.  */

  char **inputs ; /**< @brief unamed options (options without names) */
  unsigned inputs_num ; /**< @brief unamed options number */
} ;

/** @brief The additional parameters to pass to parser functions */
struct cmdline_parser_params
{
  int override; /**< @brief whether to override possibly already present options (default 0) */
  int initialize; /**< @brief whether to initialize the option structure gengetopt_args_info (default 1) */
  int check_required; /**< @brief whether to check that all required options were provided (default 1) */
  int check_ambiguity; /**< @brief whether to check for options already specified in the option structure gengetopt_args_info (default 0) */
  int print_errors; /**< @brief whether getopt_long should print an error message for a bad option (default 1) */
} ;

/** @brief the purpose string of the program */
extern const char *gengetopt_args_info_purpose;
/** @brief the usage string of the program */
extern const char *gengetopt_args_info_usage;
/** @brief all the lines making the help output */
extern const char *gengetopt_args_info_help[];

/**
 * The command line parser
 * @param argc the number of command line options
 * @param argv the command line options
 * @param args_info the structure where option information will be stored
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int cmdline_parser (int argc, char * const *argv,
  struct gengetopt_args_info *args_info);

/**
 * The command line parser (version with additional parameters - deprecated)
 * @param argc the number of command line options
 * @param argv the command line options
 * @param args_info the structure where option information will be stored
 * @param override whether to override possibly already present options
 * @param initialize whether to initialize the option structure my_args_info
 * @param check_required whether to check that all required options were provided
 * @return 0 if everything went fine, NON 0 if an error took place
 * @deprecated use cmdline_parser_ext() instead
 */
int cmdline_parser2 (int argc, char * const *argv,
  struct gengetopt_args_info *args_info,
  int override, int initialize, int check_required);

/**
 * The command line parser (version with additional parameters)
 * @param argc the number of command line options
 * @param argv the command line options
 * @param args_info the structure where option information will be stored
 * @param params additional parameters for the parser
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int cmdline_parser_ext (int argc, char * const *argv,
  struct gengetopt_args_info *args_info,
  struct cmdline_parser_params *params);

/**
 * Save the contents of the option struct into an already open FILE stream.
 * @param outfile the stream where to dump options
 * @param args_info the option struct to dump
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int cmdline_parser_dump(FILE *outfile,
  struct gengetopt_args_info *args_info);

/**
 * Save the contents of the option struct into a (text) file.
 * This file can be read by the config file parser (if generated by gengetopt)
 * @param filename the file where to save
 * @param args_info the option struct to save
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int cmdline_parser_file_save(const char *filename,
  struct gengetopt_args_info *args_info);

/**
 * Print the help
 */
void cmdline_parser_print_help(void);
/**
 * Print the version
 */
void cmdline_parser_print_version(void);

/**
 * Initializes all the fields a cmdline_parser_params structure 
 * to their default values
 * @param params the structure to initialize
 */
void cmdline_parser_params_init(struct cmdline_parser_params *params);

/**
 * Allocates dynamically a cmdline_parser_params structure and initializes
 * all its fields to their default values
 * @return the created and initialized cmdline_parser_params structure
 */
struct cmdline_parser_params *cmdline_parser_params_create(void);

/**
 * Initializes the passed gengetopt_args_info structure's fields
 * (also set default values for options that have a default)
 * @param args_info the structure to initialize
 */
void cmdline_parser_init (struct gengetopt_args_info *args_info);
/**
 * Deallocates the string fields of the gengetopt_args_info structure
 * (but does not deallocate the structure itself)
 * @param args_info the structure to deallocate
 */
void cmdline_parser_free (struct gengetopt_args_info *args_info);

/**
 * Checks that all the required options were specified
 * @param args_info the structure to check
 * @param prog_name the name of the program that will be used to print
 *   possible errors
 * @return
 */
int cmdline_parser_required (struct gengetopt_args_info *args_info,
  const char *prog_name);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* CMDLINE_H */
