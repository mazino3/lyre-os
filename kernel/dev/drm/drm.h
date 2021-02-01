#ifndef __DRM_H__
#define __DRM_H__

#include <stdint.h>
#include <lib/resource.h>
#include <stivale/stivale2.h>

#define DRM_IOCTL_GET_CAP            0xc010640c
#define DRM_CAP_DUMB_BUFFER		     0x1
#define DRM_IOCTL_MODE_GETRESOURCES  0xc04064a0
#define DRM_IOCTL_MODE_GETCONNECTOR  0xc05064a7
#define DRM_IOCTL_MODE_GETENCODER    0xc01464a6
#define DRM_IOCTL_MODE_GETCRTC       0xc06864a1
#define DRM_IOCTL_MODE_CREATE_DUMB   0xc02064b2
#define DRM_IOCTL_MODE_ADDFB         0xc01c64ae
#define DRM_IOCTL_MODE_MAP_DUMB      0xc01064b3
#define DRM_IOCTL_MODE_DESTROY_DUMB  0xc00464b4
#define DRM_IOCTL_MODE_RMFB          0xc00464af
#define DRM_IOCTL_MODE_SETCRTC       0xc06864a2

#define DRM_MODE_CONNECTED         1
#define DRM_MODE_DISCONNECTED      2
#define DRM_MODE_UNKNOWNCONNECTION 3

struct drm_get_cap {
	uint64_t capability;
	uint64_t value;
};

struct drm_mode_card_res {
	uint64_t fb_id_ptr;
	uint64_t crtc_id_ptr;
	uint64_t connector_id_ptr;
	uint64_t encoder_id_ptr;
	uint32_t count_fbs;
	uint32_t count_crtcs;
	uint32_t count_connectors;
	uint32_t count_encoders;
	uint32_t min_width;
	uint32_t max_width;
	uint32_t min_height;
	uint32_t max_height;
};

struct drm_mode_get_connector {
	uint64_t encoders_ptr;
	uint64_t modes_ptr;
	uint64_t props_ptr;
	uint64_t prop_values_ptr;

	uint32_t count_modes;
	uint32_t count_props;
	uint32_t count_encoders;

	uint32_t encoder_id; /**< Current Encoder */
	uint32_t connector_id; /**< Id */
	uint32_t connector_type;
	uint32_t connector_type_id;

	uint32_t connection;
	uint32_t mm_width;  /**< width in millimeters */
	uint32_t mm_height; /**< height in millimeters */
	uint32_t subpixel;

	uint32_t pad;
};

struct drm_mode_get_encoder {
	uint32_t encoder_id;
	uint32_t encoder_type;

	uint32_t crtc_id; /**< Id of crtc */

	uint32_t possible_crtcs;
	uint32_t possible_clones;
};

struct drm_mode_modeinfo {
	uint32_t clock;
	uint16_t hdisplay;
	uint16_t hsync_start;
	uint16_t hsync_end;
	uint16_t htotal;
	uint16_t hskew;
	uint16_t vdisplay;
	uint16_t vsync_start;
	uint16_t vsync_end;
	uint16_t vtotal;
	uint16_t vscan;

	uint32_t vrefresh;

	uint32_t flags;
	uint32_t type;
	char name[32];
};

struct drm_mode_crtc {
	uint64_t set_connectors_ptr;
	uint32_t count_connectors;

	uint32_t crtc_id; /**< Id */
	uint32_t fb_id; /**< Id of framebuffer */

	uint32_t x; /**< x Position on the framebuffer */
	uint32_t y; /**< y Position on the framebuffer */

	uint32_t gamma_size;
	uint32_t mode_valid;
	struct drm_mode_modeinfo mode;
};

struct drm_mode_create_dumb {
	uint32_t height;
	uint32_t width;
	uint32_t bpp;
	uint32_t flags;
	/* handle, pitch, size will be returned */
	uint32_t handle;
	uint32_t pitch;
	uint64_t size;
};

/* set up for mmap of a dumb scanout buffer */
struct drm_mode_map_dumb {
	/** Handle for the object being mapped. */
	uint32_t handle;
	uint32_t pad;
	/**
	 * Fake offset to use for subsequent mmap call
	 *
	 * This is a fixed-size type for 32/64 compatibility.
	 */
	uint64_t offset;
};

struct drm_mode_destroy_dumb {
	uint32_t handle;
};

struct drm_mode_fb_cmd {
	uint32_t fb_id;
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t bpp;
	uint32_t depth;
	uint32_t handle;
};

struct drm_device {
    struct resource;
    uint64_t num_fbs;
    uint64_t num_crtcs;
    uint64_t num_connectors;
    uint64_t num_encoders;
};

void init_drm(struct stivale2_struct_tag_framebuffer *framebuffer_tag);
void drm_add_device(struct drm_device* dev);

#endif
