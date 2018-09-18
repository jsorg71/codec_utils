
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>

#include <va/va.h>
#include <va/va_glx.h>
#include <VideoDecoderCapi.h>

static Display* g_disp = 0;
static int g_screenNumber = 0;
static unsigned long g_white = 0;
static unsigned long g_black = 0;
static Window g_win = 0;
static Pixmap g_pix = 0;
static int g_winWidth = 800;
static int g_winHeight = 600;
static long g_eventMask = 0;
static GC g_gc = 0;
static DecodeHandler g_dec;;
static VADisplay g_va_display = 0;
static Visual* g_visual = 0;
static XVisualInfo* g_vi = 0;
static int g_depth = 0;

static GLuint g_gl_texture = 0;
/* used for glXChooseVisual */
static GLint g_gl_visual_attr[] =
{
    GLX_RGBA,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_DOUBLEBUFFER,
    GL_NONE
};
/* used for glXChooseFBConfig */
static const GLint g_fbconfig_attrs[] =
{
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,   GLX_RGBA_BIT,
    GLX_DOUBLEBUFFER,  True,
    GLX_RED_SIZE,      8,
    GLX_GREEN_SIZE,    8, 
    GLX_BLUE_SIZE,     8,
    None
};
/* used for glXChooseFBConfig */
static const int g_pixconfig_attrs[] =
{
    GLX_BIND_TO_TEXTURE_RGBA_EXT, True,
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
    GLX_BIND_TO_TEXTURE_TARGETS_EXT, GLX_TEXTURE_2D_BIT_EXT,
    GLX_DOUBLEBUFFER, False,
    GLX_Y_INVERTED_EXT, GLX_DONT_CARE,
    None
};
/* used for glXCreatePixmap */
static const int g_pixmap_attribs[] =
{
    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
    GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
    None
};
static void* g_gl_surface = NULL;
static int g_n_fbconfigs = 0;
static int g_n_pixconfigs = 0;
static GLXFBConfig* g_fbconfigs = NULL;
static GLXFBConfig* g_pixconfigs = NULL;
static GLXContext g_gl_context = 0;
static GLXPixmap g_glxpixmap = 0;

typedef void (*t_glx_bind)(Display *, GLXDrawable, int , const int *);
typedef void (*t_glx_release)(Display *, GLXDrawable, int);

static t_glx_bind g_glXBindTexImageEXT = NULL;
static t_glx_release g_glXReleaseTexImageEXT = NULL;

#define BUF_BYTES (16 * 1024 * 1024)

static int
get_next_frame(int fd, char* data, int* bytes, int* width, int* height)
{
    int cur_offset;
    struct _header
    {
        char text[4];
        int width;
        int height;
        int bytes_follow;
    } header;

    cur_offset = lseek(fd, 0, SEEK_CUR);
    if (read(fd, &header, sizeof(header)) != sizeof(header))
    {
        return 1;
    }
    if (strncmp(header.text, "BEEF", 4) != 0)
    {
        printf("not BEEF file\n");
        lseek(fd, cur_offset, SEEK_SET);
        return 2;
    }
    if (read(fd, data, header.bytes_follow) != header.bytes_follow)
    {
        return 3;
    }
    printf("width %d height %d bytes_follow %d\n", header.width, header.height, header.bytes_follow);
    *bytes = header.bytes_follow; 
    *width = header.width;
    *height = header.height;
    return 0;
}

static int
get_next_frame_no_beef(int fd, char* data, int* bytes, int* width, int* height)
{
    int cur_offset;
    int readed;
    int index;

    cur_offset = lseek(fd, 0, SEEK_CUR);
    readed = read(fd, data, BUF_BYTES);
    if (readed < 1)
    {
        return 1;
    }
    for (index = 6; index < readed; index++)
    {
        if ((data[index - 3] == 0) && (data[index - 2] == 0) &&
            (data[index - 1] == 0) && (data[index] == 1))
        {
            *bytes = index - 3;
            lseek(fd, cur_offset + index - 3, SEEK_SET);
            return 0;
        }
    }
    *bytes = readed;
    return 0;
}

static int
gl_resize(void)
{
    VAStatus status;

    printf("gl_resize: g_winWidth %d g_winHeight %d\n", g_winWidth, g_winHeight);
    if (g_gl_surface != NULL)
    {
        vaDestroySurfaceGLX(g_va_display, g_gl_surface);
        g_gl_surface = NULL;
    }
    if (g_glxpixmap != 0)
    {
        glXDestroyPixmap(g_disp, g_glxpixmap);
        g_glxpixmap = 0;
    }
    if (g_gl_texture != 0)
    {
        glDeleteTextures(0, &g_gl_texture);
        g_gl_texture = 0;
    }
    XResizeWindow(g_disp, g_win, g_winWidth, g_winHeight);
    XFreePixmap(g_disp, g_pix);
    g_pix = XCreatePixmap(g_disp, g_win, g_winWidth, g_winHeight, g_depth);
    g_glxpixmap = glXCreatePixmap(g_disp, g_pixconfigs[0], g_pix, g_pixmap_attribs);
    if (g_glxpixmap == 0)
    {
        return 1;
    }
    printf("gl_resize: g_glxpixmap %ld\n", g_glxpixmap);
    glEnable(GL_TEXTURE_2D);
    g_gl_texture = 0;
    glGenTextures(1, &g_gl_texture);
    glBindTexture(GL_TEXTURE_2D, g_gl_texture);
    g_glXBindTexImageEXT(g_disp, g_glxpixmap, GLX_FRONT_EXT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    g_glXReleaseTexImageEXT(g_disp, g_glxpixmap, GLX_FRONT_LEFT_EXT);
    glBindTexture(GL_TEXTURE_2D, 0);
    printf("gl_resize: g_gl_texture %d\n", g_gl_texture);
    status = vaCreateSurfaceGLX(g_va_display, GL_TEXTURE_2D, g_gl_texture, &g_gl_surface);
    printf("gl_resize: vaCreateSurfaceGLX rv %d\n", status);
    if (status != 0)
    {
        return 1;
    }
    return 0; 
}

static int
gl_setup(void)
{
    g_vi = glXChooseVisual(g_disp, g_screenNumber, g_gl_visual_attr);
    printf("gl_setup: g_vi %p\n", g_vi);
    if (g_vi == NULL)
    {
        return 1;
    }
    g_fbconfigs = glXChooseFBConfig(g_disp, g_screenNumber, g_fbconfig_attrs, &g_n_fbconfigs);
    printf("gl_setup: g_fbconfigs %p g_n_fbconfigs %d\n", g_fbconfigs, g_n_fbconfigs);
    if (g_fbconfigs == NULL)
    {
        return 1;
    }
    g_gl_context = glXCreateNewContext(g_disp, g_fbconfigs[0], GLX_RGBA_TYPE, NULL, 1);
    printf("gl_setup: g_gl_context %p\n", g_gl_context);
    if (g_gl_context == NULL)
    {
        return 1;
    }
    glXMakeCurrent(g_disp, g_win, g_gl_context);
    g_glXBindTexImageEXT = (t_glx_bind) glXGetProcAddress((const GLubyte *)"glXBindTexImageEXT");
    g_glXReleaseTexImageEXT = (t_glx_release) glXGetProcAddress((const GLubyte *)"glXReleaseTexImageEXT");
    printf("gl_setup: g_glXBindTexImageEXT %p g_glXReleaseTexImageEXT %p\n", g_glXBindTexImageEXT, g_glXReleaseTexImageEXT);
    if ((g_glXBindTexImageEXT == NULL) || (g_glXReleaseTexImageEXT == NULL))
    {
        return 1;
    }
    g_pixconfigs = glXChooseFBConfig(g_disp, 0, g_pixconfig_attrs, &g_n_pixconfigs);
    printf("gl_resize: g_pixconfigs %p n_pixconfigs %d\n", g_pixconfigs, g_n_pixconfigs);
    if (g_pixconfigs == NULL)
    {
        return 1;
    }
    if (gl_resize() != 0)
    {
        return 1;
    }
    return 0;
}

int
main(int argc, char** argv)
{
    int major;
    int minor;
    NativeDisplay nd;
    VideoConfigBuffer cb;
    int fd;
    char* data;
    int bytes;
    int width;
    int height;
    VideoDecodeBuffer ib;
    const VideoFormatInfo* fi;
    VideoFrame* vf;
    int status;
    int flags;
    int x;
    int y;
    int w;
    int h;
    XEvent expose;
    XEvent evt;
    int error;
    int beef_header;

    fd = open(argv[1], O_RDONLY);
    if (argc < 2)
    {
        printf("error\n");
        return 1;
    }
    if (fd == -1)
    {
        printf("error opening %s\n", argv[1]);
        return 1;
    }
    g_disp = XOpenDisplay(NULL);
    g_screenNumber = DefaultScreen(g_disp);
    g_white = WhitePixel(g_disp, g_screenNumber);
    g_black = BlackPixel(g_disp, g_screenNumber);
    g_depth = DefaultDepth(g_disp, DefaultScreen(g_disp));
    g_visual = DefaultVisual(g_disp, DefaultScreen(g_disp));
    g_win = XCreateSimpleWindow(g_disp, DefaultRootWindow(g_disp),
                                50, 50, g_winWidth, g_winHeight,
                                0, g_black, g_white);
    XMapWindow(g_disp, g_win);
    g_eventMask = StructureNotifyMask | VisibilityChangeMask |
                  ButtonPressMask | ButtonReleaseMask | KeyPressMask |
                  ExposureMask;
    XSelectInput(g_disp, g_win, g_eventMask);
    g_gc = XCreateGC(g_disp, g_win, 0, NULL);
    g_pix = XCreatePixmap(g_disp, g_win, g_winWidth, g_winHeight, g_depth);

    g_va_display = vaGetDisplayGLX(g_disp);
    vaInitialize(g_va_display, &major, &minor);

    if (gl_setup() != 0)
    {
        printf("gl_setup failed, can not use GLX\n");
        return 1;
    }

    g_dec = createDecoder("video/h264");
    memset(&nd, 0, sizeof(nd));
    nd.type = NATIVE_DISPLAY_VA;
    nd.handle = (long) g_va_display;
    decodeSetNativeDisplay(g_dec, &nd);
    memset(&cb, 0, sizeof(cb));
    cb.profile = VAProfileNone;
    cb.enableLowLatency = 1;
    if (decodeStart(g_dec, &cb) != YAMI_SUCCESS)
    {
        printf("decodeStart error\n");
    }

    beef_header = 1;
    flags = 0;
    data = (char*)malloc(BUF_BYTES);

    while (1)
    {
        XNextEvent(g_disp, &evt);
        if (evt.type == Expose)
        {
            x = evt.xexpose.x;
            y = evt.xexpose.y;
            w = evt.xexpose.width;
            h = evt.xexpose.height;
            XCopyArea(g_disp, g_pix, g_win, g_gc, x, y, w, h, x, y);
        }
        else if (evt.type == KeyPress)
        {
            bytes = 0;
            width = 0;
            height = 0;
            if (beef_header)
            {
                error = get_next_frame(fd, data, &bytes, &width, &height);
                if (error == 2)
                {
                    beef_header = 0;
                    error = get_next_frame_no_beef(fd, data, &bytes, &width, &height);
                }
            }
            else
            {
                error = get_next_frame_no_beef(fd, data, &bytes, &width, &height);
            }
            if (error != 0) 
            {
                printf("end of file\n");
                break;
            }

            printf("get_next_frame bytes %d\n", bytes);
            memset(&ib, 0, sizeof(ib));
            ib.data = (unsigned char*)data;
            ib.size = bytes;
            ib.flag = VIDEO_DECODE_BUFFER_FLAG_FRAME_END;
            status = decodeDecode(g_dec, &ib);
            if (status == YAMI_DECODE_FORMAT_CHANGE)
            {
                printf("format change\n");
                fi = decodeGetFormatInfo(g_dec);
                width = fi->width;
                height = fi->height;
                status = decodeDecode(g_dec, &ib);
                if ((width != g_winWidth) || (height != g_winHeight))
                {
                    g_winWidth = width;
                    g_winHeight = height;
                    gl_resize();
                }
            }
            if (status == YAMI_SUCCESS)
            {
                vf = decodeGetOutput(g_dec);
                if ((vf != 0) && (vf->surface != 0))
                {
                    printf("got frame\n");
                    status = vaCopySurfaceGLX(g_va_display, g_gl_surface, vf->surface, flags);
                    if (status != VA_STATUS_SUCCESS)
                    {
                        printf("vaCopySurfaceGLX failed\n");
                        break;
                    }
                    vf->free(vf);
                    memset(&expose, 0, sizeof(expose));
                    expose.type = Expose;
                    expose.xexpose.display = g_disp;
                    expose.xexpose.window = g_win;
                    expose.xexpose.width = g_winWidth;
                    expose.xexpose.height = g_winHeight;
                    XSendEvent(g_disp, g_win, 0, 0, &expose);
                }
            }
        }
    }
    XDestroyWindow(g_disp, g_win);
    XCloseDisplay(g_disp);
    return 0;
}
