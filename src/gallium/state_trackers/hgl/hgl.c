/*
 * Copyright 2012-2013, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *      Artur Wyszynski, harakash@gmail.com
 *      Alexander von Gluck IV, kallisti5@unixzen.com
 */


#include "main/context.h"
#include "main/framebuffer.h"
#include "main/renderbuffer.h"
#include "pipe/p_format.h"
#include "util/u_atomic.h"
#include "util/u_memory.h"

#include "hgl_context.h"


#ifdef DEBUG
#   define TRACE(x...) printf("hgl:state_tracker: " x)
#   define CALLED() TRACE("CALLED: %s\n", __PRETTY_FUNCTION__)
#else
#   define TRACE(x...)
#   define CALLED()
#endif
#define ERROR(x...) printf("hgl:state_tracker: " x)


static boolean
hgl_st_framebuffer_flush_front(struct st_context_iface *stctx,
	struct st_framebuffer_iface* stfb, enum st_attachment_type statt)
{
	CALLED();

	struct hgl_context* context = (struct hgl_context*)stfb->st_manager_private;

	if (!context) {
		ERROR("%s: Couldn't obtain valid hgl_context!\n", __func__);
		return FALSE;
	}

	#if 0
	struct stw_st_framebuffer *stwfb = stw_st_framebuffer(stfb);
	pipe_mutex_lock(stwfb->fb->mutex);

	struct pipe_resource* resource = textures[statt];
	if (resource)
		stw_framebuffer_present_locked(...);
	#endif

	return TRUE;
}


/**
 * Called by the st manager to validate the framebuffer (allocate
 * its resources).
 */
static boolean
hgl_st_framebuffer_validate(struct st_context_iface *stctx,
	struct st_framebuffer_iface *stfbi, const enum st_attachment_type *statts,
	unsigned count, struct pipe_resource **out)
{
	CALLED();

	if (!stfbi) {
		ERROR("%s: Invalid st framebuffer interface!\n", __func__);
		return FALSE;
	}

	struct hgl_context* context = (struct hgl_context*)stfbi->st_manager_private;

	if (!context) {
		ERROR("%s: Couldn't obtain valid hgl_context!\n", __func__);
		return FALSE;
	}

	int32 width = 0;
	int32 height = 0;
	get_bitmap_size(context->bitmap, &width, &height);

	struct pipe_resource templat;
	memset(&templat, 0, sizeof(templat));
	templat.target = PIPE_TEXTURE_RECT;
	templat.width0 = width;
	templat.height0 = height;
	templat.depth0 = 1;
	templat.array_size = 1;
	templat.usage = PIPE_USAGE_DEFAULT;

	if (context->stVisual && context->manager && context->manager->screen) {
		TRACE("%s: Updating resources\n", __func__);
		for (unsigned i = 0; i < count; i++) {
			enum pipe_format format = PIPE_FORMAT_NONE;
			unsigned bind = 0;

			switch(statts[i]) {
				case ST_ATTACHMENT_FRONT_LEFT:
				case ST_ATTACHMENT_BACK_LEFT:
					format = context->stVisual->color_format;
					bind = PIPE_BIND_DISPLAY_TARGET
						| PIPE_BIND_RENDER_TARGET;
					break;
				case ST_ATTACHMENT_DEPTH_STENCIL:
					format = context->stVisual->depth_stencil_format;
					bind = PIPE_BIND_DEPTH_STENCIL;
					break;
				case ST_ATTACHMENT_ACCUM:
					format = context->stVisual->accum_format;
					bind = PIPE_BIND_RENDER_TARGET;
					break;
				default:
					format = PIPE_FORMAT_NONE;
					break;
			}

			if (format != PIPE_FORMAT_NONE) {
				templat.format = format;
				templat.bind = bind;

				struct pipe_screen* screen = context->manager->screen;
				context->textures[i] = screen->resource_create(screen, &templat);
				out[i] = context->textures[i];
			}
		}
	}

	return TRUE;
}


/**
 * Create new framebuffer
 */
struct hgl_buffer *
hgl_create_st_framebuffer(struct hgl_context* context)
{
	CALLED();

	struct hgl_buffer *buffer = CALLOC_STRUCT(hgl_buffer);

	assert(context);
	assert(context->stVisual);

	if (buffer) {
		// Copy context visual into framebuffer
		memcpy(&buffer->visual, context->stVisual, sizeof(struct st_visual));

		// calloc our st_framebuffer interface
		buffer->stfbi = CALLOC_STRUCT(st_framebuffer_iface);
		if (!buffer->stfbi) {
			ERROR("%s: Couldn't calloc framebuffer!\n", __func__);
			return NULL;
		}

		struct st_framebuffer_iface* stfbi = buffer->stfbi;
		p_atomic_set(&stfbi->stamp, 1);
		stfbi->flush_front = hgl_st_framebuffer_flush_front;
		stfbi->validate = hgl_st_framebuffer_validate;
		stfbi->st_manager_private = (void*)context;
		stfbi->visual = &buffer->visual;

		// TODO: Do we need linked list?
	}

   return buffer;
}