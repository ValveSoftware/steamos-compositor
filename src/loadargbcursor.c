#include "X11/Xlib.h"
#include "X11/extensions/Xrender.h"

#include "SDL/SDL.h"
#include "SDL/SDL_image.h"

typedef struct _tmppixel {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
}             pixel_t;

int main(int ac, char **av)
{
    int flags = IMG_INIT_JPG|IMG_INIT_PNG;
    int initted = IMG_Init(flags);
    
    if (ac != 2) {
        printf("usage: ./loadargbcursor imagefile\n");
        exit(0);
    }

    if (initted & flags != flags) {
        printf("IMG_Init: Failed to init required jpg and png support!\n");
        printf("IMG_Init: %s\n", IMG_GetError());
        exit(0);
    }

    SDL_Surface *image;
    image = IMG_Load(av[1]);
    if(!image) {
        printf("IMG_Load: %s\n", IMG_GetError());
        exit(0);
    }
    
    SDL_LockSurface(image);
    
    Display* dpy;
    
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        printf("Can't open X11 display!\n");
        exit(0);
    }
    
    XImage *ximage;
    
    char *ptr = image->pixels;
    int i = 0;
    
    pixel_t tmppixel;
    pixel_t *pixel;
    
    while (i < image->w * image->h) {
        pixel = (pixel_t *)ptr;
        
		tmppixel.a = pixel->a;
		
		if ( tmppixel.a != 0 )
		{
			tmppixel.r = pixel->b;
			tmppixel.g = pixel->g;
			tmppixel.b = pixel->r;
		}
		else
		{
			tmppixel.r = 0;
			tmppixel.g = 0;
			tmppixel.b = 0;
		}
        
        *pixel = tmppixel;
        
        i++;
        ptr += 4;
    }

    ximage = XCreateImage(dpy,
                          DefaultVisual(dpy, DefaultScreen(dpy)),
                          32,
                          ZPixmap,
                          0, //offset
                          image->pixels,
                          image->w,
                          image->h,
                          32, 
                          0);

    if(!ximage) {
        printf("Failed to create XImage from SDL_surface.\n");
        exit(0);
    }
    
    Pixmap pixmap;
    
    pixmap = XCreatePixmap(dpy, DefaultRootWindow(dpy),
                           image->w, image->h, 32);

    if(!pixmap) {
        printf("Failed to create X pixmap.\n");
        exit(0);
    }
    
    GC gc;
    
    gc = XCreateGC(dpy, pixmap, 0, NULL);

    if(!gc) {
        printf("Failed to create X GC.\n");
        exit(0);
    }

    XPutImage(dpy, pixmap, gc, ximage, 0, 0, 0, 0, image->w, image->h);
    
    XRenderPictFormat *pictformat = XRenderFindStandardFormat(dpy, PictStandardARGB32);

    XRenderPictureAttributes attributes;

    Picture picture = XRenderCreatePicture(dpy, pixmap, pictformat, 0, NULL);
    
    Cursor              curs;
    
    curs = XRenderCreateCursor(dpy, picture, 0, 0);
    
    XDefineCursor(dpy, DefaultRootWindow(dpy), curs);
    XSync(dpy, 0);
}
