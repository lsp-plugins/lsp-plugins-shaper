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

        typedef struct point_t
        {
            float x, y;
        } point_t;

        void make_bezier(point_t *dst, float a, float b, float s1, float s2)
        {
            point_t v1  = point_t{ a, b };
            point_t v2  = point_t{ a - 1.0f, b - 1.0f };

            dst[0]      = point_t{ 0.0f, 0.0f };
            dst[1]      = point_t{ v1.x * s1, v1.y * s1 };
            dst[2]      = point_t{ 1.0f + v2.x * s2, 1.0f + v2.y * s2 };
            dst[3]      = point_t{ 1.0f, 1.0f };
        }

        point_t bezier_eval(const point_t *vp, size_t np, float t)
        {
            point_t *xp = static_cast<point_t *>(alloca(sizeof(point_t) * (np - 1)));
            const point_t *sp = vp;

            for ( ; np > 1; --np)
            {
                for (size_t i=1; i<np; ++i)
                {
                    xp[i-1].x = sp[i].x * t + sp[i-1].x * (1.0f - t);
                    xp[i-1].y = sp[i].y * t + sp[i-1].y * (1.0f - t);
                }

                sp  = xp;
            }

            return sp[0];
        }

        void make_matrix(double *m, const point_t *bc, size_t np, float a, float b, size_t n)
        {
            // Compute the tangent values
            a   = lsp::lsp_limit(a, 0.0f, 1.0f);
            b   = lsp::lsp_limit(b, 0.0f, 1.0f);

            double k1   = b / a;
            double k2   = (1.0f - b) / (1.0f - a);

            // Allocate matrix
            size_t rs   = n + 1;
            size_t sz   = n * rs;
            for (size_t i=0; i<sz; ++i)
                m[i]        = 0.0;

            double *r   = m;

            // row 0: y(0)
            r[0]        = 0.0;
            r[1]        = 1.0;
            r          += rs;

            // row 1: y'(0)
            r[0]        = k1;
            r[1]        = 0.0;
            r[2]        = 1.0;
            r          += rs;

            // row 2: y'(1)
            r[0]        = k2;
            for (size_t i=0; i<n; ++i)
                r[i+1]      = i;
            r          += rs;

            // row 3: y(1)
            for (size_t i=0; i<rs; ++i)
                r[i]        = 1.0;
            r          += rs;

            // All other rows
            double s    = 1.0 / (n - 3);

            for (size_t j=0; j < n-4; ++j)
            {
                double  t   = (j + 1) * s;
                point_t p   = bezier_eval(bc, np, t);
                double x    = 1.0f;

                r[0]        = p.y;
                for (size_t i=0; i<n; ++i)
                {
                    r[i+1]      = x;
                    x          *= p.x;
                }

                r          += rs;
            }
        }

        void swap_row(double *a, double *b, size_t n)
        {
            for (size_t i=0; i<n; ++i)
            {
                double tmp  = a[i];
                a[i]        = b[i];
                b[i]        = tmp;
            }
        }

        void subtract(double *r, const double *x, double k, size_t n)
        {
            for (size_t i=0; i<n; ++i)
                r[i]       -= x[i] * k;
        }

        void triangulate_matrix(double *m, size_t n)
        {
            const size_t rs   = n + 1;

            for (size_t i=n-1; i>0; --i)
            {
                double *r   = &m[rs * i];

                // Place the row with nonzero value at the specified position
                if (r[i+1] == 0.0)
                {
                    for (double *xr = r - rs; xr >= m; xr -= rs)
                    {
                        if (xr[i+1] != 0.0)
                        {
                            swap_row(r, xr, rs);
                            break;
                        }
                    }
                }

                // Subtract row from others
                for (double *xr = r - rs; xr >= m; xr -= rs)
                {
                    if (xr[i+1] == 0.0f)
                        continue;
                    double k    = xr[i+1] / r[i+1];
                    subtract(xr, r, k, i+2);
                }
            }
        }

        void solve_matrix(float *v, const double *m, size_t n)
        {
            const size_t rs   = n + 1;

            for (size_t i=0; i<n; ++i)
            {
                const double *r = &m[rs * i];
                double s        = r[0];

                for (size_t j=0; j<i; ++j)
                    s          -= r[j + 1] * v[n - j - 1];

                v[n - i - 1]    = s / r[i+1];
            }
        }

        float eval_equation(const float *v, size_t n, float tan, float x)
        {
            float s     = (x < 0.0f) ? -1.0f : 1.0f;
            x           = fabsf(x);

            if (x >= 1.0f)
                return (1.0f + (x - 1.0f) * tan) * s;

            float y     = v[0];
            for (size_t i=1; i<n; ++i)
                y           = y * x + v[i];

            return y * s;
        }

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
            bListen         = false;
            nSync           = SYNC_ALL;
            vMatrix         = NULL;
            vOldRoots       = NULL;
            vRoots          = NULL;
            vInBuffer       = NULL;
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
            fOldOutGain     = 0.0f;
            fOutGain        = 0.0f;

            pBypass         = NULL;
            pGainIn         = NULL;
            pDry            = NULL;
            pWet            = NULL;
            pGainOut        = NULL;

            pHShift         = NULL;
            pVShift         = NULL;
            pTopScale       = NULL;
            pBottomScale    = NULL;
            pOrder          = NULL;
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
            size_t matrix_sz        = align_size(sizeof(double) * meta::shaper::ORDER_MAX * (meta::shaper::ORDER_MAX + 1), OPTIMAL_ALIGN);
            size_t roots_sz         = align_size(sizeof(float) * meta::shaper::ORDER_MAX, OPTIMAL_ALIGN);
            size_t coord_sz         = align_size(sizeof(float) * meta::shaper::GRAPH_DOTS, OPTIMAL_ALIGN);
            size_t alloc            =
                szof_channels +
                buf_sz * 2 +
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
            vMatrix                 = reinterpret_cast<double *>(ptr);
            ptr                    += matrix_sz;
            vOldRoots               = reinterpret_cast<float *>(ptr);
            ptr                    += roots_sz;
            vRoots                  = reinterpret_cast<float *>(ptr);
            ptr                    += roots_sz;
            vInBuffer               = reinterpret_cast<float *>(ptr);
            ptr                    += buf_sz;
            vBuffer                 = reinterpret_cast<float *>(ptr);
            ptr                    += buf_sz;
            vOvsBuffer              = reinterpret_cast<float *>(ptr);
            ptr                    += ovs_buf_sz;
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

                c->sRMSMeter.construct();
                c->sRMSMeter.init(1, meta::shaper::RMS_REACTIVITY);
                c->sRMSMeter.set_mode(dspu::SCM_RMS);
                c->sRMSMeter.set_reactivity(meta::shaper::RMS_REACTIVITY);
                c->sRMSMeter.set_gain(1.0f);
                c->sRMSMeter.set_source(dspu::SCS_MIDDLE);

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
            pOrder              = TRACE_PORT(ports[port_id++]);
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
                c->pRmsOut              = TRACE_PORT(ports[port_id++]);
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
                c->sRMSMeter.set_sample_rate(sr);
            }
        }

        void shaper::update_settings()
        {
            bool bypass             = pBypass->value() >= 0.5f;

            // Update common settings
            fInGain                 = pGainIn->value();
            fOutGain                = pGainOut->value();
            fDryGain                = pDry->value();
            fWetGain                = pWet->value();
            bListen                 = pListen->value() >= 0.5f;

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
            size_t order            = meta::shaper::ORDER_MIN + pOrder->value() + 1;

            if ((h_shift != fHShift) ||
                (v_shift != fVShift) ||
                (t_scale != fTopScale) ||
                (b_scale != fBottomScale) ||
                (order != nOrder))
            {
                // Copy data for the old state
                nOldOrder               = nOrder;
                fOldTangent             = fTangent;
                dsp::copy(vOldRoots, vRoots, meta::shaper::ORDER_MAX);

                // Update parameters
                bCrossfade              = nOrder > 0;
                fHShift                 = h_shift;
                fVShift                 = v_shift;
                fTopScale               = t_scale;
                fBottomScale            = b_scale;
                nOrder                  = order;
                fTangent                = (1.0f - v_shift) / (1.0f - h_shift);

                // Make bezier curve
                point_t bc[4];
                make_bezier(bc, h_shift, v_shift, b_scale, t_scale);
                make_matrix(vMatrix, bc, 4, h_shift, v_shift, order);
                triangulate_matrix(vMatrix, order);
                solve_matrix(vRoots, vMatrix, order);

                // Recompute mesh graph
                for (size_t i=0; i < meta::shaper::GRAPH_DOTS; ++i)
                    vLinGraph[i]            = eval_equation(vRoots, nOrder, fTangent, vLinCoord[i]);
                for (size_t i=0; i < meta::shaper::GRAPH_DOTS; ++i)
                    vLogGraph[i]            = eval_equation(vRoots, nOrder, fTangent, vLogCoord[i]);

                // Mark meshes for sync
                nSync                   = SYNC_ALL;
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
            }

            // Do the main processing stuff
            for (size_t offset=0; offset<samples; )
            {
                size_t to_do                = lsp_min(samples - offset, BUFFER_SIZE);
                size_t ovs_to_do            = to_do * vChannels[0].sOver.get_oversampling();

                // Process each channel independently
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c            = &vChannels[i];

                    // Apply input gain and process oversampled data
                    dsp::lramp2(vInBuffer, c->vIn, fOldInGain, fInGain, to_do);
                    c->pMeterIn->set_value(dsp::abs_max(vInBuffer, to_do));

                    c->sRMSMeter.process(vOvsBuffer, const_cast<const float **>(&vInBuffer), to_do);
                    float rms_in            = dsp::abs_max(vOvsBuffer, to_do);

                    c->sOver.upsample(vOvsBuffer, vInBuffer, to_do);

                    if (bCrossfade)
                    {
                        float k             = 1.0f / ovs_to_do;
                        for (size_t j=0; j<ovs_to_do; ++j)
                        {
                            float s             = vOvsBuffer[j];
                            float t             = j * k;
                            float s_old         = eval_equation(vOldRoots, nOldOrder, fOldTangent, s);
                            float s_new         = eval_equation(vRoots, nOrder, fTangent, s);
                            vOvsBuffer[j]       = s_old + (s_new - s_old) * t;
                        }
                    }
                    else
                    {
                        for (size_t j=0; j<ovs_to_do; ++j)
                            vOvsBuffer[j]           = eval_equation(vRoots, nOrder, fTangent, vOvsBuffer[j]);
                    }

                    c->sOver.downsample(vBuffer, vOvsBuffer, to_do);

                    // Apply latency compensation to dry signal and mix dry/wet signal
                    c->sDryDelay.process(vInBuffer, vInBuffer, to_do);
                    if (!bListen)
                    {
                        dsp::lramp1(vBuffer, fOldWetGain, fWetGain, to_do);
                        dsp::lramp_add2(vBuffer, vInBuffer, fOldDryGain, fDryGain, to_do);
                    }
                    else
                        dsp::sub2(vBuffer, vInBuffer, to_do);
                    c->pMeterOut->set_value(dsp::abs_max(vBuffer, to_do));

                    c->sRMSMeter.process(vOvsBuffer, const_cast<const float **>(&vBuffer), to_do);
                    float rms_out            = dsp::abs_max(vOvsBuffer, to_do);

                    dsp::lramp1(vBuffer, fOldOutGain, fOutGain, to_do);

                    // Apply bypass
                    c->sBypass.process(c->vOut, c->vIn, vBuffer, to_do);

                    c->pRmsOut->set_value((rms_in >= GAIN_AMP_M_72_DB) ? rms_out / rms_in : GAIN_AMP_0_DB);
                }

                // Update variables
                fOldInGain              = fInGain;
                fOldDryGain             = fDryGain;
                fOldWetGain             = fWetGain;
                fOldOutGain             = fOutGain;
                bCrossfade              = false;

                // Update pointers
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c            = &vChannels[i];

                    c->vIn                 += to_do;
                    c->vOut                += to_do;
                }
                offset                 += to_do;
            }

            // Transfer mesh state if needed
            sync_meshes();
        }

        void shaper::sync_meshes()
        {
            if (nSync & SYNC_LIN)
            {
                plug::mesh_t *m   = pLinMesh->buffer<plug::mesh_t>();
                if ((m != NULL) && (m->isEmpty()))
                {
                    dsp::copy(m->pvData[0], vLinCoord, meta::shaper::GRAPH_DOTS);
                    dsp::copy(m->pvData[1], vLinGraph, meta::shaper::GRAPH_DOTS);
                    m->data(2, meta::shaper::GRAPH_DOTS);

                    nSync      &= ~SYNC_LIN;
                }
            }
            if (nSync & SYNC_LOG)
            {
                plug::mesh_t *m   = pLogMesh->buffer<plug::mesh_t>();
                if ((m != NULL) && (m->isEmpty()))
                {
                    dsp::copy(m->pvData[0], vLogCoord, meta::shaper::GRAPH_DOTS);
                    dsp::copy(m->pvData[1], vLogGraph, meta::shaper::GRAPH_DOTS);
                    m->data(2, meta::shaper::GRAPH_DOTS);

                    nSync      &= ~SYNC_LOG;
                }
            }
        }

        void shaper::ui_activated()
        {
            nSync          = SYNC_ALL;
        }

        bool shaper::inline_display(plug::ICanvas *cv, size_t width, size_t height)
        {
            // TODO
            return false;
        }

        void shaper::dump(dspu::IStateDumper *v) const
        {
            // TODO
        }

    } /* namespace plugins */
} /* namespace lsp */


