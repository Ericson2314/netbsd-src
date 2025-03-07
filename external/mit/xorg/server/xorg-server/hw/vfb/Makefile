#	$NetBSD: Makefile,v 1.21 2021/08/19 11:03:04 mrg Exp $

.include <bsd.own.mk>

.include "../../Makefile.Xserver"

PROG=		Xvfb

.PATH:		${X11SRCDIR.xorg-server}/hw/vfb
.PATH:		${X11SRCDIR.xorg-server}/hw/vfb/man
.PATH:		${X11SRCDIR.xorg-server}/Xext
.PATH:		${X11SRCDIR.xorg-server}/Xi
.PATH:		${X11SRCDIR.xorg-server}/mi
SRCS=		InitInput.c InitOutput.c miinitext.c stubs.c

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11 \
		-I${X11SRCDIR.xorg-server}/fb \
		-I${X11SRCDIR.xorg-server}/mfb \
		-I${X11SRCDIR.xorg-server}/mi \
		-I${X11SRCDIR.xorg-server}/include \
		-I${X11SRCDIR.xorg-server}/os \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/common \
		-I${DESTDIR}${X11INCDIR}/X11/extensions \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${DESTDIR}${X11INCDIR}/xorg \
		-I${X11SRCDIR.xorg-server}/render \
		-UGLXEXT

CPPFLAGS+=	${X11FLAGS.OS_DEFINES} ${X11FLAGS.DIX} \
		-DHAS_SHM -DHAS_MMAP -UXFree86LOADER

.include "../../Makefile.common"

.if ${XSERVER_XFREE86} == "no"
CPPFLAGS+=	-UDDXTIME
.endif

CPPFLAGS.InitInput.c=	${X11FLAGS.EXTENSION} \
			-UGLXEXT
CPPFLAGS.miinitext.c=	${X11FLAGS.EXTENSION} \
			-UGLXEXT

COPTS.InitInput.c=	-Wno-error
COPTS.InitOutput.c=	-Wno-error

LDADD+=	${LDADD.dix} \
	${LDADD.fb} \
	${LDADD.mi} \
	${LDADD.xfixes} \
	${LDADD.Xext} \
	${LDADD.config} \
	${LDADD.dbe} \
	${LDADD.record} \
	${LDADD.randr} \
	${LDADD.damageext} \
	${LDADD.miext/damage} \
	${LDADD.miext/shadow} \
	${LDADD.miext/sync} \
	${LDADD.render} \
	${LDADD.Xi} \
	${LDADD.xkb} \
	${LDADD.xkbstubs} \
	${LDADD.composite} \
	${LDADD.os} \
	${LDADD.glx} \
	${LDADD.present} \
	${LDADD.dri3} \
	-lXfont2 \
	-lfreetype \
	-lXau \
	-lfontenc \
	${LIBCWRAPPER} \
	-lpixman-1 \
	-lXdmcp \
	-lz \
	-lm \
	-lGL

DPADD+=	${DPADD.dix} \
	${DPADD.fb} \
	${DPADD.mi} \
	${DPADD.xfixes} \
	${DPADD.Xext} \
	${DPADD.config} \
	${DPADD.dbe} \
	${DPADD.record} \
	${DPADD.render} \
	${DPADD.randr} \
	${DPADD.damageext} \
	${DPADD.miext/damage} \
	${DPADD.miext/shadow} \
	${DPADD.miext/sync} \
	${DPADD.Xi} \
	${DPADD.xkb} \
	${DPADD.xkbstubs} \
	${DPADD.composite} \
	${DPADD.os} \
	${DPADD.glx} \
	${DPADD.present} \
	${DPADD.dri3} \
	${LIBXFONT2} \
	${LIBFREETYPE} \
	${LIBXAU} \
	${LIBFONTENC} \
	${LIBPIXMAN1} \
	${LIBXDMCP} \
	${LIBZ} \
	${LIBM} \
	${LIBGL}

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}

.if ${MACHINE_ARCH} == "i386"
LDADD+=	-li386
DPADD+=	${LIBI386}
.endif

.if ${MACHINE_ARCH} == "x86_64"
LDADD+=	-lx86_64
DPADD+=	${LIBX86_64}
.endif

.if ${MACHINE_ARCH} == "alpha"
LDADD+=	-lalpha
DPADD+=	${LIBALPHA}
.endif

COPTS.miinitext.c+=	-Wno-error	# XXX xf86bigfstr.h

.include <bsd.x11.mk>
.include <bsd.prog.mk>
