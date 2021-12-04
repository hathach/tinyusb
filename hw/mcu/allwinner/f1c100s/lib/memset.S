/*
 * memcpy.S
 */
	.text

    .global memset
    .type memset, %function
    .align 4

memset:
	stmfd	sp!, {r0}				/* remember address for return value */
	and		r1, r1, #0x000000ff		/* we write bytes */

	cmp		r2, #0x00000004			/* do we have less than 4 bytes */
	blt		.Lmemset_lessthanfour

	/* first we will word align the address */
	ands	r3, r0, #0x00000003		/* get the bottom two bits */
	beq		.Lmemset_addraligned	/* the address is word aligned */

	rsb		r3, r3, #0x00000004
	sub		r2, r2, r3
	cmp		r3, #0x00000002
	strb	r1, [r0], #0x0001		/* set 1 byte */
	strgeb	r1, [r0], #0x0001		/* set another byte */
	strgtb	r1, [r0], #0x0001		/* and a third */

	cmp		r2, #0x00000004
	blt		.Lmemset_lessthanfour

	/* now we must be word aligned */
.Lmemset_addraligned:
	orr		r3, r1, r1, lsl #8		/* repeat the byte into a word */
	orr		r3, r3, r3, lsl #16

	/* we know we have at least 4 bytes ... */
	cmp		r2, #0x00000020			/* if less than 32 then use words */
	blt		.Lmemset_lessthan32

	/* we have at least 32 so lets use quad words */
	stmfd	sp!, {r4-r6}			/* store registers */
	mov		r4, r3					/* duplicate data */
	mov		r5, r3
	mov		r6, r3

.Lmemset_loop16:
	stmia	r0!, {r3-r6}			/* store 16 bytes */
	sub		r2, r2, #0x00000010		/* adjust count */
	cmp		r2, #0x00000010			/* still got at least 16 bytes ? */
	bgt		.Lmemset_loop16

	ldmfd	sp!, {r4-r6}			/* restore registers */

	/* do we need to set some words as well ? */
	cmp		r2, #0x00000004
	blt		.Lmemset_lessthanfour

	/* have either less than 16 or less than 32 depending on route taken */
.Lmemset_lessthan32:

	/* we have at least 4 bytes so copy as words */
.Lmemset_loop4:
	str		r3, [r0], #0x0004
	sub		r2, r2, #0x0004
	cmp		r2, #0x00000004
	bge		.Lmemset_loop4

.Lmemset_lessthanfour:
	cmp		r2, #0x00000000
	ldmeqfd	sp!, {r0}
	moveq	pc, lr					/* zero length so exit */

	cmp		r2, #0x00000002
	strb	r1, [r0], #0x0001		/* set 1 byte */
	strgeb	r1, [r0], #0x0001		/* set another byte */
	strgtb	r1, [r0], #0x0001		/* and a third */

	ldmfd	sp!, {r0}
	mov		pc, lr					/* exit */
