#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "../api.h"

// pe timestamps
enum hat_vers {
    DLC21 = 1557549916, // any%
    DLC231 = 1561041656, // tas
    DLC232 = 1565114742, // 110%
    VER_NUM = 3
};

const u64 fps_ptrs[VER_NUM][2] = {
    { 0x11BC360, 0x710 },
    { 0x11F6F10, 0x710 },
    { 0x11F9FE0, 0x710 }
};

s32 hat_pid;
u32 hat_ver_idx;

// return zero if write failed
u8 lag(f32 milliseconds) {
    f32 orig_fps;
    f32 new_fps = 1000.0f / milliseconds;

    // bad pointer resolution
    void* fps_ptr = (void*)(read_u64(hat_pid, (void*)(0x0000000140000000 + fps_ptrs[hat_ver_idx][0])) + fps_ptrs[hat_ver_idx][1]);

    read_bytes(hat_pid, fps_ptr, sizeof(orig_fps), &orig_fps);  
    if(!write_bytes(hat_pid, fps_ptr, sizeof(new_fps), &new_fps)) {
        return 0;
    }

    usleep(milliseconds * 300);

    // write back original
    write_bytes(hat_pid, fps_ptr, sizeof(orig_fps), &orig_fps);

    return 1;
}

char* input_names[64];
char input_dev_path[256];
u16 input_code = UINT16_MAX;

int main(int argc, char** argv) {
    puts("Starting hatlag...");

    // try opening bind file
    FILE* bind_f = fopen("bindkb.cfg", "r");

    // assume !bind_f means it didn't exist
    if(!bind_f || (argc > 1 && strcmp(argv[1], "bind") == 0)) {
        puts("Creating bindkb.cfg file, as it doesn't exist, or the bind command was specified\n");

        if(bind_f) {
            fclose(bind_f);
        }

        DIR* input_d = opendir("/dev/input/by-id");
        struct dirent* dir;

        if(input_d) {
            s32 input_i = 0;
            s32 chosen_input;

            while((dir = readdir(input_d)) != NULL) {
                if(dir->d_name[0] != '.') {
                    printf("%2d: %s\n", input_i + 1, dir->d_name);

                    input_names[input_i] = malloc(256);
                    strncpy(input_names[input_i], dir->d_name, 256);

                    input_i++;
                }
            }
            closedir(input_d);

            printf("Please choose an input device to use: ");
            scanf("%d", &chosen_input);
            chosen_input--;

            if(chosen_input < 0 || chosen_input > input_i) {
                puts("Please choose an input device in range...");
                return 1;
            }

            snprintf(input_dev_path, sizeof(input_dev_path), "/dev/input/by-id/%s", input_names[chosen_input]);

            for(u32 i = 0; i < input_i; i++) {
                free(input_names[i]);
            }

            s32 dev_fd = open(input_dev_path, O_RDONLY);
            if(dev_fd == -1) {
                printf("Failed to open %s, not good...\n", input_dev_path);
                return 1;
            }

            printf("\nYou chose %s\n", input_dev_path);
            puts("Please press the input you want to bind twice, with at least four seconds in between");
            puts("If you don't see any messages pertaining your pressed input, you may have either:\n\tChosen the wrong input device\n\tNot ran this program as root");            

            u32 ie_count = 0;
            u32 ie_max = 64;
            struct input_event* ie_history = malloc(sizeof(struct input_event) * ie_max);
            
            struct input_event ie;
            while(read(dev_fd, &ie, sizeof(ie))) {
                if(ie.type == EV_KEY) {
                    printf("Pressed %hu\n", ie.code);

                    // i sadly don't know how to make a map
                    u8 found = 0;
                    u8 chosen = 0;
                    for(u32 i = 0; i < ie_count; i++) {
                        // exists
                        if(ie_history[i].code == ie.code) {
                            if(ie_history[i].time.tv_sec < ie.time.tv_sec - 4) {
                                input_code = ie.code;
                                chosen = 1;
                            }

                            ie_history[i] = ie;
                            found = 1;

                            break;
                        }
                    }

                    if(chosen) {
                        break;
                    }

                    // doesn't exist
                    if(!found) {
                        if(ie_count == ie_max) {
                            ie_max *= 2;
                            ie_history = realloc(ie_history, sizeof(struct input_event) * ie_max);
                        }

                        ie_history[ie_count] = ie;
                        ie_count++;
                    }
                }
            }
            close(dev_fd);

            // failed to find key
            if(input_code == UINT16_MAX) {
                puts("Failed to find a key code, did you run as root?");
                return 1;
            }
            else {
                printf("Found key code %hu for device %s, saving to bindkb.cfg...\n", input_code, input_dev_path);
            }
        }
        else {
            puts("Unable to open /dev/input/by-id, not good...");
            return 1;
        }

        bind_f = fopen("bindkb.cfg", "w");
        if(bind_f == NULL) {
            puts("Unable to open bindkb.cfg for writing, not good...");
            return 1;
        }

        char buf[256];
        snprintf(buf, sizeof(buf), "%d\n%s", input_code, input_dev_path);
        fwrite(buf, strlen(buf), 1, bind_f);
        fclose(bind_f);

        puts("Done configuring binds, returning to normal operation...");
    }
    else {
        fscanf(bind_f, "%hd\n%s", &input_code, input_dev_path);
        fclose(bind_f);
    }

    while(1) {
        puts("Trying to find HatinTimeGame.exe...");

        hat_pid = pid_from_name("HatinTimeGame.exe");
        if(hat_pid != -1) {
            printf("Found HatinTimeGame.exe with pid %d\n", hat_pid);

            // bad
            u32 timestamp = read_u32(hat_pid, (void*)0x000000014000003C);
            timestamp = read_u32(hat_pid, (void*)(0x0000000140000000 + timestamp + 0x08));

            switch(timestamp) {
                case DLC21: hat_ver_idx = 0; break;
                case DLC231: hat_ver_idx = 1; break;
                case DLC232: hat_ver_idx = 2; break;
                default: printf("This version of A Hat in Time is not supported at the moment (%u)\n", timestamp); return 1;
            }

            s32 dev_fd = open(input_dev_path, O_RDONLY);
            if(dev_fd == -1) {
                printf("Failed to open %s, retrying in a second...\n", input_dev_path);
                sleep(1);
                continue;
            }

            struct input_event ie;
            u8 lag_fail = 0;
            struct timeval last_time;
            while(read(dev_fd, &ie, sizeof(ie))) {
                if(ie.code == input_code && ie.type == EV_KEY) {
                    //__builtin_dump_struct(&ie, &printf);

                    if(!lag(400)) {
                        lag_fail = 1;
                        break;
                    }
                }
                // disconnect heuristic (same timestamp for the last 200 events)
                else {
                    lag_fail++;
                    if(ie.time.tv_sec != last_time.tv_sec || ie.time.tv_usec != last_time.tv_usec) {
                        lag_fail = 0;
                    }
                    else if(lag_fail > 200) {
                        break;
                    }

                    last_time = ie.time;
                }
            }
            close(dev_fd);

            if(lag_fail) {
                continue;
            }

            puts("If you see this message, you may not be running this program as root. Make sure you are");

            return 0;
        }
        else {
            sleep(5);
        }
    }

    return 0;
}
