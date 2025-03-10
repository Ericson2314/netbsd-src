/*	$NetBSD: ext2fs_vnops.c,v 1.135 2021/07/18 23:57:15 dholland Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ufs_vnops.c	8.14 (Berkeley) 10/26/94
 * Modified for ext2fs by Manuel Bouyer.
 */

/*
 * Copyright (c) 1997 Manuel Bouyer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	@(#)ufs_vnops.c	8.14 (Berkeley) 10/26/94
 * Modified for ext2fs by Manuel Bouyer.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ext2fs_vnops.c,v 1.135 2021/07/18 23:57:15 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/lockf.h>
#include <sys/pool.h>
#include <sys/signalvar.h>
#include <sys/kauth.h>

#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufsmount.h>

#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>
#include <ufs/ext2fs/ext2fs_dir.h>
#include <ufs/ext2fs/ext2fs_xattr.h>

extern int prtactive;

static int ext2fs_chmod(struct vnode *, int, kauth_cred_t, struct lwp *);
static int ext2fs_chown(struct vnode *, uid_t, gid_t, kauth_cred_t,
				struct lwp *);
static int ext2fs_makeinode(struct vattr *, struct vnode *, struct vnode **,
				struct componentname *, int);

union _qcvt {
	int64_t	qcvt;
	int32_t val[2];
};

#define SETHIGH(q, h) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_HIGHWORD] = (h); \
	(q) = tmp.qcvt; \
}
#define SETLOW(q, l) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_LOWWORD] = (l); \
	(q) = tmp.qcvt; \
}

/*
 * Create a regular file
 */
int
ext2fs_create(void *v)
{
	struct vop_create_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	int	error;

	error = ext2fs_makeinode(ap->a_vap, ap->a_dvp, ap->a_vpp, ap->a_cnp, 1);

	if (error)
		return error;
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	VOP_UNLOCK(*ap->a_vpp);
	return 0;
}

/*
 * Mknod vnode call
 */
/* ARGSUSED */
int
ext2fs_mknod(void *v)
{
	struct vop_mknod_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vattr *vap = ap->a_vap;
	struct vnode **vpp = ap->a_vpp;
	struct inode *ip;
	int error;

	if ((error = ext2fs_makeinode(vap, ap->a_dvp, vpp, ap->a_cnp, 1)) != 0)
		return error;
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	ip = VTOI(*vpp);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	VOP_UNLOCK(*vpp);
	return 0;
}

/*
 * Open called.
 *
 * Just check the APPEND flag.
 */
/* ARGSUSED */
int
ext2fs_open(void *v)
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;

	/*
	 * Files marked append-only must be opened for appending.
	 */
	if ((VTOI(ap->a_vp)->i_e2fs_flags & EXT2_APPEND) &&
		(ap->a_mode & (FWRITE | O_APPEND)) == FWRITE)
		return EPERM;
	return 0;
}

static int
ext2fs_check_possible(struct vnode *vp, struct inode *ip, mode_t mode)
{

	/*
	 * Disallow write attempts on read-only file systems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the file system.
	 */
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return EROFS;
			break;
		default:
			break;
		}
	}

	/* If immutable bit set, nobody gets to write it. */
	if ((mode & VWRITE) && (ip->i_e2fs_flags & EXT2_IMMUTABLE))
		return EPERM;

	return 0;
}

static int
ext2fs_check_permitted(struct vnode *vp, struct inode *ip, accmode_t accmode,
    kauth_cred_t cred)
{

	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(accmode,
	    vp->v_type, ip->i_e2fs_mode & ALLPERMS), vp, NULL,
	    genfs_can_access(vp, cred, ip->i_uid, ip->i_gid,
	    ip->i_e2fs_mode & ALLPERMS, NULL, accmode));
}

int
ext2fs_access(void *v)
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		accmode_t  a_accmode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	accmode_t mode = ap->a_accmode;
	int error;

	error = ext2fs_check_possible(vp, ip, mode);
	if (error)
		return error;

	error = ext2fs_check_permitted(vp, ip, mode, ap->a_cred);

	return error;
}

/* ARGSUSED */
int
ext2fs_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct vattr *vap = ap->a_vap;

	EXT2FS_ITIMES(ip, NULL, NULL, NULL);
	/*
	 * Copy from inode table
	 */
	vap->va_fsid = ip->i_dev;
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_e2fs_mode & ALLPERMS;
	vap->va_nlink = ip->i_e2fs_nlink;
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	vap->va_rdev = (dev_t)fs2h32(ip->i_din.e2fs_din->e2di_rdev);
	vap->va_size = vp->v_size;
	EXT2_DINODE_TIME_GET(&vap->va_atime, ip->i_din.e2fs_din, e2di_atime, EXT2_DINODE_SIZE(ip->i_e2fs));
	EXT2_DINODE_TIME_GET(&vap->va_mtime, ip->i_din.e2fs_din, e2di_mtime, EXT2_DINODE_SIZE(ip->i_e2fs));
	EXT2_DINODE_TIME_GET(&vap->va_ctime, ip->i_din.e2fs_din, e2di_ctime, EXT2_DINODE_SIZE(ip->i_e2fs));
	if (EXT2_DINODE_FITS(ip->i_din.e2fs_din, e2di_crtime, EXT2_DINODE_SIZE(ip->i_e2fs))) {
		EXT2_DINODE_TIME_GET(&vap->va_birthtime, ip->i_din.e2fs_din, e2di_crtime, EXT2_DINODE_SIZE(ip->i_e2fs));
	}

	vap->va_flags = 0;
	vap->va_flags |= (ip->i_e2fs_flags & EXT2_NODUMP) ? UF_NODUMP : 0;
#ifdef EXT2FS_SYSTEM_FLAGS
	vap->va_flags |= (ip->i_e2fs_flags & EXT2_IMMUTABLE) ? SF_IMMUTABLE : 0;
	vap->va_flags |= (ip->i_e2fs_flags & EXT2_APPEND) ? SF_APPEND : 0;
#else
	vap->va_flags |= (ip->i_e2fs_flags & EXT2_IMMUTABLE) ? UF_IMMUTABLE : 0;
	vap->va_flags |= (ip->i_e2fs_flags & EXT2_APPEND) ? UF_APPEND : 0;
#endif

	vap->va_gen = ip->i_e2fs_gen;
	/* this doesn't belong here */
	if (vp->v_type == VBLK)
		vap->va_blocksize = BLKDEV_IOSIZE;
	else if (vp->v_type == VCHR)
		vap->va_blocksize = MAXBSIZE;
	else
		vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_bytes = dbtob(ext2fs_nblock(ip));
	vap->va_type = vp->v_type;
	vap->va_filerev = ip->i_modrev;
	return 0;
}

/*
 * Set attribute vnode op. called from several syscalls
 */
int
ext2fs_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vattr *vap = ap->a_vap;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	kauth_cred_t cred = ap->a_cred;
	struct lwp *l = curlwp;
	int error;
	kauth_action_t action = KAUTH_VNODE_WRITE_FLAGS;
	bool changing_sysflags = false;

	/*
	 * Check for unsettable attributes.
	 */
	if ((vap->va_type != VNON) || (vap->va_nlink != (nlink_t)VNOVAL) ||
	    (vap->va_fsid != VNOVAL) || (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) || (vap->va_rdev != VNOVAL) ||
	    ((int)vap->va_bytes != VNOVAL) || (vap->va_gen != VNOVAL)) {
		return EINVAL;
	}
	if (vap->va_flags != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return EROFS;

		/*
		 * Check if we're allowed to change the flags.
		 * If EXT2FS_SYSTEM_FLAGS is set, then the flags are treated
		 * as system flags, otherwise they're considered to be user
		 * flags.
		 */
#ifdef EXT2FS_SYSTEM_FLAGS
		/* Indicate we're changing system flags if we are. */
		if ((vap->va_flags & SF_APPEND) ||
		     (vap->va_flags & SF_IMMUTABLE)) {
			action |= KAUTH_VNODE_WRITE_SYSFLAGS;
			changing_sysflags = true;
		}

		/* Indicate the node has system flags if it does. */
		if (ip->i_e2fs_flags & (EXT2_APPEND | EXT2_IMMUTABLE)) {
			action |= KAUTH_VNODE_HAS_SYSFLAGS;
		}
#endif /* EXT2FS_SYSTEM_FLAGS */

		error = kauth_authorize_vnode(cred, action, vp, NULL,
		    genfs_can_chflags(vp, cred, ip->i_uid, changing_sysflags));
		if (error)
			return error;

		ip->i_e2fs_flags &= ~(EXT2_APPEND | EXT2_IMMUTABLE | EXT2_NODUMP);
#ifdef EXT2FS_SYSTEM_FLAGS
		ip->i_e2fs_flags |=
		    (vap->va_flags & SF_APPEND) ?  EXT2_APPEND : 0 |
		    (vap->va_flags & SF_IMMUTABLE) ? EXT2_IMMUTABLE : 0;
#else
		ip->i_e2fs_flags |=
		    (vap->va_flags & UF_APPEND) ? EXT2_APPEND : 0 |
		    (vap->va_flags & UF_IMMUTABLE) ? EXT2_IMMUTABLE : 0;
#endif
		ip->i_e2fs_flags |=
		    (vap->va_flags & UF_NODUMP) ? EXT2_NODUMP : 0;   
		ip->i_flag |= IN_CHANGE;
		if (vap->va_flags & (IMMUTABLE | APPEND))
			return 0;
	}
	if (ip->i_e2fs_flags & (EXT2_APPEND | EXT2_IMMUTABLE))
		return EPERM;
	/*
	 * Go through the fields and update iff not VNOVAL.
	 */
	if (vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return EROFS;
		error = ext2fs_chown(vp, vap->va_uid, vap->va_gid, cred, l);
		if (error)
			return error;
	}
	if (vap->va_size != VNOVAL) {
		/*
		 * Disallow write attempts on read-only file systems;
		 * unless the file is a socket, fifo, or a block or
		 * character device resident on the file system.
		 */
		switch (vp->v_type) {
		case VDIR:
			return EISDIR;
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return EROFS;
		default:
			break;
		}
		error = ext2fs_truncate(vp, vap->va_size, 0, cred);
		if (error)
			return error;
	}
	ip = VTOI(vp);
	if (vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL || vap->va_birthtime.tv_sec != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return EROFS;
		error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_TIMES, vp,
		    NULL, genfs_can_chtimes(vp, cred, ip->i_uid,
		    vap->va_vaflags));
		if (error)
			return error;
		if (vap->va_atime.tv_sec != VNOVAL)
			if (!(vp->v_mount->mnt_flag & MNT_NOATIME))
				ip->i_flag |= IN_ACCESS;
		if (vap->va_mtime.tv_sec != VNOVAL) {
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
			if (vp->v_mount->mnt_flag & MNT_RELATIME)
				ip->i_flag |= IN_ACCESS;
		}
		if (vap->va_birthtime.tv_sec != VNOVAL &&
		    EXT2_DINODE_FITS(ip->i_din.e2fs_din, e2di_crtime, EXT2_DINODE_SIZE(ip->i_e2fs))) {

			EXT2_DINODE_TIME_SET(&vap->va_birthtime, ip->i_din.e2fs_din, e2di_crtime, EXT2_DINODE_SIZE(ip->i_e2fs));
		}
		error = ext2fs_update(vp, &vap->va_atime, &vap->va_mtime,
			UPDATE_WAIT);
		if (error)
			return error;
	}
	error = 0;
	if (vap->va_mode != (mode_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return EROFS;
		error = ext2fs_chmod(vp, (int)vap->va_mode, cred, l);
	}
	VN_KNOTE(vp, NOTE_ATTRIB);
	return error;
}

/*
 * Change the mode on a file.
 * Inode must be locked before calling.
 */
static int
ext2fs_chmod(struct vnode *vp, int mode, kauth_cred_t cred, struct lwp *l)
{
	struct inode *ip = VTOI(vp);
	int error;

	error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_SECURITY, vp,
	    NULL, genfs_can_chmod(vp, cred, ip->i_uid, ip->i_gid, mode));
	if (error)
		return error;

	ip->i_e2fs_mode &= ~ALLPERMS;
	ip->i_e2fs_mode |= (mode & ALLPERMS);
	ip->i_flag |= IN_CHANGE;
	return 0;
}

/*
 * Perform chown operation on inode ip;
 * inode must be locked prior to call.
 */
static int
ext2fs_chown(struct vnode *vp, uid_t uid, gid_t gid, kauth_cred_t cred,
		struct lwp *l)
{
	struct inode *ip = VTOI(vp);
	uid_t ouid;
	gid_t ogid;
	int error;

	if (uid == (uid_t)VNOVAL)
		uid = ip->i_uid;
	if (gid == (gid_t)VNOVAL)
		gid = ip->i_gid;

	error = kauth_authorize_vnode(cred, KAUTH_VNODE_CHANGE_OWNERSHIP, vp,
	    NULL, genfs_can_chown(vp, cred, ip->i_uid, ip->i_gid, uid, gid));
	if (error)
		return error;

	ogid = ip->i_gid;
	ouid = ip->i_uid;

	ip->i_e2fs_gid = gid & 0xffff;
	ip->i_e2fs_uid = uid & 0xffff;
	if (ip->i_e2fs->e2fs.e2fs_rev > E2FS_REV0) {
		ip->i_e2fs_gid_high = (gid >> 16) & 0xffff;
		ip->i_e2fs_uid_high = (uid >> 16) & 0xffff;
	} else {
		ip->i_e2fs_gid_high = 0;
		ip->i_e2fs_uid_high = 0;
	}
	if (ouid != uid || ogid != gid) {
		ext2fs_set_inode_guid(ip);
		ip->i_flag |= IN_CHANGE;
	}
	if (ouid != uid && (ip->i_e2fs_mode & ISUID) &&
	    kauth_authorize_vnode(cred, KAUTH_VNODE_RETAIN_SUID,
	    vp, NULL, EPERM) != 0)
		ip->i_e2fs_mode &= ~ISUID;
	if (ogid != gid && (ip->i_e2fs_mode & ISGID) &&
	    kauth_authorize_vnode(cred, KAUTH_VNODE_RETAIN_SGID,
	    vp, NULL, EPERM) != 0)
		ip->i_e2fs_mode &= ~ISGID;
	return 0;
}

int
ext2fs_remove(void *v)
{
	struct vop_remove_v2_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct inode *ip;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct ufs_lookup_results *ulr;
	int error;

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	ip = VTOI(vp);
	if (vp->v_type == VDIR ||
		(ip->i_e2fs_flags & (EXT2_IMMUTABLE | EXT2_APPEND)) ||
		(VTOI(dvp)->i_e2fs_flags & EXT2_APPEND)) {
		error = EPERM;
	} else {
		error = ext2fs_dirremove(dvp, ulr, ap->a_cnp);
		if (error == 0) {
			ip->i_e2fs_nlink--;
			ip->i_flag |= IN_CHANGE;
		}
	}

	VN_KNOTE(vp, NOTE_DELETE);
	VN_KNOTE(dvp, NOTE_WRITE);
	if (dvp == vp)
		vrele(vp);
	else
		vput(vp);
	return error;
}

/*
 * ext2fs_link: create hard link.
 */
int
ext2fs_link(void *v)
{
	struct vop_link_v2_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vnode *vp = ap->a_vp;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip;
	int error;
	struct ufs_lookup_results *ulr;

	KASSERT(dvp != vp);
	KASSERT(vp->v_type != VDIR);
	KASSERT(dvp->v_mount == vp->v_mount);

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	error = vn_lock(vp, LK_EXCLUSIVE);
	if (error) {
		VOP_ABORTOP(dvp, cnp);
		goto out2;
	}
	ip = VTOI(vp);
	if ((nlink_t)ip->i_e2fs_nlink >= EXT2FS_LINK_MAX) {
		VOP_ABORTOP(dvp, cnp);
		error = EMLINK;
		goto out1;
	}
	if (ip->i_e2fs_flags & (EXT2_IMMUTABLE | EXT2_APPEND)) {
		VOP_ABORTOP(dvp, cnp);
		error = EPERM;
		goto out1;
	}
	ip->i_e2fs_nlink++;
	ip->i_flag |= IN_CHANGE;
	error = ext2fs_update(vp, NULL, NULL, UPDATE_WAIT);
	if (!error)
		error = ext2fs_direnter(ip, dvp, ulr, cnp);
	if (error) {
		ip->i_e2fs_nlink--;
		ip->i_flag |= IN_CHANGE;
	}
out1:
	VOP_UNLOCK(vp);
out2:
	VN_KNOTE(vp, NOTE_LINK);
	VN_KNOTE(dvp, NOTE_WRITE);
	return error;
}

/*
 * Mkdir system call
 */
int
ext2fs_mkdir(void *v)
{
	struct vop_mkdir_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vnode		*dvp = ap->a_dvp;
	struct componentname	*cnp = ap->a_cnp;
	struct inode		*ip, *dp = VTOI(dvp);
	struct vnode		*tvp;
	struct ext2fs_dirtemplate dirtemplate;
	int			error;
	struct ufs_lookup_results *ulr;

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	KASSERT(ap->a_vap->va_type == VDIR);

	/*
	 * Acquire the inode, but don't sync/direnter it just yet
	 */
	error = ext2fs_makeinode(ap->a_vap, ap->a_dvp, &tvp, ap->a_cnp, 0);
	if (error)
		goto out;

	/* the link count is going to be 2 when all is done */
	ip = VTOI(tvp);
	ip->i_e2fs_nlink = 2;

	/*
	 * Bump link count in parent directory
	 * to reflect work done below.  Should
	 * be done before reference is created
	 * so reparation is possible if we crash.
	 */
	if (dp->i_e2fs_nlink != EXT2FS_LINK_INF)
		dp->i_e2fs_nlink++;

	/*
	 * If we hit the link limit, for directories just set the nlink
	 * to special value 1, which means the link count is bigger
	 * than EXT2FS_LINK_MAX.
	 */
	if ((nlink_t)dp->i_e2fs_nlink >= EXT2FS_LINK_MAX) {
		dp->i_e2fs_nlink = EXT2FS_LINK_INF;

		/* set the feature flag DIR_NLINK if not set already */
		if (!EXT2F_HAS_ROCOMPAT_FEATURE(dp->i_e2fs, EXT2F_ROCOMPAT_DIR_NLINK)) {
                	dp->i_e2fs->e2fs.e2fs_features_rocompat |= EXT2F_ROCOMPAT_DIR_NLINK;
                	dp->i_e2fs->e2fs_fmod = 1;
		}
	}

	dp->i_flag |= IN_CHANGE;
	if ((error = ext2fs_update(dvp, NULL, NULL, UPDATE_DIROP)) != 0)
		goto bad;

	/* Initialize directory with "." and ".." from static template. */
	memset(&dirtemplate, 0, sizeof(dirtemplate));
	dirtemplate.dot_ino = h2fs32(ip->i_number);
	dirtemplate.dot_reclen = h2fs16(12);
	dirtemplate.dot_namlen = 1;
	if (EXT2F_HAS_INCOMPAT_FEATURE(dp->i_e2fs, EXT2F_INCOMPAT_FTYPE)) {
		dirtemplate.dot_type = EXT2_FT_DIR;
	}
	dirtemplate.dot_name[0] = '.';
	dirtemplate.dotdot_ino = h2fs32(dp->i_number);
	dirtemplate.dotdot_reclen = h2fs16(VTOI(dvp)->i_e2fs->e2fs_bsize - 12);
	dirtemplate.dotdot_namlen = 2;
	if (EXT2F_HAS_INCOMPAT_FEATURE(dp->i_e2fs, EXT2F_INCOMPAT_FTYPE)) {
		dirtemplate.dotdot_type = EXT2_FT_DIR;
	}
	dirtemplate.dotdot_name[0] = dirtemplate.dotdot_name[1] = '.';
	error = ufs_bufio(UIO_WRITE, tvp, (void *)&dirtemplate,
	    sizeof (dirtemplate), (off_t)0, IO_NODELOCKED|IO_SYNC,
	    cnp->cn_cred, (size_t *)0, NULL);
	if (error) {
		if (dp->i_e2fs_nlink != EXT2FS_LINK_INF)
			dp->i_e2fs_nlink--;
		dp->i_flag |= IN_CHANGE;
		goto bad;
	}
	if (VTOI(dvp)->i_e2fs->e2fs_bsize > dvp->v_mount->mnt_stat.f_bsize)
		panic("ext2fs_mkdir: blksize"); /* XXX should grow with balloc() */
	else {
		error = ext2fs_setsize(ip, VTOI(dvp)->i_e2fs->e2fs_bsize);
		if (error) {
			if (dp->i_e2fs_nlink != EXT2FS_LINK_INF)
				dp->i_e2fs_nlink--;
			dp->i_flag |= IN_CHANGE;
			goto bad;
		}
		ip->i_flag |= IN_CHANGE;
		uvm_vnp_setsize(tvp, ext2fs_size(ip));
	}

	/* Directory set up, now install its entry in the parent directory. */
	error = ext2fs_direnter(ip, dvp, ulr, cnp);
	if (error != 0) {
		if (dp->i_e2fs_nlink != EXT2FS_LINK_INF)
			dp->i_e2fs_nlink--;
		dp->i_flag |= IN_CHANGE;
	}
bad:
	/*
	 * No need to do an explicit ext2fs_truncate here, vrele will do this
	 * for us because we set the link count to 0.
	 */
	if (error) {
		ip->i_e2fs_nlink = 0;
		ip->i_flag |= IN_CHANGE;
		vput(tvp);
	} else {
		VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
		VOP_UNLOCK(tvp);
		*ap->a_vpp = tvp;
	}
out:
	return error;
}

/*
 * Rmdir system call.
 */
int
ext2fs_rmdir(void *v)
{
	struct vop_rmdir_v2_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip, *dp;
	int error;
	struct ufs_lookup_results *ulr;

	ip = VTOI(vp);
	dp = VTOI(dvp);

	/* XXX should handle this material another way */
	ulr = &dp->i_crap;
	UFS_CHECK_CRAPCOUNTER(dp);

	/*
	 * No rmdir "." please.
	 */
	if (dp == ip) {
		vrele(vp);
		return EINVAL;
	}
	/*
	 * Verify the directory is empty (and valid).
	 * (Rmdir ".." won't be valid since
	 *  ".." will contain a reference to
	 *  the current directory and thus be
	 *  non-empty.)
	 */
	error = 0;
	if ((ip->i_e2fs_nlink != 2 && ip->i_e2fs_nlink != EXT2FS_LINK_INF) ||
	    !ext2fs_dirempty(ip, dp->i_number, cnp->cn_cred)) {
		error = ENOTEMPTY;
		goto out;
	}
	if ((dp->i_e2fs_flags & EXT2_APPEND) ||
				 (ip->i_e2fs_flags & (EXT2_IMMUTABLE | EXT2_APPEND))) {
		error = EPERM;
		goto out;
	}
	/*
	 * Delete reference to directory before purging
	 * inode.  If we crash in between, the directory
	 * will be reattached to lost+found,
	 */
	error = ext2fs_dirremove(dvp, ulr, cnp);
	if (error != 0)
		goto out;
	if (dp->i_e2fs_nlink != EXT2FS_LINK_INF)
		dp->i_e2fs_nlink--;
	dp->i_flag |= IN_CHANGE;
	VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
	cache_purge(dvp);
	/*
	 * Truncate inode.  The only stuff left
	 * in the directory is "." and "..".  The
	 * "." reference is inconsequential since
	 * we're quashing it.  The ".." reference
	 * has already been adjusted above.  We've
	 * removed the "." reference and the reference
	 * in the parent directory, but there may be
	 * other hard links so decrement by 2 and
	 * worry about them later.
	 */
	ip->i_e2fs_nlink -= 2;
	error = ext2fs_truncate(vp, (off_t)0, IO_SYNC, cnp->cn_cred);
	cache_purge(ITOV(ip));
out:
	VN_KNOTE(vp, NOTE_DELETE);
	vput(vp);
	return error;
}

/*
 * symlink -- make a symbolic link
 */
int
ext2fs_symlink(void *v)
{
	struct vop_symlink_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;
	struct vnode	*vp, **vpp;
	struct inode	*ip;
	int		len, error;

	vpp = ap->a_vpp;
	KASSERT(ap->a_vap->va_type == VLNK);
	error = ext2fs_makeinode(ap->a_vap, ap->a_dvp, vpp, ap->a_cnp, 1);
	if (error)
		return error;
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	vp = *vpp;
	len = strlen(ap->a_target);
	ip = VTOI(vp);
	if (len < ip->i_ump->um_maxsymlinklen) {
		memcpy(ip->i_din.e2fs_din->e2di_shortlink, ap->a_target, len);
		error = ext2fs_setsize(ip, len);
		if (error)
			goto bad;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		if (vp->v_mount->mnt_flag & MNT_RELATIME)
			ip->i_flag |= IN_ACCESS;
		uvm_vnp_setsize(vp, len);
	} else
		error = ufs_bufio(UIO_WRITE, vp, ap->a_target, len, (off_t)0,
		    IO_NODELOCKED, ap->a_cnp->cn_cred, (size_t *)0, NULL);
bad:
	VOP_UNLOCK(vp);
	if (error)
		vrele(vp);
	return error;
}

/*
 * Return target name of a symbolic link
 */
int
ext2fs_readlink(void *v)
{
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode	*vp = ap->a_vp;
	struct inode	*ip = VTOI(vp);
	struct ufsmount	*ump = ip->i_ump;
	int		isize;

	isize = ext2fs_size(ip);
	if (isize < ump->um_maxsymlinklen ||
	    (ump->um_maxsymlinklen == 0 && ext2fs_nblock(ip) == 0)) {
		uiomove(ip->i_din.e2fs_din->e2di_shortlink, isize, ap->a_uio);
		return 0;
	}
	return UFS_BUFRD(vp, ap->a_uio, 0, ap->a_cred);
}

/*
 * Advisory record locking support
 */
int
ext2fs_advlock(void *v)
{
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		void * a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap = v;
	struct inode *ip = VTOI(ap->a_vp);

	return lf_advlock(ap, &ip->i_lockf, ext2fs_size(ip));
}

int
ext2fs_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int a_flags;
		off_t offlo;
		off_t offhi;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int wait;
	int error;

	wait = (ap->a_flags & FSYNC_WAIT) != 0;

	if (vp->v_type == VBLK)
		error = spec_fsync(v);
	else
		error = vflushbuf(vp, ap->a_flags);
	if (error == 0 && (ap->a_flags & FSYNC_DATAONLY) == 0)
		error = ext2fs_update(vp, NULL, NULL, wait ? UPDATE_WAIT : 0);

	if (error == 0 && ap->a_flags & FSYNC_CACHE) {
		int l = 0;
		error = VOP_IOCTL(VTOI(vp)->i_devvp, DIOCCACHESYNC, &l, FWRITE,
		    curlwp->l_cred);
	}

	return error;
}

/*
 * Initialize the vnode associated with a new inode, handle aliased
 * vnodes.
 */
int
ext2fs_vinit(struct mount *mntp, int (**specops)(void *),
	int (**fifoops)(void *), struct vnode **vpp)
{
	struct timeval tv;
	struct inode *ip;
	struct vnode *vp;

	vp = *vpp;
	ip = VTOI(vp);
	switch(vp->v_type = IFTOVT(ip->i_e2fs_mode)) {
	case VCHR:
	case VBLK:
		vp->v_op = specops;
		spec_node_init(vp, fs2h32(ip->i_din.e2fs_din->e2di_rdev));
		break;
	case VFIFO:
		vp->v_op = fifoops;
		break;
	case VNON:
	case VBAD:
	case VSOCK:
	case VLNK:
	case VDIR:
	case VREG:
		break;
	}
	if (ip->i_number == UFS_ROOTINO)
                vp->v_vflag |= VV_ROOT;
	/*
	 * Initialize modrev times
	 */
	getmicrouptime(&tv);
	SETHIGH(ip->i_modrev, tv.tv_sec);
	SETLOW(ip->i_modrev, tv.tv_usec * 4294U);
	*vpp = vp;
	return 0;
}

/*
 * Allocate a new inode.
 */
static int
ext2fs_makeinode(struct vattr *vap, struct vnode *dvp, struct vnode **vpp,
		struct componentname *cnp, int do_direnter)
{
	struct inode *ip, *pdir;
	struct vnode *tvp;
	int error;
	struct ufs_lookup_results *ulr;

	pdir = VTOI(dvp);

	/* XXX should handle this material another way */
	ulr = &pdir->i_crap;
	UFS_CHECK_CRAPCOUNTER(pdir);

	*vpp = NULL;

	error = vcache_new(dvp->v_mount, dvp, vap, cnp->cn_cred, NULL, &tvp);
	if (error)
		return error;
	error = vn_lock(tvp, LK_EXCLUSIVE);
	if (error) {
		vrele(tvp);
		return error;
	}
	ip = VTOI(tvp);
	if (do_direnter) {
		/*
		 * Make sure inode goes to disk before directory entry.
		 */
		if ((error = ext2fs_update(tvp, NULL, NULL, UPDATE_WAIT)) != 0)
			goto bad;
		error = ext2fs_direnter(ip, dvp, ulr, cnp);
		if (error != 0)
			goto bad;
	}

	*vpp = tvp;
	cache_enter(dvp, *vpp, cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_flags);
	return 0;

bad:
	/*
	 * Write error occurred trying to update the inode
	 * or the directory so must deallocate the inode.
	 */
	ip->i_e2fs_nlink = 0;
	ip->i_flag |= IN_CHANGE;
	vput(tvp);
	return error;
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */
int
ext2fs_reclaim(void *v)
{
	struct vop_reclaim_v2_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	int error;

	VOP_UNLOCK(vp);

	/*
	 * The inode must be freed and updated before being removed
	 * from its hash chain.  Other threads trying to gain a hold
	 * or lock on the inode will be stalled.
	 */
	if (ip->i_omode == 1 && (vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
		ext2fs_vfree(vp, ip->i_number, ip->i_e2fs_mode);
	if ((error = ufs_reclaim(vp)) != 0)
		return error;
	if (ip->i_din.e2fs_din != NULL)
		kmem_free(ip->i_din.e2fs_din, EXT2_DINODE_SIZE(ip->i_e2fs));
	genfs_node_destroy(vp);
	pool_put(&ext2fs_inode_pool, vp->v_data);
	vp->v_data = NULL;
	return 0;
}

/* Global vfs data structures for ext2fs. */
int (**ext2fs_vnodeop_p)(void *);
const struct vnodeopv_entry_desc ext2fs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_parsepath_desc, genfs_parsepath },	/* parsepath */
	{ &vop_lookup_desc, ext2fs_lookup },		/* lookup */
	{ &vop_create_desc, ext2fs_create },		/* create */
	{ &vop_mknod_desc, ext2fs_mknod },		/* mknod */
	{ &vop_open_desc, ext2fs_open },		/* open */
	{ &vop_close_desc, ufs_close },			/* close */
	{ &vop_access_desc, ext2fs_access },		/* access */
	{ &vop_accessx_desc, genfs_accessx },		/* accessx */
	{ &vop_getattr_desc, ext2fs_getattr },		/* getattr */
	{ &vop_setattr_desc, ext2fs_setattr },		/* setattr */
	{ &vop_read_desc, ext2fs_read },		/* read */
	{ &vop_write_desc, ext2fs_write },		/* write */
	{ &vop_fallocate_desc, genfs_eopnotsupp },	/* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp },	/* fdiscard */
	{ &vop_ioctl_desc, genfs_enoioctl },		/* ioctl */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_poll_desc, genfs_poll },			/* poll */
	{ &vop_kqfilter_desc, genfs_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, genfs_revoke },		/* revoke */
	{ &vop_mmap_desc, genfs_mmap },			/* mmap */
	{ &vop_fsync_desc, ext2fs_fsync },		/* fsync */
	{ &vop_seek_desc, genfs_seek },			/* seek */
	{ &vop_remove_desc, ext2fs_remove },		/* remove */
	{ &vop_link_desc, ext2fs_link },		/* link */
	{ &vop_rename_desc, ext2fs_rename },		/* rename */
	{ &vop_mkdir_desc, ext2fs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, ext2fs_rmdir },		/* rmdir */
	{ &vop_symlink_desc, ext2fs_symlink },		/* symlink */
	{ &vop_readdir_desc, ext2fs_readdir },		/* readdir */
	{ &vop_readlink_desc, ext2fs_readlink },	/* readlink */
	{ &vop_abortop_desc, genfs_abortop },		/* abortop */
	{ &vop_inactive_desc, ext2fs_inactive },	/* inactive */
	{ &vop_reclaim_desc, ext2fs_reclaim },		/* reclaim */
	{ &vop_lock_desc, genfs_lock },			/* lock */
	{ &vop_unlock_desc, genfs_unlock },		/* unlock */
	{ &vop_bmap_desc, ext2fs_bmap },		/* bmap */
	{ &vop_strategy_desc, ufs_strategy },		/* strategy */
	{ &vop_print_desc, ufs_print },			/* print */
	{ &vop_islocked_desc, genfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, ufs_pathconf },		/* pathconf */
	{ &vop_advlock_desc, ext2fs_advlock },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, genfs_getpages },		/* getpages */
	{ &vop_putpages_desc, genfs_putpages },		/* putpages */
	{ &vop_getextattr_desc, ext2fs_getextattr },	/* getextattr */
	{ &vop_setextattr_desc, ext2fs_setextattr },	/* setextattr */
	{ &vop_listextattr_desc, ext2fs_listextattr },	/* listextattr */
	{ &vop_deleteextattr_desc, ext2fs_deleteextattr },/* deleteextattr */
	{ NULL, NULL }
};
const struct vnodeopv_desc ext2fs_vnodeop_opv_desc =
	{ &ext2fs_vnodeop_p, ext2fs_vnodeop_entries };

int (**ext2fs_specop_p)(void *);
const struct vnodeopv_entry_desc ext2fs_specop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	GENFS_SPECOP_ENTRIES,
	{ &vop_close_desc, ufsspec_close },		/* close */
	{ &vop_access_desc, ext2fs_access },		/* access */
	{ &vop_accessx_desc, genfs_accessx },		/* accessx */
	{ &vop_getattr_desc, ext2fs_getattr },		/* getattr */
	{ &vop_setattr_desc, ext2fs_setattr },		/* setattr */
	{ &vop_read_desc, ufsspec_read },		/* read */
	{ &vop_write_desc, ufsspec_write },		/* write */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_fsync_desc, ext2fs_fsync },		/* fsync */
	{ &vop_inactive_desc, ext2fs_inactive },	/* inactive */
	{ &vop_reclaim_desc, ext2fs_reclaim },		/* reclaim */
	{ &vop_lock_desc, genfs_lock },			/* lock */
	{ &vop_unlock_desc, genfs_unlock },		/* unlock */
	{ &vop_print_desc, ufs_print },			/* print */
	{ &vop_islocked_desc, genfs_islocked },		/* islocked */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getextattr_desc, ext2fs_getextattr },	/* getextattr */
	{ &vop_setextattr_desc, ext2fs_setextattr },	/* setextattr */
	{ &vop_listextattr_desc, ext2fs_listextattr },	/* listextattr */
	{ &vop_deleteextattr_desc, ext2fs_deleteextattr },/* deleteextattr */
	{ NULL, NULL }
};
const struct vnodeopv_desc ext2fs_specop_opv_desc =
	{ &ext2fs_specop_p, ext2fs_specop_entries };

int (**ext2fs_fifoop_p)(void *);
const struct vnodeopv_entry_desc ext2fs_fifoop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	GENFS_FIFOOP_ENTRIES,
	{ &vop_close_desc, ufsfifo_close },		/* close */
	{ &vop_access_desc, ext2fs_access },		/* access */
	{ &vop_accessx_desc, genfs_accessx },		/* accessx */
	{ &vop_getattr_desc, ext2fs_getattr },		/* getattr */
	{ &vop_setattr_desc, ext2fs_setattr },		/* setattr */
	{ &vop_read_desc, ufsfifo_read },		/* read */
	{ &vop_write_desc, ufsfifo_write },		/* write */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_fsync_desc, ext2fs_fsync },		/* fsync */
	{ &vop_inactive_desc, ext2fs_inactive },	/* inactive */
	{ &vop_reclaim_desc, ext2fs_reclaim },		/* reclaim */
	{ &vop_lock_desc, genfs_lock },			/* lock */
	{ &vop_unlock_desc, genfs_unlock },		/* unlock */
	{ &vop_strategy_desc, vn_fifo_bypass },		/* strategy */
	{ &vop_print_desc, ufs_print },			/* print */
	{ &vop_islocked_desc, genfs_islocked },		/* islocked */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getextattr_desc, ext2fs_getextattr },	/* getextattr */
	{ &vop_setextattr_desc, ext2fs_setextattr },	/* setextattr */
	{ &vop_listextattr_desc, ext2fs_listextattr },	/* listextattr */
	{ &vop_deleteextattr_desc, ext2fs_deleteextattr },/* deleteextattr */
	{ NULL, NULL }
};
const struct vnodeopv_desc ext2fs_fifoop_opv_desc =
	{ &ext2fs_fifoop_p, ext2fs_fifoop_entries };
