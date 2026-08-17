#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_RGB     _panel_instance;
    lgfx::Bus_RGB       _bus_instance;
    // TOUCH REMOVED, We will handle touch via ESP-IDF native driver to prevent I2C conflicts!

public:
    LGFX(void)
    {
        auto cfg = _bus_instance.config();
        cfg.panel = &_panel_instance;

        // --- 16-Bit RGB Data Pins ---
        cfg.pin_d0 = 14; cfg.pin_d1 = 38; cfg.pin_d2 = 18; cfg.pin_d3 = 17; cfg.pin_d4 = 10;
        cfg.pin_d5 = 39; cfg.pin_d6 = 0;  cfg.pin_d7 = 45; cfg.pin_d8 = 48; cfg.pin_d9 = 47; cfg.pin_d10 = 21;
        cfg.pin_d11= 1;  cfg.pin_d12= 2;  cfg.pin_d13= 42; cfg.pin_d14= 41; cfg.pin_d15= 40;

        // --- Control Pins ---
        cfg.pin_hsync   = 46;
        cfg.pin_vsync   = 3;
        cfg.pin_henable = 5;
        cfg.pin_pclk    = 7;

        // --- Timings ---
        cfg.freq_write  = 14000000;
        cfg.pclk_active_neg   = 1;
        cfg.hsync_polarity    = 0;
        cfg.hsync_front_porch = 8;
        cfg.hsync_pulse_width = 4;
        cfg.hsync_back_porch  = 8;
        cfg.vsync_polarity    = 0;
        cfg.vsync_front_porch = 8;
        cfg.vsync_pulse_width = 4;
        cfg.vsync_back_porch  = 8;
        
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);

        auto p_cfg = _panel_instance.config();
        p_cfg.memory_width  = 800;
        p_cfg.memory_height = 480;
        p_cfg.panel_width   = 800;
        p_cfg.panel_height  = 480;
        p_cfg.offset_x      = 0;
        p_cfg.offset_y      = 0;
        _panel_instance.config(p_cfg);

        setPanel(&_panel_instance);
    }
};