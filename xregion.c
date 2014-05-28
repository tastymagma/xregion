#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <err.h>

#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>


#define FORMAT_SH "%d %d %d %d\n"
#define FORMAT_GM "%dx%d+%d+%d\n"


typedef struct {
	xcb_window_t win;
	uint16_t x;
	uint16_t y;
	uint16_t w;
	uint16_t h;
} win_region_t;


void get_win_region(xcb_connection_t *c, win_region_t *region);

static void usage(void);


int
main(int argc, char *argv[])
{
	char *fmt = FORMAT_SH;
	bool putwin, putabs = false;

	for(int i=1; i < argc; i++) {
		if (!strcmp(argv[i], "-h"))
			usage();
		if (!strcmp(argv[i], "-g"))
			fmt = FORMAT_GM;
		if (!strcmp(argv[i], "-w"))
			putwin = true;
		if (!strcmp(argv[i], "-a"))
			putabs = true;
	}

	xcb_connection_t *c;
	win_region_t reg;

	if ((c = xcb_connect (NULL, NULL)) == NULL || xcb_connection_has_error(c))
		err(EXIT_FAILURE, "Could not connect to display");

	get_win_region(c, &reg);
	
	if (putwin || reg.w < 6) {
		xcb_get_geometry_reply_t *geom = xcb_get_geometry_reply(c,
				xcb_get_geometry(c, reg.win), NULL);
		reg.x = geom->x;
		reg.y = geom->y;
		reg.w = geom->width;
		reg.h = geom->height;
	}
	if (putabs) {
		reg.w = reg.w + reg.x;
		reg.h = reg.h + reg.y;
	}

	printf(fmt, reg.x, reg.y, reg.w, reg.h);
	
	xcb_disconnect(c);
	return 0;
}

static void
usage(void)
{
	err(EXIT_FAILURE, "usage: xregion [-h] [-g] [-w] [-a]");
}

void
get_win_region(xcb_connection_t *c, win_region_t *region)
{
	xcb_screen_t *scr = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
	xcb_cursor_t cur;

	xcb_generic_error_t *error;

	/*
	 * Generate gc for selection border
	 */
	xcb_gcontext_t gc = xcb_generate_id(c);
	error = xcb_request_check(c,
			xcb_create_gc_checked(c,
				gc,
				scr->root,
				XCB_GC_FUNCTION
					| XCB_GC_PLANE_MASK
					| XCB_GC_FOREGROUND
					| XCB_GC_BACKGROUND
					| XCB_GC_SUBWINDOW_MODE,
				(uint32_t[]){XCB_GX_XOR,
					scr->black_pixel ^ scr->white_pixel,
					scr->white_pixel,
					scr->black_pixel,
					XCB_SUBWINDOW_MODE_INCLUDE_INFERIORS} // draw on top
				));
	if (error != NULL) {
		err(EXIT_FAILURE, "Could not grab button; error code: %u\n",
				error->error_code);
		free(error);
	}

	/*
	 * Load cursor by name from current X cursor theme
	 */
	xcb_cursor_context_t *ctx;
	if (xcb_cursor_context_new(c, scr, &ctx) < 0)
		err(EXIT_FAILURE, "Could not initialize xcb-cursor");
	cur = xcb_cursor_load_cursor(ctx, "crosshair");

	/*
	 * Grab pointer
	 */	
	error = xcb_request_check(c, xcb_grab_button_checked(c,
		1,												// events still get to window if true
		scr->root,
		XCB_EVENT_MASK_BUTTON_PRESS
			| XCB_EVENT_MASK_BUTTON_RELEASE
			| XCB_EVENT_MASK_BUTTON_MOTION,
		XCB_GRAB_MODE_ASYNC,
		XCB_GRAB_MODE_ASYNC,
		XCB_NONE,									// window to confine to
		cur,											// cursor change
		XCB_BUTTON_INDEX_1,
		XCB_MOD_MASK_ANY));
	if (error != NULL) {
		err(EXIT_FAILURE, "Could not grab button; error code: %u\n",
				error->error_code);
		free(error);
	}
	
	xcb_flush(c);

	/*
	 * Handle events
	 */
	xcb_generic_event_t *ev;
	xcb_button_press_event_t* ev_bp;
	xcb_motion_notify_event_t* ev_bm;

	int16_t sx, sy, x, y, w, h = 0;
	bool finished = false;

	while (!finished && (ev = xcb_wait_for_event(c))) {
		switch (ev->response_type & ~0x80) {
			case XCB_MOTION_NOTIFY:
				if (x)
					xcb_poly_rectangle(c, scr->root, gc,
							1, (xcb_rectangle_t[]){{x, y, w, h}});

				ev_bm = (xcb_motion_notify_event_t*) ev;
				if (sx > ev_bm->event_x) {
					x = ev_bm->event_x;
					w = sx - ev_bm->event_x;
				} else {
					x = sx;
					w = ev_bm->event_x - sx;
				}
				if (sy > ev_bm->event_y) {
					y = ev_bm->event_y;
					h = sy - ev_bm->event_y;
				} else {
					y = sy;
					h = ev_bm->event_y - sy;
				}
				xcb_poly_rectangle(c, scr->root, gc,
							1, (xcb_rectangle_t[]){{x, y, w, h}});
				xcb_flush(c);
			case XCB_BUTTON_PRESS:
				if (!sx) {
					ev_bp = (xcb_button_press_event_t*) ev;
					sx = ev_bp->event_x;
					sy = ev_bp->event_y;
				}
				region->win = ((xcb_button_press_event_t*) ev)->child;
				break;
			case XCB_BUTTON_RELEASE:
				xcb_poly_rectangle(c, scr->root, gc,
							1, (xcb_rectangle_t[]){{x, y, w, h}});
				xcb_flush(c);

				region->x = x;
				region->y = y;
				region->h = w;
				region->w = h;

				finished = true;
			default:
				break;
		}
		free(ev);
	}

	xcb_ungrab_button(c, XCB_BUTTON_INDEX_1, scr->root, XCB_MOD_MASK_ANY);
	xcb_cursor_context_free(ctx);
}
