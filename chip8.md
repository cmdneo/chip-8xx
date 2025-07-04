CHIP-8 Assembly 
===

Statement syntax (all fields are optional):  
`[Label] [Instruction or Directive] [Comment]`

Instructions, directive and register names are **case-insesetive**.

Label
---
A label is an label-name followed by a colon(`:`), it must not be an instruction, directive or register name.  
A label-name can contain alplanumeric characters and underscores(`_`), first character must not be a digit.  
Like: `label_name:`

Comment
---
A comment starts with a semicolon(`;`) and spans till the end of the line.  
Like: `; This is a comment`

Instructions
---
In place of address a label-name can be used.


| No. | Encoding | Semantic             |
| --- | -------- | -------------------- |
| 1   | `00E0`   | `CLS`                |
| 2*  | `00EE`   | `RET`                |
| 3   | `0nnn`   | `SYS addr` (ignored) |
| 4*  | `1nnn`   | `JP addr`            |
| 5*  | `2nnn`   | `CALL addr`          |
| 6*  | `3xkk`   | `SE Vx, byte`        |
| 7*  | `4xkk`   | `SNE Vx, byte`       |
| 8*  | `5xy0`   | `SE Vx, Vy`          |
| 9   | `6xkk`   | `LD Vx, byte`        |
| 10  | `7xkk`   | `ADD Vx, byte`       |
| 11  | `8xy0`   | `LD Vx, Vy`          |
| 12  | `8xy1`   | `OR Vx, Vy`          |
| 13  | `8xy2`   | `AND Vx, Vy`         |
| 14  | `8xy3`   | `XOR Vx, Vy`         |
| 15^ | `8xy4`   | `ADD Vx, Vy`         |
| 16^ | `8xy5`   | `SUB Vx, Vy`         |
| 17^ | `8xy6`   | `SHR Vx`             |
| 18^ | `8xy7`   | `SUBN Vx, Vy`        |
| 19^ | `8xyE`   | `SHL Vx`             |
| 20* | `9xy0`   | `SNE Vx, Vy`         |
| 21  | `Annn`   | `LD I, addr`         |
| 22* | `Bnnn`   | `JP V0, addr`        |
| 23  | `Cxkk`   | `RND Vx, byte`       |
| 24^ | `Dxyn`   | `DRW Vx, Vy, nibble` |
| 25* | `Ex9E`   | `SKP Vx`             |
| 26* | `ExA1`   | `SKNP Vx`            |
| 27  | `Fx07`   | `LD Vx, DT`          |
| 28  | `Fx0A`   | `LD Vx, K`           |
| 29  | `Fx15`   | `LD DT, Vx`          |
| 30  | `Fx18`   | `LD ST, Vx`          |
| 31  | `Fx1E`   | `ADD I, Vx`          |
| 32  | `Fx29`   | `LD F, Vx`           |
| 33  | `Fx33`   | `LD B, Vx`           |
| 34  | `Fx55`   | `LD [I], Vx`         |
| 35  | `Fx65`   | `LD Vx, [I]`         |

A label can be used in place of an `addr` operand.

Operand sizes: `addr`: 12 bits, `byte`: 8 bits, `nibble`: 4 bits.

General purpose Registers: `V0`, `V1`, `V2`, `V3`, `V4`, `V5`, `V6`, `V7`, `V8`, `V9`,
`VA`, `VB`, `VC`, `VD`, `VE`, `VF`  
Special Purpose Registers: `PC`, `SP`, `I`, `DT`, `ST`


__*__ : Branch Instructions  
__^__ : Sets carry flag (`VF` register)

Directives
---

| Directive             | Function                                   |
| --------------------- | ------------------------------------------ |
| `db byte`             | Puts a byte at the current memory location |
| `%define alias subst` | Dumb Textual replacement(see below)        |

For `%define` directive `alias` can be any identifier like token and
`subst` is everything from after the alias till the end-of-line
(newline not included).

First replacements are performed in a line for aliases defined by define directive,
then that line is processed.

**WARNING**: If instructions are not aligned by 2-bytes then it is undefined behaviour.
So when using `db` directive put even number of bytes, preferably at the end of the file.


Key mapping
-----------

	Original C8 keys     Mapped to keys
	|---|---|---|---|    |---|---|---|---|
	| 1 | 2 | 3 | C |    | 1 | 2 | 3 | 4 |
	| 4 | 5 | 6 | D |    | Q | W | E | R |
	| 7 | 8 | 9 | E |    | A | S | D | F |
	| A | 0 | B | F |    | Z | X | C | V |
	|---|---|---|---|    |---|---|---|---|

