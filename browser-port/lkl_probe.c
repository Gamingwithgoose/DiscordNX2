// SPDX-License-Identifier: MIT
// Original DiscordNX2 code. Optional browser-runtime probe, never account login.
#include <switch.h>
#include <lkl_host.h>
#include <stdio.h>
#include <string.h>

static int finished;
static int result = -1;
static char message[160] = "Starting the browser's Linux runtime...";
static Mutex log_mutex;

// Kernel threads write only a local runtime log; the main thread owns the screen.
static void kernel_log(const char *text, int length)
{
    mutexLock(&log_mutex);
    FILE *file = fopen("sdmc:/discordnx2-browser-runtime.log", "a");
    if (file) {
        fwrite(text, 1, length, file);
        fclose(file);
    }
    mutexUnlock(&log_mutex);
}

static void run(void *unused)
{
    (void)unused;
    lkl_host_ops.print = kernel_log;
    int initialized = lkl_init(&lkl_host_ops);
    int started = initialized == 0 ? lkl_start_kernel("mem=64M loglevel=4 nohz=off") : initialized;
    int status = started;
    if (status == 0) {
        struct {
            char system[65], node[65], release[65], version[65], machine[65], domain[65];
        } name = {0};
        long args[6] = {(long)&name, 0, 0, 0, 0, 0};
        status = (int)lkl_syscall(__lkl__NR_uname, args);
        // LKL may report the host architecture as "lkl"; don't assume Linux process ABI.
        if (!status && strncmp(name.system, "Linux", sizeof(name.system)) != 0)
            status = -1001;
        if (!status)
            status = (int)lkl_sys_mkdir("/probe", 0700);
        if (!status)
            status = (int)lkl_sys_mount("tmpfs", "/probe", "tmpfs", 0, NULL);
        if (!status) {
            static const char expected[] = "DiscordNX2 browser runtime";
            char actual[sizeof(expected)] = {0};
            int fd = lkl_sys_open("/probe/check", LKL_O_RDWR | LKL_O_CREAT, 0600);
            if (fd < 0) status = fd;
            else {
                if (lkl_sys_write(fd, expected, sizeof(expected)) != sizeof(expected) ||
                    lkl_sys_lseek(fd, 0, 0) != 0 ||
                    lkl_sys_read(fd, actual, sizeof(actual)) != sizeof(actual) ||
                    memcmp(actual, expected, sizeof(actual)) != 0)
                    status = -1002;
                lkl_sys_close(fd);
            }
            lkl_sys_unlink("/probe/check");
            lkl_sys_umount("/probe", 0);
        }
        // Halt before reporting completion: no background guest is left running.
        int halted = lkl_sys_halt();
        if (!status && halted) status = halted;
    }
    if (!initialized) lkl_cleanup();
    result = status;
    snprintf(message, sizeof(message), "%s (result %d)",
             status == 0 ? "Runtime boot, file checks and shutdown passed" : "Runtime check failed", status);
    kernel_log(message, (int)strlen(message));
    __atomic_store_n(&finished, 1, __ATOMIC_RELEASE);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    mutexInit(&log_mutex);
    FILE *log = fopen("sdmc:/discordnx2-browser-runtime.log", "w");
    if (log) { fputs("DiscordNX2 Chromium runtime probe 1\n", log); fclose(log); }
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    Thread worker;
    Result rc = threadCreate(&worker, run, NULL, NULL, 0x80000, 0x2c, -2);
    bool created = R_SUCCEEDED(rc);
    if (created) {
        rc = threadStart(&worker);
        if (R_FAILED(rc)) { threadClose(&worker); created = false; }
    }
    if (!created) {
        snprintf(message, sizeof(message), "Could not start runtime worker (%08lx)", (unsigned long)rc);
        finished = 1;
    }
    puts("DiscordNX2 browser runtime test\n");
    puts("This checks the Linux component only. It is not a browser or CAPTCHA login.\n");
    puts("Starting runtime... Press + to exit at any time.");
    bool shown = false;
    while (appletMainLoop()) {
        if (__atomic_load_n(&finished, __ATOMIC_ACQUIRE) && !shown) {
            printf("\n%s\n\nLog: sdmc:/discordnx2-browser-runtime.log\n", message);
            shown = true;
        }
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
        svcSleepThread(16666666);
    }
    if (created && __atomic_load_n(&finished, __ATOMIC_ACQUIRE)) {
        threadWaitForExit(&worker);
        threadClose(&worker);
    }
    // A cancelled boot exits this process; don't block forever joining the kernel.
    consoleExit(NULL);
    return __atomic_load_n(&finished, __ATOMIC_ACQUIRE) ? result : 0;
}
