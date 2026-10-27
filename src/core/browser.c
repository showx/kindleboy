#include "browser.h"
#include "ui.h"
#include "skin.h"
#include "overlay.h"
#include "status.h"
#include "emu.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

typedef struct {
	int px, x, w, list_y, row_h, rows, footer_y, exit_x, exit_y, exit_w, exit_h;
} geom_t;

static void geom(const render_cfg_t *s, int cw, geom_t *g)
{
	g->px=skin_unit(cw);
	g->x=s->dst_x+8*g->px; g->w=s->game_w-16*g->px;
	g->list_y=s->dst_y+36*g->px;
	g->row_h=24*g->px;
	g->footer_y=s->dst_y+s->game_h-18*g->px;
	g->rows=(g->footer_y-g->list_y-6*g->px)/g->row_h;
	if (g->rows<1) g->rows=1;
	g->exit_w=30*g->px; g->exit_h=16*g->px;
	g->exit_x=g->x+g->w-g->exit_w; g->exit_y=s->dst_y+8*g->px;
}

static int ci_cmp(const void *a, const void *b)
{
	const unsigned char *x=a,*y=b;
	for (; *x && *y; x++,y++) {
		int cx=*x,cy=*y;
		if (cx>='A' && cx<='Z') cx+=32;
		if (cy>='A' && cy<='Z') cy+=32;
		if (cx!=cy) return cx-cy;
	}
	return *x-*y;
}

int browser_scan(browser_t *b, const char *dir)
{
	memset(b,0,sizeof *b);
	snprintf(b->dir,sizeof b->dir,"%s",dir);
	DIR *d=opendir(dir);
	if (!d) return 0;
	struct dirent *e;
	while ((e=readdir(d)) && b->count<BROWSER_MAX_ROMS) {
		const char *dot=strrchr(e->d_name,'.');
		if (e->d_name[0]=='.' || !dot || (ci_cmp(dot,".gb") && ci_cmp(dot,".gbc"))) continue;
		/* Do not offer directories or truncated filenames that cannot launch. */
		if (strlen(e->d_name)>=BROWSER_NAME_LEN) continue;
		char path[512]; snprintf(path,sizeof path,"%s/%s",dir,e->d_name);
		struct stat st;
		if (stat(path,&st)!=0 || !S_ISREG(st.st_mode)) continue;
		strcpy(b->names[b->count++],e->d_name);
	}
	closedir(d);
	qsort(b->names,b->count,BROWSER_NAME_LEN,ci_cmp);
	return b->count;
}

void browser_path(const browser_t *b, int idx, char *out, int n)
{
	if (n<=0) return;
	if (idx<0 || idx>=b->count) { out[0]=0; return; }
	snprintf(out,n,"%s/%s",b->dir,b->names[idx]);
}

static void display_name(const char *name, char *out, int max_chars)
{
	int len=(int)strlen(name);
	const char *dot=strrchr(name,'.');
	if (dot) len=(int)(dot-name);
	if (max_chars<1) { out[0]=0; return; }
	int n=len<max_chars?len:max_chars;
	for (int i=0;i<n;i++) {
		unsigned char c=(unsigned char)name[i];
		out[i]=(c=='_' || c=='-')?' ':(c>=32 && c<127?(char)c:'?');
	}
	if (len>max_chars && n>=3) out[n-3]=out[n-2]=out[n-1]='.';
	out[n]=0;
}

void browser_draw(const browser_t *b, uint8_t *c, int cw, int ch, const render_cfg_t *s, int batt)
{
	geom_t g; geom(s,cw,&g);
	skin_draw(c,cw,ch,s);
	status_draw(c,cw,ch,batt,false,batt>=0 && batt<=15,false);
	overlay_t o; overlay_layout(&o,s,cw,ch); overlay_draw(&o,c,cw,ch,false);
	int p=g.px, small=p>2?2:1;
	ui_text(c,cw,ch,g.x,s->dst_y+9*p,"GAME SELECT",p,0x00);
	char text[64]; snprintf(text,sizeof text,"%02d CARTRIDGES",b->count);
	ui_text(c,cw,ch,g.x,s->dst_y+23*p,text,small,0x00);
	ui_round_fill(c,cw,ch,g.exit_x,g.exit_y,g.exit_w,g.exit_h,2*p,0x00);
	ui_text(c,cw,ch,g.exit_x+(g.exit_w-ui_text_width("EXIT",small))/2,
		g.exit_y+(g.exit_h-8*small)/2,"EXIT",small,0xFF);
	ui_fill(c,cw,ch,g.x,g.list_y-4*p,g.w,1,0x00);
	if (!b->count) {
		int y=g.list_y+g.row_h;
		ui_text(c,cw,ch,g.x,y,"NO GAMES YET",p,0x00);
		ui_text(c,cw,ch,g.x,y+20*p,"ADD .GB / .GBC FILES",small,0x00);
		ui_text(c,cw,ch,g.x,y+30*p,"TO ROMS/GB VIA USB",small,0x00);
	} else for (int i=0;i<g.rows && b->top+i<b->count;i++) {
		int idx=b->top+i, y=g.list_y+i*g.row_h;
		bool selected=idx==b->sel;
		uint8_t fg=selected?0xFF:0x00;
		if (selected) ui_fill(c,cw,ch,g.x,y,g.w,g.row_h-2*p,0x00);
		char name[BROWSER_NAME_LEN], num[16];
		display_name(b->names[idx],name,(g.w-34*p)/(8*p));
		snprintf(num,sizeof num,"%02d",idx+1);
		ui_text(c,cw,ch,g.x+4*p,y+7*p,num,small,fg);
		ui_text(c,cw,ch,g.x+28*p,y+7*p,name,p,fg);
	}
	ui_fill(c,cw,ch,g.x,g.footer_y-5*p,g.w,1,0x00);
	ui_text(c,cw,ch,g.x,g.footer_y,"A PLAY  B EXIT",small,0x00);
	ui_text(c,cw,ch,g.x,g.footer_y+8*p,"UP/DN SELECT  L/R PAGE",small,0x00);
	snprintf(text,sizeof text,"%d/%d",b->count?b->sel+1:0,b->count);
	ui_text(c,cw,ch,g.x+g.w-ui_text_width(text,small),g.footer_y,text,small,0x00);
}

static void ensure_visible(browser_t *b,int rows)
{
	if (!b->count) { b->sel=b->top=0; return; }
	if (b->sel<0) b->sel=0;
	if (b->sel>=b->count) b->sel=b->count-1;
	if (b->sel<b->top) b->top=b->sel;
	if (b->sel>=b->top+rows) b->top=b->sel-rows+1;
	if (b->top<0) b->top=0;
}

int browser_input(browser_t *b,const plat_input_t *in,int cw,int ch,const render_cfg_t *s,bool *changed)
{
	geom_t g; geom(s,cw,&g);
	overlay_t o; overlay_layout(&o,s,cw,ch);
	ov_specials_t sp; uint8_t buttons=overlay_map(&o,in,&sp);
	uint8_t pressed=buttons & ~b->prev_buttons;
	b->prev_buttons=buttons;
	bool touch=in->count>0, tap=touch && !b->touch_prev;
	b->touch_prev=touch;
	int old=b->sel, old_top=b->top, launch=-1;
	if (in->key_up || (pressed&JOYPAD_UP)) b->sel--;
	else if (in->key_down || (pressed&JOYPAD_DOWN)) b->sel++;
	else if (pressed&JOYPAD_LEFT) b->sel-=g.rows;
	else if (pressed&JOYPAD_RIGHT) b->sel+=g.rows;
	ensure_visible(b,g.rows);
	if (b->count && (in->key_enter || (pressed&(JOYPAD_A|JOYPAD_START)))) launch=b->sel;
	if (in->key_menu || (pressed&JOYPAD_B) || (tap && sp.menu)) launch=BROWSER_QUIT;
	else if (tap && sp.deghost) launch=BROWSER_REFRESH;
	if (tap) {
		int x=in->pts[0].x,y=in->pts[0].y;
		if (x>=g.exit_x && x<g.exit_x+g.exit_w && y>=g.exit_y && y<g.exit_y+g.exit_h) launch=BROWSER_QUIT;
		else if (x>=g.x && x<g.x+g.w && y>=g.list_y && y<g.list_y+g.rows*g.row_h) {
			int idx=b->top+(y-g.list_y)/g.row_h;
			if (idx<b->count) { if (idx==b->sel) launch=idx; else b->sel=idx; }
		}
	}
	ensure_visible(b,g.rows);
	if (changed) *changed=old!=b->sel || old_top!=b->top;
	return launch;
}
