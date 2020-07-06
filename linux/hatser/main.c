#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../api.h"

// THIS FILE IS CURRENTLY WORKING FOR LIVESPLIT ONE
// YOU CAN COMMENT/UNCOMMENT CODE TO MAKE IT WORK FOR LIVESPLIT
// ADDITIONALLY, THE ANYMANY AUTOSPLITTER DOES NOT WORK

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

struct hat_save_data {
    u32 yarn;
    u32 lifetime_yarn;
    u32 pons;
    u32 timepieces;
    u32 mod_timepieces;
    u32 badge_pins;
    u32 chapter;
    u32 act;
    u32 checkpoint;
};

struct vector {
    f32 x;
    f32 y;
    f32 z;
};

struct region {
    u64 start;
    u64 end;
};

enum split_type {
    SPLIT_NORMAL,
    SPLIT_ANYMANY,
    SPLIT_N
} split_mode;

u8 should_split_normal();
u8 should_split_anymany();

u8(*split_tests[SPLIT_N])() = {
    should_split_normal,
    should_split_anymany
};

s32 hat_pid;
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
s32 socket_fd;
s32 sockets_fd;

// positive on success
u8 connect_livesplit() {
    struct sockaddr_in server;

    /*
    sockets_fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&server, 0x00, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = ip;

    bind(sockets_fd, (struct sockaddr*)&server, sizeof(server));

    listen(sockets_fd, SOMAXCONN);
    socket_fd = accept(sockets_fd, NULL, NULL);

    return 1;
    */
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

    //printf("Telling livesplit to %s\n", message);
    // use
    send(socket_fd, message, strlen(message), 0);
    //printf("told livesplit\n");
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

u32 split_idx = 0;
u8 should_start() {
    return timer.timer_state == 1 && old_timer.timer_state == 0;
}

u8 should_reset() {
    return timer.timer_state == 0 && old_timer.timer_state == 1;
}

u8 should_split_normal() {
    return (timer.time_piece_count == old_timer.time_piece_count + 1 && timer.act_timer_is_visible == 1) || timer.timer_state == 2;
}

// ~0 / -1.0f == ignore
struct anymany_condition {
    u32 yarn;
    u32 chapter;
    u32 act;
    u32 checkpoint;
    u32 paused;
    struct vector volume[2]; // low, high
};

const struct vector blank_volume[2] = {
    { -1.0f, -1.0f, -1.0f },
    { -1.0f, -1.0f, -1.0f }
};

const u32 blank_int[5] = { ~0, ~0, ~0, ~0, ~0 };

u8 should_split_anymany() {
    static struct anymany_condition conditions[87] = {
        // -Boop
        { 0, 0, 0, 0, 0, { { -820.0f, -200.0f, 285.0f }, { -790.0f, -160.0f, 305.0f } } },
        // -Spaceship
        { 0, 1, 1, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Chest
        { 1, 1, 1, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Crate
        { 2, 1, 1, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Umbrella
        { 2, 1, 1, 1, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // WTMT
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 0, 1, 3, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -DSJ
        { 1, 1, 3, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Construction
        { 2, 1, 3, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Cage
        { 3, 1, 3, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Scaffolding
        { 4, 1, 3, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Catwalk
        { 5, 1, 3, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Scare
        { 5, 1, 3, 1, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // SCfOS
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 1, 1, 2, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Trek
        { 2, 1, 2, 1, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // BB
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 2, 1, 4, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Outside
        { 2, 1, 4, 0, 1, { { -800.0f, 6800.0f, -20.0f }, { -750.0f, 6850.0f, 0.0f } } },
        // -Chest
        { 3, 1, 4, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -HQ
        { 3, 1, 4, 1, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Throne
        { 4, 1, 4, 2, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // DWTM
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 4, 2, 1, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Roof
        { 5, 2, 1, 1, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Void
        { 5, 2, 1, 5, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Chest
        { 6, 2, 1, 1, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // DBS
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 6, 2, 2, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // MotOE
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 6, 2, 3, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Roof
        { 7, 2, 3, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Balcony
        { 8, 2, 3, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // PP
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 0, 2, 2, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Entry
        { 0, 2, 2, 0, 1, { { -2640.0f, 1100.0f, 0.0f }, { -25900.0f, 1140.0f, 30.0f } } },
        // MotOE Rift
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 0, 2, 2, 0, 1, { { 7390.0f, 250.0f, -500.0f }, { 7440.0f, 280.0f, -450.0f } } },
        // Gallery
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 1, 3, 1, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // CO
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Skull
        { 3, 3, 1, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Branch A
        { 4, 3, 1, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Branch B
        { 5, 3, 1, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Branch C
        { 6, 3, 1, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Outside
        { 6, 3, 2, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Icicle
        { 7, 3, 2, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Inside
        { 7, 3, 2, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // Well
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 7, 3, 1, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hover
        { 7, 3, 4, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Barrel
        { 8, 3, 4, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // Manor
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 8, 3, 4, 0, 1, { { 2400.0f, 4170.0f, 10.0f }, { 2450.0f, 4200.0f, 50.0f } } },
        // -Entry
        { 8, 3, 4, 0, 1, { { 1875.0f, -10400.0f, 50.0f }, { 1910.0f, -10350.0f, 100.0f } } },
        // Pipe
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 8, 3, 5, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // MDS
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 8, 1, 7, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Tiki
        { 9, 1, 7, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Chest
        { 10, 1, 7, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Boat
        { 11, 1, 7, 0, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // GV
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 11, 1, 7, 0, 1, { { -980.0f, -380.0f, 2800.0f }, { -940.0f, -350.0f, 2860.0f } } },
        // -Entry
        { 11, 1, 7, 0, 1, { { -200.0f, -400.0f, 50.0f }, { -150.0f, -350.0f, 100.0f } } },
        // Sewers
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 11, 1, 6, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Death
        { 11, 1, 6, 59, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // HuMT
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 11, 1, 5, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // CtR
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Entry
        { 0, 1, 5, 0, 1, { { -200.0f, -160.0f, 50.0f }, { -150.0f, -110.0f, 100.0f } } },
        // Bazaar
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 0, 1, 5, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // Lab
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 0, 7, 99, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // MS
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // YM
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 0, 7, 5, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // YO
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 0, 7, 5, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // GM
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // GC
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Hub
        { 0, 5, 1, 0, 1, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Outside
        { 0, 5, 1, 1, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // -Throne
        { 0, 5, 1, 10, 0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
        // Finale
        { ~0, ~0, ~0, ~0, ~0, { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f } } },
    };

    u8 int_blank = 0;
    u8 pos_good = 0;

    if(should_split_normal()) {
        return 1;
    }

    struct vector pos;
    struct hat_save_data save;

    const struct anymany_condition* cond = &conditions[split_idx];

    // skip pos derefences if possible
    if(memcmp(&cond->volume, blank_volume, sizeof(blank_volume)) == 0) {
        pos_good = 1;
    }
    else {
        // player pos ptr path
        u8* ptr = (u8*)read_u64(hat_pid, (u8*)0x1411BC360);
        ptr = (u8*)read_u64(hat_pid, ptr + 0x6DC);
        ptr = (u8*)read_u64(hat_pid, ptr + 0x00);
        ptr = (u8*)read_u64(hat_pid, ptr + 0x68);
        ptr = (u8*)read_u64(hat_pid, ptr + 0x51C);

        if(ptr != NULL) {
            ptr += 0x80;

            read_bytes(hat_pid, ptr, sizeof(pos), &pos);
            if(pos.x > cond->volume[0].x &&
               pos.x < cond->volume[1].x &&
               pos.y > cond->volume[0].y &&
               pos.y < cond->volume[1].y &&
               pos.z > cond->volume[0].z &&
               pos.z < cond->volume[1].z
            ) {
                pos_good = 1;
            }
        }
    }

    if(memcmp(&cond->yarn, blank_int, sizeof(blank_int)) == 0) {
        int_blank = 1;
    }

    if(pos_good && !int_blank) {
        // save pointer path
        u8* ptr = (u8*)read_u64(hat_pid, (u8*)0x1411E1570);
        ptr = (u8*)read_u64(hat_pid, ptr + 0x68);

        if(ptr != NULL) {
            ptr += 0xF0;

            read_bytes(hat_pid, ptr, sizeof(save), &save);

            // ~0 means to ignore
            if((cond->yarn == ~0 || save.yarn == cond->yarn) &&
               (cond->chapter == ~0 || save.chapter == cond->chapter) &&
               (cond->act == ~0 || save.act == cond->act) &&
               (cond->checkpoint == ~0 || save.checkpoint == cond->checkpoint) &&
               (cond->paused == ~0 || timer.game_timer_is_paused == cond->paused)
            ) {
                return 1;
            }
        }
    }
    
    return 0;
}

int main(int argc, char** argv) {
    char* addr_str = "127.0.0.1";
    char* port_str = "16834";

    for(s32 i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
                case 'a': {
                    addr_str = argv[i] + 2;
                    break;
                }
                case 'p': {
                    port_str = argv[i] + 2;
                    break;
                }
                case 'm': {
                    if(memcmp(argv[i] + 2, "normal", sizeof("normal")) == 0) {
                        split_mode = SPLIT_NORMAL;
                    }
                    else if(memcmp(argv[i] + 2, "anymany", sizeof("anymany")) == 0) {
                        split_mode = SPLIT_ANYMANY;
                    }
                    else {
                        puts("WARN: invalid splitter specified, defaulting to normal");
                    }
                    break;
                }
            }
        }
    }

    inet_pton(AF_INET, addr_str, &ip);
    port = (u16)atoi(port_str);
    printf("Using %s:%hu to connect to LiveSplit.\n", addr_str, port);

    while(1) {
        hat_pid = pid_from_name("HatinTimeGame.exe");
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

                //if(timer.real_game_time != old_timer.real_game_time) {
                    char gtbuf[64];
                    sprintf(gtbuf, "setgametime %f\r\n", timer.real_game_time);
                    tell_livesplit(gtbuf);
                //}

                if(should_start()) {
                    split_idx = 0;
                    tell_livesplit("starttimer\r\n");
                }
                else if(split_tests[split_mode]()) {
                    split_idx++;
                    tell_livesplit("split\r\n");
                }
                else if(should_reset()) {
                    split_idx = 0;
                    tell_livesplit("reset\r\n");
                }

                // TODO: make configurable
                usleep(5000);
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
