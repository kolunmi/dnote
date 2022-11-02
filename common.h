#define X_DISPLAY_DIR "/tmp/.X11-unix"

#define VERSION "0.4"
#define SOCKET_PATH "/tmp/dnoted-socket-%s"
#define MESSAGE_SIZE BUFSIZ
#define MAX_ID_LEN 32
#define MAX_SHCMD_LEN 64
#define MAX_PATH_LEN 256

#define TITLE_STATUS "STATUS"
#define TITLE_WARNING "WARNING"
#define TITLE_REQUEST "REQUEST"
#define TITLE_COMMAND "COMMAND"

#define OPTION_ID             '0'
#define OPTION_LOCATION       '1'
#define OPTION_MIN_WIDTH      '2'
#define OPTION_JUSTIFY_CENTER '3'
#define OPTION_JUSTIFY_LEFT   '4'
#define OPTION_NO_EXPIRE      '5'
#define OPTION_EXPIRE         '6'
#define OPTION_PROGRESS_BAR   '7'
#define OPTION_SHELL_COMMAND  '8'
#define OPTION_IMAGE_PATH     '9'
#define OPTION_HEADER_IMAGE   'a'
#define OPTION_INLINE_IMAGE   'b'
