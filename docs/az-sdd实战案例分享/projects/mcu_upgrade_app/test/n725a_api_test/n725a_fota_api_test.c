#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "nwy_common.h"
#include "nwy_fota.h"

#define INPUT_BUFFER_SIZE 512U
#define DEFAULT_TIMEOUT_MSEC 3000

typedef int (*test_handler_t)(void);

typedef struct {
    int option;
    const char *api_name;
    const char *description;
    test_handler_t handler;
} test_command_t;

static int read_line(const char *prompt, char *buffer, size_t buffer_size)
{
    int result = -1;
    size_t length;

    if ((buffer != NULL) && (buffer_size > 1U)) {
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(buffer, (int)buffer_size, stdin) != NULL) {
            length = strlen(buffer);
            if ((length > 0U) && (buffer[length - 1U] == '\n')) {
                buffer[length - 1U] = '\0';
            } else {
                int ch;
                do {
                    ch = getchar();
                } while ((ch != '\n') && (ch != EOF));
            }
            result = 0;
        }
    }

    return result;
}

static int parse_unsigned(const char *text, unsigned int *value)
{
    int result = -1;
    char *end = NULL;
    unsigned long parsed;

    if ((text != NULL) && (value != NULL) && (text[0] != '\0')) {
        errno = 0;
        parsed = strtoul(text, &end, 0);
        while ((end != NULL) && ((*end == ' ') || (*end == '\t'))) {
            end++;
        }
        if ((errno == 0) && (end != text) && (end != NULL) &&
            (*end == '\0') && (parsed <= UINT_MAX)) {
            *value = (unsigned int)parsed;
            result = 0;
        }
    }

    return result;
}

static int read_unsigned(const char *prompt, unsigned int *value)
{
    char input[INPUT_BUFFER_SIZE];
    int result = -1;

    if ((read_line(prompt, input, sizeof(input)) == 0) &&
        (parse_unsigned(input, value) == 0)) {
        result = 0;
    } else {
        printf("Invalid number. Decimal and 0x-prefixed hexadecimal are accepted.\n");
    }

    return result;
}

static int confirm_operation(const char *warning)
{
    unsigned int selection = 0U;
    int confirmed = 0;

    printf("WARNING: %s\n", warning);
    if (read_unsigned("Enter 1 to continue, or 0 to cancel: ", &selection) == 0) {
        if (selection == 1U) {
            confirmed = 1;
        } else {
            printf("Operation cancelled.\n");
        }
    }

    return confirmed;
}

static int read_package_path(char *path, size_t path_size)
{
    struct stat file_stat;
    int result = -1;

    if (read_line("FOTA package path: ", path, path_size) == 0) {
        if (path[0] == '\0') {
            printf("Package path must not be empty.\n");
        } else if (stat(path, &file_stat) != 0) {
            printf("Cannot access package '%s': %s\n", path, strerror(errno));
        } else if (!S_ISREG(file_stat.st_mode)) {
            printf("Package path is not a regular file: %s\n", path);
        } else {
            result = 0;
        }
    }

    return result;
}

static const char *fota_result_name(int result)
{
    const char *name = "UNKNOWN";

    switch (result) {
        case FOTA_UPDATE_SUCCESS:
            name = "FOTA_UPDATE_SUCCESS";
            break;
        case FOTA_UPDATE_FAIL:
            name = "FOTA_UPDATE_FAIL";
            break;
        case FOTA_UPDATE_NULL:
            name = "FOTA_UPDATE_NULL";
            break;
        case FOTA_UPDATING:
            name = "FOTA_UPDATING";
            break;
        case FOTA_NEEDSYNC:
            name = "FOTA_NEEDSYNC";
            break;
        case FOTA_SYNCHING:
            name = "FOTA_SYNCHING";
            break;
        case FOTA_UPDATING_OBM:
            name = "FOTA_UPDATING_OBM";
            break;
        case FOTA_WRITEDOWNE:
            name = "FOTA_WRITEDOWNE";
            break;
        default:
            break;
    }

    return name;
}

static const char *download_result_name(int result)
{
    const char *name = "UNKNOWN";

    switch (result) {
        case FOTA_DOWNLOAD_SUCCESS:
            name = "FOTA_DOWNLOAD_SUCCESS";
            break;
        case FOTA_DOWNLOAD_FAIL:
            name = "FOTA_DOWNLOAD_FAIL";
            break;
        case FOTA_DOWNLOAD_NULL:
            name = "FOTA_DOWNLOAD_NULL";
            break;
        default:
            break;
    }

    return name;
}

static int test_get_upgrade_result(void)
{
    int result = nwy_get_upgrade_result();

    printf("nwy_get_upgrade_result() => %d (%s)\n",
           result, fota_result_name(result));
    return 0;
}

static int test_get_fota_result(void)
{
    int result = nwy_get_fota_result();

    printf("nwy_get_fota_result() => %d (%s)\n",
           result, fota_result_name(result));
    return 0;
}

static int test_get_active_system_slot(void)
{
    char input[INPUT_BUFFER_SIZE];
    char active_slot = '?';
    unsigned int timeout_msec = DEFAULT_TIMEOUT_MSEC;
    int result;

    if (read_line("Timeout in milliseconds [3000]: ", input, sizeof(input)) == 0) {
        if ((input[0] != '\0') && (parse_unsigned(input, &timeout_msec) != 0)) {
            printf("Invalid timeout; using %d ms.\n", DEFAULT_TIMEOUT_MSEC);
            timeout_msec = DEFAULT_TIMEOUT_MSEC;
        }
    }

    if (timeout_msec > INT_MAX) {
        printf("Timeout exceeds INT_MAX; API call skipped.\n");
    } else {
        result = nwy_get_active_system_slot(&active_slot, (int)timeout_msec);
        printf("nwy_get_active_system_slot() => ret=%d, slot=%c (0x%02x)\n",
               result,
               (active_slot >= ' ') ? active_slot : '?',
               (unsigned int)(unsigned char)active_slot);
    }

    return 0;
}

static int test_get_damaged_state(void)
{
    int result = nwy_get_damaged_state();

    printf("nwy_get_damaged_state() => %d", result);
    if (result < 0) {
        printf(" (query failed)\n");
    } else if (result == 0) {
        printf(" (not damaged)\n");
    } else {
        printf(" (damaged)\n");
    }
    return 0;
}

static int test_fota_ua(void)
{
    char package_path[INPUT_BUFFER_SIZE];
    int result;

    if (read_package_path(package_path, sizeof(package_path)) == 0) {
        if (confirm_operation("nwy_fota_ua starts a full-package firmware update.")) {
            result = nwy_fota_ua(package_path);
            printf("nwy_fota_ua('%s') => %d (%s)\n",
                   package_path, result, download_result_name(result));
        }
    }

    return 0;
}

static int test_fota_update(void)
{
    char package_path[INPUT_BUFFER_SIZE];
    unsigned int package_offset = 0U;
    unsigned int package_size = 0U;
    int parameters_valid = 0;
    int result;

    if ((read_package_path(package_path, sizeof(package_path)) == 0) &&
        (read_unsigned("Package offset: ", &package_offset) == 0) &&
        (read_unsigned("Package size: ", &package_size) == 0)) {
        if (package_size == 0U) {
            printf("Package size must be greater than zero.\n");
        } else if (package_offset > (UINT_MAX - package_size)) {
            printf("Package offset plus size overflows unsigned int.\n");
        } else {
            parameters_valid = 1;
        }
    }

    if (parameters_valid &&
        confirm_operation("nwy_fota_update starts a montage-package firmware update.")) {
        result = nwy_fota_update(package_path, package_offset, package_size);
        printf("nwy_fota_update('%s', %u, %u) => %d (%s)\n",
               package_path, package_offset, package_size,
               result, download_result_name(result));
    }

    return 0;
}

static int test_set_system_sync(void)
{
    int result;

    if (confirm_operation("nwy_set_system_sync changes A/B system synchronization state.")) {
        result = nwy_set_system_sync();
        printf("nwy_set_system_sync() => %d\n", result);
    }
    return 0;
}

static int test_set_active_system_slot(void)
{
    unsigned int selection = 0U;
    char active_slot = NWY_SYSTEM_SINGLE;
    int slot_valid = 0;
    int result;

    printf(" [1] NWY_SYSTEM_A ('a')\n");
    printf(" [2] NWY_SYSTEM_B ('b')\n");
    if (read_unsigned("Select active system slot: ", &selection) == 0) {
        if (selection == 1U) {
            active_slot = NWY_SYSTEM_A;
            slot_valid = 1;
        } else if (selection == 2U) {
            active_slot = NWY_SYSTEM_B;
            slot_valid = 1;
        } else {
            printf("Invalid slot selection.\n");
        }
    }

    if (slot_valid &&
        confirm_operation("Changing the active A/B slot affects the next boot.")) {
        result = nwy_set_active_system_slot(active_slot);
        printf("nwy_set_active_system_slot('%c') => %d\n", active_slot, result);
    }
    return 0;
}

static const test_command_t g_commands[] = {
    {1, "nwy_get_upgrade_result", "Query A/B control upgrade result", test_get_upgrade_result},
    {2, "nwy_get_fota_result", "Query FOTA update state", test_get_fota_result},
    {3, "nwy_get_active_system_slot", "Query the active A/B slot", test_get_active_system_slot},
    {4, "nwy_get_damaged_state", "Query system damaged state", test_get_damaged_state},
    {5, "nwy_fota_ua", "Start full-package update [DANGEROUS]", test_fota_ua},
    {6, "nwy_fota_update", "Start montage-package update [DANGEROUS]", test_fota_update},
    {7, "nwy_set_system_sync", "Set A/B synchronization [DANGEROUS]", test_set_system_sync},
    {8, "nwy_set_active_system_slot", "Change active A/B slot [DANGEROUS]", test_set_active_system_slot},
    {0, "Exit", "Leave the test tool", NULL}
};

static void print_menu(void)
{
    size_t index;

    printf("\n===============================================================\n");
    printf(" N725A FOTA API Manual Test Tool\n");
    printf("===============================================================\n");
    for (index = 0U; index < (sizeof(g_commands) / sizeof(g_commands[0])); index++) {
        printf(" [%d] %-32s %s\n",
               g_commands[index].option,
               g_commands[index].api_name,
               g_commands[index].description);
    }
    printf("===============================================================\n");
}

static int run_menu(void)
{
    unsigned int selection;
    size_t index;
    int found;
    int quit = 0;

    while (!quit) {
        print_menu();
        if (read_unsigned(">> ", &selection) != 0) {
            continue;
        }

        found = 0;
        for (index = 0U; index < (sizeof(g_commands) / sizeof(g_commands[0])); index++) {
            if (selection == (unsigned int)g_commands[index].option) {
                found = 1;
                if (g_commands[index].handler == NULL) {
                    quit = 1;
                } else {
                    (void)g_commands[index].handler();
                }
                break;
            }
        }
        if (!found) {
            printf("Unknown option: %u\n", selection);
        }
    }

    return 0;
}

int main(void)
{
    int result;
    int exit_code = 1;

    printf("N725A FOTA API manual test tool\n");
    printf("Read-only APIs are options 1-4. Options 5-8 can alter firmware or boot state.\n");

    result = nwy_sdk_init();
    printf("nwy_sdk_init() => %d\n", result);
    if (result == 0) {
        (void)run_menu();
        nwy_sdk_deinit();
        printf("nwy_sdk_deinit() done.\n");
        exit_code = 0;
    } else {
        printf("SDK initialization failed; no FOTA API was called.\n");
    }

    return exit_code;
}
