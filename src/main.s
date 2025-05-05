	.cpu arm7tdmi
	.fpu softvfp
	.eabi_attribute 20, 1	@ Tag_ABI_FP_denormal
	.eabi_attribute 21, 1	@ Tag_ABI_FP_exceptions
	.eabi_attribute 23, 3	@ Tag_ABI_FP_number_model
	.eabi_attribute 24, 1	@ Tag_ABI_align8_needed
	.eabi_attribute 25, 1	@ Tag_ABI_align8_preserved
	.eabi_attribute 26, 1	@ Tag_ABI_enum_size
	.eabi_attribute 30, 2	@ Tag_ABI_optimization_goals
	.eabi_attribute 34, 0	@ Tag_CPU_unaligned_access
	.eabi_attribute 18, 4	@ Tag_ABI_PCS_wchar_t
	.file	"main.c"
@ GNU C11 (GNU Tools for ARM Embedded Processors) version 5.3.1 20160307 (release) [ARM/embedded-5-branch revision 234589] (arm-none-eabi)
@	compiled by GNU C version 4.7.4, GMP version 4.3.2, MPFR version 2.4.2, MPC version 0.8.1
@ GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
@ options passed:  -fpreprocessed main.i -mthumb-interwork -mlong-calls
@ -auxbase-strip main.o -O2 -Wall -fverbose-asm
@ options enabled:  -faggressive-loop-optimizations -falign-functions
@ -falign-jumps -falign-labels -falign-loops -fauto-inc-dec
@ -fbranch-count-reg -fcaller-saves -fchkp-check-incomplete-type
@ -fchkp-check-read -fchkp-check-write -fchkp-instrument-calls
@ -fchkp-narrow-bounds -fchkp-optimize -fchkp-store-bounds
@ -fchkp-use-static-bounds -fchkp-use-static-const-bounds
@ -fchkp-use-wrappers -fcombine-stack-adjustments -fcommon -fcompare-elim
@ -fcprop-registers -fcrossjumping -fcse-follow-jumps -fdefer-pop
@ -fdelete-null-pointer-checks -fdevirtualize -fdevirtualize-speculatively
@ -fdwarf2-cfi-asm -fearly-inlining -feliminate-unused-debug-types
@ -fexpensive-optimizations -fforward-propagate -ffunction-cse -fgcse
@ -fgcse-lm -fgnu-runtime -fgnu-unique -fguess-branch-probability
@ -fhoist-adjacent-loads -fident -fif-conversion -fif-conversion2
@ -findirect-inlining -finline -finline-atomics
@ -finline-functions-called-once -finline-small-functions -fipa-cp
@ -fipa-cp-alignment -fipa-icf -fipa-icf-functions -fipa-icf-variables
@ -fipa-profile -fipa-pure-const -fipa-ra -fipa-reference -fipa-sra
@ -fira-hoist-pressure -fira-share-save-slots -fira-share-spill-slots
@ -fisolate-erroneous-paths-dereference -fivopts -fkeep-static-consts
@ -fleading-underscore -flifetime-dse -flra-remat -flto-odr-type-merging
@ -fmath-errno -fmerge-constants -fmerge-debug-strings
@ -fmove-loop-invariants -fomit-frame-pointer -foptimize-sibling-calls
@ -foptimize-strlen -fpartial-inlining -fpeephole -fpeephole2
@ -fprefetch-loop-arrays -freg-struct-return -freorder-blocks
@ -freorder-functions -frerun-cse-after-loop
@ -fsched-critical-path-heuristic -fsched-dep-count-heuristic
@ -fsched-group-heuristic -fsched-interblock -fsched-last-insn-heuristic
@ -fsched-pressure -fsched-rank-heuristic -fsched-spec
@ -fsched-spec-insn-heuristic -fsched-stalled-insns-dep -fschedule-insns
@ -fschedule-insns2 -fsection-anchors -fsemantic-interposition
@ -fshow-column -fshrink-wrap -fsigned-zeros -fsplit-ivs-in-unroller
@ -fsplit-wide-types -fssa-phiopt -fstdarg-opt -fstrict-aliasing
@ -fstrict-overflow -fstrict-volatile-bitfields -fsync-libcalls
@ -fthread-jumps -ftoplevel-reorder -ftrapping-math -ftree-bit-ccp
@ -ftree-builtin-call-dce -ftree-ccp -ftree-ch -ftree-coalesce-vars
@ -ftree-copy-prop -ftree-copyrename -ftree-cselim -ftree-dce
@ -ftree-dominator-opts -ftree-dse -ftree-forwprop -ftree-fre
@ -ftree-loop-if-convert -ftree-loop-im -ftree-loop-ivcanon
@ -ftree-loop-optimize -ftree-parallelize-loops= -ftree-phiprop -ftree-pre
@ -ftree-pta -ftree-reassoc -ftree-scev-cprop -ftree-sink -ftree-slsr
@ -ftree-sra -ftree-switch-conversion -ftree-tail-merge -ftree-ter
@ -ftree-vrp -funit-at-a-time -fverbose-asm -fzero-initialized-in-bss -marm
@ -mlittle-endian -mlong-calls -mpic-data-is-text-relative -msched-prolog
@ -mthumb-interwork -mvectorize-with-neon-quad

	.global	InGame
	.text
	.align	2
	.global	Handler
	.type	Handler, %function
Handler:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}	@,
	add	fp, sp, #4	@,,
	sub	sp, sp, #8	@,,
	ldr	r3, .L7	@ D.4265,
	mov	r2, #0	@ tmp129,
	strh	r2, [r3]	@ movhi	@ tmp128, *_4
	ldr	r3, .L7+4	@ D.4265,
	ldrh	r3, [r3]	@ movhi	@ tmp130, *_7
	mov	r3, r3, asl #16	@ tmp131, tmp130,
	mov	r3, r3, lsr #16	@ D.4266, tmp131,
	and	r3, r3, #1	@ D.4267, D.4267,
	cmp	r3, #0	@ D.4267,
	beq	.L2	@,
	ldr	r3, .L7+8	@ tmp132,
	ldr	r3, [r3]	@ D.4267, InGame
	cmp	r3, #0	@ D.4267,
	beq	.L3	@,
	ldr	r3, .L7+12	@ tmp133,
	mov	lr, pc
	bx	r3	@ tmp133
	str	r0, [fp, #-8]	@, isGameOver
	ldr	r3, [fp, #-8]	@ tmp134, isGameOver
	cmp	r3, #0	@ tmp134,
	beq	.L6	@,
	ldr	r3, .L7+8	@ tmp135,
	mov	r2, #0	@ tmp136,
	str	r2, [r3]	@ tmp136, InGame
	ldr	r3, .L7+16	@ tmp137,
	mov	lr, pc
	bx	r3	@ tmp137
	b	.L6	@
.L3:
	ldr	r3, .L7+20	@ D.4265,
	ldrh	r3, [r3]	@ movhi	@ tmp138, *_16
	mov	r3, r3, asl #16	@ tmp139, tmp138,
	mov	r3, r3, lsr #16	@ D.4266, tmp139,
	and	r3, r3, #8	@ D.4267, D.4267,
	cmp	r3, #0	@ D.4267,
	bne	.L6	@,
	ldr	r3, .L7+24	@ tmp140,
	mov	lr, pc
	bx	r3	@ tmp140
	ldr	r3, .L7+8	@ tmp141,
	mov	r2, #1	@ tmp142,
	str	r2, [r3]	@ tmp142, InGame
	b	.L6	@
.L2:
	ldr	r3, .L7+4	@ D.4265,
	ldrh	r3, [r3]	@ movhi	@ tmp143, *_22
	mov	r3, r3, asl #16	@ tmp144, tmp143,
	mov	r3, r3, lsr #16	@ D.4266, tmp144,
	and	r3, r3, #64	@ D.4267, D.4267,
	cmp	r3, #0	@ D.4267,
	beq	.L6	@,
	ldr	r3, .L7+28	@ tmp145,
	mov	lr, pc
	bx	r3	@ tmp145
.L6:
	ldr	r2, .L7+4	@ D.4265,
	ldr	r3, .L7+4	@ D.4265,
	ldrh	r3, [r3]	@ movhi	@ tmp146, *_28
	mov	r3, r3, asl #16	@ tmp147, tmp146,
	mov	r3, r3, lsr #16	@ D.4266, tmp147,
	strh	r3, [r2]	@ movhi	@ D.4266, *_27
	ldr	r3, .L7	@ D.4265,
	mov	r2, #1	@ tmp149,
	strh	r2, [r3]	@ movhi	@ tmp148, *_31
	mov	r0, r0	@ nop
	sub	sp, fp, #4	@,,
	@ sp needed	@
	ldmfd	sp!, {fp, lr}	@
	bx	lr	@
.L8:
	.align	2
.L7:
	.word	67109384
	.word	67109378
	.word	InGame
	.word	Update
	.word	SplashSetup
	.word	67109168
	.word	GameSetup
	.word	OnTimerEnd
	.size	Handler, .-Handler
	.align	2
	.global	main
	.type	main, %function
main:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	stmfd	sp!, {fp, lr}	@,
	add	fp, sp, #4	@,,
	ldr	r3, .L11	@ D.4268,
	mov	r2, #0	@ tmp121,
	strh	r2, [r3]	@ movhi	@ tmp120, *_1
	ldr	r3, .L11+4	@ D.4269,
	mov	r2, #65	@ tmp123,
	strh	r2, [r3]	@ movhi	@ tmp122, *_4
	mov	r3, #67108864	@ D.4270,
	mov	r2, #4416	@ tmp124,
	str	r2, [r3]	@ tmp124, *_6
	ldr	r3, .L11+8	@ D.4269,
	mov	r2, #8	@ tmp126,
	strh	r2, [r3]	@ movhi	@ tmp125, *_8
	ldr	r3, .L11+12	@ D.4269,
	ldr	r2, .L11+16	@ tmp128,
	strh	r2, [r3]	@ movhi	@ tmp127, *_10
	ldr	r3, .L11+20	@ D.4269,
	mov	r2, #2432	@ tmp130,
	strh	r2, [r3]	@ movhi	@ tmp129, *_12
	ldr	r3, .L11+24	@ D.4270,
	ldr	r2, .L11+28	@ D.4271,
	str	r2, [r3]	@ D.4271, *_14
	ldr	r3, .L11+32	@ tmp131,
	mov	lr, pc
	bx	r3	@ tmp131
	ldr	r3, .L11+36	@ tmp132,
	mov	lr, pc
	bx	r3	@ tmp132
	ldr	r3, .L11	@ D.4268,
	mov	r2, #1	@ tmp134,
	strh	r2, [r3]	@ movhi	@ tmp133, *_19
.L10:
	b	.L10	@
.L12:
	.align	2
.L11:
	.word	67109384
	.word	67109376
	.word	67108868
	.word	67108872
	.word	2179
	.word	67108874
	.word	50364412
	.word	Handler
	.word	DataSetup
	.word	SplashSetup
	.size	main, .-main
	.bss
	.align	2
	.type	InGame, %object
	.size	InGame, 4
InGame:
	.space	4
	.ident	"GCC: (GNU Tools for ARM Embedded Processors) 5.3.1 20160307 (release) [ARM/embedded-5-branch revision 234589]"
