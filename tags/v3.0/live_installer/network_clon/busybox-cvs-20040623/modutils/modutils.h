#define _PATH_MODULES	"/lib/modules/"
#define MODULE_DIR	"/lib/modules/"
#define STRVERSIONLEN	32

extern int k_version;
extern char k_strversion[];

/* temporary */
extern int flag_force_load;
extern int flag_autoclean;
extern int flag_verbose;
extern int flag_quiet;
extern int flag_export;
extern int flag_unresolved_error;
extern int flag_showerror;
extern int flag_quick;
extern int flag_dry;

int get_kernel_version(char str[STRVERSIONLEN]);

extern int depmod_main_2_4(int argc, char *argv[], int all, const char *system_map, const char *base_dir, const char *module_dir, const char *file_syms);
extern int depmod_main_2_6(int argc, char *argv[], int all, const char *system_map, const char *base_dir, const char *module_dir);
int insmod_main_2_4(int fp, int argc, char **argv, char *m_name, char *m_filename);

#define MODUTILS_MINIMAL_VERSION_2_4 0x20100
#define MODUTILS_MINIMAL_VERSION_2_6 0x20500
