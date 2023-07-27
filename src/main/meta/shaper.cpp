/*
 * Copyright (C) 2023 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2023 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-shaper
 * Created on: 26 июл 2023 г.
 *
 * lsp-plugins-shaper is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-shaper is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-shaper. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/shaper.h>

#define LSP_PLUGINS_SHAPER_VERSION_MAJOR       1
#define LSP_PLUGINS_SHAPER_VERSION_MINOR       0
#define LSP_PLUGINS_SHAPER_VERSION_MICRO       0

#define LSP_PLUGINS_SHAPER_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_SHAPER_VERSION_MAJOR, \
        LSP_PLUGINS_SHAPER_VERSION_MINOR, \
        LSP_PLUGINS_SHAPER_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        static const port_item_t oversampling_mode[] =
        {
            { "None",                   "oversampler.none"                  },
            { "2x/16bit",               "oversampler.normal.2x16bit"        },
            { "2x/24bit",               "oversampler.normal.2x24bit"        },
            { "3x/16bit",               "oversampler.normal.3x16bit"        },
            { "3x/24bit",               "oversampler.normal.3x24bit"        },
            { "4x/16bit",               "oversampler.normal.4x16bit"        },
            { "4x/24bit",               "oversampler.normal.4x24bit"        },
            { "6x/16bit",               "oversampler.normal.6x16bit"        },
            { "6x/24bit",               "oversampler.normal.6x24bit"        },
            { "8x/16bit",               "oversampler.normal.8x16bit"        },
            { "8x/24bit",               "oversampler.normal.8x24bit"        },
            { NULL,                     NULL}
        };

        static const port_item_t approximation_orders[] =
        {
            { "3rd order",              "shaper.approximation.3rd_order"    },
            { "4th order",              "shaper.approximation.4th_order"    },
            { "5th order",              "shaper.approximation.5th_order"    },
            { "6th order",              "shaper.approximation.6th_order"    },
            { "7th order",              "shaper.approximation.7th_order"    },
            { "8th order",              "shaper.approximation.8th_order"    },
            { "9th order",              "shaper.approximation.9th_order"    },
            { "10th order",             "shaper.approximation.10th_order"   },
            { "11th order",             "shaper.approximation.11th_order"   },
            { "12th order",             "shaper.approximation.12th_order"   },
            { NULL,                     NULL}
        };

        //-------------------------------------------------------------------------
        // Plugin metadata

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t shaper_mono_ports[] =
        {
            // Input and output audio ports
            PORTS_MONO_PLUGIN,

            // Input controls
            BYPASS,
            IN_GAIN,
            DRY_GAIN(0.0f),
            WET_GAIN(1.0f),
            OUT_GAIN,

            // Shaping controls
            CONTROL("hshift", "Horizontal shift", U_NONE, shaper::SHIFT),
            CONTROL("vshift", "Vertical shift", U_NONE, shaper::SHIFT),
            CONTROL("tscale", "Top scale", U_NONE, shaper::SCALE),
            CONTROL("bscale", "Bottom scale", U_NONE, shaper::SCALE),
            COMBO("order", "Approximation order", shaper::ORDER_DFL, approximation_orders),
            COMBO("ovs", "Oversampling", 0, oversampling_mode),
            SWITCH("listen", "Listen effect", 0.0f),
            MESH("gr_lin", "Linear graph", 2, shaper::GRAPH_DOTS),
            MESH("gr_log", "Logarithmic graph", 2, shaper::GRAPH_DOTS),

            // Meters
            METER_GAIN("min", "Input gain", GAIN_AMP_P_48_DB),
            METER_GAIN("mout", "Output gain", GAIN_AMP_P_48_DB),
            METER_GAIN_DFL("rms", "RMS difference meter", GAIN_AMP_P_24_DB, GAIN_AMP_0_DB),

            PORTS_END
        };

        // NOTE: Port identifiers should not be longer than 7 characters as it will overflow VST2 parameter name buffers
        static const port_t shaper_stereo_ports[] =
        {
            // Input and output audio ports
            PORTS_STEREO_PLUGIN,

            // Input controls
            BYPASS,
            IN_GAIN,
            DRY_GAIN(0.0f),
            WET_GAIN(1.0f),
            OUT_GAIN,

            // Shaping controls
            CONTROL("hshift", "Horizontal shift", U_NONE, shaper::SHIFT),
            CONTROL("vshift", "Vertical shift", U_NONE, shaper::SHIFT),
            CONTROL("tscale", "Top scale", U_NONE, shaper::SCALE),
            CONTROL("bscale", "Bottom scale", U_NONE, shaper::SCALE),
            COMBO("order", "Approximation order", shaper::ORDER_DFL, approximation_orders),
            COMBO("ovs", "Oversampling", 0, oversampling_mode),
            SWITCH("listen", "Listen effect", 0.0f),
            MESH("gr_lin", "Linear graph", 2, shaper::GRAPH_DOTS),
            MESH("gr_log", "Logarithmic graph", 2, shaper::GRAPH_DOTS),

            // Meters
            METER_GAIN("min_l", "Input gain Left", GAIN_AMP_P_48_DB),
            METER_GAIN("mout_l", "Output gain Left", GAIN_AMP_P_48_DB),
            METER_GAIN_DFL("rms_l", "RMS difference meter Left", GAIN_AMP_P_24_DB, GAIN_AMP_0_DB),
            METER_GAIN("min_r", "Input gain Right", GAIN_AMP_P_48_DB),
            METER_GAIN("mout_r", "Output gain Right", GAIN_AMP_P_48_DB),
            METER_GAIN_DFL("rms_r", "RMS difference meter Right", GAIN_AMP_P_24_DB, GAIN_AMP_0_DB),

            PORTS_END
        };

        static const int plugin_classes[]       = { C_WAVESHAPER, -1 };
        static const int clap_features_mono[]   = { CF_AUDIO_EFFECT, CF_DISTORTION, CF_MONO, -1 };
        static const int clap_features_stereo[] = { CF_AUDIO_EFFECT, CF_DISTORTION, CF_STEREO, -1 };

        const meta::bundle_t shaper_bundle =
        {
            "shaper",
            "Shaper plugin",
            B_EFFECTS,
            "This plugin performs some additional wave shaping of the audio signal",
            "" // TODO: write plugin description, should be the same to the english version in 'bundles.json'
        };

        const plugin_t shaper_mono =
        {
            "Shaper Mono",
            "Shaper Mono",
            "SH1M",
            &developers::v_sadovnikov,
            "shaper_mono",
            LSP_LV2_URI("shaper_mono"),
            LSP_LV2UI_URI("shaper_mono"),
            "shpm",
            LSP_LADSPA_SHAPER_BASE + 0,
            LSP_LADSPA_URI("shaper_mono"),
            LSP_CLAP_URI("shaper_mono"),
            LSP_PLUGINS_SHAPER_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            shaper_mono_ports,
            "effects/shaper.xml",
            NULL,
            mono_plugin_port_groups,
            &shaper_bundle
        };

        const plugin_t shaper_stereo =
        {
            "Shaper Stereo",
            "Shaper Stereo",
            "SH1S",
            &developers::v_sadovnikov,
            "shaper_stereo",
            LSP_LV2_URI("shaper_stereo"),
            LSP_LV2UI_URI("shaper_stereo"),
            "shps",
            LSP_LADSPA_SHAPER_BASE + 1,
            LSP_LADSPA_URI("shaper_stereo"),
            LSP_CLAP_URI("shaper_stereo"),
            LSP_PLUGINS_SHAPER_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE | E_INLINE_DISPLAY,
            shaper_stereo_ports,
            "effects/shaper.xml",
            NULL,
            stereo_plugin_port_groups,
            &shaper_bundle
        };
    } /* namespace meta */
} /* namespace lsp */



