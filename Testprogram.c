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

  // Hvis scandir feiler, avslutt
  if (numLoops < 0) {
    perror("scandir failed!");
    return 0;
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
    return 0;
  }

  // Hvis enhet er funnet, skriv ut path til enhet
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
                // Skriver ut event-koden til knapp som er trykket
                printf("ev.code: %d\n", ev.code);
                // Returnerer event-koden til knapp som er trykket
                return ev.code;
            }
        }
    }
  // Returnerer 0 hvis ingen knapp er trykket
  return 0;
}

int main() {
  while(1){
    readSenseHatJoystick();
    sleep(1);
  }
  return 0;
}
