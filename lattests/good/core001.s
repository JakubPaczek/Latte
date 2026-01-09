.text
.extern printInt
.extern printString
.extern error
.extern readInt
.extern readString
.extern __latte_concat
.extern strcmp
.section .rodata
.LC0:
  .asciz ""
.LC1:
  .asciz "="
.LC2:
  .asciz "hello */"
.LC3:
  .asciz "/* world"
.text
.globl __latte_str_0
__latte_str_0:
  leaq .LC0(%rip), %rax
  ret
.globl __latte_str_1
__latte_str_1:
  leaq .LC1(%rip), %rax
  ret
.globl __latte_str_2
__latte_str_2:
  leaq .LC2(%rip), %rax
  ret
.globl __latte_str_3
__latte_str_3:
  leaq .LC3(%rip), %rax
  ret
.globl main
main:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rbx
  subq $8, %rsp
.L0:
  movl $10, %ecx
  subq $16, %rsp
  movl %ecx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call fac
  movl %eax, %esi
  subq $16, %rsp
  movl %esi, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call printInt
  movl $10, %ecx
  subq $16, %rsp
  movl %ecx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call rfac
  movl %eax, %esi
  subq $16, %rsp
  movl %esi, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call printInt
  movl $10, %ecx
  subq $16, %rsp
  movl %ecx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call mfac
  movl %eax, %esi
  subq $16, %rsp
  movl %esi, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call printInt
  movl $10, %ecx
  subq $16, %rsp
  movl %ecx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call ifac
  movl %eax, %esi
  subq $16, %rsp
  movl %esi, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call printInt
  call __latte_str_0
  movq %rax, %rcx
  movl $10, %ecx
  movl %ecx, %esi
  movl $1, %ecx
  movl %ecx, %edi
  jmp .L1
.L1:
  movl $0, %ecx
  movl %esi, %eax
  cmpl %ecx, %eax
  setg %al
  movzbl %al, %eax
  movl %eax, %ebx
  testl %ebx, %ebx
  je .L3
  jmp .L2
.L2:
  movl %edi, %eax
  imull %esi, %eax
  movl %eax, %ecx
  movl %ecx, %edi
  movl $1, %ecx
  movl %esi, %eax
  subl %ecx, %eax
  movl %eax, %ebx
  movl %ebx, %esi
  jmp .L1
.L3:
  subq $16, %rsp
  movl %edi, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call printInt
  call __latte_str_1
  movq %rax, %rcx
  movl $60, %esi
  subq $16, %rsp
  movq %rcx, 0(%rsp)
  movl %esi, 8(%rsp)
  movq 0(%rsp), %rdi
  movl 8(%rsp), %esi
  addq $16, %rsp
  call repStr
  movq %rax, %rdi
  subq $16, %rsp
  movq %rdi, 0(%rsp)
  movq 0(%rsp), %rdi
  addq $16, %rsp
  call printString
  call __latte_str_2
  movq %rax, %rcx
  subq $16, %rsp
  movq %rcx, 0(%rsp)
  movq 0(%rsp), %rdi
  addq $16, %rsp
  call printString
  call __latte_str_3
  movq %rax, %rcx
  subq $16, %rsp
  movq %rcx, 0(%rsp)
  movq 0(%rsp), %rdi
  addq $16, %rsp
  call printString
  movl $0, %ecx
  movl %ecx, %eax
  addq $8, %rsp
  popq %rbx
  popq %rbp
  ret
.globl fac
fac:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rbx
  subq $8, %rsp
  subq $16, %rsp
  movl %edi, 0(%rsp)
  movl 0(%rsp), %ecx
  addq $16, %rsp
.L4:
  movl $0, %esi
  movl $0, %edi
  movl $1, %ebx
  movl %ebx, %esi
  movl %ecx, %edi
  jmp .L5
.L5:
  movl $0, %ecx
  movl %edi, %eax
  cmpl %ecx, %eax
  setg %al
  movzbl %al, %eax
  movl %eax, %ebx
  testl %ebx, %ebx
  je .L7
  jmp .L6
.L6:
  movl %esi, %eax
  imull %edi, %eax
  movl %eax, %ecx
  movl %ecx, %esi
  movl $1, %ecx
  movl %edi, %eax
  subl %ecx, %eax
  movl %eax, %ebx
  movl %ebx, %edi
  jmp .L5
.L7:
  movl %esi, %eax
  addq $8, %rsp
  popq %rbx
  popq %rbp
  ret
.globl rfac
rfac:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rbx
  subq $8, %rsp
  subq $16, %rsp
  movl %edi, 0(%rsp)
  movl 0(%rsp), %ebx
  addq $16, %rsp
.L8:
  movl $0, %ecx
  movl %ebx, %eax
  cmpl %ecx, %eax
  sete %al
  movzbl %al, %eax
  movl %eax, %esi
  testl %esi, %esi
  je .L10
  jmp .L9
.L9:
  movl $1, %ecx
  movl %ecx, %eax
  addq $8, %rsp
  popq %rbx
  popq %rbp
  ret
.L10:
  movl $1, %ecx
  movl %ebx, %eax
  subl %ecx, %eax
  movl %eax, %esi
  subq $16, %rsp
  movl %esi, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call rfac
  movl %eax, %ecx
  movl %ebx, %eax
  imull %ecx, %eax
  movl %eax, %esi
  movl %esi, %eax
  addq $8, %rsp
  popq %rbx
  popq %rbp
  ret
.L11:
.globl mfac
mfac:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rbx
  subq $8, %rsp
  subq $16, %rsp
  movl %edi, 0(%rsp)
  movl 0(%rsp), %ebx
  addq $16, %rsp
.L12:
  movl $0, %ecx
  movl %ebx, %eax
  cmpl %ecx, %eax
  sete %al
  movzbl %al, %eax
  movl %eax, %esi
  testl %esi, %esi
  je .L14
  jmp .L13
.L13:
  movl $1, %ecx
  movl %ecx, %eax
  addq $8, %rsp
  popq %rbx
  popq %rbp
  ret
.L14:
  movl $1, %ecx
  movl %ebx, %eax
  subl %ecx, %eax
  movl %eax, %esi
  subq $16, %rsp
  movl %esi, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call nfac
  movl %eax, %ecx
  movl %ebx, %eax
  imull %ecx, %eax
  movl %eax, %esi
  movl %esi, %eax
  addq $8, %rsp
  popq %rbx
  popq %rbp
  ret
.L15:
.globl nfac
nfac:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rbx
  subq $8, %rsp
  subq $16, %rsp
  movl %edi, 0(%rsp)
  movl 0(%rsp), %ebx
  addq $16, %rsp
.L16:
  movl $0, %ecx
  movl %ebx, %eax
  cmpl %ecx, %eax
  setne %al
  movzbl %al, %eax
  movl %eax, %esi
  testl %esi, %esi
  je .L18
  jmp .L17
.L17:
  movl $1, %ecx
  movl %ebx, %eax
  subl %ecx, %eax
  movl %eax, %esi
  subq $16, %rsp
  movl %esi, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call mfac
  movl %eax, %ecx
  movl %ecx, %eax
  imull %ebx, %eax
  movl %eax, %esi
  movl %esi, %eax
  addq $8, %rsp
  popq %rbx
  popq %rbp
  ret
.L18:
  movl $1, %ecx
  movl %ecx, %eax
  addq $8, %rsp
  popq %rbx
  popq %rbp
  ret
.L19:
.globl ifac
ifac:
  pushq %rbp
  movq %rsp, %rbp
  subq $16, %rsp
  movl %edi, 0(%rsp)
  movl 0(%rsp), %ecx
  addq $16, %rsp
.L20:
  movl $1, %esi
  subq $16, %rsp
  movl %esi, 0(%rsp)
  movl %ecx, 8(%rsp)
  movl 0(%rsp), %edi
  movl 8(%rsp), %esi
  addq $16, %rsp
  call ifac2f
  movl %eax, %edi
  movl %edi, %eax
  popq %rbp
  ret
.globl ifac2f
ifac2f:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rbx
  subq $24, %rsp
  subq $16, %rsp
  movl %edi, 0(%rsp)
  movl %esi, 8(%rsp)
  movl 0(%rsp), %ecx
  movl 8(%rsp), %r11d
  movl %r11d, -16(%rbp)
  addq $16, %rsp
.L21:
  movl %ecx, %eax
  cmpl -16(%rbp), %eax
  sete %al
  movzbl %al, %eax
  movl %eax, %esi
  testl %esi, %esi
  je .L23
  jmp .L22
.L22:
  movl %ecx, %eax
  addq $24, %rsp
  popq %rbx
  popq %rbp
  ret
.L23:
  movl %ecx, %eax
  cmpl -16(%rbp), %eax
  setg %al
  movzbl %al, %eax
  movl %eax, %esi
  testl %esi, %esi
  je .L25
  jmp .L24
.L24:
  movl $1, %esi
  movl %esi, %eax
  addq $24, %rsp
  popq %rbx
  popq %rbp
  ret
.L25:
  movl $0, %eax
  movl %eax, -24(%rbp)
  movl %ecx, %eax
  addl -16(%rbp), %eax
  movl %eax, %esi
  movl $2, %edi
  movl %esi, %eax
  movl %esi, %eax
  cdq
  idivl %edi
  movl %eax, %ebx
  movl %eax, %ebx
  movl %ebx, -24(%rbp)
  subq $16, %rsp
  movl %ecx, 0(%rsp)
  movl -24(%rbp), %r11d
  movl %r11d, 8(%rsp)
  movl 0(%rsp), %edi
  movl 8(%rsp), %esi
  addq $16, %rsp
  call ifac2f
  movl %eax, %ebx
  movl $1, %ecx
  movl -24(%rbp), %eax
  addl %ecx, %eax
  movl %eax, %esi
  subq $16, %rsp
  movl %esi, 0(%rsp)
  movl -16(%rbp), %r11d
  movl %r11d, 8(%rsp)
  movl 0(%rsp), %edi
  movl 8(%rsp), %esi
  addq $16, %rsp
  call ifac2f
  movl %eax, %ecx
  movl %ebx, %eax
  imull %ecx, %eax
  movl %eax, %esi
  movl %esi, %eax
  addq $24, %rsp
  popq %rbx
  popq %rbp
  ret
.globl repStr
repStr:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rbx
  subq $24, %rsp
  subq $16, %rsp
  movq %rdi, 0(%rsp)
  movl %esi, 8(%rsp)
  movq 0(%rsp), %rbx
  movl 8(%rsp), %r11d
  movl %r11d, -16(%rbp)
  addq $16, %rsp
.L26:
  call __latte_str_0
  movq %rax, %rcx
  movq %rcx, %rsi
  movl $0, %ecx
  movl %ecx, -24(%rbp)
  jmp .L27
.L27:
  movl -24(%rbp), %eax
  cmpl -16(%rbp), %eax
  setl %al
  movzbl %al, %eax
  movl %eax, %ecx
  testl %ecx, %ecx
  je .L29
  jmp .L28
.L28:
  subq $16, %rsp
  movq %rsi, 0(%rsp)
  movq %rbx, 8(%rsp)
  movq 0(%rsp), %rdi
  movq 8(%rsp), %rsi
  addq $16, %rsp
  call __latte_concat
  movq %rax, %rcx
  movq %rcx, %rsi
  movl $1, %ecx
  movl -24(%rbp), %eax
  addl %ecx, %eax
  movl %eax, %edi
  movl %edi, -24(%rbp)
  jmp .L27
.L29:
  movq %rsi, %rax
  addq $24, %rsp
  popq %rbx
  popq %rbp
  ret
