#include <dev/drm/rawfb/rawfb.h>
#include <dev/drm/drm.h>
#include <dev/dev.h>
#include <lib/resource.h>

#include <lib/dynarray.h>
#include <lib/print.h>
#include <lib/alloc.h>
#include <lib/errno.h>
#include <stdbool.h>
#include <mm/vmm.h>

struct plainfb_dumb_buffer {
	uint32_t height;
	uint32_t width;
	uint32_t bpp;
	uint32_t flags;
	uint32_t handle;
	uint32_t pitch;
	uint64_t size;
    bool valid;

    void* buffer;
};

struct plainfb_fb {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t bpp;
	uint32_t depth;
    uint32_t dumb_buffer_off;

    bool valid;
};

size_t vesa_height = 0;
size_t vesa_width = 0;
size_t vesa_bpp = 0;
size_t vesa_pitch = 0;
uint64_t vesa_framebuffer_addr = 0;

DYNARRAY_NEW(struct plainfb_dumb_buffer*, dumb_buffers);
DYNARRAY_NEW(struct plainfb_fb*, fbs);

static int rawfb_ioctl(struct resource *this, int request, void *argp) {
    struct drm_device *this_drm = (struct drm_device*)this;
    switch (request) {
        case DRM_IOCTL_GET_CAP : {
            print("drm get capability\n");
            struct drm_get_cap *cap = argp;
            if (cap->capability == DRM_CAP_DUMB_BUFFER) {
                cap->value = 1;
                return 0;
            }
            break;
        }
        case DRM_IOCTL_MODE_GETRESOURCES : {
            print("drm get resources\n");
            struct drm_mode_card_res *res = argp;
            if (!res->fb_id_ptr) {
                res->count_fbs = this_drm->num_fbs;
            } else {
                for (size_t i = 0; i < fbs.length; i++) {
                    if (fbs.storage[i]->valid) {
                        ((uint32_t*)res->fb_id_ptr)[i] = i;
                    }
                }
            }

            if (!res->crtc_id_ptr) {
                res->count_crtcs = this_drm->num_crtcs;
            } else {
              ((uint32_t*)res->crtc_id_ptr)[0] = 1;
            }

            if (!res->connector_id_ptr) {
                res->count_connectors = this_drm->num_connectors;
            }  else {
                ((uint32_t*)res->connector_id_ptr)[0] = 1;
            }

            if (!res->encoder_id_ptr) {
                res->count_encoders = this_drm->num_encoders;
            } else {
                ((uint32_t*)res->encoder_id_ptr)[0] = 1;
            }

            res->min_width = vesa_width;
            res->min_height = vesa_height;
            res->max_width = vesa_width;
            res->max_height = vesa_height;
            print("drm get resources done\n");
            return 0;
        }
        case DRM_IOCTL_MODE_GETCONNECTOR : {
            struct drm_mode_get_connector *res = argp;
            if (!res->encoders_ptr) {
                res->count_encoders = 1;
            } else {
                ((uint32_t*)res->encoders_ptr)[0] = 1;
            }

            if (!res->modes_ptr) {
                res->count_modes = 1;
            } else {
                struct drm_mode_modeinfo *modes = (struct drm_mode_modeinfo*)res->modes_ptr;
                modes[0].hdisplay = vesa_width;
                modes[0].vdisplay = vesa_height;
            }

            //unsupported for now
            res->props_ptr = 0;
            res->prop_values_ptr = 0;
            res->encoder_id = 1;
            res->connector_id = 1;

            res->connection = DRM_MODE_CONNECTED;
            return 0;
        }

        case DRM_IOCTL_MODE_GETENCODER : {
            struct drm_mode_get_encoder *res = argp;

            res->encoder_id = 1;
            //determine if we can figure this out
            res->encoder_type = 0;
            res->crtc_id = 1;
            //bit n set to 1 -> crtc n works with this encoder
            res->possible_crtcs = 1;
            res->possible_clones = 0;
            return 0;
        }

        case DRM_IOCTL_MODE_GETCRTC : {
            struct drm_mode_crtc *res = argp;
            if (res->crtc_id != 1) {
                //TODO check the proper error to return here
                return -1;
            }

            res->count_connectors = 1;
            //fill out mode info
            res->mode_valid = 1;
            res->mode.hdisplay = vesa_width;
            res->mode.vdisplay = vesa_height;
            res->x = 0;
            res->y = 0;
            return 0;
        }

        case DRM_IOCTL_MODE_SETCRTC : {
            //we ignore all mode-related stuff because we cannot change the mode on the bios framebuffer
            print("modeset crtc start %X %X\n", vesa_framebuffer_addr);
            struct drm_mode_crtc *res = argp;
            if (res->crtc_id != 1) {
                //TODO check the proper error to return here
                return -1;
            }
            void* fb_data = dumb_buffers.storage[fbs.storage[res->fb_id]->dumb_buffer_off]->buffer;
            size_t fb_size = dumb_buffers.storage[fbs.storage[res->fb_id]->dumb_buffer_off]->size;
//            memset(fb_data, 0xFF, fb_size);
            memcpy((void*)(vesa_framebuffer_addr), fb_data, fb_size);
            print("modeset crtc %x\n", fb_size);
            return 0;
        }
        
        case DRM_IOCTL_MODE_CREATE_DUMB : {
            struct drm_mode_create_dumb *res = argp;

            size_t pitch = vesa_pitch;
            size_t size = res->height * pitch;
            print("create dumb size: %X\n", size);

            struct plainfb_dumb_buffer *nb = alloc(sizeof(struct plainfb_dumb_buffer));
            nb->height = res->height;
            nb->width = res->width;
            nb->bpp = res->bpp;
            nb->bpp = res->flags;
            nb->pitch = res->pitch;
            nb->size = size;
            nb->buffer = alloc(size);
            nb->valid = true;

            res->pitch = pitch;
            res->size = size;
            res->handle = DYNARRAY_INSERT(dumb_buffers, nb);
            return 0;
        }
        
        case DRM_IOCTL_MODE_ADDFB : {
            struct drm_mode_fb_cmd *res = argp;
            struct plainfb_fb *fb = alloc(sizeof(struct plainfb_fb));
            fb->depth = res->depth;
            if (res->handle > (dumb_buffers.length - 1) || !dumb_buffers.storage[res->handle]->valid) {
                print("dumb buffer does not exist\n");
                //TODO check the proper error to return here
                return -1;
            }
            fb->dumb_buffer_off = res->handle;
            fb->bpp = res->bpp;
            fb->depth = res->depth;
            fb->width = res->width;
            fb->height = res->height;
            fb->pitch = res->pitch;
            res->fb_id = DYNARRAY_INSERT(fbs, fb);
            this_drm->num_fbs++;
            return 0;
        }

        case DRM_IOCTL_MODE_MAP_DUMB : {
            struct drm_mode_map_dumb *res = argp;
            if (res->handle > (dumb_buffers.length - 1) || !dumb_buffers.storage[res->handle]->valid) {
                //TODO check the proper error to return here
                print("dumb buffer does not exist\n");
                return -1;
            }
            res->offset = res->handle;
            return 0;
        }
        
        case DRM_IOCTL_MODE_DESTROY_DUMB : {
            struct drm_mode_destroy_dumb *res = argp;
            print("dumb buffer does not exist\n");
            if (res->handle > (dumb_buffers.length - 1) || !dumb_buffers.storage[res->handle]->valid) {
                //TODO check the proper error to return here
                return -1;
            }
            free(dumb_buffers.storage[res->handle]->buffer);
            dumb_buffers.storage[res->handle]->valid = false;
            return 0;
        }

        case DRM_IOCTL_MODE_RMFB : {
            uint32_t *id = argp;
            print("dumb buffer does not exist\n");
            if (*id > (fbs.length - 1) || !fbs.storage[*id]->valid) {
                //TODO check the proper error to return here
                return -1;
            }
            fbs.storage[*id]->valid = false;
            this_drm->num_fbs--;
            return 0;
        }

        default: {
            print("Unhandled ioctl: %x\n", request);
            return ENXIO;
        }
    }
    return 0;
}

void init_rawfbdev(struct stivale2_struct_tag_framebuffer *framebuffer_tag) {
    struct drm_device *dri = resource_create(sizeof(struct drm_device));
    dri->ioctl = rawfb_ioctl;
    dri->num_crtcs = 1;
    dri->num_connectors = 1;
    dri->num_encoders = 1;

    drm_add_device(dri);

    vesa_height = framebuffer_tag->framebuffer_height;
    vesa_width = framebuffer_tag->framebuffer_width;
    vesa_bpp = framebuffer_tag->framebuffer_bpp;
    vesa_framebuffer_addr  = framebuffer_tag->framebuffer_addr + MEM_PHYS_OFFSET;
    vesa_pitch = framebuffer_tag->framebuffer_pitch;
}
