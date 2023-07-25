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

#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/plug-fw/meta/func.h>

#include <private/plugins/shaper.h>

namespace lsp
{
    static plug::IPort *TRACE_PORT(plug::IPort *p)
    {
        lsp_trace("  port id=%s", (p)->metadata()->id);
        return p;
    }

    inline namespace
    {
        /* The size of temporary buffer for audio processing */
        constexpr size_t BUFFER_SIZE  = 0x200;

    } /* namespace */

    namespace plugins
    {
        //---------------------------------------------------------------------
        // Plugin factory
        static const meta::plugin_t *plugins[] =
        {
            &meta::shaper_mono,
            &meta::shaper_stereo
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            return new shaper(meta);
        }

        static plug::Factory factory(plugin_factory, plugins, 2);

        //---------------------------------------------------------------------
        dspu::over_mode_t shaper::all_oversampling_modes[] =
        {
            dspu::over_mode_t::OM_NONE,
            dspu::over_mode_t::OM_LANCZOS_2X16BIT,
            dspu::over_mode_t::OM_LANCZOS_2X24BIT,
            dspu::over_mode_t::OM_LANCZOS_3X16BIT,
            dspu::over_mode_t::OM_LANCZOS_3X24BIT,
            dspu::over_mode_t::OM_LANCZOS_4X16BIT,
            dspu::over_mode_t::OM_LANCZOS_4X24BIT,
            dspu::over_mode_t::OM_LANCZOS_6X16BIT,
            dspu::over_mode_t::OM_LANCZOS_6X24BIT,
            dspu::over_mode_t::OM_LANCZOS_8X16BIT,
            dspu::over_mode_t::OM_LANCZOS_8X24BIT
        };

        //---------------------------------------------------------------------
        // Implementation
        shaper::shaper(const meta::plugin_t *meta):
            Module(meta)
        {
            // Compute the number of audio channels by the number of inputs
            nChannels       = 0;
            for (const meta::port_t *p = meta->ports; p->id != NULL; ++p)
                if (meta::is_audio_in_port(p))
                    ++nChannels;

            // Initialize fields
            vChannels       = NULL;
            nOldOrder       = 0;
            nOrder          = 0;
            bCrossfade      = false;
            vMatrix         = NULL;
            vOldRoots       = NULL;
            vRoots          = NULL;
            vBuffer         = NULL;
            vOvsBuffer      = NULL;
            vLinCoord       = NULL;
            vLinGraph       = NULL;
            vLogCoord       = NULL;
            vLogGraph       = NULL;

            fHShift         = 0.0f;
            fVShift         = 0.0f;
            fTopScale       = 0.0f;
            fBottomScale    = 0.0f;
            fOldTangent     = 0.0f;
            fTangent        = 0.0f;
            fOldInGain      = 0.0f;
            fInGain         = 0.0f;
            fOldDryGain     = 0.0f;
            fDryGain        = 0.0f;
            fOldWetGain     = 0.0f;
            fWetGain        = 0.0f;

            pBypass         = NULL;
            pGainIn         = NULL;
            pDry            = NULL;
            pWet            = NULL;
            pGainOut        = NULL;

            pHShift         = NULL;
            pVShift         = NULL;
            pTopScale       = NULL;
            pBottomScale    = NULL;
            pOversampling   = NULL;
            pListen         = NULL;
            pLinMesh        = NULL;
            pLogMesh        = NULL;

            pData           = NULL;
        }

        shaper::~shaper()
        {
            destroy();
        }

        void shaper::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            // Call parent class for initialization
            Module::init(wrapper, ports);

            // Estimate the number of bytes to allocate
            size_t szof_channels    = align_size(sizeof(channel_t) * nChannels, OPTIMAL_ALIGN);
            size_t buf_sz           = BUFFER_SIZE * sizeof(float);
            size_t ovs_buf_sz       = buf_sz * meta::shaper::OVERSAMPLING_MAX;
            size_t matrix_sz        = align_size(sizeof(float) * meta::shaper::ORDER_MAX * (meta::shaper::ORDER_MAX + 1), OPTIMAL_ALIGN);
            size_t roots_sz         = align_size(sizeof(float) * meta::shaper::ORDER_MAX, OPTIMAL_ALIGN);
            size_t coord_sz         = align_size(sizeof(float) * meta::shaper::GRAPH_DOTS, OPTIMAL_ALIGN);
            size_t alloc            =
                szof_channels +
                buf_sz +
                ovs_buf_sz +
                matrix_sz +
                roots_sz * 2 +
                coord_sz * 4;

            // Allocate memory-aligned data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;

            // Initialize pointers to channels and temporary buffer
            vChannels               = reinterpret_cast<channel_t *>(ptr);
            ptr                    += szof_channels;
            vMatrix                 = reinterpret_cast<float *>(matrix_sz);
            ptr                    += matrix_sz;
            vOldRoots               = reinterpret_cast<float *>(roots_sz);
            ptr                    += roots_sz;
            vRoots                  = reinterpret_cast<float *>(roots_sz);
            ptr                    += roots_sz;
            vBuffer                 = reinterpret_cast<float *>(ptr);
            ptr                    += buf_sz;
            vOvsBuffer              = reinterpret_cast<float *>(ptr);
            ptr                    += buf_sz;
            vLinCoord               = reinterpret_cast<float *>(ptr);
            ptr                    += coord_sz;
            vLinGraph               = reinterpret_cast<float *>(ptr);
            ptr                    += coord_sz;
            vLogCoord               = reinterpret_cast<float *>(ptr);
            ptr                    += coord_sz;
            vLogGraph               = reinterpret_cast<float *>(ptr);
            ptr                    += coord_sz;

            dsp::fill_zero(vOldRoots, meta::shaper::ORDER_MAX);
            dsp::fill_zero(vRoots, meta::shaper::ORDER_MAX);

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                c->sBypass.construct();
                c->sOver.construct();
                if (!c->sOver.init())
                    return;
                c->sDryDelay.construct();
                if (!c->sDryDelay.init(meta::shaper::OVERSAMPLING_MAX + BUFFER_SIZE * 2))
                    return;

                c->vIn                  = NULL;
                c->vOut                 = NULL;

                c->pIn                  = NULL;
                c->pOut                 = NULL;
                c->pMeterIn             = NULL;
                c->pMeterOut            = NULL;
            }

            // Bind ports
            lsp_trace("Binding ports");
            size_t port_id      = 0;

            // Bind input audio ports
            for (size_t i=0; i<nChannels; ++i)
                vChannels[i].pIn    = TRACE_PORT(ports[port_id++]);

            // Bind output audio ports
            for (size_t i=0; i<nChannels; ++i)
                vChannels[i].pOut   = TRACE_PORT(ports[port_id++]);

            // Bind common ports
            lsp_trace("Binding common ports");
            pBypass             = TRACE_PORT(ports[port_id++]);
            pGainIn             = TRACE_PORT(ports[port_id++]);
            pDry                = TRACE_PORT(ports[port_id++]);
            pWet                = TRACE_PORT(ports[port_id++]);
            pGainOut            = TRACE_PORT(ports[port_id++]);

            pHShift             = TRACE_PORT(ports[port_id++]);
            pVShift             = TRACE_PORT(ports[port_id++]);
            pTopScale           = TRACE_PORT(ports[port_id++]);
            pBottomScale        = TRACE_PORT(ports[port_id++]);
            pOversampling       = TRACE_PORT(ports[port_id++]);
            pListen             = TRACE_PORT(ports[port_id++]);
            pLinMesh            = TRACE_PORT(ports[port_id++]);
            pLogMesh            = TRACE_PORT(ports[port_id++]);

            // Bind channel ports
            lsp_trace("Binding channel meters");

            // Bind ports for audio processing channels
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                c->pMeterIn             = TRACE_PORT(ports[port_id++]);
                c->pMeterOut            = TRACE_PORT(ports[port_id++]);
            }

            // Initialize horizontal axis (linear)
            float delta = (meta::shaper::GRAPH_LIN_MAX - meta::shaper::GRAPH_LIN_MIN) / (meta::shaper::GRAPH_DOTS-1);
            for (size_t i=0; i<meta::shaper::GRAPH_DOTS; ++i)
                vLinCoord[i]    = meta::shaper::GRAPH_LIN_MIN + delta * i;

            // Initialize horizontal axis (logarithmic) in range of -72 .. 0 db
            delta = (meta::shaper::GRAPH_DB_MAX - meta::shaper::GRAPH_DB_MIN) / (meta::shaper::GRAPH_DOTS-1);
            for (size_t i=0; i<meta::shaper::GRAPH_DOTS; ++i)
                vLogCoord[i]    = dspu::db_to_gain(meta::shaper::GRAPH_DB_MIN + delta * i);

        }

        void shaper::destroy()
        {
            Module::destroy();

            // Destroy channels
            if (vChannels != NULL)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];

                    c->sBypass.destroy();
                    c->sOver.destroy();
                    c->sDryDelay.destroy();
                }
                vChannels   = NULL;
            }

            // Forget about buffers
            vBuffer     = NULL;
            vOvsBuffer  = NULL;

            // Free previously allocated data chunk
            if (pData != NULL)
            {
                free_aligned(pData);
                pData       = NULL;
            }
        }

        void shaper::update_sample_rate(long sr)
        {
            // Update sample rate for the bypass processors
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];

                c->sBypass.init(sr);
                c->sOver.set_sample_rate(sr);
            }
        }

        void shaper::update_settings()
        {
            float out_gain          = pGainOut->value();
            bool bypass             = pBypass->value() >= 0.5f;

            // Update common settings
            fDryGain                = pDry->value() * out_gain;
            fWetGain                = pWet->value() * out_gain;

            // Update channel settings
            dspu::over_mode_t omode = all_oversampling_modes[size_t(pOversampling->value())];
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                c->sBypass.set_bypass(bypass);

                if (c->sOver.mode() != omode)
                {
                    c->sOver.set_mode(omode);
                    c->sOver.set_filtering(true);
                    c->sOver.update_settings();

                    c->sDryDelay.set_delay(c->sOver.latency());
                    c->sDryDelay.clear();
                }
            }

            // Report latency
            set_latency(vChannels[0].sOver.latency());

            // Check if we need to update approximation curve
            float h_shift           = pHShift->value();
            float v_shift           = pVShift->value();
            float t_scale           = pTopScale->value();
            float b_scale           = pBottomScale->value();

            if ((h_shift != fHShift) ||
                (v_shift != fVShift) ||
                (t_scale != fTopScale) ||
                (b_scale != fBottomScale))
            {
                bCrossfade              = nOrder > 0;

                // Copy data for the old state
                nOldOrder               = nOrder;
                fOldTangent             = fTangent;
                dsp::copy(vOldRoots, vRoots, meta::shaper::ORDER_MAX);

                // TODO: compute the matrix

                // Recompute mesh graph

                // Mark meshes for sync
                nSync                   = SYNC_LIN | SYNC_LOG;
            }

            // Need to update graph values?
            if (nSync)
            {
                // Query inline display for redraw
                pWrapper->query_display_draw();
            }
        }

        void shaper::process(size_t samples)
        {
            // Perform the routing
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];
                c->vIn                  = c->pIn->buffer<float>();
                c->vOut                 = c->pOut->buffer<float>();

                // Measure the input level
                c->pMeterIn->set_value(dsp::abs_max(c->vIn, samples) * fInGain);
            }

            // Transfer mesh state if needed
            sync_meshes();
        }

        void shaper::dump(dspu::IStateDumper *v) const
        {
        }

    } /* namespace plugins */
} /* namespace lsp */


