
#define DT_UNKNOWN       0
#define DT_FIFO          1
#define DT_CHR           2
#define DT_DIR           4
#define DT_BLK           6
#define DT_REG           8
#define DT_LNK          10
#define DT_SOCK         12
#define DT_WHT          14 

struct ps2dirent
{
	/** relative filename */
	char d_name[256];
	unsigned char  d_type;
};

typedef struct PS2DIR
{
	/** handle used against fio */
	int  d_fd;

	/** entry returned at readdir */
	struct ps2dirent *d_entry;
	char d_name[256];
} PS2DIR;

int Setup_Pad(void);
void Wait_Pad_Ready(void);
void WaitPadReady(int port, int slot);
int ps2init();
void ps2delay(int count);
int ps2quit();

extern int fileXio_mode;

int ps2Dopen(char *path);
int ps2Dclose(int fd);
int ps2Dread(int fd, struct ps2dirent *dirinfo);
int ps2Getstat(const char *path, struct stat *dfstat);
PS2DIR * ps2Opendir(char *dirname);
int ps2Closedir(PS2DIR *dir);
struct ps2dirent *ps2Readdir(PS2DIR *dirp);
int ps2Stat(char *name, struct stat *ps2stat);
int ps2Chdir(char *path);
int ps2Getcwd(char *current_dir_name, int max_path);

void copy_path(char *main_path, char *file_path, char *out);
int check_dir(char *path, int is_main);

int ps2DebugScreenInit();

int ps2GetMainPath(char *path, char *argv);
char *ps2fgets(char *str, int size, FILE_TAG_TYPE stream);
int clock_gettime(struct timespec *ts);

