#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../api.h"

struct __attribute__((packed)) hat_timer {
    u32 start_magic;
    u32 timer_state;
    f64 unpause_time;
    u32 game_timer_is_paused;
    u32 act_timer_is_paused;
    u32 act_timer_is_visible;
    u32 unpause_time_is_dirty;
    u32 just_got_time_piece;
    f64 game_time;
    f64 act_time;
    f64 real_game_time;
    f64 real_act_time;
    u32 time_piece_count;
    u32 end_magic;
};

struct region {
    u64 start;
    u64 end;
};

void* timer_ptr; // the address in HatinTimeGame.exe
struct hat_timer timer;
struct hat_timer old_timer;

// updates timer_ptr, returning positive on success
u8 find_timer(s32 pid) {
    char buf[256];
    struct region region;

    snprintf(buf, sizeof(buf), "pmap -A 0x140000000,0x150000000 %d | grep 'K rw---'", pid);

    FILE* cmd = popen(buf, "r");

    u64 region_size;
    char suffix;
    fscanf(cmd, "%lX %lu%c", &region.start, &region_size, &suffix);
    pclose(cmd);

    // usually K
    if(suffix == 'K') {
        region_size *= 1024;
    }
    // i don't even know if this occurs in pmap
    else if(suffix == 'M') {
        region_size *= 1024 * 1024;
    }

    region.end = region.start + region_size;

    puts("Searching for timer in memory...");
    // dumb, simple search
    u8 found = 0;
    for(u64 ofs = region.start; ofs < region.end; ofs += 4) {
        // TIMR
        if(read_u32(pid, (void*)ofs) == 0x524D4954) {
            // just to make sure
            read_bytes(pid, (void*)ofs, sizeof(timer), &timer);

            // now we are sure
            if(timer.start_magic == 0x524D4954 && timer.end_magic == 0x20444E45) {
                timer_ptr = (void*)ofs;
                printf("Found timer at %p\n", timer_ptr);

                found = 1;
                break;
            }
        }
    }

    return found;
}


u32 ip;
u16 port;
struct sockaddr_in server;
s32 socket_fd;

// positive on success
u8 connect_livesplit() {
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd < 0) {
        return 0;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = ip;

    if(connect(socket_fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
        return 0;
    }

    return 1;
}

void tell_livesplit(char* message) {
    if(socket_fd <= 0) {
        connect_livesplit();
    }

    // use
    send(socket_fd, message, strlen(message), 0);
}

// remember to free!
char* ask_livesplit(char* message) {
    if(socket_fd <= 0) {
        connect_livesplit();
    }

    // ask
    const ssize_t sent = send(socket_fd, message, strlen(message), 0);

    // failure, try to recover
    if(sent == -1) {
        connect_livesplit();
        return ask_livesplit(message);
    }
    else {
        char* response = malloc(1024);
        const ssize_t nread = read(socket_fd, response, 1024);

        if(nread < 0) {
            return NULL;
        }
        else {
            return response;
        }
    }
}

u8 should_start() {
    return timer.timer_state == 1 && old_timer.timer_state == 0;
}

u8 should_split() {
    return (timer.time_piece_count == old_timer.time_piece_count + 1 && timer.act_timer_is_visible == 1) || timer.timer_state == 2;
}

u8 should_reset() {
    return timer.timer_state == 0 && old_timer.timer_state == 1;
}

int main(int argc, char** argv) {
    if(argc < 2) {
        inet_pton(AF_INET, "127.0.0.1", &ip);
        port = 16834; // default LiveSplit port
        puts("No LiveSplit IP specified, assuming localhost:16384...");
    }
    else if(argc < 3) {
        inet_pton(AF_INET, argv[1], &ip);
        port = 16834;
        printf("Using %s:16834 to connect to LiveSplit...\n", argv[1]);
    }
    else {
        inet_pton(AF_INET, argv[1], &ip);
        port = (u16)strtoul(argv[2], NULL, 10);
        printf("Using %s:%hu to connect to LiveSplit...\n", argv[1], port);
    }

    while(1) {
        s32 hat_pid = pid_from_name("HatinTimeGame.exe");
        while(hat_pid == -1) {
            puts("Unable to find HatinTimeGame.exe process, retrying in a few seconds...");
            sleep(5);
            hat_pid = pid_from_name("HatinTimeGame.exe");
        }

        if(find_timer(hat_pid)) {
            if(!connect_livesplit()) {
                puts("Unable to connect to LiveSplit Server, please make sure it is turned on");
                continue;
            }

            tell_livesplit("initgametime\r\n");

            // main autosplitter loop
            while(1) {
                old_timer = timer;
                read_bytes(hat_pid, timer_ptr, sizeof(timer), &timer);

                // failure to read process memory
                if(timer.start_magic == 0) {
                    break;
                }

                if(should_start()) {
                    tell_livesplit("starttimer\r\n");
                }
                else if(should_split()) {
                    tell_livesplit("split\r\n");
                }
                else if(should_reset()) {
                    tell_livesplit("reset\r\n");
                }

                char gtbuf[64];
                sprintf(gtbuf, "setgametime %f\r\n", timer.real_game_time);
                tell_livesplit(gtbuf);

                // TODO: make configurable
                usleep(10000);
            }
        }
        else {
            puts("Found HatinTimeGame.exe, but could not find timer, retrying in a few seconds...");
            sleep(5);
            continue;
        }
    }

    return 0;
}
