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

#ifndef PRIVATE_PLUGINS_SHAPER_H_
#define PRIVATE_PLUGINS_SHAPER_H_

#include <lsp-plug.in/dsp-units/ctl/Bypass.h>
#include <lsp-plug.in/dsp-units/util/Delay.h>
#include <lsp-plug.in/dsp-units/util/Oversampler.h>
#include <lsp-plug.in/plug-fw/plug.h>
#include <private/meta/shaper.h>

namespace lsp
{
    namespace plugins
    {
        /**
         * Base class for the latency compensation delay
         */
        class shaper: public plug::Module
        {
            private:
                shaper & operator = (const shaper &);
                shaper (const shaper &);

            protected:
                typedef struct channel_t
                {
                    dspu::Bypass        sBypass;            // Bypass
                    dspu::Oversampler   sOver;              // Oversampler
                    dspu::Delay         sDryDelay;          // Dry delay

                    float              *vIn;                // Input data
                    float              *vOut;               // Output data

                    plug::IPort        *pIn;                // Input port
                    plug::IPort        *pOut;               // Output port
                    plug::IPort        *pMeterIn;           // Input meter
                    plug::IPort        *pMeterOut;          // Output meter
                } channel_t;

                enum mesh_sync_t
                {
                    SYNC_LIN            = 1 << 0,
                    SYNC_LOG            = 1 << 1,

                    SYNC_ALL            = SYNC_LIN | SYNC_LOG
                };

            protected:
                static dspu::over_mode_t        all_oversampling_modes[];

            protected:
                size_t              nChannels;          // Number of channels
                channel_t          *vChannels;          // Audio channels
                size_t              nOldOrder;          // Old approximation order
                size_t              nOrder;             // Actual approximation order
                bool                bCrossfade;         // Crossfade mode
                size_t              nSync;              // Mesh sync
                double             *vMatrix;            // Matrix buffer
                float              *vOldRoots;          // Old equation roots
                float              *vRoots;             // Actual Equation roots
                float              *vBuffer;            // Temporary buffer
                float              *vOvsBuffer;         // Temporary buffer oversampled
                float              *vLinCoord;          // Linear graph coordinates
                float              *vLinGraph;          // Linear graph coordinates
                float              *vLogCoord;          // Logarithmic graph coordinates
                float              *vLogGraph;          // Logarithmic graph coordinates

                float               fHShift;            // Horizontal shift value
                float               fVShift;            // Vertical shift value
                float               fTopScale;          // Top scale
                float               fBottomScale;       // Bottom scale
                float               fOldTangent;        // Old tangent value
                float               fTangent;           // Actual tangent value
                float               fOldInGain;         // Old input gain
                float               fInGain;            // Actual input gain
                float               fOldDryGain;        // Old dry gain
                float               fDryGain;           // Actual dry gain
                float               fOldWetGain;        // Old wet gain
                float               fWetGain;           // Actual wet gain

                plug::IPort        *pBypass;            // Bypass
                plug::IPort        *pGainIn;            // Input gain
                plug::IPort        *pDry;               // Dry gain
                plug::IPort        *pWet;               // Wet gain
                plug::IPort        *pGainOut;           // Output gain

                plug::IPort        *pHShift;            // Horizontal shift
                plug::IPort        *pVShift;            // Vertical shift
                plug::IPort        *pTopScale;          // Top scale
                plug::IPort        *pBottomScale;       // Bottom scale
                plug::IPort        *pOversampling;      // Oversampling
                plug::IPort        *pListen;            // Listen
                plug::IPort        *pLinMesh;           // Linear mesh output
                plug::IPort        *pLogMesh;           // Logarithmic mesh output

                uint8_t            *pData;              // Allocated data

            protected:
                void                sync_meshes();

            public:
                explicit shaper(const meta::plugin_t *meta);
                virtual ~shaper() override;

                virtual void        init(plug::IWrapper *wrapper, plug::IPort **ports) override;
                virtual void        destroy() override;

            public:
                virtual void        update_sample_rate(long sr) override;
                virtual void        update_settings() override;
                virtual void        process(size_t samples) override;
                virtual void        ui_activated() override;
                virtual bool        inline_display(plug::ICanvas *cv, size_t width, size_t height) override;
                virtual void        dump(dspu::IStateDumper *v) const override;
        };

    } /* namespace plugins */
} /* namespace lsp */


#endif /* PRIVATE_PLUGINS_SHAPER_H_ */

