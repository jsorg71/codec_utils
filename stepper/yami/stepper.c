
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <va/va.h>
#include <va/va_x11.h>
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
static int g_depth = 0;

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

    g_va_display = vaGetDisplay(g_disp);
    vaInitialize(g_va_display, &major, &minor);
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
                    XResizeWindow(g_disp, g_win, g_winWidth, g_winHeight);
                    XFreePixmap(g_disp, g_pix);
                    g_pix = XCreatePixmap(g_disp, g_win, g_winWidth, g_winHeight, g_depth);
                }
            }
            if (status == YAMI_SUCCESS)
            {
                vf = decodeGetOutput(g_dec);
                if ((vf != 0) && (vf->surface != 0))
                {
                    printf("got frame\n");
                    status = vaPutSurface(g_va_display, vf->surface, g_pix,
                                          0, 0, g_winWidth, g_winHeight,
                                          0, 0, g_winWidth, g_winHeight,
                                          0, 0, flags);
                    if (status != VA_STATUS_SUCCESS)
                    {
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
