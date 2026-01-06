.text
.extern printInt
.extern printString
.extern error
.extern readInt
.extern readString
.extern strlen
.extern malloc
.extern memcpy
.extern strcmp
.extern __divsi3
.extern __modsi3
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
__latte_str_0:
  movl $.LC0, %eax
  ret
__latte_str_1:
  movl $.LC1, %eax
  ret
__latte_str_2:
  movl $.LC2, %eax
  ret
__latte_str_3:
  movl $.LC3, %eax
  ret
__latte_concat:
  pushl %ebp
  movl %esp, %ebp
  pushl %ebx
  pushl %esi
  pushl %edi
  movl 8(%ebp), %esi
  movl 12(%ebp), %edi
  pushl %esi
  call strlen
  addl $4, %esp
  movl %eax, %ebx
  pushl %edi
  call strlen
  addl $4, %esp
  movl %eax, %ecx
  leal 1(%ebx,%ecx), %eax
  pushl %eax
  call malloc
  addl $4, %esp
  movl %eax, %edx
  pushl %ebx
  pushl %esi
  pushl %edx
  call memcpy
  addl $12, %esp
  leal (%edx,%ebx), %eax
  leal 1(%ecx), %ecx
  pushl %ecx
  pushl %edi
  pushl %eax
  call memcpy
  addl $12, %esp
  movl %edx, %eax
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret
.globl main
main:
  pushl %ebp
  movl %esp, %ebp
  pushl %ebx
  pushl %esi
.L0:
  movl $10, %ecx
  pushl %ecx
  call fac
  addl $4, %esp
  movl %eax, %edx
  pushl %edx
  call printInt
  addl $4, %esp
  movl $10, %ecx
  pushl %ecx
  call rfac
  addl $4, %esp
  movl %eax, %edx
  pushl %edx
  call printInt
  addl $4, %esp
  movl $10, %ecx
  pushl %ecx
  call mfac
  addl $4, %esp
  movl %eax, %edx
  pushl %edx
  call printInt
  addl $4, %esp
  movl $10, %ecx
  pushl %ecx
  call ifac
  addl $4, %esp
  movl %eax, %edx
  pushl %edx
  call printInt
  addl $4, %esp
  call __latte_str_0
  movl %eax, %ecx
  movl $10, %ecx
  movl %ecx, %edx
  movl $1, %ecx
  movl %ecx, %ebx
  jmp .L1
.L1:
  movl $0, %ecx
  movl %edx, %eax
  cmpl %ecx, %eax
  setg %al
  movzbl %al, %eax
  movl %eax, %esi
  testl %esi, %esi
  je .L3
  jmp .L2
.L2:
  movl %ebx, %eax
  imull %edx, %eax
  movl %eax, %ecx
  movl %ecx, %ebx
  movl $1, %ecx
  movl %edx, %eax
  subl %ecx, %eax
  movl %eax, %esi
  movl %esi, %edx
  jmp .L1
.L3:
  pushl %ebx
  call printInt
  addl $4, %esp
  call __latte_str_1
  movl %eax, %ecx
  movl $60, %edx
  pushl %edx
  pushl %ecx
  call repStr
  addl $8, %esp
  movl %eax, %ebx
  pushl %ebx
  call printString
  addl $4, %esp
  call __latte_str_2
  movl %eax, %ecx
  pushl %ecx
  call printString
  addl $4, %esp
  call __latte_str_3
  movl %eax, %ecx
  pushl %ecx
  call printString
  addl $4, %esp
  movl $0, %ecx
  movl %ecx, %eax
  popl %esi
  popl %ebx
  popl %ebp
  ret
.globl fac
fac:
  pushl %ebp
  movl %esp, %ebp
  pushl %ebx
  pushl %esi
  movl 8(%ebp), %ebx
.L4:
  movl $0, %ecx
  movl $0, %edx
  movl $1, %ebx
  movl %ebx, %ecx
  movl %ebx, %edx
  jmp .L5
.L5:
  movl $0, %ebx
  movl %edx, %eax
  cmpl %ebx, %eax
  setg %al
  movzbl %al, %eax
  movl %eax, %esi
  testl %esi, %esi
  je .L7
  jmp .L6
.L6:
  movl %ecx, %eax
  imull %edx, %eax
  movl %eax, %ebx
  movl %ebx, %ecx
  movl $1, %ebx
  movl %edx, %eax
  subl %ebx, %eax
  movl %eax, %esi
  movl %esi, %edx
  jmp .L5
.L7:
  movl %ecx, %eax
  popl %esi
  popl %ebx
  popl %ebp
  ret
.globl rfac
rfac:
  pushl %ebp
  movl %esp, %ebp
  pushl %ebx
  movl 8(%ebp), %ebx
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
  popl %ebx
  popl %ebp
  ret
.L10:
  movl $1, %ecx
  movl %ebx, %eax
  subl %ecx, %eax
  movl %eax, %edx
  pushl %edx
  call rfac
  addl $4, %esp
  movl %eax, %ecx
  movl %ebx, %eax
  imull %ecx, %eax
  movl %eax, %edx
  movl %edx, %eax
  popl %ebx
  popl %ebp
  ret
.L11:
.globl mfac
mfac:
  pushl %ebp
  movl %esp, %ebp
  pushl %ebx
  movl 8(%ebp), %ebx
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
  popl %ebx
  popl %ebp
  ret
.L14:
  movl $1, %ecx
  movl %ebx, %eax
  subl %ecx, %eax
  movl %eax, %edx
  pushl %edx
  call nfac
  addl $4, %esp
  movl %eax, %ecx
  movl %ebx, %eax
  imull %ecx, %eax
  movl %eax, %edx
  movl %edx, %eax
  popl %ebx
  popl %ebp
  ret
.L15:
.globl nfac
nfac:
  pushl %ebp
  movl %esp, %ebp
  pushl %ebx
  movl 8(%ebp), %ebx
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
  pushl %edx
  call mfac
  addl $4, %esp
  movl %eax, %ecx
  movl %ecx, %eax
  imull %ebx, %eax
  movl %eax, %edx
  movl %edx, %eax
  popl %ebx
  popl %ebp
  ret
.L18:
  movl $1, %ecx
  movl %ecx, %eax
  popl %ebx
  popl %ebp
  ret
.L19:
.globl ifac
ifac:
  pushl %ebp
  movl %esp, %ebp
  pushl %ebx
  movl 8(%ebp), %edx
.L20:
  movl $1, %ecx
  pushl %edx
  pushl %ecx
  call ifac2f
  addl $8, %esp
  movl %eax, %ebx
  movl %ebx, %eax
  popl %ebx
  popl %ebp
  ret
.globl ifac2f
ifac2f:
  pushl %ebp
  movl %esp, %ebp
  pushl %ebx
  pushl %esi
  pushl %edi
  subl $4, %esp
  movl 8(%ebp), %ebx
  movl 12(%ebp), %eax
  movl %eax, -16(%ebp)
.L21:
  movl %ebx, %eax
  cmpl -16(%ebp), %eax
  sete %al
  movzbl %al, %eax
  movl %eax, %ecx
  testl %ecx, %ecx
  je .L23
  jmp .L22
.L22:
  movl %ebx, %eax
  addl $4, %esp
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret
.L23:
  movl %ebx, %eax
  cmpl -16(%ebp), %eax
  setg %al
  movzbl %al, %eax
  movl %eax, %ecx
  testl %ecx, %ecx
  je .L25
  jmp .L24
.L24:
  movl $1, %ecx
  movl %ecx, %eax
  addl $4, %esp
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret
.L25:
  movl $0, %edi
  movl %ebx, %eax
  addl -16(%ebp), %eax
  movl %eax, %ecx
  movl $2, %edx
  pushl %edx
  pushl %ecx
  call __divsi3
  addl $8, %esp
  movl %eax, %esi
  movl %esi, %edi
  pushl %edi
  pushl %ebx
  call ifac2f
  addl $8, %esp
  movl %eax, %esi
  movl $1, %ecx
  movl %edi, %eax
  addl %ecx, %eax
  movl %eax, %edx
  pushl %edx
  movl -16(%ebp), %edx
  pushl %edx
  popl %edx
  pushl %edx
  call ifac2f
  addl $8, %esp
  movl %eax, %ecx
  movl %esi, %eax
  imull %ecx, %eax
  movl %eax, %edx
  movl %edx, %eax
  addl $4, %esp
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret
.globl repStr
repStr:
  pushl %ebp
  movl %esp, %ebp
  pushl %ebx
  pushl %esi
  pushl %edi
  subl $4, %esp
  movl 8(%ebp), %edi
  movl 12(%ebp), %esi
.L26:
  call __latte_str_0
  movl %eax, %ecx
  movl %ecx, -16(%ebp)
  movl $0, %ecx
  movl %ecx, %ebx
  jmp .L27
.L27:
  movl %ebx, %eax
  cmpl %esi, %eax
  setl %al
  movzbl %al, %eax
  movl %eax, %ecx
  testl %ecx, %ecx
  je .L29
  jmp .L28
.L28:
  pushl %edi
  pushl %edx
  movl -16(%ebp), %edx
  pushl %edx
  popl %edx
  call __latte_concat
  addl $8, %esp
  movl %eax, %ecx
  movl %ecx, -16(%ebp)
  movl $1, %ecx
  movl %ebx, %eax
  addl %ecx, %eax
  movl %eax, %edx
  movl %edx, %ebx
  jmp .L27
.L29:
  movl -16(%ebp), %eax
  addl $4, %esp
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret
