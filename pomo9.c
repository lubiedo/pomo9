#include <u.h>
#include <libc.h>
#include <draw.h>
#include <keyboard.h>
#include <event.h>

#define PLAY	" ▶"
#define PAUSE	" ▌▌"
#define TIMEFMT	"%02d:%02d"
#define PROGRESS(c,t)	(((c)*360)/(t))

ulong Background  = DBlack;
ulong Foreground  = DWhite;
ulong ActiveCol   = DRed;
ulong InactiveCol = DBlue;

static void reset(void);
static void bail(void);

int rest = 15;
int work = 45;
#define REST	(rest*60)
#define WORK	(work*60)
struct Pomo {
	short	status;
	short	working;
	int	rest;
	int	work;
	int	n;
};
struct Pomo pomo;
Point padding = {5,10};
Point winsize = {110,110};
Image *clock;
Image *bgcol;
Image *fgcol;
int wctl;
int timer;

static char *
gentime(int time)
{
	static char b[64];
	int m,s;
	
	m = time/60;
	s = time-(m*60);
	snprint(b, sizeof(b), TIMEFMT, m, s);
	return b;
}

static void
drawlabels(Point p, char *str)
{
	Rune pomon = 0x0000;
	char b[8];

	if(pomo.status)
		pomon = 0x2780 + (pomo.n%10);
		
	runetochar(b, &pomon);

	int strx = stringwidth(display->defaultfont, str),
		stry = display->defaultfont->ascent;
	int numx = stringwidth(display->defaultfont, b);
	Point strpos =  Pt(p.x + winsize.x/2 - Borderwidth - strx/2, p.y + winsize.y/2 - Borderwidth - stry/2);
	Point numpos  =  Pt(p.x + winsize.x/2 - Borderwidth - numx/2, strpos.y - stry*2);

	string(screen, numpos, fgcol, ZP, display->defaultfont, b);
	string(screen, strpos, fgcol, ZP, display->defaultfont, str);
}

static void
drawclock(Point p, ulong col, int phi)
{
	Image *color;
	
	color = allocimage(display, Rect(0,0,1,1), RGB24, 1, col);
	arc(screen,
		Pt(p.x + winsize.x/2 - Borderwidth, p.y + winsize.y/2 - Borderwidth),
		winsize.x/2 - Borderwidth - 2 - padding.x, winsize.y/2 - Borderwidth - 2 - padding.y, 1,
		color, ZP, 1, phi);
}

static void
redraw(void)
{
	Point p;
	if(bgcol == nil)
		bgcol = allocimage(display, Rect(0,0,1,1), RGB24, 1, Background);
	if(fgcol == nil)
		fgcol = allocimage(display, Rect(0,0,1,1), RGB24, 1, Foreground);
	
	p = screen->r.min;
	draw(screen, Rect(p.x, p.y, p.x + winsize.x, p.y + winsize.y),
		bgcol, nil, ZP);
	
	if(pomo.working && pomo.status){
		drawclock(p, ActiveCol, PROGRESS(pomo.work, WORK));
	}else{
		drawclock(p, InactiveCol, PROGRESS(pomo.rest, REST));
	}
	
	if (pomo.status) {
		drawlabels(p, gentime(pomo.working ? pomo.work : pomo.rest));
	}else{
		if(pomo.n == 0 && pomo.work == WORK) { // first time
			drawlabels(p, PLAY);
		}else{
			drawlabels(p, PAUSE);
		}
	}

	fprint(wctl, "top");
	if(fprint(wctl, "resize -dx %d -dy %d", winsize.x, winsize.y) < 0)
		fprint(2, "resize: %r");
}

static void
ekey(Rune k)
{
	switch(k){
	case 'r':
		reset();
		pomo.n = 0;
		pomo.status = 0;
		break;
	case Ketx:
	case Kdel:
		bail();
		break;
	case 0x20:
		pomo.status = pomo.status ? 0 : 1; // 0 = paused, 1 = play
		break;
	}
	redraw();
}

void
eresized(int new)
{
	if(new&&getwindow(display, Refnone) < 0)
		sysfatal("getwindow: %r");
	redraw();
}

static void bail(void) { if(wctl) close(wctl); exits(nil); }
static void
reset(void)
{
	pomo.work = WORK;
	pomo.rest = REST;
	pomo.working = 0;
}

void
main(int argc, char *argv[])
{	
	Event e;
	int et;
	
	ARGBEGIN{
	case 'w':
		work = atoi(ARGF());
		break;
	case 'r':
		rest = atoi(ARGF());
		break;
	default:
		fprint(2, "usage: %s [-w min] [-r min]\n"
			  "keybindings: 'Space' to start/stop, 'r' to reset, 'Supr'/'Ctrl+c' to exit\n", argv0);
		bail();
	}ARGEND

	pomo=(struct Pomo){0, 0, REST, WORK, 0};
	if((wctl = open("/dev/wctl", OWRITE)) < 0)
		sysfatal("opening wctl: %r:");
	if(initdraw(nil, nil, "pomo9") < 0)
		sysfatal("initdraw: %r");
	einit(Emouse|Ekeyboard);
	eresized(0);
	timer = etimer(0, 1000);
	redraw();
	for(;;){
		et = event(&e);
		switch(et){
		case Ekeyboard:
			ekey(e.kbdc);
			break;
		}
		if(et == timer){
			if(pomo.status)
				pomo.working ? pomo.work-- : pomo.rest--;
			if(!pomo.working && (pomo.work == 0 && pomo.rest == 0))
				pomo.n++;
			pomo.working = (pomo.work>0);
			if(pomo.rest == 0)
				reset();
			redraw();
		}
	}
}
