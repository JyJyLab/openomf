// Functions newlib on the Switch declares but libnx does not provide. libconfuse uses them to
// expand "~" in paths, which OpenOMF never passes; there are no user accounts to look up.
#include <pwd.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

struct passwd *getpwuid(uid_t uid) {
    (void)uid;
    return NULL;
}

struct passwd *getpwnam(const char *name) {
    (void)name;
    return NULL;
}

uid_t geteuid(void) {
    return 0;
}
