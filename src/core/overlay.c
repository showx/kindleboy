/* Game Boy controls. Hit targets are intentionally more generous than
 * the visible buttons; the diagonal corners still work for real games. */
#include "overlay.h"
#include "ui.h"
#include "skin.h"
#include "emu.h"
#include <string.h>

static void add_zone(overlay_t *o, int x, int y, int w, int h,
		     uint8_t bit, ov_special_t special, const char *label)
{
	if (o->count >= (int)(sizeof o->zones / sizeof o->zones[0])) return;
	touch_zone_t *z = &o->zones[o->count++];
	z->x = x; z->y = y; z->w = w; z->h = h;
	z->bit = bit; z->special = special; z->label = label;
}

void overlay_layout(overlay_t *o, const render_cfg_t *rc, int cw, int ch)
{
	memset(o, 0, sizeof *o);
	int u = skin_unit(cw), y0 = rc->dst_y + rc->game_h + 26*u;
	int row_y = ch - 30*u;
	o->area_y = y0;
	int dside = cw * 34 / 100;
	if (dside > row_y - y0 + 8*u) dside = row_y - y0 + 8*u;
	int c = dside / 3, dx = 16*u, dy = y0;
	add_zone(o, dx, dy, c, c, JOYPAD_UP|JOYPAD_LEFT, OV_NONE, "");
	add_zone(o, dx+c, dy, c, c, JOYPAD_UP, OV_NONE, "^");
	add_zone(o, dx+2*c, dy, c, c, JOYPAD_UP|JOYPAD_RIGHT, OV_NONE, "");
	add_zone(o, dx, dy+c, c, c, JOYPAD_LEFT, OV_NONE, "<");
	add_zone(o, dx+2*c, dy+c, c, c, JOYPAD_RIGHT, OV_NONE, ">");
	add_zone(o, dx, dy+2*c, c, c, JOYPAD_DOWN|JOYPAD_LEFT, OV_NONE, "");
	add_zone(o, dx+c, dy+2*c, c, c, JOYPAD_DOWN, OV_NONE, "v");
	add_zone(o, dx+2*c, dy+2*c, c, c, JOYPAD_DOWN|JOYPAD_RIGHT, OV_NONE, "");
	int bs = 48*u;
	/* Shorter control bands must still leave SELECT/START unobstructed. */
	int max_bs = row_y - y0 - 38*u;
	if (bs > max_bs) bs = max_bs;
	int ax = cw - 10*u - bs, ay = y0 + 8*u;
	add_zone(o, ax, ay, bs, bs, JOYPAD_A, OV_NONE, "A");
	add_zone(o, ax-bs-10*u, ay+24*u, bs, bs, JOYPAD_B, OV_NONE, "B");
	int sx = cw * 42 / 100;
	add_zone(o, sx-4*u, row_y-6*u, 50*u, 30*u, JOYPAD_SELECT, OV_NONE, "SELECT");
	add_zone(o, sx+48*u, row_y-6*u, 50*u, 30*u, JOYPAD_START, OV_NONE, "START");
	add_zone(o, cw-146*u, 0, 48*u, 20*u, 0, OV_DEGHOST, "CLEAN");
	add_zone(o, cw-94*u, 0, 32*u, 20*u, 0, OV_FF, "3X");
	add_zone(o, cw-58*u, 0, 50*u, 20*u, 0, OV_MENU, "MENU");
}

#define VIS_MENU (1u << 8)
#define VIS_CLEAN (1u << 9)
#define VIS_FF (1u << 10)
#define DIR_BITS (JOYPAD_UP|JOYPAD_DOWN|JOYPAD_LEFT|JOYPAD_RIGHT)
#define FEEDBACK_STEP_US 230000ull /* one DU update, no continuous animation */

static void utility(const touch_zone_t *z, uint8_t *c, int cw, int ch, bool on, bool down)
{
	int u = skin_unit(cw), x = z->x + u, y = z->y + 3*u;
	int w = z->w - 2*u, h = z->h - 6*u;
	ui_fill(c, cw, ch, z->x, z->y, z->w, z->h, 0xFF);
	ui_round_fill(c, cw, ch, x+u, y+u, w, h, 3*u, 0xAA);
	if (down) { x+=u; y+=u; }
	ui_round_fill(c, cw, ch, x, y, w, h, 3*u, 0x00);
	bool filled=on || down;
	if (!filled) ui_round_fill(c, cw, ch, x+1, y+1, w-2, h-2, 3*u, 0xFF);
	ui_text(c, cw, ch, x+(w-ui_text_width(z->label,u))/2,
		y+(h-8*u)/2, z->label, u, filled ? 0xFF : 0x00);
}

static void dpad(const overlay_t *o, uint8_t *c, int cw, int ch, uint16_t bits)
{
	int u = skin_unit(cw);
	int x = o->zones[0].x, y = o->zones[0].y, cell = o->zones[0].w;
	int side = 3*cell, inset = 4*u;
	ui_round_fill(c, cw, ch, x-2*u, y-2*u, side+4*u, side+4*u, 12*u, 0xAA);
	ui_round_fill(c, cw, ch, x-2*u+1, y-2*u+1, side+4*u-2, side+4*u-2, 12*u, 0xFF);
	ui_round_fill(c, cw, ch, x+cell+u, y+2*u, cell, side, 3*u, 0xAA);
	ui_round_fill(c, cw, ch, x+u, y+cell+2*u, side, cell, 3*u, 0xAA);
	int shift=(bits & DIR_BITS)?u:0;
	x+=shift; y+=shift;
	ui_round_fill(c, cw, ch, x+cell, y, cell, side, 3*u, 0x00);
	ui_round_fill(c, cw, ch, x, y+cell, side, cell, 3*u, 0x00);
	if (!shift) ui_line(c, cw, ch, x+cell+inset, y+2*u, x+2*cell-inset, y+2*u, 0xAA);
	ui_circle(c, cw, ch, x+side/2, y+side/2, cell/4, 0x55);
	const char *arrows[] = {"^", "<", ">", "v"};
	const int ids[] = {1,3,4,6};
	for (int i=0; i<4; i++) {
		const touch_zone_t *z = &o->zones[ids[i]];
		bool down=(bits & z->bit)!=0;
		if (down) ui_round_fill(c,cw,ch,z->x+shift+2*u,z->y+shift+2*u,
			cell-4*u,cell-4*u,2*u,0xFF);
		ui_text(c, cw, ch, z->x+shift+(cell-8*u)/2, z->y+shift+(cell-8*u)/2,
			arrows[i], u, down?0x00:0xAA);
	}

}

static void button(const touch_zone_t *z, uint8_t *c, int cw, int ch, bool down)
{
	int u=skin_unit(cw);
	if (z->bit == JOYPAD_A || z->bit == JOYPAD_B) {
		/* Reserve space for the entire shadow and travel inside the target. */
		int r=z->w/2-4*u-1, cx=z->x+z->w/2, cy=z->y+z->h/2;
		ui_circle(c,cw,ch,cx+u,cy+2*u,r+2*u,0xAA);
		if (down) { cx+=u; cy+=2*u; }
		ui_circle(c,cw,ch,cx,cy,r+2*u,down?0x00:0x55);
		ui_circle(c,cw,ch,cx,cy,r,down?0xFF:0x00);
		if (!down) ui_line(c,cw,ch,cx-r/2,cy-r+3*u,cx+r/2,cy-r+3*u,0xAA);
		ui_text(c,cw,ch,cx-4*(u+1),cy-4*(u+1),z->label,u+1,down?0x00:0xFF);
	} else {
		int px=u>2?2:1, bx=z->x+4*u, by=z->y+6*u, bw=42*u, bh=12*u;
		ui_round_fill(c,cw,ch,bx+u,by+2*u,bw,bh,6*u,0xAA);
		if (down) { bx+=u; by+=2*u; }
		ui_round_fill(c,cw,ch,bx,by,bw,bh,6*u,0x00);
		if (down) ui_round_fill(c,cw,ch,bx+u,by+u,bw-2*u,bh-2*u,5*u,0xFF);
		else ui_line(c,cw,ch,bx+6*u,by+2*u,bx+bw-6*u,by+2*u,0xAA);
		/* Labels stay still, even while the key cap sinks. */
		int lx=z->x+4*u, ly=z->y+24*u;
		ui_text(c,cw,ch,lx+(bw-ui_text_width(z->label,px))/2,ly,z->label,px,0x55);
	}
}

static uint16_t zone_mask(const touch_zone_t *z)
{
	if (z->special==OV_MENU) return VIS_MENU;
	if (z->special==OV_DEGHOST) return VIS_CLEAN;
	if (z->special==OV_FF) return VIS_FF;
	return z->bit;
}

/* All drawing and all local refreshes use the same complete visual bounds,
 * including the part of each raised shadow outside its touch target. */
static overlay_damage_t bounds(const overlay_t *o,int group,int cw,int ch)
{
	int u=skin_unit(cw);
	const touch_zone_t *z=&o->zones[group?group+7:0];
	overlay_damage_t r={z->x,z->y,z->w,z->h};
	if (!group) { r.x-=2*u; r.y-=2*u; r.w=3*z->w+4*u+1; r.h=r.w; }
	if (r.x<0) { r.w+=r.x; r.x=0; }
	if (r.y<0) { r.h+=r.y; r.y=0; }
	if (r.x+r.w>cw) r.w=cw-r.x;
	if (r.y+r.h>ch) r.h=ch-r.y;
	return r;
}

static void draw_group(const overlay_t *o,int group,uint8_t *c,int cw,int ch,uint16_t bits,bool ff_on)
{
	overlay_damage_t r=bounds(o,group,cw,ch);
	ui_fill(c,cw,ch,r.x,r.y,r.w,r.h,0xFF);
	if (!group) dpad(o,c,cw,ch,bits);
	else {
		const touch_zone_t *z=&o->zones[group+7];
		bool down=(bits & zone_mask(z))!=0;
		if (z->special!=OV_NONE) utility(z,c,cw,ch,ff_on && z->special==OV_FF,down);
		else button(z,c,cw,ch,down);
	}
}

void overlay_draw(const overlay_t *o,uint8_t *c,int cw,int ch,bool ff_on)
{
	for (int g=0;g<OVERLAY_MAX_DAMAGE;g++) draw_group(o,g,c,cw,ch,0,ff_on);
}

void overlay_draw_special(const overlay_t *o, uint8_t *c, int cw, int ch,
		ov_special_t which, bool inverted, int *ox, int *oy, int *ow, int *oh)
{
	*ox=*oy=*ow=*oh=0;
	for (int i=0; i<o->count; i++) {
		const touch_zone_t *z=&o->zones[i];
		if (z->special != which) continue;
		utility(z,c,cw,ch,inverted,false);
		*ox=z->x; *oy=z->y; *ow=z->w; *oh=z->h;
		return;
	}
}

uint8_t overlay_map(const overlay_t *o, const plat_input_t *in, ov_specials_t *sp)
{
	uint8_t bits=in->direct_buttons;
	if (sp) memset(sp,0,sizeof *sp);
	for (int t=0; t<in->count; t++) {
		int px=in->pts[t].x, py=in->pts[t].y;
		for (int i=0; i<o->count; i++) {
			const touch_zone_t *z=&o->zones[i];
			if (px<z->x || px>=z->x+z->w || py<z->y || py>=z->y+z->h) continue;
			switch (z->special) {
			case OV_MENU: if (sp) sp->menu=true; break;
			case OV_DEGHOST: if (sp) sp->deghost=true; break;
			case OV_FF: if (sp) sp->ff=true; break;
			default: bits |= z->bit; break;
			}
		}
	}
	return bits;
}

void overlay_feedback_reset(overlay_feedback_t *f,bool ff_on)
{
	memset(f,0,sizeof *f); f->ff_on=ff_on;
}

static void fast_shades(uint8_t *c,int cw,const overlay_damage_t *r)
{
	/* DU accepts only black/white. Texture the small gray shadows instead of
	 * flashing the whole screen or waiting for a slow grayscale waveform. */
	static const uint8_t bayer[2][2]={{0,2},{3,1}};
	for (int y=r->y;y<r->y+r->h;y++) for (int x=r->x;x<r->x+r->w;x++) {
		uint8_t *p=&c[(size_t)y*cw+x];
		int coverage=((255-*p)*4+127)/255;
		*p=bayer[y&1][x&1]<coverage?0x00:0xFF;
	}
}

void overlay_feedback_draw(const overlay_t *o,const overlay_feedback_t *f,uint8_t *c,int cw,int ch)
{
	for (int g=0;g<OVERLAY_MAX_DAMAGE;g++) {
		draw_group(o,g,c,cw,ch,f->shown,f->ff_on);
		/* Keep held controls crisp when a list/menu repaint reconstructs them. */
		uint16_t mask=g?zone_mask(&o->zones[g+7]):DIR_BITS;
		if (f->shown & mask) { overlay_damage_t r=bounds(o,g,cw,ch); fast_shades(c,cw,&r); }
	}
}

int overlay_feedback_update(const overlay_t *o,overlay_feedback_t *f,uint8_t *c,int cw,int ch,
	uint8_t buttons,const ov_specials_t *sp,bool ff_on,uint64_t now,overlay_damage_t damage[OVERLAY_MAX_DAMAGE])
{
	uint16_t actual=buttons;
	if (sp) { if (sp->menu) actual|=VIS_MENU; if (sp->deghost) actual|=VIS_CLEAN; if (sp->ff) actual|=VIS_FF; }
	f->pending |= actual & ~f->observed;
	f->observed=actual;
	if (now<f->next_us) return 0;
	uint16_t want=actual | f->pending;
	for (int i=0;i<11;i++) if ((f->shown & (1u<<i)) && now<f->release_after[i]) want|=1u<<i;
	/* A fresh direction replaces a released direction's visual latch. */
	if ((actual|f->pending)&DIR_BITS) want=(want & ~DIR_BITS) | ((actual|f->pending)&DIR_BITS);
	f->pending &= ~want;
	uint16_t changed=want ^ f->shown;
	for (int i=0;i<11;i++) if ((want & ~f->shown)&(1u<<i)) f->release_after[i]=now+FEEDBACK_STEP_US;
	if (ff_on!=f->ff_on) changed |= VIS_FF;
	f->shown=want; f->ff_on=ff_on;
	int n=0;
	for (int g=0;g<OVERLAY_MAX_DAMAGE;g++) {
		uint16_t mask=g?zone_mask(&o->zones[g+7]):DIR_BITS;
		if (!(changed & mask)) continue;
		draw_group(o,g,c,cw,ch,want,ff_on);
		damage[n]=bounds(o,g,cw,ch); fast_shades(c,cw,&damage[n]); n++;
	}
	if (n) f->next_us=now+FEEDBACK_STEP_US;
	return n;
}

int overlay_feedback_wait_ms(const overlay_feedback_t *f,uint64_t now,int idle_ms)
{
	if (!f->pending && f->shown==f->observed) return idle_ms;
	uint64_t left=now<f->next_us?f->next_us-now:0;
	int ms=(int)((left+999)/1000);
	if (ms<1) ms=1;
	return ms<idle_ms?ms:idle_ms;
}
