/* Default settings; many can be overriden by command line. */
static const unsigned int def_expire = 3;         /* seconds until window disapears */
static const unsigned int def_min_width = 100;    /* minimum window width */
static const int def_center_text = 1;             /* center text in window*/
static const int detectmon = 0;                   /* display to current monitor upon startup */

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

static const char *colors[SchemeLast][2] = {
	             /*     fg         bg       */
	[SchemeNorm] = { "#bbbbbb", "#222222" },
	[SchemeSel] = { "#eeeeee", "#005577" },
	[SchemeBar] = { "#4499bb", "#444444" },
};

static const unsigned int border_width = 2;   /* Size of the window border */

static unsigned int bar_outer_pad = 7;    /* cosmetic padding between window edge and progress bar background */
static unsigned int bar_inner_pad = 4;    /* cosmetic padding between progress bar background and foreground */
