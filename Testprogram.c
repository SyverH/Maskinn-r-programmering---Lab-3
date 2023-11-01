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
    return 1;
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
    return 1;
  }

  if(deviceLocated){
    fprintf(stdout, "INFO: using Sense HAT Joystick at %s\n", path);
  }
  

  //yess
  while (1) {
        if (read(fd, &ev, sizeof(struct input_event)) == -1) {
            perror("Lesing av hendelse feilet");
            close(fd);
            return 1;
        }

        if (ev.type == EV_KEY && (ev.code == KEY_UP || ev.code == KEY_DOWN || ev.code == KEY_LEFT || ev.code == KEY_RIGHT)) {
            if (ev.value == 0) {
                // Knappen slippes
                printf("Retning: Stopp\n");
            } else if (ev.value == 1) {
                // Knappen trykkes ned
                if (ev.code == KEY_UP) {
                    printf("Retning: Opp\n");
                } else if (ev.code == KEY_DOWN) {
                    printf("Retning: Ned\n");
                } else if (ev.code == KEY_LEFT) {
                    printf("Retning: Venstre\n");
                } else if (ev.code == KEY_RIGHT) {
                    printf("Retning: Høyre\n");
                }
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
