
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <wels/codec_api.h>

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
static Visual* g_visual = 0;
static int g_depth = 0;

static ISVCDecoder* g_oh264Decoder = 0;
static SDecodingParam g_oh264DecoderParam;

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

#define LCLAMP(_val, _min, _max) (_val) < 0 ? 0 : ((_val) > 255 ? 255: (_val))

static int
yuv420_to_argb8888_no_sse2(unsigned char *yp, unsigned char *up, unsigned char *vp,
                           unsigned int sy, unsigned int suv,
                           int width, int height,
                           unsigned int *rgb, unsigned int srgb)
{
    unsigned char* y08;
    unsigned char* y18;
    unsigned char* u8;
    unsigned char* v8;
    unsigned int* dst08;
    unsigned int* dst18;
    int y, u, v;
    int r, g, b;
    int c, d, e;
    int index, jndex;

    for (jndex = 0; jndex < height; jndex += 2)
    {
        y08 = yp + sy * jndex;
        y18 = yp + sy * jndex + sy;
        u8 = up + suv * (jndex / 2);
        v8 = vp + suv * (jndex / 2);

        dst08 = rgb + srgb * jndex;
        dst18 = rgb + srgb * jndex + srgb;

        index = 0;
        while (index <= width - 2)
        {
            y = *(y08++);
            u = *(u8++);
            v = *(v8++);
            c = y - 16;
            d = u - 128;
            e = v - 128;
            b = (0x4A * c             + 0x81 * d) >> 6;
            g = (0x4A * c - 0x34 * e  - 0x19 * d) >> 6;
            r = (0x4A * c + 0x66 * e) >> 6;
            b = LCLAMP(b, 0, 255);
            g = LCLAMP(g, 0, 255);
            r = LCLAMP(r, 0, 255);
            *(dst08++) = (r << 16) | (g << 8) | b;

            y = *(y18++);
            c = y - 16;
            b = (0x4A * c             + 0x81 * d) >> 6;
            g = (0x4A * c - 0x34 * e  - 0x19 * d) >> 6;
            r = (0x4A * c + 0x66 * e) >> 6;
            b = LCLAMP(b, 0, 255);
            g = LCLAMP(g, 0, 255);
            r = LCLAMP(r, 0, 255);
            *(dst18++) = (r << 16) | (g << 8) | b;

            y = *(y08++);
            c = y - 16;
            b = (0x4A * c             + 0x81 * d) >> 6;
            g = (0x4A * c - 0x34 * e  - 0x19 * d) >> 6;
            r = (0x4A * c + 0x66 * e) >> 6;
            b = LCLAMP(b, 0, 255);
            g = LCLAMP(g, 0, 255);
            r = LCLAMP(r, 0, 255);
            *(dst08++) = (r << 16) | (g << 8) | b;

            y = *(y18++);
            c = y - 16;
            b = (0x4A * c             + 0x81 * d) >> 6;
            g = (0x4A * c - 0x34 * e  - 0x19 * d) >> 6;
            r = (0x4A * c + 0x66 * e) >> 6;
            b = LCLAMP(b, 0, 255);
            g = LCLAMP(g, 0, 255);
            r = LCLAMP(r, 0, 255);
            *(dst18++) = (r << 16) | (g << 8) | b;

            index += 2;
        }
    }
    return 0;
}

int
main(int argc, char** argv)
{
    int fd;
    char* data;
    int bytes;
    int width;
    int height;
    int x;
    int y;
    int w;
    int h;
    XEvent expose;
    XEvent evt;
    int error;
    int beef_header;
    XImage* image;
    char* idata;

    SBufferInfo targetInfo;
    unsigned char *targetBuffer[3];

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

    beef_header = 1;
    data = (char*)malloc(BUF_BYTES);

    error = WelsCreateDecoder(&g_oh264Decoder);
    printf("main: WelsCreateDecoder error %d\n", error);

    memset(&g_oh264DecoderParam, 0, sizeof(SDecodingParam));
    //g_oh264DecoderParam.eOutputColorFormat = videoFormatI420;
    g_oh264DecoderParam.uiTargetDqLayer = 255;
    g_oh264DecoderParam.eEcActiveIdc = ERROR_CON_DISABLE;
    g_oh264DecoderParam.sVideoProperty.size = sizeof(g_oh264DecoderParam.sVideoProperty);
    g_oh264DecoderParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
    error = (*g_oh264Decoder)->Initialize(g_oh264Decoder, &g_oh264DecoderParam);
    printf("main: Initialize error %d\n", error);

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
            error = (*g_oh264Decoder)->DecodeFrame2(g_oh264Decoder, (unsigned char*)data, bytes,
                                            (unsigned char**)&targetBuffer, &targetInfo);
            printf("main: DecodeFrame2 error %d\n", error);
            if ((error == 0) && (targetInfo.iBufferStatus == 0))
            {
                error = (*g_oh264Decoder)->DecodeFrame2(g_oh264Decoder, NULL, 0,
                                            (unsigned char**)&targetBuffer, &targetInfo);
                printf("main: DecodeFrame2 error %d\n", error);
            }
            if ((error == 0) && (targetInfo.iBufferStatus == 1))
            {
            }
            else
            {
                printf("error\n");
                break;
            }

            if ((width != g_winWidth) || (height != g_winHeight))
            {
                g_winWidth = width;
                g_winHeight = height;
                XResizeWindow(g_disp, g_win, g_winWidth, g_winHeight);
                XFreePixmap(g_disp, g_pix);
                g_pix = XCreatePixmap(g_disp, g_win, g_winWidth, g_winHeight, g_depth);
            }
            idata = (char*)malloc(width * height * 4);
            yuv420_to_argb8888_no_sse2(targetBuffer[0], targetBuffer[1], targetBuffer[2],
                                        targetInfo.UsrData.sSystemBuffer.iStride[0],
                                        targetInfo.UsrData.sSystemBuffer.iStride[1],
                                        width, height,
                                        (unsigned int*)idata, width);
            image = XCreateImage(g_disp, g_visual, 24, ZPixmap, 0, idata, width, height, 32, width * 4);
            XPutImage(g_disp, g_pix, g_gc, image, 0, 0, 0, 0, width, height);
            image->data = 0;
            XDestroyImage(image);
            free(idata);
            memset(&expose, 0, sizeof(expose));
            expose.type = Expose;
            expose.xexpose.display = g_disp;
            expose.xexpose.window = g_win;
            expose.xexpose.width = g_winWidth;
            expose.xexpose.height = g_winHeight;
            XSendEvent(g_disp, g_win, 0, 0, &expose);
        }
    }
    XDestroyWindow(g_disp, g_win);
    XCloseDisplay(g_disp);
    return 0;
}
