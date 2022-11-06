Chip-8xx
========

Chip-8 emulator with assembler written in C++.  
See [chip8.md](/chip8.md) for information about CHIP-8 assembly.


Examples
--------

Program to show the pressed key:

```
	ld v0, 5
	ld v1, 5

loop:
	ld v4, K
	ld F, v4
	cls
	drw v0, v1, 5
	jp loop

```

Program to move a ball using WASD keys:

```
define W v2
define A v3
define S v4
define D v5
define KEY vC
define XPOS v0
define YPOS v1
define LIM vA
define ONE vB

	ld I, ball ; Load sprite
	ld ONE, 1
	ld XPOS, 0
	ld YPOS, 0
	ld W, 0x5 ; W
	ld A, 0x7 ; A
	ld S, 0x8 ; S
	ld D, 0x9 ; D
loop:
	ld KEY, K
	; Move X
	sknp A
	sub XPOS, ONE
	sknp D
	add XPOS, ONE
	; Move Y
	sknp S
	add YPOS, ONE ; Y is inverted
	sknp W
	sub YPOS, ONE
	; Constrain
	ld LIM, 63
	and XPOS, LIM
	ld LIM, 31
	and YPOS, LIM
	; Clear & Draw
	cls
	drw XPOS, YPOS, 4
	jp loop


ball:
db 0b11000000
db 0b11000000
```
