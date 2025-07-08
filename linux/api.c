#include "api.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#include <sys/uio.h>

s32 pid_from_name(const char* name) {
    char buf[256];

    // exclude arguments matching name (should probably clean up name before adding it to regex, but alas)
    snprintf(buf, sizeof(buf), "ps -eo pid,cmd | grep -E '[[:space:][:digit:]]+ [^ ]*%s'", name);

    FILE* cmd = popen(buf, "r");
    fgets(buf, sizeof(buf), cmd);
    s32 pid = strtoul(buf, NULL, 10);

    pclose(cmd);

    // assume this means it couldn't find it
    if(pid <= 0) {
        return -1;
    }

    return pid;
}

u8 read_u8(s32 pid, void* addr) {
    struct iovec local;
    struct iovec remote;
    u8 val;

    local.iov_base = &val;
    local.iov_len = sizeof(val);

    remote.iov_base = addr;
    remote.iov_len = sizeof(val);

    ssize_t nread = process_vm_readv(pid, &local, 1, &remote, 1, 0);
    if(nread < sizeof(val)) {
        // bad
        return 0;
    }

    return val;
}

u16 read_u16(s32 pid, void* addr) {
    struct iovec local;
    struct iovec remote;
    u16 val;

    local.iov_base = &val;
    local.iov_len = sizeof(val);

    remote.iov_base = addr;
    remote.iov_len = sizeof(val);

    ssize_t nread = process_vm_readv(pid, &local, 1, &remote, 1, 0);
    if(nread < sizeof(val)) {
        // bad
        return 0;
    }

    return val;
}

u32 read_u32(s32 pid, void* addr) {
    struct iovec local;
    struct iovec remote;
    u32 val;

    local.iov_base = &val;
    local.iov_len = sizeof(val);

    remote.iov_base = addr;
    remote.iov_len = sizeof(val);

    ssize_t nread = process_vm_readv(pid, &local, 1, &remote, 1, 0);
    if(nread < sizeof(val)) {
        // bad
        return 0;
    }

    return val;
}

u64 read_u64(s32 pid, void* addr) {
    struct iovec local;
    struct iovec remote;
    u64 val;

    local.iov_base = &val;
    local.iov_len = sizeof(val);

    remote.iov_base = addr;
    remote.iov_len = sizeof(val);

    ssize_t nread = process_vm_readv(pid, &local, 1, &remote, 1, 0);
    if(nread < sizeof(val)) {
        // bad
        return 0;
    }

    return val;
}

void read_bytes(s32 pid, void* addr, u64 len, void* buf) {
    struct iovec local;
    struct iovec remote;

    local.iov_base = buf;
    local.iov_len = len;

    remote.iov_base = addr;
    remote.iov_len = len;

    // bad: need to check for error
    process_vm_readv(pid, &local, 1, &remote, 1, 0);
}

u64 aob_scan(s32 pid, const u8* signature, const u8* mask, u64 len, u64 from, u64 to) {
    struct iovec local;
    struct iovec remote;
    const u64 region_len = to - from;
    
    u8* const buf = malloc(region_len);
    if(buf == NULL) {
        return 0;
    }

    local.iov_base = buf;
    local.iov_len = region_len;

    remote.iov_base = (void*)from;
    remote.iov_len = region_len;

    const ssize_t nread = process_vm_readv(pid, &local, 1, &remote, 1, 0);
    if(nread < region_len) {
        free(buf);
        return 0;
    }
    
    for(u8* ofs = buf; ofs < buf + region_len - len; ofs++) {
        for(u64 i = 0; i < len; i++) {
            if((ofs[i] & mask[i]) != (signature[i] & mask[i])) {
                break;
            }

            if(i == len - 1) {
                free(buf);

                // offset from `from` instead of `buf`
                return (u64)(ofs - buf + from);
            }
        }
    }

    free(buf);
    return 0;
}

u8 write_bytes(s32 pid, void* addr, u64 len, void* buf) {
    struct iovec local;
    struct iovec remote;

    local.iov_base = buf;
    local.iov_len = len;

    remote.iov_base = addr;
    remote.iov_len = len;

    const ssize_t nwritten = process_vm_writev(pid, &local, 1, &remote, 1, 0);
    return nwritten == len;
}

#elif defined(_WIN32)
// TODO
#endif
