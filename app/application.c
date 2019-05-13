#include <application.h>
#include <math.h>

bool gps_running;
bc_module_gps_position_t last_valid_position;
bc_module_gps_position_t last_sent_position;
bc_led_t led;
bc_led_t gps_led_r;
bc_led_t gps_led_g;
bc_module_sigfox_t sigfox_module;

static void charging_handler(void *param);
void gps_module_event_handler(bc_module_gps_event_t event, void *event_param);
void sigfox_module_event_handler(bc_module_sigfox_t *self, bc_module_sigfox_event_t event, void *event_param);
void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param);
void gpsToBuffer(uint8_t *buf, float latitude, float longitude);
float distance(float lat1, float lon1, float lat2, float lon2);

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

    gps_running = false;
    last_valid_position.latitude = 0;
    last_valid_position.longitude = 0;
    last_sent_position.latitude = 0;
    last_sent_position.longitude = 0;

    // LTC4150 Coulomb Counter
    // POL
    bc_gpio_init(BC_GPIO_P17);
    bc_gpio_set_mode(BC_GPIO_P17, BC_GPIO_MODE_INPUT);
    // INT
    bc_gpio_init(BC_GPIO_P16);
    bc_gpio_set_mode(BC_GPIO_P16, BC_GPIO_MODE_INPUT);
    // VIO
    bc_gpio_init(BC_GPIO_P15);
    bc_gpio_set_mode(BC_GPIO_P15, BC_GPIO_MODE_OUTPUT);
    bc_gpio_set_output(BC_GPIO_P15, 1);
    // GND
    bc_gpio_init(BC_GPIO_P14);
    bc_gpio_set_mode(BC_GPIO_P14, BC_GPIO_MODE_OUTPUT);
    bc_gpio_set_output(BC_GPIO_P14, 0);
    // Periodically check polarity
    bc_scheduler_register(charging_handler, NULL, 0);
}

void application_task(void)
{
    if (last_valid_position.latitude != 0 && last_valid_position.longitude != 0 &&
        (last_valid_position.latitude != last_sent_position.latitude ||
        last_valid_position.longitude != last_sent_position.longitude) &&
        (last_sent_position.latitude == 0 ||
        distance(last_valid_position.latitude, last_valid_position.longitude,
            last_sent_position.latitude, last_sent_position.longitude) > 100.0))
    {
        uint8_t buffer[8];
        gpsToBuffer(buffer, last_valid_position.latitude, last_valid_position.longitude);
        if (bc_module_sigfox_send_rf_frame(&sigfox_module, buffer, sizeof(buffer)))
        {
            last_sent_position.latitude = last_valid_position.latitude;
            last_sent_position.longitude = last_valid_position.longitude;
        }
        else
        {
            bc_scheduler_plan_current_relative(10 * 1000);
        }
    }

    bc_scheduler_plan_current_relative(5 * 60 * 1000);
}

static void charging_handler(void *param)
{
    uint8_t polarity = bc_gpio_get_input(BC_GPIO_P17);
    if (!gps_running && polarity)
    {
        gps_running = true;
        bc_module_gps_start();
    }
    else if (gps_running && !polarity)
    {
        gps_running = false;
        bc_module_gps_stop();
    }
    bc_scheduler_plan_current_relative(1000);
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
        bc_module_gps_position_t position;

        if (bc_module_gps_get_position(&position))
        {
            last_valid_position.latitude = position.latitude;
            last_valid_position.longitude = position.longitude;
        }

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
        last_sent_position.latitude = 0;
        last_sent_position.longitude = 0;
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

// https://www.movable-type.co.uk/scripts/latlong.html

float deg2rad(float degrees)
{
    return degrees * 3.141592653589793238462643383279502884 / 180.0;
}

// Distance in meters between earth coordinates
float distance(float lat1, float lon1, float lat2, float lon2)
{
    float earthRadiusKm = 6371;

    float d_lat = deg2rad(lat2 - lat1);
    float d_lon = deg2rad(lon2 - lon1);

    lat1 = deg2rad(lat1);
    lat2 = deg2rad(lat2);

    float a = sinf(d_lat / 2) * sinf(d_lat / 2) +
            sinf(d_lon / 2) * sinf(d_lon / 2) * cosf(lat1) * cosf(lat2);
    float c = 2 * atan2f(sqrtf(a), sqrtf(1 - a));
    return earthRadiusKm * c * 1000.0;
}
