#include "menu.h"
#include "ui.h"
#include "skin.h"
#include "overlay.h"
#include "status.h"
#include "emu.h"
#include <string.h>
#include <stdio.h>

static const menu_action_t actions[]={MENU_RESUME,MENU_SAVE_STATE,MENU_LOAD_STATE,
	MENU_TOGGLE_QUALITY,MENU_TOGGLE_AUDIO,MENU_DEGHOST,MENU_QUIT_TO_BROWSER,MENU_QUIT};
#define ITEM_COUNT ((int)(sizeof actions/sizeof actions[0]))

static void item_label(int i,bool quality,bool audio,char *out,int n)
{
	switch (actions[i]) {
	case MENU_RESUME: snprintf(out,n,"RESUME"); break;
	case MENU_SAVE_STATE: snprintf(out,n,"SAVE STATE"); break;
	case MENU_LOAD_STATE: snprintf(out,n,"LOAD STATE"); break;
	case MENU_TOGGLE_QUALITY: snprintf(out,n,"MODE: %s",quality?"QUALITY":"FAST"); break;
	case MENU_TOGGLE_AUDIO: snprintf(out,n,"SOUND: %s",audio?"ON":"OFF"); break;
	case MENU_DEGHOST: snprintf(out,n,"CLEAN SCREEN"); break;
	case MENU_QUIT_TO_BROWSER: snprintf(out,n,"GAME SELECT"); break;
	case MENU_QUIT: snprintf(out,n,"EXIT EMULATOR"); break;
	default: out[0]=0; break;
	}
}

typedef struct { int px,x,w,y,h,gap; } geom_t;
static void geom(const render_cfg_t *s,int cw,geom_t *g)
{
	g->px=skin_unit(cw); g->x=s->dst_x+10*g->px; g->w=s->game_w-20*g->px;
	g->h=16*g->px; g->gap=2*g->px;
	g->y=s->dst_y+(s->game_h-ITEM_COUNT*(g->h+g->gap)+g->gap)/2+4*g->px;
}

void menu_reset(menu_t *m) { memset(m,0,sizeof *m); m->prev_buttons=0xFF; }

void menu_draw(const menu_t *m,uint8_t *c,int cw,int ch,const render_cfg_t *s,bool quality,bool audio,int batt)
{
	geom_t g; geom(s,cw,&g);
	skin_draw(c,cw,ch,s);
	status_draw(c,cw,ch,batt,false,batt>=0 && batt<=15,false);
	overlay_t o; overlay_layout(&o,s,cw,ch); overlay_draw(&o,c,cw,ch,false);
	int p=g.px, small=p>2?2:1;
	ui_text(c,cw,ch,g.x,s->dst_y+10*p,"PAUSED",p,0x00);
	ui_fill(c,cw,ch,g.x,s->dst_y+25*p,g.w,1,0x00);
	for (int i=0;i<ITEM_COUNT;i++) {
		int y=g.y+i*(g.h+g.gap); char label[32]; item_label(i,quality,audio,label,sizeof label);
		bool selected=i==m->sel;
		if (selected) ui_fill(c,cw,ch,g.x,y,g.w,g.h,0x00);
		ui_text(c,cw,ch,g.x+5*p,y+(g.h-8*p)/2,selected?">":" ",p,selected?0xFF:0x00);
		ui_text(c,cw,ch,g.x+20*p,y+(g.h-8*p)/2,label,p,selected?0xFF:0x00);
	}
	ui_text(c,cw,ch,g.x,s->dst_y+s->game_h-15*p,"A CONFIRM  B RESUME",small,0x00);
}

menu_action_t menu_input(menu_t *m,const plat_input_t *in,int cw,int ch,const render_cfg_t *s,bool *changed)
{
	geom_t g; geom(s,cw,&g);
	overlay_t o; overlay_layout(&o,s,cw,ch);
	ov_specials_t sp; uint8_t buttons=overlay_map(&o,in,&sp);
	uint8_t pressed=buttons & ~m->prev_buttons; m->prev_buttons=buttons;
	bool touch=in->count>0, tap=touch && !m->touch_prev; m->touch_prev=touch;
	int old=m->sel; menu_action_t act=MENU_NONE;
	if (in->key_up || (pressed&JOYPAD_UP)) m->sel=(m->sel+ITEM_COUNT-1)%ITEM_COUNT;
	else if (in->key_down || (pressed&JOYPAD_DOWN)) m->sel=(m->sel+1)%ITEM_COUNT;
	if (in->key_enter || (pressed&(JOYPAD_A|JOYPAD_START))) act=actions[m->sel];
	if (in->key_menu || (pressed&JOYPAD_B) || (tap && sp.menu)) act=MENU_RESUME;
	else if (tap && sp.deghost) act=MENU_DEGHOST;
	if (tap) {
		int x=in->pts[0].x,y=in->pts[0].y;
		for (int i=0;i<ITEM_COUNT;i++) {
			int iy=g.y+i*(g.h+g.gap);
			if (x>=g.x && x<g.x+g.w && y>=iy && y<iy+g.h) { m->sel=i; act=actions[i]; break; }
		}
	}
	if (changed) *changed=old!=m->sel;
	return act;
}
