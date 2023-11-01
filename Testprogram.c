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

int readSenseHatJoystick() {
  struct input_event ev;
  struct timeval timeout;
  struct dirent **subDirectoryNameList;

  char path[64];

  bool deviceLocated = false;

  // This loop will scan the /dev/input directory for devices, allowing all devices through
  int numLoops = scandir("/dev/input", &subDirectoryNameList, NULL, alphasort);

  // Initialises fd as an error
  int fd = -1;

  if (numLoops < 0) {
    perror("scandir failed!");
    return 0;
  }

  for (int i = 0; i < numLoops; i++) {
    if (strstr(subDirectoryNameList[i]->d_name, "event")) {
      sprintf(path, "/dev/input/%s", subDirectoryNameList[i]->d_name);
      
      fd = open(path, O_RDONLY);

      if (fd != -1) {
        // Initialises a char array where i can store the name of the devices
        char deviceName[256];
        ioctl(fd, EVIOCGNAME(sizeof(deviceName)), deviceName);

        if (strstr(deviceName, "Raspberry Pi Sense HAT Joystick")) {
          deviceLocated = true;
          break;
        }
      }
      close(fd);
    }
    free(subDirectoryNameList[i]);
  }
  free(subDirectoryNameList);

  if (!deviceLocated) {
    fprintf(stderr, "ERROR: could not locate Sense HAT Joystick\n");
    return 0;
  }

  if(deviceLocated){
    fprintf(stdout, "INFO: using Sense HAT Joystick at %s\n", path);
  }
  

  //yess
  while (1) {
        // Hvis avlesning av hendelse feiler, avslutt
        if (read(fd, &ev, sizeof(struct input_event)) == -1) {
            perror("Lesing av hendelse feilet");
            close(fd);
            return 0;
        }
        
        // Begrenser hvilke event-types som returneres
        if (ev.type == EV_KEY && (ev.code == KEY_UP || ev.code == KEY_DOWN || ev.code == KEY_LEFT || ev.code == KEY_RIGHT || ev.code == KEY_ENTER)) {
            // Rising edge detector, så knappen ikke leses av flere ganger
            if (ev.value == 0) {
            } else if (ev.value == 1) {
                printf("ev.code: %d\n", ev.code);
                return ev.code;
                // Knappen trykkes ned
                /*
                if (ev.code == KEY_UP) {
                    printf("Retning: Opp\n");
                } else if (ev.code == KEY_DOWN) {
                    printf("Retning: Ned\n");
                } else if (ev.code == KEY_LEFT) {
                    printf("Retning: Venstre\n");
                } else if (ev.code == KEY_RIGHT) {
                    printf("Retning: Høyre\n");
                } else if (ev.code == 28) {
                    printf("Retning: Enter\n");
                }
                */
            }
        }
    }

  return 0;
}

int main() {
  while(1){
    readSenseHatJoystick();
    sleep(1);
  }
  return 0;
}
