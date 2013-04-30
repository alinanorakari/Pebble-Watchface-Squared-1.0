#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#define MY_UUID { 0x0F, 0xA6, 0xE1, 0x51, 0xCF, 0x7E, 0x45, 0x39, 0x8E, 0xE2, 0x02, 0x71, 0xD4, 0xE5, 0x12, 0x0A }
PBL_APP_INFO(MY_UUID,
             "Squared", "lastfuture",
             1, 2, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_WATCH_FACE);

Window window;

#define US_DATE true // true == MM/DD, false == DD/MM
#define NO_ZERO true // false == replaces leading Zero for hour, day, month with a "cycler"
#define TILE_SIZE 10
#define NUMSLOTS 8
#define SPACING_X TILE_SIZE
#define SPACING_Y TILE_SIZE
#define DIGIT_CHANGE_ANIM_DURATION 1800
#define STARTDELAY 2500

#define FONT blocks
	
typedef struct {
	Layer layer;
	int   prevDigit;
	int   curDigit;
	int   divider;
	uint32_t normTime;
} digitSlot;

digitSlot slot[NUMSLOTS];

AnimationImplementation animImpl;
Animation anim;
bool splashEnded = false;

unsigned char blocks[][5][5] =  {{
    {1,1,1,1,1},
    {1,0,0,0,1},
    {1,0,1,0,1},
    {1,0,0,0,1},
    {1,1,1,1,1}
}, {
    {1,1,0,1,1},
    {0,0,0,0,1},
    {1,1,1,0,1},
    {0,0,0,0,1},
    {1,1,1,0,1}
}, {
    {1,1,1,1,1},
    {0,0,0,0,1},
    {1,1,1,1,1},
    {1,0,0,0,0},
    {1,1,1,1,1}
}, {
    {1,1,1,1,1},
    {0,0,0,0,1},
    {0,1,1,1,1},
    {0,0,0,0,1},
    {1,1,1,1,1}
}, {
    {1,0,1,0,1},
    {1,0,0,0,1},
    {1,1,1,1,1},
    {0,0,0,0,1},
    {1,1,1,0,1}
}, {
    {1,1,1,1,1},
    {1,0,0,0,0},
    {1,1,1,1,1},
    {0,0,0,0,1},
    {1,1,1,1,1}
}, {
    {1,1,1,1,1},
    {1,0,0,0,0},
    {1,1,1,1,1},
    {1,0,0,0,1},
    {1,1,1,1,1}
}, {
    {1,1,1,1,1},
    {0,0,0,0,1},
    {1,0,1,0,1},
    {1,0,1,0,1},
    {1,0,1,0,1}
}, {
    {1,1,1,1,1},
    {1,0,0,0,1},
    {1,1,1,1,1},
    {1,0,0,0,1},
    {1,1,1,1,1}
}, {
    {1,1,1,1,1},
    {1,0,0,0,1},
    {1,1,1,1,1},
    {0,0,0,0,1},
    {1,1,1,1,1}
}, {
    {1,1,1,1,1},
    {0,0,0,0,0},
    {1,1,1,1,1},
    {0,0,0,0,0},
    {1,1,1,1,1}
}, {
    {1,0,1,0,1},
    {1,0,1,0,1},
    {1,0,1,0,1},
    {1,0,1,0,1},
    {1,0,1,0,1}
}};

int startDigit[NUMSLOTS] = {
	11,10,10,11,11,10,10,11
};

#define FONT_HEIGHT_BLOCKS (sizeof *FONT / sizeof **FONT)
#define FONT_WIDTH_BLOCKS (sizeof **FONT)
#define TOTALBLOCKS FONT_HEIGHT_BLOCKS * FONT_WIDTH_BLOCKS
#define FONT_HEIGHT FONT_HEIGHT_BLOCKS*TILE_SIZE
#define FONT_WIDTH FONT_WIDTH_BLOCKS*TILE_SIZE

#define SCREEN_WIDTH 144

#define TILES_X ( \
    FONT_WIDTH + SPACING_X + FONT_WIDTH)
#define TILES_Y ( \
    FONT_HEIGHT + SPACING_Y + FONT_HEIGHT)

#define ORIGIN_X ((SCREEN_WIDTH - TILES_X)/2)
#define ORIGIN_Y TILE_SIZE*1.5
	
GRect slotFrame(int i) {
	int x, y, w, h;
	if (i<4) {
		w = FONT_WIDTH;
		h = FONT_HEIGHT;
		if (i%2) {
			x = ORIGIN_X + FONT_WIDTH + SPACING_X; // i = 1 or 3
		} else {
			x = ORIGIN_X; // i = 0 or 2
		}
		
		if (i<2) {
			y = ORIGIN_Y;
		} else {
			y = ORIGIN_Y + FONT_HEIGHT + SPACING_Y;
		}
	} else {
		w = FONT_WIDTH/2;
		h = FONT_HEIGHT/2;
		x = ORIGIN_X + (FONT_WIDTH + SPACING_X) * (i - 4) / 2;
		y = ORIGIN_Y + (FONT_HEIGHT + SPACING_Y) * 2;
	}
	return GRect(x, y, w, h);
}

void updateSlot(digitSlot *slot, GContext *ctx) {
	int t, tx, ty, w, offs, shift, total, tilesize, widthadjust;
	total = TOTALBLOCKS;
	widthadjust = 0;
	if (slot->divider == 2) {
		widthadjust = 1;
	}
	tilesize = TILE_SIZE/slot->divider;
	uint32_t skewedNormTime = slot->normTime*2;
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_rect(ctx, GRect(0, 0, slot->layer.bounds.size.w, slot->layer.bounds.size.h), 0, GCornerNone);
	for (t=0; t<total; t++) {
		w = 0;
		offs = 0;
		tx = t % FONT_WIDTH_BLOCKS;
		ty = t / FONT_HEIGHT_BLOCKS;
		shift = tx-ty;
		if (FONT[slot->curDigit][ty][tx] == FONT[slot->prevDigit][ty][tx]) {
			if(FONT[slot->curDigit][ty][tx]) {
				w = tilesize;
			}
		} else {
			if (slot->normTime == ANIMATION_NORMALIZED_MAX && FONT[slot->curDigit][ty][tx]) {
				w = tilesize;
			} else {
				if(FONT[slot->curDigit][ty][tx] || FONT[slot->prevDigit][ty][tx]) {
					offs = (skewedNormTime*TILE_SIZE/ANIMATION_NORMALIZED_MAX)+shift;
					if(FONT[slot->curDigit][ty][tx]) {
						w = offs;
						offs = 0;
					} else {
						w = tilesize-offs;
					}
				}
			}
		}
		if (w < 0) {
			w = 0;
		} else if (w > tilesize) {
			w = tilesize;
		}
		if (offs < 0) {
			offs = 0;
		} else if (offs > tilesize) {
			offs = tilesize;
		}
		if (w > 0) {
			graphics_context_set_fill_color(ctx, GColorWhite);
			graphics_fill_rect(ctx, GRect((tx*tilesize)+offs-(tx*widthadjust), ty*tilesize-(ty*widthadjust), w-widthadjust, tilesize-widthadjust), 0, GCornerNone);
		}
	}
}

unsigned short get_display_hour(unsigned short hour) {
    if (clock_is_24h_style()) {
        return hour;
    }
    unsigned short display_hour = hour % 12;
    return display_hour ? display_hour : 12;
}


void handle_tick(AppContextRef ctx, PebbleTickEvent *evt) {
	PblTm now;
	int ho, mi, da, mo, i;

    if (splashEnded) {
        if (animation_is_scheduled(&anim))
            animation_unschedule(&anim);
        
        get_time(&now);
        
        ho = get_display_hour(now.tm_hour);
        mi = now.tm_min;
        da = now.tm_mday;
        mo = now.tm_mon+1;
        
        for (i=0; i<NUMSLOTS; i++) {
            slot[i].prevDigit = slot[i].curDigit;
        }
        
        slot[0].curDigit = ho/10;
        slot[1].curDigit = ho%10;
        slot[2].curDigit = mi/10;
        slot[3].curDigit = mi%10;
		if (US_DATE) {
			slot[6].curDigit = da/10;
			slot[7].curDigit = da%10;
			slot[4].curDigit = mo/10;
			slot[5].curDigit = mo%10;
		} else {
			slot[4].curDigit = da/10;
			slot[5].curDigit = da%10;
			slot[6].curDigit = mo/10;
			slot[7].curDigit = mo%10;
		}
		
		if (NO_ZERO) {
			if (slot[0].curDigit == 0) {
				slot[0].curDigit = 10;
				if (slot[0].prevDigit == 10) {
					slot[0].curDigit++;
				}
			}
			if (slot[4].curDigit == 0) {
				slot[4].curDigit = 10;
				if (slot[4].prevDigit == 10) {
					slot[4].curDigit++;
				}
			}
			if (slot[6].curDigit == 0) {
				slot[6].curDigit = 10;
				if (slot[6].prevDigit == 10) {
					slot[6].curDigit++;
				}
			}
		}
        animation_schedule(&anim);
    }
}

void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie) {
    splashEnded = true;
    handle_tick(ctx, NULL);
}


void initSlot(int i, Layer *parent) {
	slot[i].normTime = ANIMATION_NORMALIZED_MAX;
	slot[i].prevDigit = 0;
	slot[i].curDigit = startDigit[i];
	if (i<4) {
		slot[i].divider = 1;
	} else {
		slot[i].divider = 2;
	}
	layer_init(&slot[i].layer, slotFrame(i));
	slot[i].layer.update_proc = (LayerUpdateProc)updateSlot;
	layer_add_child(parent, &slot[i].layer);
}

void animateDigits(struct Animation *anim, const uint32_t normTime) {
	int i;
	for (i=0; i<NUMSLOTS; i++) {
		if (slot[i].curDigit != slot[i].prevDigit) {
			slot[i].normTime = normTime;
			layer_mark_dirty(&slot[i].layer);
		}
	}
}

void handle_init(AppContextRef ctx) {
	Layer *rootLayer;
	int i;

	window_init(&window, "Squared");
	window_stack_push(&window, true /* Animated */);
	window_set_background_color(&window, GColorBlack);

	rootLayer = window_get_root_layer(&window);

	for (i=0; i<NUMSLOTS; i++) {
		initSlot(i, rootLayer);
	}

	animImpl.setup = NULL;
	animImpl.update = animateDigits;
	animImpl.teardown = NULL;

	animation_init(&anim);
	animation_set_delay(&anim, 0);
	animation_set_duration(&anim, DIGIT_CHANGE_ANIM_DURATION);
	animation_set_implementation(&anim, &animImpl);

	app_timer_send_event(ctx, STARTDELAY /* milliseconds */, 0);
}

void pbl_main(void *params) {
	PebbleAppHandlers handlers = {
		.init_handler = &handle_init,
		.timer_handler = &handle_timer,

		.tick_info = {
			.tick_handler = &handle_tick,
			.tick_units   = MINUTE_UNIT
		}
	};
	app_event_loop(params, &handlers);
}