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

#define BIND_FILE "bind1.cfg"

s32 hat_pid;
u32 hat_ver_idx;
void* fps_ptr = NULL;

// return zero if write failed
u8 lag(f32 milliseconds) {
    f32 orig_fps;
    f32 new_fps = 1000.0f / milliseconds;

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
size_t input_code_n = 0;
u16 input_codes[8] = {UINT16_MAX};

int main(int argc, char** argv) {
    puts("Starting hatlag...");

    // try opening bind file
    FILE* bind_f = fopen(BIND_FILE, "r");

    // assume !bind_f means it didn't exist
    if(!bind_f || (argc > 1 && strcmp(argv[1], "bind") == 0)) {
        puts("Creating " BIND_FILE " file, as it doesn't exist, or the bind command was specified\n");

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
                                input_codes[input_code_n] = ie.code;
                                input_code_n++;
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
            if(input_codes[0] == UINT16_MAX) {
                puts("Failed to find a key code, did you run as root?");
                return 1;
            }
            else {
                printf("Found key code %hu for device %s, saving to " BIND_FILE "...\n", input_codes[0], input_dev_path);
            }
        }
        else {
            puts("Unable to open /dev/input/by-id, not good...");
            return 1;
        }

        bind_f = fopen(BIND_FILE, "w");
        if(bind_f == NULL) {
            puts("Unable to open " BIND_FILE " for writing, not good...");
            return 1;
        }

        char buf[256];
        snprintf(buf, sizeof(buf), "%s\n%d", input_dev_path, input_codes[0]);
        fwrite(buf, strlen(buf), 1, bind_f);
        fclose(bind_f);

        puts("Done configuring binds, returning to normal operation...");
    }
    else {
        fscanf(bind_f, "%s", input_dev_path);
        while(1) {
            if(fscanf(bind_f, "%hd", &input_codes[input_code_n]) != 1) {
                break;
            }
            input_code_n++;
        }

        fclose(bind_f);
    }

    while(1) {
        puts("Trying to find HatinTimeGame.exe...");

        hat_pid = pid_from_name("HatinTimeGame.exe");
        if(hat_pid != -1) {
            printf("Found HatinTimeGame.exe with pid %d\n", hat_pid);

            static const u8 signature[] = {
                0x48, 0x8B, 0x05, 0xFE, 0xFE, 0xFE, 0xFE, 0x81, 0x88, 0xFE, 0xFE, 0xFE, 0xFE, 0x00, 0x00, 0x80, 0x00
            };
            static const u8 mask[] = {
                0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF
            };

            // 0xA00000 is arbitrary (but 0x160000 is too small, and likely 0x900000)
            const u64 fps_code = aob_scan(hat_pid, signature, mask, sizeof(signature), 0x0000000140000000, 0x0000000140000000 + 0xA00000);
            if(fps_code == 0) {
                puts("Failed to find FPS code");
                continue;
            }

            #define MOV_IMM_OFS 3
            #define MOV_LEN 7
            #define FPS_OFS 0x710
            const u32 fps_rel = read_u32(hat_pid, (void*)(fps_code + MOV_IMM_OFS));
            void* fps_ptr_struct = (void*)(fps_code + fps_rel + MOV_LEN);
            fps_ptr = (void*)(read_u64(hat_pid, fps_ptr_struct) + FPS_OFS);

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
                int found_key = 0;
                int break_out = 0;
                for(size_t i = 0; i < input_code_n; i++) {
                    if(ie.code == input_codes[i] && ie.type == EV_KEY) {
                        found_key = 1;
                        //__builtin_dump_struct(&ie, &printf);

                        if(!lag(400)) {
                            lag_fail = 1;
                            break_out = 1;
                        }
                        
                        break;
                    }
                }
                if(break_out) {
                    break;
                }

                // disconnect heuristic (same timestamp for the last 200 events)
                if(!found_key) {
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
