#	$NetBSD: Makefile,v 1.4 2021/08/11 23:02:39 maya Exp $

.include "../Makefile.serverlib"
.include "../Makefile.servermod"

LIB=	glamor

.PATH:	${X11SRCDIR.xorg-server}/${LIB}
SRCS= \
	glamor.c \
	glamor_copy.c \
	glamor_core.c \
	glamor_dash.c \
	glamor_font.c \
	glamor_glx.c \
	glamor_composite_glyphs.c \
	glamor_image.c \
	glamor_lines.c \
	glamor_segs.c \
	glamor_render.c \
	glamor_gradient.c \
	glamor_prepare.c \
	glamor_program.c \
	glamor_rects.c \
	glamor_spans.c \
	glamor_text.c \
	glamor_transfer.c \
	glamor_transform.c \
	glamor_trapezoid.c \
	glamor_triangles.c \
	glamor_addtraps.c \
	glamor_glyphblt.c \
	glamor_points.c \
	glamor_pixmap.c \
	glamor_largepixmap.c \
	glamor_picture.c \
	glamor_vbo.c \
	glamor_window.c \
	glamor_fbo.c \
	glamor_compositerects.c \
	glamor_utils.c \
	glamor_sync.c
SRCS+= \
	glamor_xv.c
.if ${HAVE_XORG_GLAMOR} == "no"
SRCS+= \
	glamor_egl_stubs.c
.else
SRCS+= \
	glamor_egl.c \
	glamor_eglmodule.c
.endif

CPPFLAGS+=      -I${DESTDIR}${X11INCDIR}/xorg \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${DESTDIR}${X11INCDIR}/libdrm \
		-I${X11SRCDIR.xorg-server}/glamor \
		-I${X11SRCDIR.xorg-server}/../include
CPPFLAGS+=      ${X11FLAGS.DIX}

COPTS+=		${${ACTIVE_CC} == "clang":? -Wno-error=enum-conversion :}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
