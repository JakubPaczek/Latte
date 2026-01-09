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
.L0:
  movl $10, %ecx
  subq $16, %rsp
  movl %ecx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call fac
  movl %eax, %edx
  subq $16, %rsp
  movl %edx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call printInt
  movl $10, %ecx
  subq $16, %rsp
  movl %ecx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call rfac
  movl %eax, %edx
  subq $16, %rsp
  movl %edx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call printInt
  movl $10, %ecx
  subq $16, %rsp
  movl %ecx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call mfac
  movl %eax, %edx
  subq $16, %rsp
  movl %edx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call printInt
  movl $10, %ecx
  subq $16, %rsp
  movl %ecx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call ifac
  movl %eax, %edx
  subq $16, %rsp
  movl %edx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call printInt
  call __latte_str_0
  movl %eax, %ecx
  movl $10, %ecx
  movl %ecx, %edx
  movl $1, %ecx
  movl %ecx, %esi
  jmp .L1
.L1:
  movl $0, %ecx
  movl %edx, %eax
  cmpl %ecx, %eax
  setg %al
  movzbl %al, %eax
  movl %eax, %edi
  testl %edi, %edi
  je .L3
  jmp .L2
.L2:
  movl %esi, %eax
  imull %edx, %eax
  movl %eax, %ecx
  movl %ecx, %esi
  movl $1, %ecx
  movl %edx, %eax
  subl %ecx, %eax
  movl %eax, %edi
  movl %edi, %edx
  jmp .L1
.L3:
  subq $16, %rsp
  movl %esi, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call printInt
  call __latte_str_1
  movl %eax, %ecx
  movl $60, %edx
  subq $16, %rsp
  movl %ecx, 0(%rsp)
  movl %edx, 8(%rsp)
  movl 0(%rsp), %edi
  movl 8(%rsp), %esi
  addq $16, %rsp
  call repStr
  movl %eax, %esi
  subq $16, %rsp
  movl %esi, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call printString
  call __latte_str_2
  movl %eax, %ecx
  subq $16, %rsp
  movl %ecx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call printString
  call __latte_str_3
  movl %eax, %ecx
  subq $16, %rsp
  movl %ecx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call printString
  movl $0, %ecx
  movl %ecx, %eax
  popq %rbp
  ret
.globl fac
fac:
  pushq %rbp
  movq %rsp, %rbp
  movl %edi, %esi
.L4:
  movl $0, %ecx
  movl $0, %edx
  movl $1, %esi
  movl %esi, %ecx
  movl %esi, %edx
  jmp .L5
.L5:
  movl $0, %esi
  movl %edx, %eax
  cmpl %esi, %eax
  setg %al
  movzbl %al, %eax
  movl %eax, %edi
  testl %edi, %edi
  je .L7
  jmp .L6
.L6:
  movl %ecx, %eax
  imull %edx, %eax
  movl %eax, %esi
  movl %esi, %ecx
  movl $1, %esi
  movl %edx, %eax
  subl %esi, %eax
  movl %eax, %edi
  movl %edi, %edx
  jmp .L5
.L7:
  movl %ecx, %eax
  popq %rbp
  ret
.globl rfac
rfac:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rbx
  subq $8, %rsp
  movl %edi, %ebx
.L8:
  movl $0, %ecx
  movl %ebx, %eax
  cmpl %ecx, %eax
  sete %al
  movzbl %al, %eax
  movl %eax, %edx
  testl %edx, %edx
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
  movl %eax, %edx
  subq $16, %rsp
  movl %edx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call rfac
  movl %eax, %ecx
  movl %ebx, %eax
  imull %ecx, %eax
  movl %eax, %edx
  movl %edx, %eax
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
  movl %edi, %ebx
.L12:
  movl $0, %ecx
  movl %ebx, %eax
  cmpl %ecx, %eax
  sete %al
  movzbl %al, %eax
  movl %eax, %edx
  testl %edx, %edx
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
  movl %eax, %edx
  subq $16, %rsp
  movl %edx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call nfac
  movl %eax, %ecx
  movl %ebx, %eax
  imull %ecx, %eax
  movl %eax, %edx
  movl %edx, %eax
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
  movl %edi, %ebx
.L16:
  movl $0, %ecx
  movl %ebx, %eax
  cmpl %ecx, %eax
  setne %al
  movzbl %al, %eax
  movl %eax, %edx
  testl %edx, %edx
  je .L18
  jmp .L17
.L17:
  movl $1, %ecx
  movl %ebx, %eax
  subl %ecx, %eax
  movl %eax, %edx
  subq $16, %rsp
  movl %edx, 0(%rsp)
  movl 0(%rsp), %edi
  addq $16, %rsp
  call mfac
  movl %eax, %ecx
  movl %ecx, %eax
  imull %ebx, %eax
  movl %eax, %edx
  movl %edx, %eax
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
  movl %edi, %edx
.L20:
  movl $1, %ecx
  subq $16, %rsp
  movl %ecx, 0(%rsp)
  movl %edx, 8(%rsp)
  movl 0(%rsp), %edi
  movl 8(%rsp), %esi
  addq $16, %rsp
  call ifac2f
  movl %eax, %esi
  movl %esi, %eax
  popq %rbp
  ret
.globl ifac2f
ifac2f:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rbx
  subq $24, %rsp
  movl %esi, -16(%rbp)
  movl %edi, %ecx
.L21:
  movl %ecx, %eax
  cmpl -16(%rbp), %eax
  sete %al
  movzbl %al, %eax
  movl %eax, %edx
  testl %edx, %edx
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
  movl %eax, %edx
  testl %edx, %edx
  je .L25
  jmp .L24
.L24:
  movl $1, %edx
  movl %edx, %eax
  addq $24, %rsp
  popq %rbx
  popq %rbp
  ret
.L25:
  movl $0, %ebx
  movl %ecx, %eax
  addl -16(%rbp), %eax
  movl %eax, %edx
  movl $2, %esi
  movl %edx, %eax
  movl %edx, %eax
  cdq
  idivl %esi
  movl %eax, %edi
  movl %eax, %edi
  movl %edi, %ebx
  subq $16, %rsp
  movl %ecx, 0(%rsp)
  movl %ebx, 8(%rsp)
  movl 0(%rsp), %edi
  movl 8(%rsp), %esi
  addq $16, %rsp
  call ifac2f
  movl %eax, -24(%rbp)
  movl $1, %ecx
  movl %ebx, %eax
  addl %ecx, %eax
  movl %eax, %edx
  subq $16, %rsp
  movl %edx, 0(%rsp)
  movl -16(%rbp), %r11d
  movl %r11d, 8(%rsp)
  movl 0(%rsp), %edi
  movl 8(%rsp), %esi
  addq $16, %rsp
  call ifac2f
  movl %eax, %ecx
  movl -24(%rbp), %eax
  imull %ecx, %eax
  movl %eax, %edx
  movl %edx, %eax
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
  movl %esi, -16(%rbp)
  movl %edi, -24(%rbp)
.L26:
  call __latte_str_0
  movl %eax, %ecx
  movl %ecx, %edx
  movl $0, %ecx
  movl %ecx, %ebx
  jmp .L27
.L27:
  movl %ebx, %eax
  cmpl -16(%rbp), %eax
  setl %al
  movzbl %al, %eax
  movl %eax, %ecx
  testl %ecx, %ecx
  je .L29
  jmp .L28
.L28:
  subq $16, %rsp
  movl %edx, 0(%rsp)
  movl -24(%rbp), %r11d
  movl %r11d, 8(%rsp)
  movl 0(%rsp), %edi
  movl 8(%rsp), %esi
  addq $16, %rsp
  call __latte_concat
  movl %eax, %ecx
  movl %ecx, %edx
  movl $1, %ecx
  movl %ebx, %eax
  addl %ecx, %eax
  movl %eax, %esi
  movl %esi, %ebx
  jmp .L27
.L29:
  movl %edx, %eax
  addq $24, %rsp
  popq %rbx
  popq %rbp
  ret
