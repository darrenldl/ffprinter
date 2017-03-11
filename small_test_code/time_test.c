#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main (void) {
    time_t raw_time;
    struct tm* time_info;

    char buffer[80];

    time (&raw_time);

    time_info = gmtime(&raw_time);
    strftime (buffer,80,"Now it's %H:%M %y/%m/%d in UTC\n",time_info);
    puts(buffer);
    
    printf("timezone : %d\n", timezone);

    time_info = localtime(&raw_time);
    strftime (buffer,80,"Now it's %H:%M %y/%m/%d in local timezone\n",time_info);
    puts(buffer);

    printf("timezone : %d\n", timezone);

    return 0;
}
