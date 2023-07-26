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

#ifndef PRIVATE_META_SHAPER_H_
#define PRIVATE_META_SHAPER_H_

#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/const.h>

namespace lsp
{
    //-------------------------------------------------------------------------
    // Plugin metadata
    namespace meta
    {
        typedef struct shaper
        {
            static constexpr size_t GRAPH_DOTS          = 256;
            static constexpr size_t ORDER_MIN           = 4;
            static constexpr size_t ORDER_MAX           = 12;
            static constexpr size_t ORDER_DFL           = 8 - ORDER_MIN;
            static constexpr size_t OVERSAMPLING_MAX    = 8;

            static constexpr float  RMS_REACTIVITY      = 40.0f;

            static constexpr float  GRAPH_DB_MIN        = -72.0f;
            static constexpr float  GRAPH_DB_MAX        = 0.0f;
            static constexpr float  GRAPH_LIN_MIN       = 0.0f;
            static constexpr float  GRAPH_LIN_MAX       = 1.0f;

            static constexpr float  SHIFT_MIN           = 0.1f;
            static constexpr float  SHIFT_MAX           = 0.9f;
            static constexpr float  SHIFT_DFL           = 0.5f;
            static constexpr float  SHIFT_STEP          = 0.0005f;

            static constexpr float  SCALE_MIN           = 0.25f;
            static constexpr float  SCALE_MAX           = 1.75f;
            static constexpr float  SCALE_DFL           = 1.0f;
            static constexpr float  SCALE_STEP          = 0.0005f;
        } shaper;

        // Plugin type metadata
        extern const plugin_t shaper_mono;
        extern const plugin_t shaper_stereo;

    } /* namespace meta */
} /* namespace lsp */

#endif /* PRIVATE_META_SHAPER_H_ */
