
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

static int g_term_pipe[2] = { -1, -1 };
static Display* g_disp = 0;

/*****************************************************************************/
static void
sig_int(int sig)
{
    (void)sig;
    if (write(g_term_pipe[1], "sig", 4) != 4)
    {
    }
}

/*****************************************************************************/
static void
sig_pipe(int sig)
{
    (void)sig;
}

/*****************************************************************************/
static int
main1(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    g_disp = XOpenDisplay(NULL);
    return 0;
}

/*****************************************************************************/
int
main(int argc, char** argv)
{
    int error;
    int rv;

    signal(SIGINT, sig_int);
    signal(SIGTERM, sig_int);
    signal(SIGPIPE, sig_pipe);
    error = pipe(g_term_pipe);
    if (error == 0)
    {
        rv = main1(argc, argv);
        close(g_term_pipe[0]);
        close(g_term_pipe[1]);
    }
    return rv;
}
