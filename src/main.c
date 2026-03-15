#include <stdio.h>

#include "btstack.h"
#include "picokey_pairing_monitor.h"
#include "picokey_usb_hid.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

// Implemented by BTstack example source linked from the Pico SDK.
int btstack_main(int argc, const char *argv[]);

static btstack_timer_source_t usb_hid_task_timer;

static void usb_hid_task_timer_handler(btstack_timer_source_t *ts)
{
    picokey_usb_hid_task();
    btstack_run_loop_set_timer(ts, 2);
    btstack_run_loop_add_timer(ts);
}

int main(void)
{
    stdio_init_all();

    picokey_usb_hid_init();

    if (cyw43_arch_init()) {
        printf("failed to initialise cyw43_arch\n");
        return 1;
    }

    btstack_main(0, NULL);
    picokey_pairing_monitor_init();

    btstack_run_loop_set_timer_handler(&usb_hid_task_timer, usb_hid_task_timer_handler);
    btstack_run_loop_set_timer(&usb_hid_task_timer, 2);
    btstack_run_loop_add_timer(&usb_hid_task_timer);

    btstack_run_loop_execute();

    return 0;
}