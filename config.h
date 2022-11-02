#define MAX_NOTIFICATIONS 10
#define MAX_IMAGE_MEM 5 /* max number of recent images to keep in memory */

static const unsigned int def_expire = 10;                /* seconds until window disapears */
static const unsigned int def_min_width = 100;            /* minimum window width */
static const int def_center_text = 1;                     /* center text in window */
static const unsigned int border_width = 2;               /* size of the window border */
static const unsigned int text_padding = 2;               /* cosmetic padding between lines of text */
static const unsigned int contents_padding_vertical = 0;  /* vertical cosmetic padding between window edge and window contents */
static unsigned int bar_outer_padding = 7;                /* cosmetic padding between window edge and progress bar background */
static unsigned int bar_inner_padding = 4;                /* cosmetic padding between progress bar background and foreground */
static const int border_padding = 50;                     /* cosmetic padding between notifications and screen border */
static const int inter_padding = 20;                      /* cosmetic padding between notifications */

/* def_location values correspond to these positions on your screen
	|---+---+---|
	| 6 | 5 | 4 |
	|---+---+---|
	| 7 | 0 | 3 |
	|---+---+---|
	| 8 | 1 | 2 |
	|---+---+---|
*/
static const unsigned int def_location = 1;

/* default X11 font or font set */
static const char *fonts[] = {
	"monospace:size=18"
};

/* color schemes */
static const char *colors[SchemeLast][2] = {
	             /*     fg         bg       */
	[SchemeNorm] = { "#bbbbbb", "#222222" },
	[SchemeSel]  = { "#eeeeee", "#005577" },
	[SchemeBar]  = { "#4499bb", "#444444" },
};
