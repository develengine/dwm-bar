#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>


#define BUFFER_SIZE 512
#define SLEEP_TIME 30

const char *beginning = " ";
const char *ending = " ";
const char *separator = " ";


#define FORMAT_OUTPUT(...) { \
    int stringSize = snprintf(NULL, 0, __VA_ARGS__); \
    char *string = malloc(stringSize + 1); \
    sprintf(string, __VA_ARGS__); \
    return string; \
}

#define LENGTH(a) (sizeof(a) / sizeof(a[0]))


typedef struct timespec timespec_t;

typedef struct {
    char *(*function)(void);
    double updateInterval;
    timespec_t lastUpdate;
} Entry;


static Display *display;
static int screen;
static Window root;


const char *weekDays[] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };

static char *timeDateFunction()
{
    time_t currentTime;
    struct tm *values;

    time(&currentTime);
    values = localtime(&currentTime);

    FORMAT_OUTPUT("%s%s%d/%d%s%02d:%02d",
                  weekDays[values->tm_wday],
                  separator,
                  values->tm_mday,
                  values->tm_mon + 1,
                  separator,
                  values->tm_hour,
                  values->tm_min);
}


static char *volumeFunction()
{
    const char token[] = "  Front Left: ";
    char fallback[] = "audio unknown";
    FILE *fp;
    char parseBuffer[512];
    char *valuePointer = fallback;

    fp = popen("amixer -D pulse sget Master", "r");

    while (fgets(parseBuffer, sizeof(parseBuffer), fp) != NULL) {
        if (strncmp(parseBuffer, token, sizeof(token) - 1) == 0) {
            for (int i = sizeof(token); i < strlen(parseBuffer); ++i) {
                char c = parseBuffer[i];
                if (c == '[') {
                    valuePointer = parseBuffer+i+1;
                } else if (c == ']') {
                    parseBuffer[i] = 0;
                    break;
                }
            }
            break;
        }
    }

    pclose(fp);

    FORMAT_OUTPUT("[Vol %s]", valuePointer);
}


Entry entries[] = { 
    { volumeFunction, 10.0, 0 },
    { timeDateFunction, 1.0, 0 },
};

char *stringOutputs[sizeof(entries) / sizeof(Entry)] = { 0 };


static void show()
{
    int infoLength = 0;
    int paddingLength = (LENGTH(stringOutputs) - 1) * strlen(separator)
                      + strlen(beginning) + strlen(ending);

    for (int i = 0; i < LENGTH(stringOutputs); i++) {
        infoLength += strlen(stringOutputs[i]);
    }

    char *text = malloc(infoLength + paddingLength + 1);
    int offset = sprintf(text, "%s", beginning);

    for (int i = 0; i < LENGTH(stringOutputs); i++) {
        if (i > 0) {
            offset += sprintf(text + offset, "%s", separator);
        }

        offset += sprintf(text + offset, "%s", stringOutputs[i]);
    }

    sprintf(text + offset, "%s", ending);

    XStoreName(display, root, text);
    XFlush(display);

    free(text);
}


static double subtractTime(timespec_t a, timespec_t b)
{
    return (double)(a.tv_sec - b.tv_sec) + ((a.tv_nsec - b.tv_nsec) / 1000000000.0);
}


static double clipToZero(double x)
{
    if (x < 0) {
        return 0;
    }

    return x;
}


static void update(timespec_t *sleep)
{
    const double EPSILON = 0.0001;
    timespec_t currentTime;
    clock_gettime(CLOCK_REALTIME, &currentTime);
    double shortestTime = 9999999.9;

    for (int e = 0; e < LENGTH(entries); e++) {
        Entry *entry = entries + e;
        double timePassed = subtractTime(currentTime, entry->lastUpdate);

        if (timePassed + EPSILON > entry->updateInterval) {
            if (stringOutputs[e]) {
                free(stringOutputs[e]);
            }

            stringOutputs[e] = entry->function();
            entry->lastUpdate = currentTime;
        }

        double timeLeft = entry->updateInterval - subtractTime(currentTime, entry->lastUpdate);
        shortestTime = shortestTime < timeLeft ? shortestTime : timeLeft;
    }

    sleep->tv_sec = (time_t)clipToZero(shortestTime);
    sleep->tv_nsec = (long)clipToZero(fmod(shortestTime, 1) * 1000000000);
}


int main(int argc, char *argv[])
{
    int loop = argc > 1 && strcmp(argv[1], "loop") == 0;

    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "ERROR: Failed call to XOpenDisplay.\n");
        exit(-1);
    }

    screen = DefaultScreen(display);
    root = RootWindow(display, screen);

    timespec_t sleepTime;

    update(&sleepTime);
    show();

    int it = 0;

    while(loop) {
        nanosleep(&sleepTime, NULL);
        update(&sleepTime);
        show();
    }

    for (int i = 0; i < LENGTH(stringOutputs); i++) {
        free(stringOutputs[i]);
    }

    return 0;
}

