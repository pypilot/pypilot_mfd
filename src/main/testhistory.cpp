#include <unistd.h>
#include <stdint.h>
#include <math.h>

#include "display.h"
#include "history.h"

// g++ -g -o testhistory testhistory.cpp history.cpp

void history_reset();


int main()
{
    history_setup();
    history_set_time(260824, 11, 4, 0);

    for(int i=0; i<20; i++)
        history_poll();
    
    history_put(WIND_SPEED, 1);
    usleep(1e6);
    history_put(WIND_SPEED, 2);
    usleep(1e6);
    history_put(WIND_SPEED, 3);
    usleep(1e6);

    history_reset();

    for(;;) {
        history_poll();
        usleep(1e5);
    }
}
