# Simple x86/x64 Detours library

A very lightweight library for x86 and x64 JMP-Hooking with instruction rewriting and basic code relocation functionality. Makes heavy use of the [Zydis](https://github.com/zyantific/zydis) disassembler ❤️.

## Motivation

Jump hooks (also called "Detours", [originating from MS](https://github.com/microsoft/Detours)) are usually pretty simple in an 32-bit address space. Many APIs that you might want to intercept start with prologue assembler code such as:

```assembly
88 FF       mov edi, edi
55          push ebp
8B EC       mov ebp, esp
```

The `mov edi, edi` instruction is functionally useless, however it was introduced to allow for hotpatching: Note how none of these 3 instructions contain EIP-relative operands, and conveniently add up to exactly 5 bytes which is the size of a _relative_ `JMP` instruction. We usually use the following instruction to place such a hook:

```
E9 cd   |   JMP rel32   |   Jump near, relative, displacement relative to next instruction.
```

Since in 32-bit processes the virtual address space has a size of `2^32 Bytes = 4 GiB` and the jump's displacement has a size of 32 bits, we are able to reach _any_ address in the virtual address space.

So how does this form of hooking work in a nutshell?
```
1. Assume we want to hook a function at address A.

2. Starting from the instruction at address A, count the number of instructions until we cover at least 5 bytes. In the above case we need 3 instructions:

88 FF   mov edi, edi    (+2 bytes)
55      push ebp        (+1 bytes)
8B EC   mov ebp, esp    (+2 bytes)

Note that the instructions do not always perfectly occupy 5 bytes. We still have to save the _entire_ instruction, even if its opcodes are only partially covered.

3. Store the instructions' opcodes we counted in a temporary buffer.

4. Allocate memory to hold at least all of the stored opcodes plus an additional relative JMP (5 bytes). This memory region is called a "trampoline".

5. In the allocated memory, store the saved opcodes and place a JMP instruction right after them back to the next instruction (in this case to A+5).

6. At address A, place a jump to our own function (the "hook function") that we want to execute instead of the original function.

7. Return the address of the allocated block of memory and jump to it in our hook function after we are done doing whatever we want to do. This will both execute the original, overwritten instructions and let control flow continue from where it originally left off.
```

This will effectively introduce a redirection of control flow (a "hook"), where if the original function is called, control flow gets transferred to our hook function, we can do stuff in the context of the original function and then transfer control back to the original function.

We might ask two questions:
* What if the function I want to hook does not support hotpatching through the above mechanism?
* What if the function I want to hook does not use a stack frame pointer (ebp)?

Why can these 2 scenarios be problematic? If the function does support hotpatching and uses a frame pointer, then we will always exactly have 3 _non-relative_ (or _absolute_) instructions that we can safely overwrite to place a hook. Non-relative meaning that the instructions are not position-dependent. If they were, then executing them at another address leads to undefined behavior. In the worst case, the program might crash.

> An instruction is relative if it contains at least 1 EIP-relative operand.

Take for example a `JE rel32` instruction. Using some simple algebra, we can easily calculate the new displacement value it would have if this instruction is located in our allocated memory region.

What if we had a `JE rel8` instead? Our allocated memory region would likely be much further away than reachable through an 8-bit displacement. This is the first case of instruction rewriting we need: _operand widening_.

Every relative instruction will be rewritten such that each relative operand will use the `rel32` variant of the instruction, if possible. This is usually already sufficient in the 32-bit case. Instruction rewriting will often cause our copied instructions to desynchronize from the original code layout, meaning the calculation of the displacmement offsets will need to dynamically adjusted. Without rewriting, the displacement calculation would be as easy as adding `alloc_base - original_base` to every displacement value.

----

Now let's look at what happens if we switch to a 64-bit virtual address space. The main problem is the following: **The x86-64 architecture does not support 64-bit displacement values**. This implies that if your allocated region is further away than reachable through a 32-bit displacement, we cannot relocate the original instructions in-place at all.

**Example:**

```
cmp dword ptr ds:[0x00007FFB979942F0], r11d
```
The left operand `dword ptr ds:[0x00007FFB979942F0]` is relative but refers to some location addressable using 32 bits. `0x00007FFB979942F0` is the actual address of the value that my disassembler was nice enough to compute from the displacement automatically (but it is not encoded like this in the instruction!). If our allocated region is further away than 4 GiB from the original location, then we cannot refer to this value using displacements. This introduces the second case of instruction rewriting: _instruction absolutization_ (Very fancy name, I know).

We can equivalently write the following instructions as a replacement for the above `cmp`:

```
push rax
mov rax, [0x00007FFB979942F0]
cmp eax, r11d
pop rax
```

Note how all of these 4 instructions are _absolute_. A restriction is that only into `RAX` we can directly load a value from a 64-bit address. If we use another register such as `RBX`, then the instruction would be relative, encoded with a 32-bit displacement:
```
48:A1 F0429997FB7F0000 | mov rax, [0x00007FFB979942F0]  // valid, absolute
      ^^^^^^^^^^^^^^^^ absolute address

48:8B 1D00000000       | mov rbx, [0x00007FFB979942F0]  // needs displacement
      ^^^^^^^^^^ displacement, data is at +0x1D relative to next instruction
```
What if the original instruction uses `RAX`? This means we cannot use `RAX` as a temporary register, but this is the only one we can enforce absolute addressing with.

To solve this problem we will make use of a _relocation table_ within the trampoline memory region. This way we can be sure that its entries are reachable through 32-bit displacements. However, we still have to handle rewriting JCC & CALL instructions. They will make use of a jump table in addition to the relocation table.

## Algorithm

The basic algorithm implemented works like this:
```
- First pass:   * Iterate over original instructions,
                * Mark relative operands,
                * Annotate with the absolute address the 
                  relative operands would actually point to

- Create jump table at address `code_cave + 0x50`

- Create relocation table at address `code_cave + 0x100`

- Initialize jump & address tables:
    * First relocation table entry -> original_function + size
    * First jump table entry       -> First relocation table entry

- If instruction is not relative, copy it as is

- If instruction is relative, and neither a JCC nor a CALL, rewrite as follows:
    * Original:   op reg, [mem]

    * If original instruction does not use RAX: set `unused_reg := RAX`,
    * Otherwise:
        1. Create relocation table entry whose value is the annotated absolute address
        2. Set `mem = offset to relocation table entry`

    * Rewritten:  push unused_reg
                  mov unused_reg, [mem]
                  op reg, unused_reg
                  pop unused_reg

- If instruction is JCC or CALL,
    * Create entry in address table containing its absolute address
    * Create entry in jump table jumping to the entry in address table
    * Modify JCC/CALL to jump to the corresponding jump table entry
```

Here's an example to which the above algorithm was applied:
```
Original code: 
	sub rsp, 38										<- not relative
	xor r11d, r11d									<- not relative
	cmp dword ptr ds:[0x00007FFB979942F0], r11d		<- annotate abs_address = 0x00007FFB979942F0
	je 0x00007FFB9795C32E							<- annotate abs_address = 0x00007FFB9795C32E
	call 0x00000000AFFEB3B3							<- annotate abs_address = 0x00000000AFFEB3B3

Rewritten code:
	sub rsp, 38
	xor r11d, r11d
	push rax
	mov rax, [0x00007FFB979942F0]
	cmp eax, r11d
	pop rax
	je jump_table_entry_1
	call jump_table_entry_2
	jmp jump_table_entry_0

jump_table_entry_0:
	jmp qword ptr ds:[reloc_table_entry_0]
jump_table_entry_1:
	jmp qword ptr ds:[reloc_table_entry_1]
jump_table_entry_2:
	jmp qword ptr ds:[reloc_table_entry_2]

reloc_table_entry_0:
	dq (original_function + size)
reloc_table_entry_1:
	dq 0x00007FFB9795C32E
reloc_table_entry_2:
	dq 0x00007FFBAFFEB3B3
```