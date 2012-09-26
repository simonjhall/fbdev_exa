.macro qsub a,b,c
	uxtab16 \a, \b, \c, ror #8
.endm

.macro usat a,b,c
	uxtb16 \a,\c, ror \b
.endm

.macro qadd a,b,c
	uqadd8 \a,\b,\c
.endm

