/* Behavioral checks for the new touchscreen layout, independent of the art. */
#include "../src/core/overlay.h"
#include "../src/core/browser.h"
#include "../src/core/menu.h"
#include "../src/core/status.h"
#include "../src/core/emu.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static plat_input_t contact(const touch_zone_t *z)
{
	plat_input_t in={0}; in.count=1;
	in.pts[0].x=z->x+z->w/2; in.pts[0].y=z->y+z->h/2;
	return in;
}

static const touch_zone_t *find(const overlay_t *o,uint8_t bit,ov_special_t sp)
{
	for (int i=0;i<o->count;i++) if (o->zones[i].bit==bit && o->zones[i].special==sp) return &o->zones[i];
	assert(!"Missing control"); return NULL;
}

static bool overlaps(const touch_zone_t *a,const touch_zone_t *b)
{
	return a->x<b->x+b->w && b->x<a->x+a->w && a->y<b->y+b->h && b->y<a->y+a->h;
}

static int browser_press(browser_t *b,plat_input_t in,int w,int h,const render_cfg_t *s)
{
	bool changed; plat_input_t released={0};
	browser_input(b,&released,w,h,s,&changed);
	return browser_input(b,&in,w,h,s,&changed);
}

static menu_action_t menu_press(menu_t *m,plat_input_t in,int w,int h,const render_cfg_t *s)
{
	bool changed; plat_input_t released={0};
	menu_input(m,&released,w,h,s,&changed);
	return menu_input(m,&in,w,h,s,&changed);
}

static bool inside(int x,int y,const overlay_damage_t *r)
{
	return x>=r->x && x<r->x+r->w && y>=r->y && y<r->y+r->h;
}

static void check_feedback(const overlay_t *o,const render_cfg_t *s,int w,int h)
{
	size_t bytes=(size_t)w*h;
	uint8_t *c=malloc(bytes), *before=malloc(bytes), *rest=malloc(bytes);
	assert(c && before && rest);
	for (int z=0;z<o->count;z++) {
		memset(c,0xAA,bytes); overlay_draw(o,c,w,h,false); memcpy(before,c,bytes);
		overlay_feedback_t f; overlay_feedback_reset(&f,false);
		plat_input_t in=contact(&o->zones[z]); ov_specials_t sp;
		uint8_t bits=overlay_map(o,&in,&sp);
		overlay_damage_t damage[OVERLAY_MAX_DAMAGE];
		int n=overlay_feedback_update(o,&f,c,w,h,bits,&sp,false,0,damage);
		assert(n==1); /* diagonals share one D-pad refresh */
		int changed=0;
		for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
			bool local=inside(x,y,&damage[0]);
			if (!local) assert(c[(size_t)y*w+x]==before[(size_t)y*w+x]);
			else {
				assert(c[(size_t)y*w+x]==0 || c[(size_t)y*w+x]==255);
				changed += c[(size_t)y*w+x]!=before[(size_t)y*w+x];
			}
			if (x>=s->dst_x && x<s->dst_x+s->game_w && y>=s->dst_y && y<s->dst_y+s->game_h) assert(!local);
		}
		assert(changed>0);
		assert(overlay_feedback_update(o,&f,c,w,h,bits,&sp,false,500000,damage)==0); /* hold: no animation */
		ov_specials_t released={0};
		assert(overlay_feedback_update(o,&f,c,w,h,0,&released,false,500001,damage)==1);
		assert(f.shown==0 && overlay_feedback_wait_ms(&f,500001,500)==500);
		memcpy(rest,c,bytes);
		assert(overlay_feedback_update(o,&f,c,w,h,bits,&sp,false,1000000,damage)==1);
		assert(overlay_feedback_update(o,&f,c,w,h,0,&released,false,1500000,damage)==1);
		assert(!memcmp(rest,c,bytes)); /* repeated press/release leaves no trails */
		assert(overlay_feedback_update(o,&f,c,w,h,0,&released,false,2000000,damage)==0);
	}
	/* A short tap queued during an in-flight visual update must still appear. */
	overlay_feedback_t f; overlay_feedback_reset(&f,false);
	overlay_damage_t damage[OVERLAY_MAX_DAMAGE]; ov_specials_t sp={0};
	assert(overlay_feedback_update(o,&f,c,w,h,JOYPAD_A,&sp,false,0,damage)==1);
	assert(overlay_feedback_update(o,&f,c,w,h,0,&sp,false,1000,damage)==0);
	assert(overlay_feedback_update(o,&f,c,w,h,JOYPAD_B,&sp,false,2000,damage)==0);
	assert(overlay_feedback_update(o,&f,c,w,h,0,&sp,false,3000,damage)==0);
	assert(f.shown==JOYPAD_A && overlay_feedback_wait_ms(&f,3000,500)==227);
	assert(overlay_feedback_update(o,&f,c,w,h,0,&sp,false,230000,damage)==2);
	assert(f.shown==JOYPAD_B);
	assert(overlay_feedback_update(o,&f,c,w,h,0,&sp,false,460000,damage)==1 && f.shown==0);
	assert(overlay_feedback_update(o,&f,c,w,h,JOYPAD_UP|JOYPAD_A,&sp,false,1000000,damage)==2);
	assert(overlay_feedback_update(o,&f,c,w,h,0,&sp,false,1500000,damage)==2);
	/* Persistent 3X indication survives release of its momentary key cap. */
	sp.ff=true;
	assert(overlay_feedback_update(o,&f,c,w,h,0,&sp,true,2000000,damage)==1);
	sp.ff=false;
	assert(overlay_feedback_update(o,&f,c,w,h,0,&sp,true,2500000,damage)==1 && f.ff_on && !f.shown);
	assert(overlay_feedback_update(o,&f,c,w,h,0,&sp,true,3000000,damage)==0);
	free(rest); free(before); free(c);
}

static void check_size(int w,int h)
{
	render_cfg_t s; render_layout(&s,w,h,status_height(w),h/3);
	overlay_t o; overlay_layout(&o,&s,w,h);
	assert(s.game_w==GB_W*s.scale && s.game_h==GB_H*s.scale);
	if (w==600 && h==800) assert(s.scale==3);
	for (int i=0;i<o.count;i++) {
		const touch_zone_t *z=&o.zones[i];
		assert(z->x>=0 && z->y>=0 && z->w>0 && z->h>0 && z->x+z->w<=w && z->y+z->h<=h);
		touch_zone_t lcd={.x=s.dst_x,.y=s.dst_y,.w=s.game_w,.h=s.game_h};
		assert(!overlaps(z,&lcd));
		for (int j=i+1;j<o.count;j++) assert(!overlaps(z,&o.zones[j]));
		plat_input_t in=contact(z); ov_specials_t sp;
		assert(overlay_map(&o,&in,&sp)==z->bit);
		assert(sp.menu==(z->special==OV_MENU));
		assert(sp.ff==(z->special==OV_FF));
		assert(sp.deghost==(z->special==OV_DEGHOST));
	}
	/* Separate A/B targets can be held together, as can diagonal plus A. */
	const touch_zone_t *a=find(&o,JOYPAD_A,OV_NONE), *b=find(&o,JOYPAD_B,OV_NONE);
	plat_input_t both=contact(a), ib=contact(b); both.count=2; both.pts[1]=ib.pts[0];
	assert(overlay_map(&o,&both,NULL)==(JOYPAD_A|JOYPAD_B));
	plat_input_t diagonal=contact(find(&o,JOYPAD_UP|JOYPAD_LEFT,OV_NONE));
	diagonal.count=2; diagonal.pts[1]=both.pts[0];
	assert(overlay_map(&o,&diagonal,NULL)==(JOYPAD_UP|JOYPAD_LEFT|JOYPAD_A));
	plat_input_t down=contact(find(&o,JOYPAD_DOWN,OV_NONE)), up=contact(find(&o,JOYPAD_UP,OV_NONE));
	plat_input_t right=contact(find(&o,JOYPAD_RIGHT,OV_NONE)), left=contact(find(&o,JOYPAD_LEFT,OV_NONE));
	plat_input_t confirm=contact(a), back=contact(b), released={0};
	bool changed;
	static browser_t picker; memset(&picker,0,sizeof picker);
	picker.count=20;
	assert(browser_press(&picker,down,w,h,&s)==-1 && picker.sel==1);
	assert(browser_input(&picker,&down,w,h,&s,&changed)==-1 && picker.sel==1 && !changed); /* hold once */
	assert(browser_press(&picker,right,w,h,&s)==-1 && picker.sel>1 && picker.top>0);
	assert(browser_press(&picker,left,w,h,&s)==-1 && picker.sel==1);
	assert(browser_press(&picker,confirm,w,h,&s)==1);
	assert(browser_press(&picker,back,w,h,&s)==BROWSER_QUIT);
	assert(browser_press(&picker,contact(find(&o,0,OV_DEGHOST)),w,h,&s)==BROWSER_REFRESH);
	picker.count=0; picker.sel=picker.top=0;
	assert(browser_press(&picker,up,w,h,&s)==-1 && picker.sel==0 && picker.top==0);
	assert(browser_press(&picker,right,w,h,&s)==-1 && picker.sel==0);
	assert(browser_press(&picker,confirm,w,h,&s)==-1);
	menu_t m; menu_reset(&m);
	assert(menu_press(&m,down,w,h,&s)==MENU_NONE && m.sel==1);
	assert(menu_input(&m,&down,w,h,&s,&changed)==MENU_NONE && m.sel==1);
	assert(menu_press(&m,confirm,w,h,&s)==MENU_SAVE_STATE);
	assert(menu_press(&m,back,w,h,&s)==MENU_RESUME);
	assert(menu_press(&m,up,w,h,&s)==MENU_NONE && m.sel==0);
	assert(menu_press(&m,up,w,h,&s)==MENU_NONE && m.sel==7);
	assert(menu_press(&m,confirm,w,h,&s)==MENU_QUIT);
	/* The touch that opens a menu must not close it again while held. */
	menu_reset(&m); m.touch_prev=true;
	plat_input_t menu_touch=contact(find(&o,0,OV_MENU));
	assert(menu_input(&m,&menu_touch,w,h,&s,&changed)==MENU_NONE);
	assert(menu_press(&m,menu_touch,w,h,&s)==MENU_RESUME);
	/* Returning to a list while A is held must not reopen a game. */
	picker.count=20; picker.prev_buttons=0xFF; picker.touch_prev=true;
	assert(browser_input(&picker,&confirm,w,h,&s,&changed)==-1);
	browser_input(&picker,&released,w,h,&s,&changed);
	/* Draw into guarded memory: clipping is also checked under ASan. */
	size_t bytes=(size_t)w*h;
	uint8_t *buf=malloc(bytes+64); assert(buf); memset(buf,0xCC,bytes+64);
	uint8_t *c=buf+32;
	strcpy(picker.names[0],"A very long cartridge name that must be truncated at the screen edge.gbc");
	browser_draw(&picker,c,w,h,&s,77);
	menu_draw(&m,c,w,h,&s,false,false,77);
	for (int i=0;i<32;i++) assert(buf[i]==0xCC && buf[32+bytes+i]==0xCC);
	/* Clock updates must leave all three utility keys byte-identical. */
	uint8_t *saved=malloc(bytes); assert(saved); memcpy(saved,c,bytes);
	status_draw(c,w,h,9,true,true,true);
	for (int i=0;i<o.count;i++) if (o.zones[i].special!=OV_NONE) {
		const touch_zone_t *z=&o.zones[i];
		for (int y=z->y;y<z->y+z->h;y++) assert(!memcmp(c+y*w+z->x,saved+y*w+z->x,z->w));
	}
	free(saved); free(buf);
	check_feedback(&o,&s,w,h);
	printf("UI %dx%d: controls, combinations, picker, menu, clipping and status PASS\n",w,h);
	printf("Feedback %dx%d: local damage, held/released keys, queued taps, idle and 3X PASS\n",w,h);
}

int main(void)
{
	check_size(600,800); check_size(800,1072); check_size(1072,1448);
	return 0;
}
