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
.text
__latte_str_0:
  movl $.LC0, %eax
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
  pushl %edi
  subl $8, %esp
.L0:
  movl $0, %ecx
  movl $0, %ebx
  movl $0, %esi
  movl $0, %edi
  movl $0, %eax
  movl %eax, -16(%ebp)
  movl $0, %eax
  movl %eax, -20(%ebp)
  movl $10, %edx
  movl %edx, %ecx
  movl $20, %edx
  movl %edx, %ebx
  movl $30, %edx
  movl %edx, %esi
  movl $40, %edx
  movl %edx, %edi
  movl $50, %edx
  movl %edx, -16(%ebp)
  movl $60, %edx
  movl %edx, -20(%ebp)
  pushl %ecx
  call printInt
  addl $4, %esp
  movl %ebx, %eax
  addl %esi, %eax
  movl %eax, %ecx
  movl %ecx, %eax
  addl %edi, %eax
  movl %eax, %edx
  movl %edx, %eax
  addl -16(%ebp), %eax
  movl %eax, %ecx
  movl %ecx, %eax
  addl -20(%ebp), %eax
  movl %eax, %edx
  pushl %edx
  call printInt
  addl $4, %esp
  movl $0, %ecx
  movl %ecx, %eax
  addl $8, %esp
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret
