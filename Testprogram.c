#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <linux/input.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <poll.h>

#include <dirent.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <sys/mman.h>

struct input_event ev;

// Initialises fd as an error
int fd = -1;
int fbfd = -1;

typedef struct {
  uint16_t pixel[8][8]; // 8x8 pixel array
}pixeltype;

pixeltype* pixelarray;

bool joystickInit(){
    struct input_event ev;
    struct dirent **subDirectoryNameList;
    
    char path[64];
    
    bool deviceLocated = false;
    
    // This loop will scan the /dev/input directory for devices, allowing all devices through
    int numLoops = scandir("/dev/input", &subDirectoryNameList, NULL, alphasort);
    
    // Hvis scandir feiler, avslutt
    if (numLoops < 0) {
        perror("scandir failed!");
        return false;
    }
    
    for (int i = 0; i < numLoops; i++) {
        if (strstr(subDirectoryNameList[i]->d_name, "event")) {
        sprintf(path, "/dev/input/%s", subDirectoryNameList[i]->d_name);
        
        fd = open(path, O_RDONLY);
    
        if (fd != -1) {
            // Initialiserer deviceName som et char array der jeg kan lagre navnet på enheten som leses
            char deviceName[256];
            ioctl(fd, EVIOCGNAME(sizeof(deviceName)), deviceName);
    
            // Hvis enheten er funnet, avslutt
            if (strstr(deviceName, "Raspberry Pi Sense HAT Joystick")) {
            deviceLocated = true;
            break;
            }
        }
        close(fd);
        }
        // Gi slipp på minne som er allokert av scandir()
        free(subDirectoryNameList[i]);
    }
    // Gi slipp på minne som er allokert av scandir()
    free(subDirectoryNameList);
    
    // Hvis enhet ikke er funnet, avslutt
    if (!deviceLocated) {
        fprintf(stderr, "ERROR: could not locate Sense HAT Joystick\n");
        return false;
    }

    // Hvis enhet er funnet, skriv ut path til enhet
    if(deviceLocated){
        fprintf(stdout, "INFO: using Sense HAT Joystick at %s\n", path);
    }
    return true;
}

int main() {
  bool test = joystickInit();
  printf("%d\n", test);
  while(1){
    readSenseHatJoystick();
    sleep(1);
  }
  return 0;
}
