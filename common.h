#define VERSION "0.5"

#define X_DISPLAY_DIR "/tmp/.X11-unix"
#define SOCKET_PATH "/tmp/dnoted-socket-%s"
#define MESSAGE_SIZE BUFSIZ
#define MAX_ID_LEN 32
#define MAX_SHCMD_LEN 64
#define MAX_PATH_LEN 256

#define TITLE_STATUS "STATUS"
#define TITLE_WARNING "WARNING"
#define TITLE_REQUEST "REQUEST"
#define TITLE_COMMAND "COMMAND"

#define DNOTE_PREFIX '0'
#define DNOTEC_PREFIX '1'

#define DNOTE_OPTION_ID                 '0'
#define DNOTE_OPTION_LOCATION           '1'
#define DNOTE_OPTION_MIN_WIDTH          '2'
#define DNOTE_OPTION_JUSTIFY_CENTER     '3'
#define DNOTE_OPTION_JUSTIFY_LEFT       '4'
#define DNOTE_OPTION_NO_EXPIRE          '5'
#define DNOTE_OPTION_EXPIRE             '6'
#define DNOTE_OPTION_PROGRESS_BAR       '7'
#define DNOTE_OPTION_SHELL_COMMAND      '8'
#define DNOTE_OPTION_IMAGE_PATH         '9'
#define DNOTE_OPTION_HEADER_IMAGE       'a'
#define DNOTE_OPTION_INLINE_IMAGE       'b'
#define DNOTE_OPTION_PRECISE_LOCATION   'c'
#define DNOTE_OPTION_IMMUTABLE          'd'

#define DNOTEC_OPTION_LIST          '0'
#define DNOTEC_OPTION_KILL          '1'
#define DNOTEC_OPTION_CLEAR         '2'
#define DNOTEC_OPTION_RENEW         '3'
#define DNOTEC_OPTION_SELECT        '4'
#define DNOTEC_OPTION_IMAGE_LOAD    '5'
#define DNOTEC_OPTION_IMAGE_LIST    '6'
