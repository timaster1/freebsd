# $FreeBSD$

.include <bsd.own.mk>

.if ${MK_FORTH} != "no"
# Build the add-in FORTH interpreter.
SUBDIR+=		ficl
.endif

# Build EFI library.
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE} == "i386" || ${MACHINE_ARCH} == "ia64"
SUBDIR+=		efi
.endif

# Build Open Firmware library.
.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "sparc64"
SUBDIR+=		ofw
.endif

# Build U-Boot library.
.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "arm"
SUBDIR+=		uboot
.endif

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE} == "i386"
SUBDIR+=		zfs
.endif

# Pick the machine-dependent subdir based on the target architecture.
ADIR=			${MACHINE:S/amd64/i386/:S/sun4v/sparc64/}
.if exists(${.CURDIR}/${ADIR}/.)
SUBDIR+=		${ADIR}
.endif

.include <bsd.subdir.mk>
