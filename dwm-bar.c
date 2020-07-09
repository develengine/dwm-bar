#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFER_SIZE 512
#define SLEEP_TIME 30

const char *beginning = " ";
const char *ending = " ";
const char *separator = " ";


const char *weekDays[] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };

int timeDateFunction(char *buffer, int available)
{
    time_t currentTime;
    struct tm *values;

    time(&currentTime);
    values = localtime(&currentTime);

    return snprintf(buffer, available, "%s%s%d/%d%s%02d:%02d",
                    weekDays[values->tm_wday],
                    separator,
                    values->tm_mday,
                    values->tm_mon + 1,
                    separator,
                    values->tm_hour,
                    values->tm_min);
}

int volumeFunction(char *buffer, int available)
{
    const char token[] = "  Front Left: ";
    char fallback[] = "audio unknown";
    FILE *fp;
    char parseBuffer[512];
    char *valuePointer = fallback;

    fp = popen("amixer -D pulse sget Master", "r");

    while (fgets(parseBuffer, sizeof(parseBuffer), fp) != NULL)
    {
        if (strncmp(parseBuffer, token, sizeof(token) - 1) == 0)
        {
            for (int i = sizeof(token); i < strlen(parseBuffer); ++i)
            {
                char c = parseBuffer[i];
                if (c == '[')
                    valuePointer = parseBuffer+i+1;
                else if (c == ']')
                {
                    parseBuffer[i] = 0;
                    break;
                }
            }
            break;
        }
    }

    pclose(fp);

    return snprintf(buffer, available, "[Vol %s]", valuePointer);
}


int (*functions[])(char*, int) = { volumeFunction, timeDateFunction };


void updateBar()
{
    int ret, available, offset = 0;
    char command[BUFFER_SIZE] = { 0 };
    char *commandStrings[] = { "xsetroot", "-name", command, NULL };

    ret = snprintf(command, BUFFER_SIZE, "%s", beginning);
    offset += ret;

    for (int f = 0; f < sizeof(functions) / sizeof(functions[0]); ++f)
    {
        int required;

        if (f > 0)
        {
            available = BUFFER_SIZE - offset - 1;
            required = snprintf(command+offset, available, "%s", separator);
            
            if (required > available)
                break;

            offset += required;
        }

        available = BUFFER_SIZE - offset - 1;
        required = functions[f](command+offset, available);

        if (required > available)
            break;

        offset += required;
    }

    available = BUFFER_SIZE - offset - 1;
    snprintf(command+offset, available, "%s", ending);

    if (fork() == 0)
    {
        setsid();
        execvp(commandStrings[0], commandStrings);
    }
//     wait(&ret);
}

int main(int argc, char *argv[])
{
    int loop = argc > 1 && strcmp(argv[1], "loop") == 0;

    updateBar();

    while (loop)
    {
        sleep(SLEEP_TIME);
        updateBar();
    }

    return 0;
}

