/*	$NetBSD: msg_259.c,v 1.16 2021/08/31 19:26:23 rillig Exp $	*/
# 3 "msg_259.c"

// Test for message: argument #%d is converted from '%s' to '%s' due to prototype [259]

/*
 * See also msg_297, but that requires the flags -a -p -P, which are not
 * enabled in the default NetBSD build.
 */

/* lint1-only-if: lp64 */
/* lint1-extra-flags: -h */

void plain_char(char);
void signed_int(int);
void unsigned_int(unsigned int);
void signed_long(long);
void unsigned_long(unsigned long);
void signed_long_long(long long);
void unsigned_long_long(unsigned long long);

void
change_in_type_width(char c, int i, long l)
{
	plain_char(c);
	signed_int(c);
	/* No warning 259 on LP64, only on ILP32 */
	signed_long(c);

	plain_char(i);		/* XXX: why no warning? */
	signed_int(i);
	/* No warning 259 on LP64, only on ILP32 */
	signed_long(i);

	plain_char(l);		/* XXX: why no warning? */
	/* expect+1: from 'long' to 'int' due to prototype [259] */
	signed_int(l);
	signed_long(l);
}

/*
 * Converting a signed integer type to its corresponding unsigned integer
 * type (C99 6.2.5p6) is usually not a problem since the actual values of the
 * expressions are usually not anywhere near the maximum signed value.  From
 * a technical standpoint, it is correct to warn here since even small
 * negative numbers may result in very large positive numbers.
 *
 * A common case where it occurs is when the difference of two pointers is
 * converted to size_t.  The type ptrdiff_t is defined to be signed, but in
 * many practical cases, the expression is '(end - start)', which makes the
 * resulting value necessarily positive.
 */
void
signed_to_unsigned(int si, long sl, long long sll)
{
	/* expect+1: warning: argument #1 is converted from 'int' to 'unsigned int' due to prototype [259] */
	unsigned_int(si);

	/* expect+1: warning: argument #1 is converted from 'long' to 'unsigned int' due to prototype [259] */
	unsigned_int(sl);

	/* expect+1: warning: argument #1 is converted from 'long long' to 'unsigned int' due to prototype [259] */
	unsigned_int(sll);

	/*
	 * XXX: Why no warning?  Even though 'unsigned long' is 64 bits
	 * wide, it cannot represent negative 32-bit values.
	 */
	unsigned_long(si);

	/* expect+1: warning: argument #1 is converted from 'long' to 'unsigned long' due to prototype [259] */
	unsigned_long(sl);
	/* expect+1: warning: argument #1 is converted from 'long long' to 'unsigned long' due to prototype [259] */
	unsigned_long(sll);

	/*
	 * XXX: Why no warning?  Even though 'unsigned long long' is 64 bits
	 * wide, it cannot represent negative 32-bit values.
	 */
	unsigned_long_long(si);

	/* expect+1: warning: argument #1 is converted from 'long' to 'unsigned long long' due to prototype [259] */
	unsigned_long_long(sl);

	/* expect+1: warning: argument #1 is converted from 'long long' to 'unsigned long long' due to prototype [259] */
	unsigned_long_long(sll);
}

void
unsigned_to_signed(unsigned int ui, unsigned long ul, unsigned long long ull)
{
	/* expect+1: warning: argument #1 is converted from 'unsigned int' to 'int' due to prototype [259] */
	signed_int(ui);
	/* expect+1: warning: argument #1 is converted from 'unsigned long' to 'int' due to prototype [259] */
	signed_int(ul);
	/* expect+1: warning: argument #1 is converted from 'unsigned long long' to 'int' due to prototype [259] */
	signed_int(ull);
	signed_long(ui);
	/* expect+1: warning: argument #1 is converted from 'unsigned long' to 'long' due to prototype [259] */
	signed_long(ul);
	/* expect+1: warning: argument #1 is converted from 'unsigned long long' to 'long' due to prototype [259] */
	signed_long(ull);
	signed_long_long(ui);
	/* expect+1: warning: argument #1 is converted from 'unsigned long' to 'long long' due to prototype [259] */
	signed_long_long(ul);
	/* expect+1: warning: argument #1 is converted from 'unsigned long long' to 'long long' due to prototype [259] */
	signed_long_long(ull);
}

void
signed_to_signed(signed int si, signed long sl, signed long long sll)
{
	signed_int(si);
	/* expect+1: warning: argument #1 is converted from 'long' to 'int' due to prototype [259] */
	signed_int(sl);
	/* expect+1: warning: argument #1 is converted from 'long long' to 'int' due to prototype [259] */
	signed_int(sll);
	signed_long(si);
	signed_long(sl);
	/* expect+1: warning: argument #1 is converted from 'long long' to 'long' due to prototype [259] */
	signed_long(sll);
	signed_long_long(si);
	/* expect+1: warning: argument #1 is converted from 'long' to 'long long' due to prototype [259] */
	signed_long_long(sl);
	signed_long_long(sll);
}

void
unsigned_to_unsigned(unsigned int ui, unsigned long ul, unsigned long long ull)
{
	unsigned_int(ui);
	/* expect+1: warning: argument #1 is converted from 'unsigned long' to 'unsigned int' due to prototype [259] */
	unsigned_int(ul);
	/* expect+1: warning: argument #1 is converted from 'unsigned long long' to 'unsigned int' due to prototype [259] */
	unsigned_int(ull);
	unsigned_long(ui);
	unsigned_long(ul);
	/* expect+1: warning: argument #1 is converted from 'unsigned long long' to 'unsigned long' due to prototype [259] */
	unsigned_long(ull);
	unsigned_long_long(ui);
	/* expect+1: warning: argument #1 is converted from 'unsigned long' to 'unsigned long long' due to prototype [259] */
	unsigned_long_long(ul);
	unsigned_long_long(ull);
}

void
pass_sizeof_as_smaller_type(void)
{
	/*
	 * XXX: Even though the expression has type size_t, it has a constant
	 * value that fits effortless into an 'unsigned int', it's so small
	 * that it would even fit into a 3-bit bit-field, so lint should not
	 * warn here.
	 */
	/* expect+1: warning: argument #1 is converted from 'unsigned long' to 'unsigned int' due to prototype [259] */
	unsigned_int(sizeof(int));
}
