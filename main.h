#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

struct bot_config_t
{
	const char *config_file;
	bool enable_coredump;
	bool load_and_exit;
};

extern struct bot_config_t g_bot_config;
extern int g_program_argc;
extern char **g_program_argv;
extern bool g_restart;


#endif // MAIN_H_INCLUDED

