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
  movl $0, %eax
  movl $0, %ebx
  movl $0, %esi
  movl $0, %edi
  movl $0, %eax
  movl %eax, -16(%ebp)
  movl $0, %eax
  movl %eax, -20(%ebp)
  movl $10, %ecx
  movl %ecx, %eax
  movl $20, %ecx
  movl %ecx, %ebx
  movl $30, %ecx
  movl %ecx, %esi
  movl $40, %ecx
  movl %ecx, %edi
  movl $50, %ecx
  movl %ecx, -16(%ebp)
  movl $60, %ecx
  movl %ecx, -20(%ebp)
  pushl %eax
  call printInt
  addl $4, %esp
  movl $0, %eax
  addl %esi, %ebx
  movl %ebx, %eax
  addl %edi, %eax
  movl %eax, %ecx
  pushl %edx
  movl -16(%ebp), %edx
  addl %edx, %ecx
  movl %ecx, %eax
  popl %edx
  pushl %edx
  movl -20(%ebp), %edx
  addl %edx, %eax
  movl %eax, %ecx
  popl %edx
  pushl %ecx
  call printInt
  addl $4, %esp
  movl $0, %eax
  movl $0, %eax
  addl $8, %esp
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret