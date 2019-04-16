#include <application.h>

bool running = false;
bc_module_gps_position_t position;
bc_led_t led;
bc_led_t gps_led_r;
bc_led_t gps_led_g;
bc_module_sigfox_t sigfox_module;

void gps_module_event_handler(bc_module_gps_event_t event, void *event_param);
void sigfox_module_event_handler(bc_module_sigfox_t *self, bc_module_sigfox_event_t event, void *event_param);
void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param);
void gpsToBuffer(uint8_t *buf, float latitude, float longitude);

void application_init(void)
{
    bc_led_init(&led, BC_GPIO_LED, false, false);

    if (!bc_module_gps_init())
    {
        bc_led_set_mode(&led, BC_LED_MODE_BLINK);
    }
    else
    {
        bc_module_gps_set_event_handler(gps_module_event_handler, NULL);
    }

    bc_led_init_virtual(&gps_led_r, BC_MODULE_GPS_LED_RED, bc_module_gps_get_led_driver(), 0);
    bc_led_init_virtual(&gps_led_g, BC_MODULE_GPS_LED_GREEN, bc_module_gps_get_led_driver(), 0);

    bc_module_sigfox_init(&sigfox_module, BC_MODULE_SIGFOX_REVISION_R2);
    bc_module_sigfox_set_event_handler(&sigfox_module, sigfox_module_event_handler, NULL);

    static bc_button_t button;
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);
}

void application_task(void)
{
    if (running)
    {
        bc_module_gps_stop();
        running = false;

        if (position.latitude != 0 && position.longitude != 0)
        {
            uint8_t buffer[8];
            gpsToBuffer(buffer, position.latitude, position.longitude);
            bc_module_sigfox_send_rf_frame(&sigfox_module, buffer, sizeof(buffer));
        }

        bc_scheduler_plan_current_relative(5 * 60 * 1000);
    }
    else
    {
        position.latitude = 0;
        position.longitude = 0;

        bc_module_gps_start();
        running = true;

        bc_scheduler_plan_current_relative(60 * 1000);
    }
}

void gps_module_event_handler(bc_module_gps_event_t event, void *event_param)
{
    if (event == BC_MODULE_GPS_EVENT_START)
    {
        bc_led_set_mode(&gps_led_g, BC_LED_MODE_ON);
    }
    else if (event == BC_MODULE_GPS_EVENT_STOP)
    {
        bc_led_set_mode(&gps_led_g, BC_LED_MODE_OFF);
    }
    else if (event == BC_MODULE_GPS_EVENT_UPDATE)
    {
        bc_module_gps_get_position(&position);

        bc_module_gps_invalidate();
    }
    else if (event == BC_MODULE_GPS_EVENT_ERROR)
    {
        bc_led_set_mode(&gps_led_g, BC_LED_MODE_BLINK);
    }
}

void sigfox_module_event_handler(bc_module_sigfox_t *self, bc_module_sigfox_event_t event, void *event_param)
{
    if (event == BC_MODULE_SIGFOX_EVENT_SEND_RF_FRAME_START)
    {
        bc_led_set_mode(&gps_led_r, BC_LED_MODE_ON);
    }
    else if (event == BC_MODULE_SIGFOX_EVENT_SEND_RF_FRAME_DONE)
    {
        bc_led_set_mode(&gps_led_r, BC_LED_MODE_OFF);
    }
    else if (event == BC_MODULE_SIGFOX_EVENT_ERROR)
    {
        bc_led_set_mode(&gps_led_r, BC_LED_MODE_BLINK);
    }
}

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_scheduler_plan_now(0);
    }
}

// https://github.com/thesolarnomad/lora-serialization

void _intToBytes(uint8_t *buf, int32_t i, uint8_t byteSize)
{
    for (uint8_t x = 0; x < byteSize; x++)
    {
        buf[x] = (uint8_t) (i >> (x*8));
    }
}

void gpsToBuffer(uint8_t *buf, float latitude, float longitude)
{
    int32_t lat = latitude * 1e6;
    int32_t lng = longitude * 1e6;

    _intToBytes(buf, lat, 4);
    _intToBytes(buf + 4, lng, 4);
}
