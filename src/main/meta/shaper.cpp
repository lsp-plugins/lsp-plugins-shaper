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
            { "None",                   "flanger.oversampler.none"          },
            { "2X Medium",              "flanger.oversampler.2x_medium"     },
            { "2X High",                "flanger.oversampler.2x_high"       },
            { "3X Medium",              "flanger.oversampler.3x_medium"     },
            { "3X High",                "flanger.oversampler.3x_high"       },
            { "4X Medium",              "flanger.oversampler.4x_medium"     },
            { "4X High",                "flanger.oversampler.4x_high"       },
            { "6X Medium",              "flanger.oversampler.6x_medium"     },
            { "6X High",                "flanger.oversampler.6x_high"       },
            { "8X Medium",              "flanger.oversampler.8x_medium"     },
            { "8X High",                "flanger.oversampler.8x_high"       },
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
            COMBO("ovs", "Oversampling", 0, oversampling_mode),
            SWITCH("listen", "Listen effect", 0.0f),
            MESH("gr_lin", "Linear graph", 2, shaper::GRAP_DOTS),
            MESH("gr_log", "Logarithmic graph", 2, shaper::GRAP_DOTS),

            // Meters
            METER_GAIN("g_in", "Input gain", GAIN_AMP_P_48_DB),
            METER_GAIN("g_out", "Output gain", GAIN_AMP_P_48_DB),

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

            // Output controls
            CONTROL("hshift", "Horizontal shift", U_NONE, shaper::SHIFT),
            CONTROL("vshift", "Vertical shift", U_NONE, shaper::SHIFT),
            CONTROL("tscale", "Top scale", U_NONE, shaper::SCALE),
            CONTROL("bscale", "Bottom scale", U_NONE, shaper::SCALE),
            COMBO("ovs", "Oversampling", 0, oversampling_mode),
            SWITCH("listen", "Listen effect", 0.0f),

            // Shaping controls
            CONTROL("hshift", "Horizontal shift", U_NONE, shaper::SHIFT),
            CONTROL("vshift", "Vertical shift", U_NONE, shaper::SHIFT),
            CONTROL("tscale", "Top scale", U_NONE, shaper::SCALE),
            CONTROL("bscale", "Bottom scale", U_NONE, shaper::SCALE),
            COMBO("ovs", "Oversampling", 0, oversampling_mode),
            SWITCH("listen", "Listen effect", 0.0f),
            MESH("gr_lin", "Linear graph", 2, shaper::GRAPH_DOTS),
            MESH("gr_log", "Logarithmic graph", 2, shaper::GRAPH_DOTS),

            // Meters
            METER_GAIN("g_in_l", "Input gain Left", GAIN_AMP_P_48_DB),
            METER_GAIN("g_out_l", "Output gain Left", GAIN_AMP_P_48_DB),
            METER_GAIN("g_in_r", "Input gain Right", GAIN_AMP_P_48_DB),
            METER_GAIN("g_out_r", "Output gain Right", GAIN_AMP_P_48_DB),

            PORTS_END
        };

        static const int plugin_classes[]       = { C_WAVESHAPER, -1 };
        static const int clap_features_mono[]   = { CF_AUDIO_EFFECT, CF_DISTORTION, CF_MONO, -1 };
        static const int clap_features_stereo[] = { CF_AUDIO_EFFECT, CF_DISTORTION, CF_STEREO, -1 };

        const meta::bundle_t shaper_bundle =
        {
            "shaper",
            "Shaper plugin",
            B_UTILITIES,
            "", // TODO: provide ID of the video on YouTube
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
            1,              // TODO: fill valid LADSPA identifier (positive decimal integer)
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
            2,              // TODO: fill valid LADSPA identifier (positive decimal integer)
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



