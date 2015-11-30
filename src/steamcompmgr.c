/*
 * Based on xcompmgr by Keith Packard et al.
 * http://cgit.freedesktop.org/xorg/app/xcompmgr/
 * Original xcompmgr legal notices follow:
 *
 * Copyright © 2003 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */


/* Modified by Matthew Hawn. I don't know what to say here so follow what it
 *   says above. Not that I can really do anything about it
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/xf86vmode.h>

#define GL_GLEXT_PROTOTYPES
#define GLX_GLEXT_LEGACY

#include <GL/glx.h>
#include "glext.h"
#include "GL/glxext.h"

PFNGLXSWAPINTERVALEXTPROC				__pointer_to_glXSwapIntervalEXT;

void (*__pointer_to_glXBindTexImageEXT) (Display     *display, 
										 GLXDrawable drawable, 
										 int         buffer,
										 const int   *attrib_list);

void (*__pointer_to_glXReleaseTexImageEXT) (Display     *display, 
											GLXDrawable drawable, 
											int         buffer);

PFNGLGENPATHSNVPROC 					__pointer_to_glGenPathsNV;
PFNGLPATHGLYPHRANGENVPROC				__pointer_to_glPathGlyphRangeNV;
PFNGLGETPATHMETRICRANGENVPROC			__pointer_to_glGetPathMetricRangeNV;
PFNGLGETPATHSPACINGNVPROC 				__pointer_to_glGetPathSpacingNV;
PFNGLSTENCILFILLPATHINSTANCEDNVPROC 	__pointer_to_glStencilFillPathInstancedNV;
PFNGLSTENCILSTROKEPATHINSTANCEDNVPROC 	__pointer_to_glStencilStrokePathInstancedNV;
PFNGLCOVERFILLPATHINSTANCEDNVPROC 		__pointer_to_glCoverFillPathInstancedNV;
PFNGLCOVERSTROKEPATHINSTANCEDNVPROC 	__pointer_to_glCoverStrokePathInstancedNV;

typedef struct _ignore {
	struct _ignore	*next;
	unsigned long	sequence;
} ignore;

typedef struct _win {
	struct _win		*next;
	Window		id;
	Pixmap		pixmap;
	GLXPixmap	glxPixmap;
	GLXFBConfig fbConfig;
	GLuint		texName;
	XWindowAttributes	a;
	int			mode;
	int			damaged;
	Damage		damage;
	unsigned int	opacity;
	unsigned long	map_sequence;
	unsigned long	damage_sequence;
	
	Bool isSteam;
	unsigned long long int gameID;
	Bool isOverlay;
	Bool isFullscreen;
	Bool isHidden;
	Bool sizeHintsSpecified;
	unsigned int requestedWidth;
	unsigned int requestedHeight;
	Bool nudged;
	Bool ignoreOverrideRedirect;
	Bool validContents;
	
	Bool mouseMoved;
} win;

typedef struct _conv {
	int	    size;
	double  *data;
} conv;

static win		*list;
static int		scr;
static Window		root;
static Picture		rootPicture;
static Picture		rootBuffer;
static Picture		blackPicture;
static Picture		transBlackPicture;
static XserverRegion	allDamage;
static Bool		clipChanged;
static int		root_height, root_width;
static ignore		*ignore_head, **ignore_tail = &ignore_head;
static int		xfixes_event, xfixes_error;
static int		damage_event, damage_error;
static int		composite_event, composite_error;
static int		render_event, render_error;
static int		xshape_event, xshape_error;
static int		xfixes_event, xfixes_error;
static Bool		synchronize;
static int		composite_opcode;

static Window	currentFocusWindow;
static Window	currentOverlayWindow;
static Window	currentNotificationWindow;

static Window	unredirectedWindow;

static Window	ourWindow;
static XEvent	exposeEvent;

Bool			gameFocused;

unsigned int 	gamesRunningCount;

float			overscanScaleRatio = 1.0;
float			zoomScaleRatio = 1.0;
float			globalScaleRatio = 1.0f;

Bool			focusedWindowNeedsScale;
float			cursorScaleRatio;
int				cursorOffsetX, cursorOffsetY;
PointerBarrier	scaledFocusBarriers[4];
int 			cursorX, cursorY;
Bool 			cursorImageDirty = True;
int 			cursorHotX, cursorHotY;
int				cursorWidth, cursorHeight;
GLuint			cursorTextureName;

Bool			cursorVisible = True;
Bool			hideCursorForScale;
Bool			hideCursorForMovement;
unsigned int	lastCursorMovedTime;

Bool			focusDirty = False;

unsigned long	damageSequence = 0;

#define			CURSOR_HIDE_TIME 10000

Bool			gotXError = False;

win				fadeOutWindow;
Bool			fadeOutWindowGone;
unsigned int	fadeOutStartTime;

#define			FADE_OUT_DURATION 200

/* find these once and be done with it */
static Atom		steamAtom;
static Atom		gameAtom;
static Atom		overlayAtom;
static Atom		gamesRunningAtom;
static Atom		screenZoomAtom;
static Atom		screenScaleAtom;
static Atom		opacityAtom;
static Atom		winTypeAtom;
static Atom		winDesktopAtom;
static Atom		winDockAtom;
static Atom		winToolbarAtom;
static Atom		winMenuAtom;
static Atom		winUtilAtom;
static Atom		winSplashAtom;
static Atom		winDialogAtom;
static Atom		winNormalAtom;
static Atom		sizeHintsAtom;
static Atom		fullscreenAtom;
static Atom		WMStateAtom;
static Atom		WMStateHiddenAtom;

GLXContext glContext;

/* opacity property name; sometime soon I'll write up an EWMH spec for it */
#define OPACITY_PROP		"_NET_WM_WINDOW_OPACITY"
#define GAME_PROP			"STEAM_GAME"
#define STEAM_PROP			"STEAM_BIGPICTURE"
#define OVERLAY_PROP		"STEAM_OVERLAY"
#define GAMES_RUNNING_PROP 	"STEAM_GAMES_RUNNING"
#define SCREEN_SCALE_PROP	"STEAM_SCREEN_SCALE"
#define SCREEN_MAGNIFICATION_PROP	"STEAM_SCREEN_MAGNIFICATION"

#define TRANSLUCENT	0x00000000
#define OPAQUE		0xffffffff

GLuint textPathObjects;
GLfloat textYMin;
GLfloat textYMax;
GLfloat textXAdvance[256];

#define			FRAME_RATE_SAMPLING_PERIOD 160

unsigned int	frameCounter;
unsigned int	lastSampledFrameTime;
float			currentFrameRate;

static void
init_text_rendering(void)
{
	textPathObjects = __pointer_to_glGenPathsNV(256);
	
	__pointer_to_glPathGlyphRangeNV(textPathObjects,
									GL_STANDARD_FONT_NAME_NV, "Sans", GL_BOLD_BIT_NV,
								 0, 256, GL_USE_MISSING_GLYPH_NV, ~0, 30);
	
	/* Query font and glyph metrics. */
	GLfloat font_data[4];
	__pointer_to_glGetPathMetricRangeNV(GL_FONT_Y_MIN_BOUNDS_BIT_NV |
	GL_FONT_Y_MAX_BOUNDS_BIT_NV |
	GL_FONT_UNDERLINE_POSITION_BIT_NV |
	GL_FONT_UNDERLINE_THICKNESS_BIT_NV,
	textPathObjects + ' ', 1, 4 * sizeof(GLfloat),
										font_data);
	
	textYMin = font_data[0];
	textYMax = font_data[1];
	
	__pointer_to_glGetPathMetricRangeNV(GL_GLYPH_HORIZONTAL_BEARING_ADVANCE_BIT_NV,
										textPathObjects, 256,
									 0,
									 &textXAdvance[0]);
}

static XserverRegion
win_extents (Display *dpy, win *w);

static Bool		doRender = True;
static Bool		drawDebugInfo = False;
static Bool		debugEvents = False;
static Bool		allowUnredirection = False;

const int tfpAttribs[] = {
	GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
	GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGB_EXT,
	None
};

const int tfpAttribsRGBA[] = {
	GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
	GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
	None
};

static unsigned int
get_time_in_milliseconds (void)
{
	struct timeval  tv;
	
	gettimeofday (&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void
discard_ignore (Display *dpy, unsigned long sequence)
{
	while (ignore_head)
	{
		if ((long) (sequence - ignore_head->sequence) > 0)
		{
			ignore  *next = ignore_head->next;
			free (ignore_head);
			ignore_head = next;
			if (!ignore_head)
				ignore_tail = &ignore_head;
		}
		else
			break;
	}
}

static void
set_ignore (Display *dpy, unsigned long sequence)
{
	ignore  *i = malloc (sizeof (ignore));
	if (!i)
		return;
	i->sequence = sequence;
	i->next = NULL;
	*ignore_tail = i;
	ignore_tail = &i->next;
}

static int
should_ignore (Display *dpy, unsigned long sequence)
{
	discard_ignore (dpy, sequence);
	return ignore_head && ignore_head->sequence == sequence;
}

static win *
find_win (Display *dpy, Window id)
{
	win	*w;
	
	if (id == None)
	{
		return NULL;
	}
	
	for (w = list; w; w = w->next)
	{
		if (w->id == id)
		{
			return w;
		}
	}
	// Didn't find, must be a children somewhere; try again with parent.
	Window root = None;
	Window parent = None;
	Window *children = NULL;
	unsigned int childrenCount;
	set_ignore (dpy, NextRequest (dpy));
	XQueryTree(dpy, id, &root, &parent, &children, &childrenCount);
	if (children)
		XFree(children);
	
	if (root == parent || parent == None)
	{
		return NULL;
	}
	
	return find_win(dpy, parent);
}

static void
set_win_hidden (Display *dpy, win *w, Bool hidden)
{
	if (!w || w->id == None)
	{
		return;
	}
	
	if (w->isHidden == hidden)
	{
		return;
	}
	
	
	if (hidden == True)
	{
		XChangeProperty(dpy, w->id, WMStateAtom, XA_ATOM, 32,
						PropModeReplace, (unsigned char *)&WMStateHiddenAtom, 1);
	}
	else
	{
		XChangeProperty(dpy, w->id, WMStateAtom, XA_ATOM, 32,
						PropModeReplace, (unsigned char *)NULL, 0);
	}
	
	w->isHidden = hidden;
}

static XserverRegion
win_extents (Display *dpy, win *w)
{
	XRectangle	    r;
	
	r.x = w->a.x;
	r.y = w->a.y;
	r.width = w->a.width + w->a.border_width * 2;
	r.height = w->a.height + w->a.border_width * 2;
	
	return XFixesCreateRegion (dpy, &r, 1);
}

GLXFBConfig win_fbconfig(Display *display, Window id)
{
	XWindowAttributes attrib;
	VisualID visualid;
	GLXFBConfig *fbconfigs;
	int nfbconfigs, i, value;
	XVisualInfo *visinfo;
	
	attrib.visual = 0x0;
	XGetWindowAttributes (display, id, &attrib);
	
	if (!attrib.visual)
		return None;
	
	visualid = XVisualIDFromVisual (attrib.visual);
	
	fbconfigs = glXGetFBConfigs (display, scr, &nfbconfigs);
	for (i = 0; i < nfbconfigs; i++)
	{
		visinfo = glXGetVisualFromFBConfig (display, fbconfigs[i]);
		if (!visinfo || visinfo->visualid != visualid)
			continue;
		
		glXGetFBConfigAttrib (display, fbconfigs[i], GLX_DRAWABLE_TYPE, &value);
		if (!(value & GLX_PIXMAP_BIT))
			continue;
		
		glXGetFBConfigAttrib (display, fbconfigs[i],
							  GLX_BIND_TO_TEXTURE_TARGETS_EXT,
						&value);
		if (!(value & GLX_TEXTURE_2D_BIT_EXT))
			continue;
		
		glXGetFBConfigAttrib (display, fbconfigs[i],
							  GLX_BIND_TO_TEXTURE_RGBA_EXT,
						&value);
		if (value == False)
		{
			glXGetFBConfigAttrib (display, fbconfigs[i],
								  GLX_BIND_TO_TEXTURE_RGB_EXT,
						 &value);
			if (value == False)
				continue;
		}
		
		glXGetFBConfigAttrib(display, fbconfigs[i],
							 GLX_SAMPLE_BUFFERS,
					   &value);
		if (value)
			continue;
		
		// 		glXGetFBConfigAttrib (display, fbconfigs[i],
		// 							  GLX_Y_INVERTED_EXT,
		// 						&value);
		// 		if (value == True)
		// 		{
		// 			top = 0.0f;
		// 			bottom = 1.0f;
		// 		}
		// 		else
		// 		{
		// 			top = 1.0f;
		// 			bottom = 0.0f;
		// 		}
		
		break;
	}
	
	if (i == nfbconfigs)
	{
		fprintf (stderr, "Could not get fbconfig from window\n");
		return None;
	}
	
	return fbconfigs[i];
}

static void
teardown_win_resources (Display *dpy, win *w)
{
	if (!w)
		return;
	
	if (w->pixmap)
	{
		glBindTexture (GL_TEXTURE_2D, w->texName);
		__pointer_to_glXReleaseTexImageEXT (dpy, w->glxPixmap, GLX_FRONT_LEFT_EXT);
		glBindTexture (GL_TEXTURE_2D, 0);
		w->texName = 0;
		glXDestroyPixmap(dpy, w->glxPixmap);
		w->glxPixmap = None;
		
		XFreePixmap(dpy, w->pixmap);
		w->pixmap = None;
	}
	
	w->damaged = 0;
	w->validContents = False;
}

static void
ensure_win_resources (Display *dpy, win *w)
{
	if (!w || !w->fbConfig)
		return;
	
	if (!w->pixmap)
	{
		w->pixmap = XCompositeNameWindowPixmap (dpy, w->id);
		w->glxPixmap = glXCreatePixmap (dpy, w->fbConfig, w->pixmap, w->isOverlay ? tfpAttribsRGBA : tfpAttribs);
		
		glBindTexture (GL_TEXTURE_2D, w->texName);
		__pointer_to_glXBindTexImageEXT (dpy, w->glxPixmap, GLX_FRONT_LEFT_EXT, NULL);
		
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
}

static void
apply_cursor_state (Display *dpy)
{
	Bool newCursorVisibility = True;
	
	if (hideCursorForScale || hideCursorForMovement)
		newCursorVisibility = False;
	
	if (newCursorVisibility != cursorVisible)
	{
		cursorVisible = newCursorVisibility;
		
		if (cursorVisible)
			XFixesShowCursor(dpy, DefaultRootWindow(dpy));
		else
			XFixesHideCursor(dpy, DefaultRootWindow(dpy));
	}
}

static void
handle_mouse_movement(Display *dpy, int posX, int posY)
{
	// Some stuff likes to warp in-place
	if (cursorX == posX && cursorY == posY)
		return;
	
	cursorX = posX;
	cursorY = posY;
	
	win *w = find_win(dpy, currentFocusWindow);
	
	if (w && focusedWindowNeedsScale && gameFocused)
	{
		w->damaged = 1;
	}
	
	// Ignore the first events as it's likely to be non-user-initiated warps
	// Account for one warp from us, one warp from the app and one warp from
	// the toolkit.
	if (w && (w->mouseMoved++ < 3))
		return;
	
	lastCursorMovedTime = get_time_in_milliseconds();
	
	hideCursorForMovement = False;
	apply_cursor_state(dpy);
}

static void
paint_fake_cursor (Display *dpy, win *w)
{
	float scaledCursorX, scaledCursorY;
	
	Window window_returned, child;
	int root_x, root_y;
	int win_x, win_y;
	unsigned int mask_return;
	
	XQueryPointer(dpy, DefaultRootWindow(dpy), &window_returned,
				  &child, &root_x, &root_y, &win_x, &win_y,
			   &mask_return);
	
	handle_mouse_movement( dpy, root_x, root_y );
	
	// Also need new texture
	if (cursorImageDirty)
	{
		XFixesCursorImage* im = XFixesGetCursorImage(dpy);
		
		if (!im)
			return;
		
		cursorHotX = im->xhot;
		cursorHotY = im->yhot;
		
		cursorWidth = im->width;
		cursorHeight = im->height;
		
		unsigned int cursorDataBuffer[cursorWidth * cursorHeight];
		for (int i = 0; i < cursorWidth * cursorHeight; i++)
			cursorDataBuffer[i] = im->pixels[i];
		
		glBindTexture(GL_TEXTURE_2D, cursorTextureName);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cursorWidth, cursorHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, cursorDataBuffer);
		
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
		XFree(im);
		
		cursorImageDirty = False;
	}
	
	// Actual point on scaled screen where the cursor hotspot should be
	scaledCursorX = (win_x - w->a.x) * cursorScaleRatio * globalScaleRatio + cursorOffsetX;
	scaledCursorY = (win_y - w->a.y) * cursorScaleRatio * globalScaleRatio + cursorOffsetY;
	
	if ( zoomScaleRatio != 1.0 )
	{
		scaledCursorX += ((w->a.width / 2) - win_x) * cursorScaleRatio * globalScaleRatio;
		scaledCursorY += ((w->a.height / 2) - win_y) * cursorScaleRatio * globalScaleRatio;
	}
	
	glEnable(GL_BLEND);
	glBindTexture(GL_TEXTURE_2D, cursorTextureName);
	glEnable(GL_TEXTURE_2D);
	
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	win *mainOverlayWindow = find_win(dpy, currentOverlayWindow);
	
	float displayCursorScaleRatio = 1.0f;
	
	// Ensure the cursor looks the same size as in Steam or the overlay
	if (mainOverlayWindow)
	{
		// The first scale we need to apply is the Steam/overlay scale, if it exists
		float steamScaleX = (float)root_width / mainOverlayWindow->a.width;
		float steamScaleY = (float)root_height / mainOverlayWindow->a.height;
		
		float steamRatio = (steamScaleX < steamScaleY) ? steamScaleX : steamScaleY;
		
		displayCursorScaleRatio *= steamRatio;
		
		// Then any global scale, since it would also apply to the Steam window and its SW cursor
		displayCursorScaleRatio *= globalScaleRatio;
	}
	
	// Apply the cursor offset inside the texture using the display scale
	scaledCursorX = scaledCursorX - (cursorHotX * displayCursorScaleRatio);
	scaledCursorY = scaledCursorY - (cursorHotY * displayCursorScaleRatio);
	
	float displayCursorWidth = cursorWidth * displayCursorScaleRatio;
	float displayCursorHeight = cursorHeight * displayCursorScaleRatio;
	
	glColor3f(1.0f, 1.0f, 1.0f);
	
	glBegin (GL_QUADS);
	glTexCoord2d (0.0f, 0.0f);
	glVertex2d (scaledCursorX, scaledCursorY);
	glTexCoord2d (1.0f, 0.0f);
	glVertex2d (scaledCursorX + displayCursorWidth, scaledCursorY);
	glTexCoord2d (1.0f, 1.0f);
	glVertex2d (scaledCursorX + displayCursorWidth, scaledCursorY + displayCursorHeight);
	glTexCoord2d (0.0f, 1.0f);
	glVertex2d (scaledCursorX, scaledCursorY + displayCursorHeight);
	glEnd ();
}

static void
paint_window (Display *dpy, win *w, Bool doBlend, Bool notificationMode)
{
	int sourceWidth, sourceHeight;
	int drawXOffset = 0, drawYOffset = 0;
	Bool isScaling = False;
	float currentScaleRatio = 1.0;
	
	if (!w)
		return;
	
	if (w->isOverlay && !w->validContents)
		return;
	
	win *mainOverlayWindow = find_win(dpy, currentOverlayWindow);
	
	if (notificationMode && !mainOverlayWindow)
		return;
	
	if (notificationMode)
	{
		sourceWidth = mainOverlayWindow->a.width;
		sourceHeight = mainOverlayWindow->a.height;
	}
	else
	{
		sourceWidth = w->a.width;
		sourceHeight = w->a.height;
	}
	
	glBindTexture (GL_TEXTURE_2D, w->texName);
	glEnable(GL_TEXTURE_2D);
	
	if (sourceWidth != root_width || sourceHeight != root_height || globalScaleRatio != 1.0f)
	{
		float XRatio = (float)root_width / sourceWidth;
		float YRatio = (float)root_height / sourceHeight;
		
		currentScaleRatio = (XRatio < YRatio) ? XRatio : YRatio;
		currentScaleRatio *= globalScaleRatio;
		
		drawXOffset = (root_width - sourceWidth * currentScaleRatio) / 2.0f;
		drawYOffset = (root_height - sourceHeight * currentScaleRatio) / 2.0f;
		
		if ( zoomScaleRatio != 1.0 )
		{
			drawXOffset += ((sourceWidth / 2) - cursorX) * currentScaleRatio;
			drawYOffset += ((sourceHeight / 2) - cursorY) * currentScaleRatio;
		}
		
		isScaling = True;
	}
	
	if (doBlend)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);
	
	// If scaling and blending, we need to draw our letterbox black border with
	// the right opacity instead of relying on the clear color
	if (isScaling && doBlend && !notificationMode)
	{
		glDisable(GL_TEXTURE_2D);
		glColor4f(0.0f, 0.0f, 0.0f, (float)w->opacity / OPAQUE);
		
		glBegin (GL_QUADS);
		
		// We can't overdraw because we're blending
		
		// Top and bottom stripes, including sides
		if (drawYOffset)
		{
			glVertex2d (0.0f, 0.0f);
			glVertex2d (root_width, 0.0f);
			glVertex2d (root_width, drawYOffset);
			glVertex2d (0.0f, drawYOffset);
			
			glVertex2d (0.0f, root_height - drawYOffset);
			glVertex2d (root_width, root_height - drawYOffset);
			glVertex2d (root_width, root_height);
			glVertex2d (0.0f, root_height);
		}
		
		// Side stripes, excluding any top and bottom areas
		if (drawXOffset)
		{
			glVertex2d (0.0f, drawYOffset);
			glVertex2d (drawXOffset, drawYOffset);
			glVertex2d (drawXOffset, root_height - drawYOffset);
			glVertex2d (0.0f, root_height - drawYOffset);
			
			glVertex2d (root_width - drawXOffset, drawYOffset);
			glVertex2d (root_width, drawYOffset);
			glVertex2d (root_width, root_height - drawYOffset);
			glVertex2d (root_width - drawXOffset, root_height - drawYOffset);
		}
		
		glEnd ();
	}
	
	glEnable(GL_TEXTURE_2D);
	
	if (w->isOverlay)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
	else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
	
	glColor4f(1.0f, 1.0f, 1.0f, (float)w->opacity / OPAQUE);
	
	int originX, originY, width, height;
	
	if (notificationMode)
	{
		int xOffset = 0, yOffset = 0;
		
		width = w->a.width * currentScaleRatio;
		height = w->a.height * currentScaleRatio;
		
		if (globalScaleRatio != 1.0f)
		{
			xOffset = (root_width - root_width * globalScaleRatio) / 2.0;
			yOffset = (root_height - root_height * globalScaleRatio) / 2.0;
		}
		
		originX = root_width - xOffset - width;
		originY = root_height - yOffset - height;
	}
	else
	{
		originX = drawXOffset;
		originY = drawYOffset;
		
		width = sourceWidth * currentScaleRatio;
		height = sourceHeight * currentScaleRatio;
	}
	
	glBegin (GL_QUADS);
	glTexCoord2d (0.0f, 0.0f);
	glVertex2d (originX, originY);
	glTexCoord2d (1.0f, 0.0f);
	glVertex2d (originX + width, originY);
	glTexCoord2d (1.0f, 1.0f);
	glVertex2d (originX + width, originY + height);
	glTexCoord2d (0.0f, 1.0f);
	glVertex2d (originX, originY + height);
	glEnd ();
}

static void
paint_message (const char *message, int Y, float r, float g, float b)
{
	int messageLength = strlen(message);
	GLfloat horizontalOffsets[messageLength + 1];
	
	horizontalOffsets[0] = 0;
	
	__pointer_to_glGetPathSpacingNV(GL_ACCUM_ADJACENT_PAIRS_NV,
									(GLsizei)messageLength, GL_UNSIGNED_BYTE, message,
									textPathObjects,
								 1.0, 1.0,
								 GL_TRANSLATE_X_NV,
								 &horizontalOffsets[1]);
	
	float messageWidth = horizontalOffsets[messageLength - 1] + textXAdvance[message[messageLength - 1]];
	
	glPushMatrix();
	
	glTranslatef(root_width - messageWidth - 100, Y + textYMax - textYMin, 0.0);
	glScalef(1.0, -1.0, 1.0);
	
	glEnable(GL_STENCIL_TEST);
	
	glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
	
	__pointer_to_glStencilFillPathInstancedNV(	(GLsizei)messageLength,
												GL_UNSIGNED_BYTE, message, textPathObjects,
											GL_PATH_FILL_MODE_NV, ~0, /* Use all stencil bits */
											GL_TRANSLATE_X_NV, &horizontalOffsets[0]);
	
	glColor3f(r,g,b);
	__pointer_to_glCoverFillPathInstancedNV((GLsizei)messageLength,
											GL_UNSIGNED_BYTE, message, textPathObjects,
										 GL_PATH_FILL_COVER_MODE_NV,
										 GL_TRANSLATE_X_NV, &horizontalOffsets[0]);
	
	__pointer_to_glStencilStrokePathInstancedNV((GLsizei)messageLength,
												GL_UNSIGNED_BYTE, message, textPathObjects,
											 1, ~0, /* Use all stencil bits */
											 GL_TRANSLATE_X_NV, &horizontalOffsets[0]);
	glColor3f(0.0,0.0,0.0);
	__pointer_to_glCoverStrokePathInstancedNV((GLsizei)messageLength,
											  GL_UNSIGNED_BYTE, message, textPathObjects,
										   GL_PATH_STROKE_COVER_MODE_NV,
										   GL_TRANSLATE_X_NV, &horizontalOffsets[0]);
	
	glDisable(GL_STENCIL_TEST);
	
	glPopMatrix();
}

static void
paint_debug_info (Display *dpy)
{
	int Y = 100;
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	char messageBuffer[256];
	
	sprintf(messageBuffer, "Compositing at %.1f FPS", currentFrameRate);
	
	paint_message(messageBuffer, Y, 1.0f, 1.0f, 1.0f); Y += textYMax;
	if (find_win(dpy, currentFocusWindow))
	{
		if (gameFocused)
		{
			sprintf(messageBuffer, "Presenting game window %x", (unsigned int)currentFocusWindow);
			paint_message(messageBuffer, Y, 0.0f, 1.0f, 0.0f); Y += textYMax;
		}
		else
		{
			// must be Steam
			paint_message("Presenting Steam", Y, 1.0f, 1.0f, 0.0f); Y += textYMax;
		}
	}
	
	win *overlay = find_win(dpy, currentOverlayWindow);
	win *notification = find_win(dpy, currentNotificationWindow);
	
	if (overlay && gamesRunningCount && overlay->opacity)
	{
		sprintf(messageBuffer, "Compositing overlay at opacity %f", overlay->opacity / (float)OPAQUE);
		paint_message(messageBuffer, Y, 1.0f, 0.0f, 1.0f); Y += textYMax;
	}
	
	if (notification && gamesRunningCount && notification->opacity)
	{
		sprintf(messageBuffer, "Compositing notification at opacity %f", notification->opacity / (float)OPAQUE);
		paint_message(messageBuffer, Y, 1.0f, 0.0f, 1.0f); Y += textYMax;
	}
	
	if (focusedWindowNeedsScale) {
		paint_message("Scaling current window", Y, 0.0f, 0.0f, 1.0f); Y += textYMax;
	}
	
	if (gotXError) {
		paint_message("Encountered X11 error", Y, 1.0f, 0.0f, 0.0f); Y += textYMax;
	}
}

static void
paint_all (Display *dpy)
{
	win	*w;
	win	*overlay;
	win	*notification;
	win	*t = NULL;
	
	Bool canUnredirect = True;
	Bool overlayDamaged = False;
	
	if (unredirectedWindow)
		return;
	
	unsigned int currentTime = get_time_in_milliseconds();
	Bool fadingOut = ((currentTime - fadeOutStartTime) < FADE_OUT_DURATION && fadeOutWindow.id != None);
	
	w = find_win(dpy, currentFocusWindow);
	overlay = find_win(dpy, currentOverlayWindow);
	notification = find_win(dpy, currentNotificationWindow);
	
	if (gamesRunningCount)
	{
		if (overlay && overlay->damaged)
			overlayDamaged = True;
		if (notification && notification->damaged)
			overlayDamaged = True;
	}
	
	if (!w)
		return;
	
	// Don't pump new frames if no animation on the focus window, unless we're fading
	if (!w->damaged && !overlayDamaged && !fadeOutWindow.id)
		return;
	
	
	frameCounter++;
	
	if (frameCounter == 5)
	{
		currentFrameRate = 5 * 1000.0f / (currentTime - lastSampledFrameTime);
		lastSampledFrameTime = currentTime;
		frameCounter = 0;
	}
	
	w->damaged = 0;
	
	ensure_win_resources(dpy, w);
	ensure_win_resources(dpy, overlay);
	ensure_win_resources(dpy, notification);
	
	glViewport(0, 0, root_width, root_height);
	glLoadIdentity();
	glOrtho(0.0f, root_width, root_height, 0.0f, -1.0f, 1.0f);
	
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	// Fading out from previous window?
	if (fadingOut)
	{
		double newOpacity = ((currentTime - fadeOutStartTime) / (double)FADE_OUT_DURATION);
		
		// Draw it in the background
		fadeOutWindow.opacity = (1.0d - newOpacity) * OPAQUE;
		paint_window(dpy, &fadeOutWindow, True, False);
		
		w = find_win(dpy, currentFocusWindow);
		ensure_win_resources(dpy, w);
		
		// Blend new window on top with linear crossfade
		w->opacity = newOpacity * OPAQUE;
		
		paint_window(dpy, w, True, False);
		
		canUnredirect = False;
	}
	else
	{
		w = find_win(dpy, currentFocusWindow);
		ensure_win_resources(dpy, w);
		// Just draw focused window as normal, be it Steam or the game
		paint_window(dpy, w, False, False);
		
		if (focusedWindowNeedsScale)
		{
			canUnredirect = False;
		}
		
		if (fadeOutWindow.id) {
			
			if (fadeOutWindowGone)
			{
				// This is the only reference to these resources now.
				teardown_win_resources(dpy, &fadeOutWindow);
				fadeOutWindowGone = False;
			}
			fadeOutWindow.id = None;
			
			// Finished fading out, mark previous window hidden
			set_win_hidden(dpy, &fadeOutWindow, True);
		}
	}
	
	if (gamesRunningCount && overlay)
	{
		if (overlay->opacity)
		{
			paint_window(dpy, overlay, True, False);
			canUnredirect = False;
		}
		overlay->damaged = 0;
	}
	
	if (gamesRunningCount && notification)
	{
		if (notification->opacity)
		{
			paint_window(dpy, notification, True, True);
			canUnredirect = False;
		}
		notification->damaged = 0;
	}
	
	// Draw SW cursor if we need to
	if (w && focusedWindowNeedsScale && gameFocused)
	{
		if (!hideCursorForMovement)
			paint_fake_cursor(dpy, w);
		canUnredirect = False;
	}
	
	if (drawDebugInfo)
		paint_debug_info(dpy);
	
	glXSwapBuffers(dpy, root);
	
	if (glGetError() != GL_NO_ERROR)
	{
		fprintf (stderr, "GL error!\n");
		exit (1);
	}
	
	// Enable when hitching and blinking issues are resolved
	if (allowUnredirection && canUnredirect)
	{
		unredirectedWindow = currentFocusWindow;
		teardown_win_resources(dpy, w);
		XCompositeUnredirectWindow(dpy, unredirectedWindow, CompositeRedirectManual);
	}
}

static void
setup_pointer_barriers (Display *dpy)
{
	int i;
	win		    *w = find_win (dpy, currentFocusWindow);
	
	// If we had barriers before, get rid of them.
	for (i = 0; i < 4; i++)
	{
		if (scaledFocusBarriers[i] != None)
		{
			XFixesDestroyPointerBarrier(dpy, scaledFocusBarriers[i]);
			scaledFocusBarriers[i] = None;
		}
	}
	
	if (focusedWindowNeedsScale == False && gameFocused)
	{
		hideCursorForScale = False;
		apply_cursor_state(dpy);
		return;
	}
	
	// If we're scaling, take ownership of the cursor
	if (doRender)
	{
		hideCursorForScale = True;
		apply_cursor_state(dpy);
	}
	
	if (!gameFocused)
	{
		return;
	}
	
	// Constrain it to the window; careful, the corners will leak due to a known X server bug
	scaledFocusBarriers[0] = XFixesCreatePointerBarrier(dpy, DefaultRootWindow(dpy), 0, w->a.y, root_width, w->a.y, 0, 0, NULL);
	scaledFocusBarriers[1] = XFixesCreatePointerBarrier(dpy, DefaultRootWindow(dpy), w->a.x + w->a.width, 0, w->a.x + w->a.width, root_height, 0, 0, NULL);
	scaledFocusBarriers[2] = XFixesCreatePointerBarrier(dpy, DefaultRootWindow(dpy), root_width, w->a.y + w->a.height, 0, w->a.y + w->a.height, 0, 0, NULL);
	scaledFocusBarriers[3] = XFixesCreatePointerBarrier(dpy, DefaultRootWindow(dpy), w->a.x, root_height, w->a.x, 0, 0, 0, NULL);
	
	// Make sure the cursor is somewhere in our jail
	Window window_returned, child;
	int root_x, root_y;
	int win_x, win_y;
	unsigned int mask_return;
	
	XQueryPointer(dpy, DefaultRootWindow(dpy), &window_returned,
				  &child, &root_x, &root_y, &win_x, &win_y,
			   &mask_return);
	
	if (root_x >= w->a.width || root_y >= w->a.height)
	{
		XWarpPointer(dpy, None, currentFocusWindow, 0, 0, 0, 0, w->a.width / 2, w->a.height / 2);
	}
}

static void
determine_and_apply_focus (Display *dpy)
{
	win *w, *focus = NULL;
	
	gameFocused = False;
	
	unsigned long maxDamageSequence = 0;
	Bool usingOverrideRedirectWindow = False;
	
	unsigned int maxOpacity = 0;
	
	if (unredirectedWindow != None)
	{
		XCompositeRedirectWindow(dpy, unredirectedWindow, CompositeRedirectManual);
		ensure_win_resources(dpy, find_win(dpy, unredirectedWindow));
		unredirectedWindow = None;
	}
	
	for (w = list; w; w = w->next)
	{
		if (w->isSteam && !gameFocused)
		{
			focus = w;
		}
		
		// We allow using an override redirect window in some cases, but if we have
		// a choice between two windows we always prefer the non-override redirect one.
		Bool windowIsOverrideRedirect = w->a.override_redirect && !w->ignoreOverrideRedirect;
		
		if (w->gameID && w->a.map_state == IsViewable && w->a.class == InputOutput &&
			(w->damage_sequence >= maxDamageSequence) &&
			(!windowIsOverrideRedirect || !usingOverrideRedirectWindow))
		{
			focus = w;
			gameFocused = True;
			maxDamageSequence = w->damage_sequence;
			
			if (windowIsOverrideRedirect)
			{
				usingOverrideRedirectWindow = True;
			}
		}
		
		if (w->isOverlay)
		{
			if (w->a.width == 1920 && w->opacity >= maxOpacity)
			{
				currentOverlayWindow = w->id;
				maxOpacity = w->opacity;
			}
			else
			{
				currentNotificationWindow = w->id;
			}
		}
	}
	
	if (!focus)
	{
		currentFocusWindow = None;
		focusedWindowNeedsScale = False;
		return;
	}
	
	if (fadeOutWindow.id == None && currentFocusWindow != focus->id)
	{
		// Initiate fade out if switching focus
		w = find_win(dpy, currentFocusWindow);
		
		if (w)
		{
			ensure_win_resources(dpy, w);
			fadeOutWindow = *w;
			fadeOutStartTime = get_time_in_milliseconds();
		}
	}
	
	if (fadeOutWindow.id && currentFocusWindow != focus->id)
	{
		set_win_hidden(dpy, find_win(dpy, currentFocusWindow), True);
	}
	
	currentFocusWindow = focus->id;
	w = focus;
	
	set_win_hidden(dpy, w, False);
	
	if (w->a.width != root_width || w->a.height != root_height || globalScaleRatio != 1.0f)
	{
		float XRatio = (float)root_width / w->a.width;
		float YRatio = (float)root_height / w->a.height;
		
		focusedWindowNeedsScale = True;
		cursorScaleRatio = (XRatio < YRatio) ? XRatio : YRatio;
		
		cursorOffsetX = (root_width - w->a.width * cursorScaleRatio * globalScaleRatio) / 2.0f;
		cursorOffsetY = (root_height - w->a.height * cursorScaleRatio * globalScaleRatio) / 2.0f;
	}
	else
		focusedWindowNeedsScale = False;
	
	setup_pointer_barriers(dpy);
	
	if (gameFocused || !gamesRunningCount && list[0].id != focus->id)
	{
		XRaiseWindow(dpy, focus->id);
	}
	
	XSetInputFocus(dpy, focus->id, RevertToNone, CurrentTime);
	
	if (!focus->nudged)
	{
		XMoveWindow(dpy, focus->id, 1, 1);
		focus->nudged = True;
	}
	
	if (w->a.x != 0 || w->a.y != 0)
		XMoveWindow(dpy, focus->id, 0, 0);
	
	if (focus->isFullscreen && focusedWindowNeedsScale)
	{
		XResizeWindow(dpy, focus->id, root_width, root_height);
	}
	else if (!focus->isFullscreen && focus->sizeHintsSpecified &&
		(focus->a.width != focus->requestedWidth ||
		focus->a.height != focus->requestedHeight))
	{
		XResizeWindow(dpy, focus->id, focus->requestedWidth, focus->requestedHeight);
	}
	
	Window	    root_return = None, parent_return = None;
	Window	    *children = NULL;
	unsigned int    nchildren = 0;
	unsigned int    i = 0;
	
	XQueryTree (dpy, w->id, &root_return, &parent_return, &children, &nchildren);
	
	while (i < nchildren)
	{
		XSelectInput(dpy, children[i], PointerMotionMask);
		i++;
	}
	
	XFree (children);
}

/* Get prop from window
 *   not found: default
 *   otherwise the value
 */
static unsigned int
get_prop(Display *dpy, Window win, Atom prop, unsigned int def)
{
	Atom actual;
	int format;
	unsigned long n, left;
	
	unsigned char *data;
	int result = XGetWindowProperty(dpy, win, prop, 0L, 1L, False,
									XA_CARDINAL, &actual, &format,
								 &n, &left, &data);
	if (result == Success && data != NULL)
	{
		unsigned int i;
		memcpy (&i, data, sizeof (unsigned int));
		XFree( (void *) data);
		return i;
	}
	return def;
}

static void
get_size_hints(Display *dpy, win *w)
{
	XSizeHints hints;
	long hintsSpecified;
	
	XGetWMNormalHints(dpy, w->id, &hints, &hintsSpecified);
	
	if (hintsSpecified & (PMaxSize | PMinSize) &&
		hints.max_width * hints.max_height * hints.min_width * hints.min_height &&
		hints.max_width == hints.min_width && hints.min_height == hints.max_height)
	{
		w->requestedWidth = hints.max_width;
		w->requestedHeight = hints.max_height;
		
		w->sizeHintsSpecified = True;
	}
	else
	{
		w->sizeHintsSpecified = False;
		
		// Below block checks for a pattern that matches old SDL fullscreen applications;
		// SDL creates a fullscreen overrride-redirect window and reparents the game
		// window under it, centered. We get rid of the modeswitch and also want that
		// black border gone.
		if (w->a.override_redirect)
		{
			Window	    root_return = None, parent_return = None;
			Window	    *children = NULL;
			unsigned int    nchildren = 0;
			
			XQueryTree (dpy, w->id, &root_return, &parent_return, &children, &nchildren);
			
			if (nchildren == 1)
			{
				XWindowAttributes attribs;
				
				XGetWindowAttributes (dpy, children[0], &attribs);
				
				// If we have a unique children that isn't override-reidrect that is
				// contained inside this fullscreen window, it's probably it.
				if (attribs.override_redirect == False &&
					attribs.width <= w->a.width &&
					attribs.height <= w->a.height)
				{
					w->sizeHintsSpecified = True;
					
					w->requestedWidth = attribs.width;
					w->requestedHeight = attribs.height;
					
					XMoveWindow(dpy, children[0], 0, 0);
					
					w->ignoreOverrideRedirect = True;
				}
			}
			
			XFree (children);
		}
	}
}

static void
map_win (Display *dpy, Window id, unsigned long sequence)
{
	win		*w = find_win (dpy, id);
	
	if (!w)
		return;
	
	w->a.map_state = IsViewable;
	
	/* This needs to be here or else we lose transparency messages */
	XSelectInput (dpy, id, PropertyChangeMask | SubstructureNotifyMask |
	PointerMotionMask | LeaveWindowMask);
	
	/* This needs to be here since we don't get PropertyNotify when unmapped */
	w->opacity = get_prop (dpy, w->id, opacityAtom, TRANSLUCENT);
	
	w->isSteam = get_prop (dpy, w->id, steamAtom, 0);
	w->gameID = get_prop (dpy, w->id, gameAtom, 0);
	w->isOverlay = get_prop (dpy, w->id, overlayAtom, 0);
	
	get_size_hints(dpy, w);
	
	w->damaged = 0;
	w->damage_sequence = 0;
	w->map_sequence = sequence;
	
	w->validContents = False;
	
	focusDirty = True;
}

static void
finish_unmap_win (Display *dpy, win *w)
{
	w->damaged = 0;
	w->validContents = False;
	
	if (w->pixmap && fadeOutWindow.id != w->id)
	{
		teardown_win_resources(dpy, w);
	}
	
	if (fadeOutWindow.id == w->id)
	{
		fadeOutWindowGone = True;
	}
	
	/* don't care about properties anymore */
	set_ignore (dpy, NextRequest (dpy));
	XSelectInput(dpy, w->id, 0);
	
	clipChanged = True;
}

static void
unmap_win (Display *dpy, Window id, Bool fade)
{
	win *w = find_win (dpy, id);
	if (!w)
		return;
	w->a.map_state = IsUnmapped;
	
	focusDirty = True;
	
	finish_unmap_win (dpy, w);
}

static void
add_win (Display *dpy, Window id, Window prev, unsigned long sequence)
{
	win				*new = malloc (sizeof (win));
	win				**p;
	
	if (!new)
		return;
	if (prev)
	{
		for (p = &list; *p; p = &(*p)->next)
			if ((*p)->id == prev)
				break;
	}
	else
		p = &list;
	new->id = id;
	set_ignore (dpy, NextRequest (dpy));
	if (!XGetWindowAttributes (dpy, id, &new->a))
	{
		free (new);
		return;
	}
	new->damaged = 0;
	new->validContents = False;
	new->pixmap = None;
	new->fbConfig = win_fbconfig(dpy, new->id);
	if (new->fbConfig == None)
	{
		// XXX figure out why Thomas was Alone window doesn't work when using its
		// visual but works with that fallback to the root window visual; is it
		// because it has several samples?
		new->fbConfig = win_fbconfig(dpy, root);
	}
	glGenTextures (1, &new->texName);
	new->damage_sequence = 0;
	new->map_sequence = 0;
	if (new->a.class == InputOnly)
		new->damage = None;
	else
	{
		new->damage = XDamageCreate (dpy, id, XDamageReportRawRectangles);
		// Make sure the Windows we present have background = None for seamless unredirection
		if (allowUnredirection)
			XSetWindowBackgroundPixmap (dpy, id, None);
	}
	new->opacity = TRANSLUCENT;
	
	new->isOverlay = False;
	new->isSteam = False;
	new->gameID = 0;
	new->isFullscreen = False;
	new->isHidden = False;
	new->sizeHintsSpecified = False;
	new->requestedWidth = 0;
	new->requestedHeight = 0;
	new->nudged = False;
	new->ignoreOverrideRedirect = False;
	
	new->mouseMoved = False;
	
	new->next = *p;
	*p = new;
	if (new->a.map_state == IsViewable)
		map_win (dpy, id, sequence);
	
	focusDirty = True;
}

static void
restack_win (Display *dpy, win *w, Window new_above)
{
	Window  old_above;
	
	if (w->next)
		old_above = w->next->id;
	else
		old_above = None;
	if (old_above != new_above)
	{
		win **prev;
		
		/* unhook */
		for (prev = &list; *prev; prev = &(*prev)->next)
			if ((*prev) == w)
				break;
			*prev = w->next;
		
		/* rehook */
		for (prev = &list; *prev; prev = &(*prev)->next)
		{
			if ((*prev)->id == new_above)
				break;
		}
		w->next = *prev;
		*prev = w;
		
		focusDirty = True;
	}
}

static void
configure_win (Display *dpy, XConfigureEvent *ce)
{
	win		    *w = find_win (dpy, ce->window);
	XserverRegion   damage = None;
	
	if (!w || w->id != ce->window)
	{
		if (ce->window == root)
		{
			root_width = ce->width;
			root_height = ce->height;
		}
		return;
	}
	
	w->a.x = ce->x;
	w->a.y = ce->y;
	if (w->a.width != ce->width || w->a.height != ce->height)
	{
		if (w->pixmap)
		{
			XFreePixmap (dpy, w->pixmap);
			w->pixmap = None;
		}
	}
	w->a.width = ce->width;
	w->a.height = ce->height;
	w->a.border_width = ce->border_width;
	w->a.override_redirect = ce->override_redirect;
	restack_win (dpy, w, ce->above);
	
	focusDirty = True;
}

static void
circulate_win (Display *dpy, XCirculateEvent *ce)
{
	win	    *w = find_win (dpy, ce->window);
	Window  new_above;
	
	if (!w || w->id != ce->window)
		return;
	
	if (ce->place == PlaceOnTop)
		new_above = list->id;
	else
		new_above = None;
	restack_win (dpy, w, new_above);
	clipChanged = True;
}

static void
finish_destroy_win (Display *dpy, Window id, Bool gone)
{
	win	**prev, *w;
	
	for (prev = &list; (w = *prev); prev = &w->next)
		if (w->id == id)
		{
			if (gone)
				finish_unmap_win (dpy, w);
			*prev = w->next;
			if (w->damage != None)
			{
				set_ignore (dpy, NextRequest (dpy));
				XDamageDestroy (dpy, w->damage);
				w->damage = None;
			}
			free (w);
			break;
		}
}

static void
destroy_win (Display *dpy, Window id, Bool gone, Bool fade)
{
	if (currentFocusWindow == id && gone)
		currentFocusWindow = None;
	if (currentOverlayWindow == id && gone)
		currentOverlayWindow = None;
	if (currentNotificationWindow == id && gone)
		currentNotificationWindow = None;
	focusDirty = True;
	
	finish_destroy_win (dpy, id, gone);
}

static void
damage_win (Display *dpy, XDamageNotifyEvent *de)
{
	win	*w = find_win (dpy, de->drawable);
	win *focus = find_win(dpy, currentFocusWindow);
	
	if (!w)
		return;
	
	w->validContents = True;
	
	if (w->isOverlay && !w->opacity)
		return;
	
	// First damage event we get, compute focus; we only want to focus damaged
	// windows to have meaningful frames.
	if (w->gameID && w->damage_sequence == 0)
		focusDirty = True;
	
	w->damage_sequence = damageSequence++;
	
	// If we just passed the focused window, we might be eliglible to take over
	if (focus && focus != w && w->gameID &&
		w->damage_sequence > focus->damage_sequence)
		focusDirty = True;
	
	w->damaged = 1;
	
	if (w->damage)
		XDamageSubtract(dpy, w->damage, None, None);
}

static int
error (Display *dpy, XErrorEvent *ev)
{
	int	    o;
	const char    *name = NULL;
	static char buffer[256];
	
	if (should_ignore (dpy, ev->serial))
		return 0;
	
	if (ev->request_code == composite_opcode &&
		ev->minor_code == X_CompositeRedirectSubwindows)
	{
		fprintf (stderr, "Another composite manager is already running\n");
		exit (1);
	}
	
	o = ev->error_code - xfixes_error;
	switch (o) {
		case BadRegion: name = "BadRegion";	break;
		default: break;
	}
	o = ev->error_code - damage_error;
	switch (o) {
		case BadDamage: name = "BadDamage";	break;
		default: break;
	}
	o = ev->error_code - render_error;
	switch (o) {
		case BadPictFormat: name ="BadPictFormat"; break;
		case BadPicture: name ="BadPicture"; break;
		case BadPictOp: name ="BadPictOp"; break;
		case BadGlyphSet: name ="BadGlyphSet"; break;
		case BadGlyph: name ="BadGlyph"; break;
		default: break;
	}
	
	if (name == NULL)
	{
		buffer[0] = '\0';
		XGetErrorText (dpy, ev->error_code, buffer, sizeof (buffer));
		name = buffer;
	}
	
	fprintf (stderr, "error %d: %s request %d minor %d serial %lu\n",
			 ev->error_code, (strlen (name) > 0) ? name : "unknown",
			 ev->request_code, ev->minor_code, ev->serial);
	
	gotXError = True;
	/*    abort ();	    this is just annoying to most people */
	return 0;
}

static void
usage (char *program)
{
	fprintf (stderr, "%s v%s\n", program, "v4");
	fprintf (stderr, "usage: %s [options]\n", program);
	fprintf (stderr, "Options\n");
	fprintf (stderr, "   -d display\n      Specifies which display should be managed.\n");
	fprintf (stderr, "   -r radius\n      Specifies the blur radius for client-side shadows. (default 12)\n");
	fprintf (stderr, "   -o opacity\n      Specifies the translucency for client-side shadows. (default .75)\n");
	fprintf (stderr, "   -l left-offset\n      Specifies the left offset for client-side shadows. (default -15)\n");
	fprintf (stderr, "   -t top-offset\n      Specifies the top offset for clinet-side shadows. (default -15)\n");
	fprintf (stderr, "   -I fade-in-step\n      Specifies the opacity change between steps while fading in. (default 0.028)\n");
	fprintf (stderr, "   -O fade-out-step\n      Specifies the opacity change between steps while fading out. (default 0.03)\n");
	fprintf (stderr, "   -D fade-delta-time\n      Specifies the time between steps in a fade in milliseconds. (default 10)\n");
	fprintf (stderr, "   -a\n      Use automatic server-side compositing. Faster, but no special effects.\n");
	fprintf (stderr, "   -c\n      Draw client-side shadows with fuzzy edges.\n");
	fprintf (stderr, "   -C\n      Avoid drawing shadows on dock/panel windows.\n");
	fprintf (stderr, "   -f\n      Fade windows in/out when opening/closing.\n");
	fprintf (stderr, "   -F\n      Fade windows during opacity changes.\n");
	fprintf (stderr, "   -n\n      Normal client-side compositing with transparency support\n");
	fprintf (stderr, "   -s\n      Draw server-side shadows with sharp edges.\n");
	fprintf (stderr, "   -S\n      Enable synchronous operation (for debugging).\n");
	exit (1);
}

static Bool
register_cm (Display *dpy)
{
	Window w;
	Atom a;
	static char net_wm_cm[] = "_NET_WM_CM_Sxx";
	
	snprintf (net_wm_cm, sizeof (net_wm_cm), "_NET_WM_CM_S%d", scr);
	a = XInternAtom (dpy, net_wm_cm, False);
	
	w = XGetSelectionOwner (dpy, a);
	if (w != None)
	{
		XTextProperty tp;
		char **strs;
		int count;
		Atom winNameAtom = XInternAtom (dpy, "_NET_WM_NAME", False);
		
		if (!XGetTextProperty (dpy, w, &tp, winNameAtom) &&
			!XGetTextProperty (dpy, w, &tp, XA_WM_NAME))
		{
			fprintf (stderr,
					 "Another composite manager is already running (0x%lx)\n",
					 (unsigned long) w);
			return False;
		}
		if (XmbTextPropertyToTextList (dpy, &tp, &strs, &count) == Success)
		{
			fprintf (stderr,
					 "Another composite manager is already running (%s)\n",
					 strs[0]);
			
			XFreeStringList (strs);
		}
		
		XFree (tp.value);
		
		return False;
	}
	
	w = XCreateSimpleWindow (dpy, RootWindow (dpy, scr), 0, 0, 1, 1, 0, None,
							 None);
	
	Xutf8SetWMProperties (dpy, w, "steamcompmgr", "steamcompmgr", NULL, 0, NULL, NULL,
						  NULL);
	
	Atom atomWmCheck = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	XChangeProperty(dpy, root, atomWmCheck,
					XA_WINDOW, 32, PropModeReplace, (unsigned char *)&w, 1);
	XChangeProperty(dpy, w, atomWmCheck,
					XA_WINDOW, 32, PropModeReplace, (unsigned char *)&w, 1);
	
	
	Atom fullScreenSupported = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	XChangeProperty(dpy, root, XInternAtom(dpy, "_NET_SUPPORTED", False),
					XA_ATOM, 32, PropModeAppend, (unsigned char *)&fullScreenSupported, 1);
	
	XSetSelectionOwner (dpy, a, w, 0);
	
	ourWindow = w;
	
	exposeEvent.type = Expose;
	exposeEvent.xexpose.window = ourWindow;
	exposeEvent.xexpose.width = 1;
	exposeEvent.xexpose.height = 1;
	
	return True;
}

int
main (int argc, char **argv)
{
	Display	   *dpy;
	XEvent	    ev;
	Window	    root_return, parent_return;
	Window	    *children;
	unsigned int    nchildren;
	int		    i;
	XRenderPictureAttributes	pa;
	XRectangle	    *expose_rects = NULL;
	int		    size_expose = 0;
	int		    n_expose = 0;
	int		    composite_major, composite_minor;
	char	    *display = NULL;
	int		    o;
	
	while ((o = getopt (argc, argv, "D:I:O:d:r:o:l:t:scnufFCaSvV")) != -1)
	{
		switch (o) {
			case 'd':
				display = optarg;
				break;
			case 'n':
				doRender = False;
				break;
			case 'S':
				synchronize = True;
				break;
			case 'v':
				drawDebugInfo = True;
				break;
			case 'V':
				debugEvents = True;
				break;
			case 'u':
				allowUnredirection = True;
				break;
			default:
				usage (argv[0]);
				break;
		}
	}
	
	dpy = XOpenDisplay (display);
	if (!dpy)
	{
		fprintf (stderr, "Can't open display\n");
		exit (1);
	}
	XSetErrorHandler (error);
	if (synchronize)
		XSynchronize (dpy, 1);
	scr = DefaultScreen (dpy);
	root = RootWindow (dpy, scr);
	
	if (!XRenderQueryExtension (dpy, &render_event, &render_error))
	{
		fprintf (stderr, "No render extension\n");
		exit (1);
	}
	if (!XQueryExtension (dpy, COMPOSITE_NAME, &composite_opcode,
		&composite_event, &composite_error))
	{
		fprintf (stderr, "No composite extension\n");
		exit (1);
	}
	XCompositeQueryVersion (dpy, &composite_major, &composite_minor);
	
	if (!XDamageQueryExtension (dpy, &damage_event, &damage_error))
	{
		fprintf (stderr, "No damage extension\n");
		exit (1);
	}
	if (!XFixesQueryExtension (dpy, &xfixes_event, &xfixes_error))
	{
		fprintf (stderr, "No XFixes extension\n");
		exit (1);
	}
	if (!XShapeQueryExtension (dpy, &xshape_event, &xshape_error))
	{
		fprintf (stderr, "No XShape extension\n");
		exit (1);
	}
	if (!XFixesQueryExtension (dpy, &xfixes_event, &xfixes_error))
	{
		fprintf (stderr, "No XFixes extension\n");
		exit (1);
	}
	
	if (!register_cm(dpy))
	{
		exit (1);
	}
	
	/* get atoms */
	steamAtom = XInternAtom (dpy, STEAM_PROP, False);
	gameAtom = XInternAtom (dpy, GAME_PROP, False);
	overlayAtom = XInternAtom (dpy, OVERLAY_PROP, False);
	opacityAtom = XInternAtom (dpy, OPACITY_PROP, False);
	gamesRunningAtom = XInternAtom (dpy, GAMES_RUNNING_PROP, False);
	screenScaleAtom = XInternAtom (dpy, SCREEN_SCALE_PROP, False);
	screenZoomAtom = XInternAtom (dpy, SCREEN_MAGNIFICATION_PROP, False);
	winTypeAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE", False);
	winDesktopAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
	winDockAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	winToolbarAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
	winMenuAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
	winUtilAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
	winSplashAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
	winDialogAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	winNormalAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
	sizeHintsAtom = XInternAtom (dpy, "WM_NORMAL_HINTS", False);
	fullscreenAtom = XInternAtom (dpy, "_NET_WM_STATE_FULLSCREEN", False);
	WMStateAtom = XInternAtom (dpy, "_NET_WM_STATE", False);
	WMStateHiddenAtom = XInternAtom (dpy, "_NET_WM_STATE_HIDDEN", False);
	
	pa.subwindow_mode = IncludeInferiors;
	
	root_width = DisplayWidth (dpy, scr);
	root_height = DisplayHeight (dpy, scr);
	
	rootPicture = XRenderCreatePicture (dpy, root,
										XRenderFindVisualFormat (dpy,
																 DefaultVisual (dpy, scr)),
										CPSubwindowMode,
									 &pa);
	allDamage = None;
	clipChanged = True;
	
	XWindowAttributes rootAttribs;
	XVisualInfo visualInfoTemplate;
	int visualInfoCount;
	XVisualInfo *rootVisualInfo;
	
	XGetWindowAttributes (dpy, root, &rootAttribs);
	
	visualInfoTemplate.visualid = XVisualIDFromVisual (rootAttribs.visual);
	
	rootVisualInfo = XGetVisualInfo (dpy, VisualIDMask, &visualInfoTemplate, &visualInfoCount);
	if (!visualInfoCount)
	{
		fprintf (stderr, "Could not get root window visual info\n");
		exit (1);
	}
	
	glContext = glXCreateContext(dpy, rootVisualInfo, NULL, True);
	if (!glContext)
	{
		fprintf (stderr, "Could not create GLX context\n");
		exit (1);
	}
	if (!glXMakeCurrent(dpy, root, glContext))
	{
		fprintf (stderr, "Could not make GL context current\n");
		exit (1);
	}
	
	__pointer_to_glXSwapIntervalEXT = (void *)glXGetProcAddress("glXSwapIntervalEXT");
	if (__pointer_to_glXSwapIntervalEXT)
	{
		__pointer_to_glXSwapIntervalEXT(dpy, root, 1);
	}
	else
	{
		fprintf (stderr, "Could not find glXSwapIntervalEXT proc pointer\n");
	}
	
	__pointer_to_glXBindTexImageEXT = (void *)glXGetProcAddress("glXBindTexImageEXT");
	__pointer_to_glXReleaseTexImageEXT = (void *)glXGetProcAddress("glXReleaseTexImageEXT");
	
	if (!__pointer_to_glXBindTexImageEXT || !__pointer_to_glXReleaseTexImageEXT)
	{
		fprintf (stderr, "Could not get GLX_EXT_texture_from_pixmap entrypoints!\n");
		exit (1);
	}
	
	if (!strstr(glGetString(GL_EXTENSIONS), "GL_NV_path_rendering"))
	{
		drawDebugInfo = False;
	}
	else
	{
		__pointer_to_glGenPathsNV = (PFNGLGENPATHSNVPROC) glXGetProcAddress("glGenPathsNV");
		__pointer_to_glPathGlyphRangeNV = (PFNGLPATHGLYPHRANGENVPROC) glXGetProcAddress("glPathGlyphRangeNV");
		__pointer_to_glGetPathMetricRangeNV = (PFNGLGETPATHMETRICRANGENVPROC) glXGetProcAddress("glGetPathMetricRangeNV");
		__pointer_to_glGetPathSpacingNV = (PFNGLGETPATHSPACINGNVPROC) glXGetProcAddress("glGetPathSpacingNV");
		__pointer_to_glStencilFillPathInstancedNV = (PFNGLSTENCILFILLPATHINSTANCEDNVPROC) glXGetProcAddress("glStencilFillPathInstancedNV");
		__pointer_to_glStencilStrokePathInstancedNV = (PFNGLSTENCILSTROKEPATHINSTANCEDNVPROC) glXGetProcAddress("glStencilStrokePathInstancedNV");
		__pointer_to_glCoverFillPathInstancedNV = (PFNGLCOVERFILLPATHINSTANCEDNVPROC) glXGetProcAddress("glCoverFillPathInstancedNV");
		__pointer_to_glCoverStrokePathInstancedNV = (PFNGLCOVERSTROKEPATHINSTANCEDNVPROC) glXGetProcAddress("glCoverStrokePathInstancedNV");
	}
	
	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &cursorTextureName);
	
	if (drawDebugInfo)
		init_text_rendering();
	
	XFree(rootVisualInfo);
	
	XGrabServer (dpy);
	
	if (doRender)
	{
		XCompositeRedirectSubwindows (dpy, root, CompositeRedirectManual);
	}
	XSelectInput (dpy, root,
				  SubstructureNotifyMask|
				  ExposureMask|
				  StructureNotifyMask|
				  FocusChangeMask|
				  PointerMotionMask|
				  LeaveWindowMask|
				  PropertyChangeMask);
	XShapeSelectInput (dpy, root, ShapeNotifyMask);
	XFixesSelectCursorInput(dpy, root, XFixesDisplayCursorNotifyMask);
	XQueryTree (dpy, root, &root_return, &parent_return, &children, &nchildren);
	for (i = 0; i < nchildren; i++)
		add_win (dpy, children[i], i ? children[i-1] : None, 0);
	XFree (children);
	
	XUngrabServer (dpy);
	
	XF86VidModeLockModeSwitch(dpy, scr, True);
	
	// Start it with the cursor hidden until moved by user
	hideCursorForMovement = True;
	apply_cursor_state(dpy);
	
	gamesRunningCount = get_prop(dpy, root, gamesRunningAtom, 0);
	overscanScaleRatio = get_prop(dpy, root, screenScaleAtom, 0xFFFFFFFF) / (double)0xFFFFFFFF;
	zoomScaleRatio = get_prop(dpy, root, screenZoomAtom, 0xFFFF) / (double)0xFFFF;
	
	globalScaleRatio = overscanScaleRatio * zoomScaleRatio;
	
	determine_and_apply_focus(dpy);
	
	for (;;)
	{
		focusDirty = False;
		
		do {
			XNextEvent (dpy, &ev);
			if ((ev.type & 0x7f) != KeymapNotify)
				discard_ignore (dpy, ev.xany.serial);
			if (debugEvents)
			{
				printf ("event %x\n", ev.type);
			}
			switch (ev.type) {
				case CreateNotify:
					if (ev.xcreatewindow.parent == root)
						add_win (dpy, ev.xcreatewindow.window, 0, ev.xcreatewindow.serial);
					break;
				case ConfigureNotify:
					configure_win (dpy, &ev.xconfigure);
					break;
				case DestroyNotify:
				{
					win * w = find_win(dpy, ev.xdestroywindow.window);
					
					if (w && w->id == ev.xdestroywindow.window)
						destroy_win (dpy, ev.xdestroywindow.window, True, True);
					break;
				}
				case MapNotify:
				{
					win * w = find_win(dpy, ev.xmap.window);
					
					if (w && w->id == ev.xmap.window)
						map_win (dpy, ev.xmap.window, ev.xmap.serial);
					break;
				}
				case UnmapNotify:
				{
					win * w = find_win(dpy, ev.xunmap.window);
					
					if (w && w->id == ev.xunmap.window)
						unmap_win (dpy, ev.xunmap.window, True);
					break;
				}
				case ReparentNotify:
					if (ev.xreparent.parent == root)
						add_win (dpy, ev.xreparent.window, 0, ev.xreparent.serial);
					else
					{
						win * w = find_win(dpy, ev.xreparent.window);
						
						if (w && w->id == ev.xreparent.window)
						{
							destroy_win (dpy, ev.xreparent.window, False, True);
						}
						else
						{
							// If something got reparented _to_ a toplevel window,
							// go check for the fullscreen workaround again.
							w = find_win(dpy, ev.xreparent.parent);
							if (w)
							{
								get_size_hints(dpy, w);
								focusDirty = True;
							}
						}
					}
					break;
				case CirculateNotify:
					circulate_win (dpy, &ev.xcirculate);
					break;
				case Expose:
					break;
				case PropertyNotify:
					/* check if Trans property was changed */
					if (ev.xproperty.atom == opacityAtom)
					{
						/* reset mode and redraw window */
						win * w = find_win(dpy, ev.xproperty.window);
						win * mainOverlayWindow = find_win(dpy, currentOverlayWindow);
						if (w && w->isOverlay)
						{
							unsigned int newOpacity = get_prop(dpy, w->id, opacityAtom, TRANSLUCENT);
							
							if (newOpacity != w->opacity)
							{
								w->damaged = 1;
								w->opacity = newOpacity;
							}
							
							if (w->opacity && w->isOverlay && unredirectedWindow != None)
							{
								XCompositeRedirectWindow(dpy, unredirectedWindow, CompositeRedirectManual);
								ensure_win_resources(dpy, find_win(dpy, unredirectedWindow));
								unredirectedWindow = None;
							}
							
							if (w->isOverlay)
							{
								set_win_hidden(dpy, w, w->opacity == TRANSLUCENT);
							}
							
							unsigned int maxOpacity = 0;
							
							for (w = list; w; w = w->next)
							{
								if (w->isOverlay)
								{
									if (w->a.width == 1920 && w->opacity >= maxOpacity)
									{
										currentOverlayWindow = w->id;
										maxOpacity = w->opacity;
									}
								}
							}
						}
					}
					if (ev.xproperty.atom == steamAtom)
					{
						win * w = find_win(dpy, ev.xproperty.window);
						if (w)
						{
							w->isSteam = get_prop(dpy, w->id, steamAtom, 0);
							focusDirty = True;
						}
					}
					if (ev.xproperty.atom == gameAtom)
					{
						win * w = find_win(dpy, ev.xproperty.window);
						if (w)
						{
							w->gameID = get_prop(dpy, w->id, gameAtom, 0);
							focusDirty = True;
						}
					}
					if (ev.xproperty.atom == overlayAtom)
					{
						win * w = find_win(dpy, ev.xproperty.window);
						if (w)
						{
							w->isOverlay = get_prop(dpy, w->id, overlayAtom, 0);
							focusDirty = True;
							
							// Overlay windows need a RGBA pixmap, so destroy the old one there
							// It'll be reallocated as RGBA in ensure_win_resources()
							if (w->pixmap && w->isOverlay)
							{
								teardown_win_resources(dpy, w);
							}
						}
					}
					if (ev.xproperty.atom == sizeHintsAtom)
					{
						win * w = find_win(dpy, ev.xproperty.window);
						if (w)
						{
							get_size_hints(dpy, w);
							focusDirty = True;
						}
					}
					if (ev.xproperty.atom == gamesRunningAtom)
					{
						gamesRunningCount = get_prop(dpy, root, gamesRunningAtom, 0);
						
						focusDirty = True;
					}
					if (ev.xproperty.atom == screenScaleAtom)
					{
						overscanScaleRatio = get_prop(dpy, root, screenScaleAtom, 0xFFFFFFFF) / (double)0xFFFFFFFF;
						
						globalScaleRatio = overscanScaleRatio * zoomScaleRatio;
						
						win *w;
						
						if (w = find_win(dpy, currentFocusWindow))
							w->damaged = 1;
						
						focusDirty = True;
					}
					if (ev.xproperty.atom == screenZoomAtom)
					{
						zoomScaleRatio = get_prop(dpy, root, screenZoomAtom, 0xFFFF) / (double)0xFFFF;
						
						globalScaleRatio = overscanScaleRatio * zoomScaleRatio;
						
						win *w;
						
						if (w = find_win(dpy, currentFocusWindow))
							w->damaged = 1;
						
						focusDirty = True;
					}
					break;
					case ClientMessage:
					{
						win * w = find_win(dpy, ev.xclient.window);
						if (w)
						{
							if (ev.xclient.data.l[1] == fullscreenAtom)
							{
								w->isFullscreen = ev.xclient.data.l[0];
								
								focusDirty = True;
							}
						}
						break;
					}
					case LeaveNotify:
						if (ev.xcrossing.window == currentFocusWindow)
						{
							// This shouldn't happen due to our pointer barriers,
							// but there is a known X server bug; warp to last good
							// position.
							XWarpPointer(dpy, None, currentFocusWindow, 0, 0, 0, 0,
										 cursorX, cursorY);
						}
						break;
					case MotionNotify:
					{
						win * w = find_win(dpy, ev.xmotion.window);
						if (w && w->id == currentFocusWindow)
						{
							handle_mouse_movement( dpy, ev.xmotion.x, ev.xmotion.y );
						}
						break;
					}
					default:
						if (ev.type == damage_event + XDamageNotify)
						{
							damage_win (dpy, (XDamageNotifyEvent *) &ev);
						}
						else if (ev.type == xfixes_event + XFixesCursorNotify)
						{
							cursorImageDirty = True;
						}
						break;
			}
		} while (QLength (dpy));
		
		if (focusDirty == True)
			determine_and_apply_focus(dpy);
		
		if (doRender)
		{
			paint_all(dpy);
			
			// If we're in the middle of a fade, pump an event into the loop to
			// make sure we keep pushing frames even if the app isn't updating.
			if (fadeOutWindow.id)
				XSendEvent(dpy, ourWindow, True, ExposureMask, &exposeEvent);
			
			Window window_returned, child;
			int root_x, root_y;
			int win_x, win_y;
			unsigned int mask_return;
			
			XQueryPointer(dpy, DefaultRootWindow(dpy), &window_returned,
						  &child, &root_x, &root_y, &win_x, &win_y,
						&mask_return);
			
			if ( mask_return & ( Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask ) )
			{
				hideCursorForMovement = False;
				lastCursorMovedTime = get_time_in_milliseconds();
				apply_cursor_state(dpy);
			}
			
			if (!hideCursorForMovement &&
				(get_time_in_milliseconds() - lastCursorMovedTime) > CURSOR_HIDE_TIME)
			{
				hideCursorForMovement = True;
				apply_cursor_state(dpy);
				
				// If hiding and was drawing the fake cursor, force redraw
				win *w = find_win(dpy, currentFocusWindow);
				
				// Rearm warp count
				if (w)
				{
					w->mouseMoved = 0;
				}
				
				if (w && focusedWindowNeedsScale && gameFocused)
				{
					w->damaged = 1;
				}
			}
		}
	}
}
