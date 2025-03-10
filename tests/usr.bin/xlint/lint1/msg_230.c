/*	$NetBSD: msg_230.c,v 1.8 2021/08/28 15:25:10 rillig Exp $	*/
# 3 "msg_230.c"

// Test for message: nonportable character comparison, op %s [230]

/* lint1-flags: -S -g -p -w */
/* lint1-only-if: schar */

/*
 * C11 6.2.5p15 defines that 'char' has the same range, representation, and
 * behavior as either 'signed char' or 'unsigned char'.
 *
 * The portable range of 'char' is from 0 to 127 since all lint platforms
 * define CHAR_SIZE to be 8.
 *
 * See msg_162.c, which covers 'signed char' and 'unsigned char'.
 */

void
compare_plain_char(char c)
{
	/* expect+1: warning: nonportable character comparison, op == [230] */
	if (c == -129)
		return;
	/* expect+1: warning: nonportable character comparison, op == [230] */
	if (c == -128)
		return;
	/* expect+1: warning: nonportable character comparison, op == [230] */
	if (c == -1)
		return;
	if (c == 0)
		return;
	if (c == 127)
		return;
	/* expect+1: warning: nonportable character comparison, op == [230] */
	if (c == 128)
		return;
	/* expect+1: warning: nonportable character comparison, op == [230] */
	if (c == 255)
		return;
	/* expect+1: warning: nonportable character comparison, op == [230] */
	if (c == 256)
		return;
}

void
compare_plain_char_yoda(char c)
{
	/* expect+1: warning: nonportable character comparison, op == [230] */
	if (-129 == c)
		return;
	/* expect+1: warning: nonportable character comparison, op == [230] */
	if (-128 == c)
		return;
	/* expect+1: warning: nonportable character comparison, op == [230] */
	if (-1 == c)
		return;
	if (0 == c)
		return;
	if (127 == c)
		return;
	/* expect+1: warning: nonportable character comparison, op == [230] */
	if (128 == c)
		return;
	/* expect+1: warning: nonportable character comparison, op == [230] */
	if (255 == c)
		return;
	/* expect+1: warning: nonportable character comparison, op == [230] */
	if (256 == c)
		return;
}

void
compare_lt(char c)
{

	/* expect+1: warning: nonportable character comparison, op > [230] */
	if (c > -2)
		return;
	/* expect+1: warning: nonportable character comparison, op >= [230] */
	if (c >= -1)
		return;

	/*
	 * XXX: The following two comparisons have the same effect, yet lint
	 * only warns about one of them.
	 */
	/* expect+1: warning: nonportable character comparison, op > [230] */
	if (c > -1)
		return;
	/*
	 * On platforms where char is unsigned, lint warns that the
	 * comparison always evaluates to true; see msg_230_uchar.c.
	 */
	if (c >= 0)
		return;

	/*
	 * XXX: The following two comparisons have the same effect, yet lint
	 * only warns about one of them.
	 */
	if (c > 127)
		return;
	/* expect+1: warning: nonportable character comparison, op >= [230] */
	if (c >= 128)
		return;

	/* expect+1: warning: nonportable character comparison, op > [230] */
	if (c > 128)
		return;
	/* expect+1: warning: nonportable character comparison, op >= [230] */
	if (c >= 129)
		return;
}
