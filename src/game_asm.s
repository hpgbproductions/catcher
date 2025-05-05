@ #define OBJ_FROG(f)				(48 + f)		// 48~63

@ extern void DrawFrogSprite(int oam_num, int scr_x, int scr_y, int facing, int jump, int type, int frozen, int priority);
.GLOBL DrawFrogSprite
DrawFrogSprite:
	LDMFD	SP, {R4-R7}				@ Get the remaining arguments from the stack
	STMFD   SP!, {R4-R11, LR}     	@ save content of r4-r11 and link register into the sp register
	
	@ BEGIN FUNCTION
	@ Memory map:
			@ R0 oam_num
			@ R1 scr_x
			@ R2 scr_y
			@ R3 facing
			@ R4 jump
			@ R5 frog type ==> palbank
			@ R6 is frozen ==> attr0/1/2 address
			@ R7 priority
			@ R8 attr construction area
			@ R9 bitmask construction area
			@ R10 sprite TID
			@ R11 
	
	@ Set palbank
	CMP		R6, #0					@ Check if frozen
	MOVNE	R5, #4					@ Yes ==> palbank = 4
	ADDEQ	R5, R5, #1				@ No ==> palbank = color + 1
	
	@ Get address of attr0
	MOV		R6, #0x07000000			@ R6 = attr(0, 0) = 0x07000000
	ADD		R6, R6, R0, LSL #3		@ R6 += obj*sizeof(obj) = ptr to attr(frog, 0)
	
	@ BEGIN attr0
	LDRH	R8, [R6]				@ Load halfword attr0 into R8, no update
	AND		R2, R2, #0xFF			@ Constrain y to supported values (0xFF is bitmask of attr0_y)
	BIC		R8, R8, #0xFF			@ Clear current value of y
	ORR		R8, R8, R2				@ Apply value of y
	BIC		R8, R8, #0x200			@ Unhide the frog (does nothing if already visible)
	STRH	R8, [R6], #2			@ Store halfword in attr0, then update to attr1
	@ END attr0
	
	@ BEGIN attr1
	LDRH	R8, [R6]				@ Load halfword attr1 into R8, no update
	MOV		R9, #0x100				@ bitmask = 0x100
	ADD		R9, R9, #0xFF			@ bitmask = 0x1FF ==> bitmask of attr1_x
	AND		R1, R1, R9				@ Constrain x to supported values
	BIC		R8, R8, R9				@ Clear current value of x
	ORR		R8, R8, R1				@ Apply value of x
	
	CMP		R3, #2					@ Frog facing left?
	ORREQ	R8, R8, #0x1000			@ Yes ==> horizontal flip
	BICNE	R8, R8, #0x1000			@ No ==> unflip
	STRH	R8, [R6], #2			@ Store halfword in attr1, then update to attr2
	@ END attr1
	
	@ BEGIN attr2
	LDRH	R8, [R6]				@ Load halfword attr1 into R8, no update
	ADD		R9, R9, #0x200			@ bitmask = 0x3FF ==> bitmask of attr2_id
	BIC		R8, R8, R9				@ Clear current sprite id
	
	CMP		R3, #1					@ Frog facing down?
	MOV		R10, #48				@ Yes ==> TID of frog front grounded and skip other CMP
	BEQ		DrawFrogSprite_GotInitialTid
	
	CMP		R3, #3					@ Frog facing up?
	MOVEQ	R10, #56				@ Yes ==> TID of frog back grounded
	MOVNE	R10, #64				@ No ==> TID of frog right grounded (shared with left)
	
	DrawFrogSprite_GotInitialTid:
	CMP		R4, #0					@ Frog on the ground?
	ADDNE	R10, R10, #4			@ No ==> in the air, add 4 to get airborne frog TID
	ORR		R8, R8, R10				@ Apply TID
	
	BIC		R8, R8, #0xFC00			@ Clear current priority and palbank
	ORR		R8, R8, R7, LSL #10		@ Apply priority
	ORR		R8, R8, R5, LSL #12		@ Apply palbank
	STRH	R8, [R6]				@ Store halfword in attr2
	@ END attr2
	
	LDMFD   SP!, {R4-R11, LR}    	@ Recover past state of r4-r11 and link register from sp register
	MOV     PC, LR					@ Branch back to lr (go back to C code that called this function)

