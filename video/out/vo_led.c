/*
 * video output driver for ledcontroller IP core
 *
 * by Sergiusz Bazanski <sergiusz@bazanski.pl>
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>

#include "config.h"
#include "vo.h"
#include "video/mp_image.h"

#include "input/keycodes.h"
#include "input/input.h"
#include "common/msg.h"
#include "input/input.h"

// TODO; get this from the kernel once the driver stabilizes
#define Q3KLED_ADDRESS 0x7aa00000
#define Q3KLED_SIZE 0x1000

struct priv {
    /* image infos */
    int image_format;
    int image_width;
    int image_height;

    int screen_w, screen_h;

    int memfile;
    volatile uint32_t *memory;

    uint8_t *temp_buffer;
};

static const unsigned int depth = 4;

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *priv = vo->priv;
    priv->image_height = params->h;
    priv->image_width  = params->w;
    priv->image_format = params->imgfmt;


    talloc_free(priv->temp_buffer);
    priv->temp_buffer = talloc_array(NULL, uint8_t, depth * priv->image_width * priv->image_height);

    return 0;
}

// TODO: remove unnecessary copy (ignore flip, copy word-by-word to mapped baffer in draw_image)

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *priv = vo->priv;
    memcpy_pic(priv->temp_buffer, mpi->planes[0], priv->image_width * depth, priv->image_height,
               priv->image_width * depth, mpi->stride[0]);
    talloc_free(mpi);
}

static void flip_page(struct vo *vo)
{
    struct priv *priv = vo->priv;
    volatile uint32_t *out = priv->memory;
    for (int x = 0; x < 128; x++) {
        if (x > priv->image_width)
            break;
        for (int y = 0; y < 128; y++) {
            if (y > priv->image_height)
                break;
            uint32_t pixel = ((uint32_t*)priv->temp_buffer)[x + y * priv->image_width];
            out[x+y*128] = pixel;
        }
    }
}

static void uninit(struct vo *vo)
{
    struct priv *priv = vo->priv;
    talloc_free(priv->temp_buffer);
    priv->temp_buffer = NULL;
}

static int preinit(struct vo *vo)
{
    struct priv *priv = vo->priv;
    priv->memfile = open("/dev/mem", O_RDWR | O_SYNC);
    if (priv->memfile < 0) {
        MP_ERR(vo, "could not open /dev/mem");
        return ENOSYS;
    }
    priv->memory = (uint32_t *)(mmap(0, Q3KLED_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, priv->memfile, Q3KLED_ADDRESS));
    if (priv->memory == NULL) {
        MP_ERR(vo, "could not mmap /dev/mem");
        return ENOSYS;
    }

    return 0;
}

static int query_format(struct vo *vo, int format)
{
    return format == IMGFMT_BGR32;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    return VO_NOTIMPL;
}

const struct vo_driver video_out_led = {
    .name = "led",
    .description = "led",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
};
