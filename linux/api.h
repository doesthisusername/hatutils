#ifndef LIB_API_H
#define LIB_API_H

#ifdef __linux__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float f32;
typedef double f64;
#elif defined(_WIN32)
// TODO
#endif

// returns pid of first matching process name, -1 on failure
s32 pid_from_name(const char* name);


// reading memory

// returns u8 read from address addr in process pid
u8 read_u8(s32 pid, void* addr);

// returns u16 read from address addr in process pid
u16 read_u16(s32 pid, void* addr);

// returns u32 read from address addr in process pid
u32 read_u32(s32 pid, void* addr);

// returns u64 read from address addr in process pid
u64 read_u64(s32 pid, void* addr);

/*
// returns s8 read from address addr in process pid
s8 read_s8(s32 pid, u64 addr);

// returns s16 read from address addr in process pid
s16 read_s16(s32 pid, u64 addr);

// returns s32 read from address addr in process pid
s32 read_s32(s32 pid, u64 addr);

// returns s64 read from address addr in process pid
s64 read_s64(s32 pid, u64 addr);
*/
// reads len bytes starting at address addr in process pid, storing result in buf
void read_bytes(s32 pid, void* addr, u64 len, void* buf);


// finds first occurrence of `signature`, only counting bits in `mask`, and returns its address
u64 aob_scan(s32 pid, const u8* signature, const u8* mask, u64 len, u64 from, u64 to);

// writing memory
/*
// writes val to address addr in process pid, returning positive if successful
u8 write_u8(s32 pid, u64 addr, u8 val);

// writes val to address addr in process pid, returning positive if successful
u8 write_u16(s32 pid, u64 addr, u16 val);

// writes val to address addr in process pid, returning positive if successful
u8 write_u32(s32 pid, u64 addr, u32 val);

// writes val to address addr in process pid, returning positive if successful
u8 write_u64(s32 pid, u64 addr, u64 val);

// writes val to address addr in process pid, returning positive if successful
u8 write_s8(s32 pid, u64 addr, s8 val);

// writes val to address addr in process pid, returning positive if successful
u8 write_s16(s32 pid, u64 addr, s16 val);

// writes val to address addr in process pid, returning positive if successful
u8 write_s32(s32 pid, u64 addr, s32 val);

// writes val to address addr in process pid, returning positive if successful
u8 write_s64(s32 pid, u64 addr, s64 val);*/

// writes len bytes of buf to address addr in process pid, returning positive if successful
u8 write_bytes(s32 pid, void* addr, u64 len, void* buf);

#endif // LIB_API_H
