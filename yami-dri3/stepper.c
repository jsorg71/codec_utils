
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlib-xcb.h>

#include <xcb/dri3.h>

#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>

#include <VideoDecoderCapi.h>

static Display* g_disp = 0;
static xcb_connection_t* g_xcb = 0;
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
static int g_drm_fd = -1;
static int g_pixmap_fd = -1;
static VASurfaceID g_pixmap_surface = 0;
static VAConfigID g_config_id = 0;
static VAContextID g_vpp_ctx = 0;

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
va_copy_surface(VASurfaceID src_sur,
                int srcx, int srcy, int srcwidth, int srcheight,
                int dstx, int dsty, int dstwidth, int dstheight)
{
    int status;
    VABufferID pipeline_buf;
    VARectangle input_region;
    VARectangle output_region;
    VAProcPipelineParameterBuffer* pipeline_param;
    void* buf;

    printf("va_copy_surface: srcx %d srcy %d srcwidth %d srcheight %d\n", srcx, srcy, srcwidth, srcheight);
    status = vaCreateBuffer(g_va_display, g_vpp_ctx, VAProcPipelineParameterBufferType, sizeof(VAProcPipelineParameterBuffer), 1, NULL, &pipeline_buf);
    printf("va_copy_surface: vaCreateBuffer status %d\n", status);
    input_region.x = srcx;
    input_region.y = srcy;
    input_region.width = srcwidth;
    input_region.height = srcheight;
    output_region.x = dstx;
    output_region.y = dsty;
    output_region.width = dstwidth;
    output_region.height = dstheight;
    status = vaMapBuffer(g_va_display, pipeline_buf, &buf);
    printf("va_copy_surface: vaMapBuffer status %d\n", status);
    pipeline_param = (VAProcPipelineParameterBuffer*)buf;
    memset(pipeline_param, 0, sizeof(VAProcPipelineParameterBuffer));
    pipeline_param->surface = src_sur;
    pipeline_param->surface_region = &input_region;
    pipeline_param->output_region = &output_region;
    status = vaUnmapBuffer(g_va_display, pipeline_buf);
    printf("va_copy_surface: vaUnmapBuffer status %d\n", status);
    status = vaBeginPicture(g_va_display, g_vpp_ctx, g_pixmap_surface);
    printf("va_copy_surface: vaBeginPicture status %d\n", status);
    status = vaRenderPicture(g_va_display, g_vpp_ctx, &pipeline_buf, 1);
    printf("va_copy_surface: vaRenderPicture status %d\n", status);
    status = vaEndPicture(g_va_display, g_vpp_ctx);
    printf("va_copy_surface: vaEndPicture status %d\n", status);
    status = vaSyncSurface(g_va_display, g_pixmap_surface);
    printf("va_copy_surface: vaSyncSurface status %d\n", status);
    vaDestroyBuffer(g_va_display, pipeline_buf);
    return 0;
}

static int
resize_dri3()
{
    xcb_dri3_buffer_from_pixmap_cookie_t buffer_from_pixmap_cookie;
    xcb_dri3_buffer_from_pixmap_reply_t* buffer_from_pixmap_reply;
    xcb_generic_error_t* error;
    int* fds;
    VAStatus vaStatus;
    VASurfaceAttribExternalBuffers external;
    VASurfaceAttrib attribs[2];
    unsigned long handle;

    if (g_vpp_ctx != 0)
    {
        vaDestroyContext(g_va_display, g_vpp_ctx);
        g_vpp_ctx = 0;
    }
    if (g_pixmap_surface != 0)
    {
        vaDestroySurfaces(g_va_display, &g_pixmap_surface, 1);
        g_pixmap_surface = 0;
    }
    if (g_pixmap_fd != -1)
    {
        close(g_pixmap_fd);
        g_pixmap_fd = -1;
    }
    XResizeWindow(g_disp, g_win, g_winWidth, g_winHeight);
    XFreePixmap(g_disp, g_pix);
    g_pix = XCreatePixmap(g_disp, g_win, g_winWidth, g_winHeight, g_depth);

    buffer_from_pixmap_cookie = xcb_dri3_buffer_from_pixmap(g_xcb, g_pix);
    buffer_from_pixmap_reply = xcb_dri3_buffer_from_pixmap_reply(g_xcb, buffer_from_pixmap_cookie, &error);
    if (buffer_from_pixmap_reply != NULL)
    {
        printf("nfd %d width %d height %d\n", buffer_from_pixmap_reply->nfd, buffer_from_pixmap_reply->width, buffer_from_pixmap_reply->height);
    }
    else
    {
        printf("buffer_from_pixmap_reply failed\n");
        free(buffer_from_pixmap_reply);
        free(error);
        return 1;
    }
    fds = xcb_dri3_buffer_from_pixmap_reply_fds(g_xcb, buffer_from_pixmap_reply);
    if (fds != NULL)
    {
        g_pixmap_fd = fds[0];
    }
    else
    {
        printf("xcb_dri3_buffer_from_pixmap_reply_fds failed\n");
        free(buffer_from_pixmap_reply);
        free(error);
        return 1;
    }

    handle = (unsigned long)g_pixmap_fd;

    memset(&external, 0, sizeof(external));
    external.pixel_format = VA_FOURCC_BGRX;
    external.width = buffer_from_pixmap_reply->width;
    external.height = buffer_from_pixmap_reply->height;
    external.data_size = buffer_from_pixmap_reply->width * buffer_from_pixmap_reply->height * 4;
    external.num_planes = 1;
    external.pitches[0] = buffer_from_pixmap_reply->stride;
    external.buffers = &handle;
    external.num_buffers = 1;

    memset(attribs, 0, sizeof(attribs));
    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].type = VASurfaceAttribMemoryType;
    attribs[0].value.type = VAGenericValueTypeInteger;
    attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

    attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
    attribs[1].value.type = VAGenericValueTypePointer;
    attribs[1].value.value.p = &external;

    vaStatus = vaCreateSurfaces(g_va_display, VA_RT_FORMAT_RGB32,
                                buffer_from_pixmap_reply->width, buffer_from_pixmap_reply->height,
                                &g_pixmap_surface, 1, attribs, 2);

    printf("resize_dri3: vaStatus %d g_pixmap_surface %d\n", vaStatus, g_pixmap_surface);

    if (vaStatus != 0)
    {
        free(buffer_from_pixmap_reply);
        free(error);
        return 1;
    }

    free(buffer_from_pixmap_reply);
    free(error);
    error = NULL;

    vaStatus = vaCreateContext(g_va_display, g_config_id, g_winWidth, g_winHeight, VA_PROGRESSIVE, &g_pixmap_surface, 1, &g_vpp_ctx);
    printf("resize_dri3: vaCreateContext vaStatus %d\n", vaStatus);

    return 0; 
}

static int
setup_dri3(void)
{
    int major;
    int minor;
    xcb_dri3_query_version_cookie_t query_version_cookie;
    xcb_dri3_query_version_reply_t* query_version_reply;
    xcb_dri3_open_cookie_t open_cookie;
    xcb_dri3_open_reply_t* open_reply;
    xcb_generic_error_t* error;
    int* fds;
    int status;

    printf("setup_dri3\n");
    error = NULL;
    query_version_cookie = xcb_dri3_query_version(g_xcb, XCB_DRI3_MAJOR_VERSION, XCB_DRI3_MINOR_VERSION);
    query_version_reply = xcb_dri3_query_version_reply(g_xcb, query_version_cookie, &error);
    if (query_version_reply != NULL)
    {
        printf("setup_dri3: DRI3 major_version %d minor_version %d\n", query_version_reply->major_version, query_version_reply->minor_version);
    }
    else
    {
        printf("xcb_dri3_query_version_reply failed\n");
        free(query_version_reply);
        free(error);
        return 1;
    }
    free(query_version_reply);
    free(error);
    error = NULL;
    open_cookie = xcb_dri3_open(g_xcb, DefaultRootWindow(g_disp), None);
    open_reply = xcb_dri3_open_reply(g_xcb, open_cookie, &error);
    if (open_reply != NULL)
    {
        if (open_reply->nfd != 1)
        {
            printf("xcb_dri3_open_reply bad nfd\n");
            free(open_reply);
            free(error);
            return 1;
        }
    }
    else
    {
        printf("xcb_dri3_open_reply failed\n");
        free(open_reply);
        free(error);
        return 1;
    }
    fds = xcb_dri3_open_reply_fds(g_xcb, open_reply);
    if (fds != NULL)
    {
        g_drm_fd = fds[0];
    }
    else
    {
        printf("xcb_dri3_open_reply_fds failed\n");
        free(open_reply);
        free(error);
        return 1;
    }
    free(open_reply);
    free(error);
    error = NULL;

    g_va_display = vaGetDisplayDRM(g_drm_fd); 
    vaInitialize(g_va_display, &major, &minor);

    status = vaCreateConfig(g_va_display, VAProfileNone, VAEntrypointVideoProc, NULL, 0, &g_config_id);
    printf("setup_dri3: vaCreateConfig status %d\n", status);

    return resize_dri3();
}

int
main(int argc, char** argv)
{
    VideoConfigBuffer cb;
    int fd;
    char* data;
    int bytes;
    int width;
    int height;
    NativeDisplay nd;
    VideoDecodeBuffer ib;
    const VideoFormatInfo* fi;
    VideoFrame* vf;
    int status;
    //int flags;
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
    if (g_disp == NULL)
    {
        printf("XOpenDisplay failed\n");
        return 1;
    }
    g_xcb = XGetXCBConnection(g_disp);
    if (g_xcb == NULL)
    {
        printf("XGetXCBConnection failed\n");
        return 1;
    }
    printf("main\n");
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

    if (setup_dri3() != 0)
    {
        printf("setup_dri3 failed\n");
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
    //flags = 0;
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
                    resize_dri3();
                }
            }
            if (status == YAMI_SUCCESS)
            {
                vf = decodeGetOutput(g_dec);
                if ((vf != 0) && (vf->surface != 0))
                {
                    printf("got frame\n");
                    status = va_copy_surface(vf->surface,
                                             0, 0, g_winWidth, g_winHeight,
                                             0, 0, g_winWidth, g_winHeight);
                    if (status != 0)
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
