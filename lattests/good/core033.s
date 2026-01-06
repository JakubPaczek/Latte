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
  subl $32, %esp
.L0:
  movl $0, %eax
  movl $0, %ecx
  movl $0, %edx
  movl $0, %ebx
  movl $0, %esi
  movl $0, %eax
  movl %eax, -40(%ebp)
  movl $0, %eax
  movl %eax, -16(%ebp)
  movl $0, %eax
  movl %eax, -20(%ebp)
  movl $0, %eax
  movl %eax, -24(%ebp)
  movl $0, %eax
  movl %eax, -28(%ebp)
  movl $0, %eax
  movl %eax, -32(%ebp)
  movl $0, %eax
  movl %eax, -36(%ebp)
  movl $1, %edi
  movl %edi, %eax
  movl $2, %edi
  movl %edi, %ecx
  movl $3, %edi
  movl %edi, %edx
  movl $4, %edi
  movl %edi, %ebx
  movl $5, %edi
  movl %edi, %esi
  movl $6, %edi
  movl %edi, -40(%ebp)
  movl $7, %edi
  movl %edi, -16(%ebp)
  movl $8, %edi
  movl %edi, -20(%ebp)
  movl $9, %edi
  movl %edi, -24(%ebp)
  movl $10, %edi
  movl %edi, -28(%ebp)
  movl $11, %edi
  movl %edi, -32(%ebp)
  movl $12, %edi
  movl %edi, -36(%ebp)
  movl $0, %eax
  movl %eax, -44(%ebp)
  addl %ecx, %eax
  movl %eax, %edi
  addl %edx, %edi
  movl %edi, %eax
  addl %ebx, %eax
  movl %eax, %ecx
  addl %esi, %ecx
  movl %ecx, %eax
  pushl %edx
  movl -40(%ebp), %edx
  addl %edx, %eax
  movl %eax, %ecx
  popl %edx
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
  pushl %edx
  movl -24(%ebp), %edx
  addl %edx, %ecx
  movl %ecx, %eax
  popl %edx
  pushl %edx
  movl -28(%ebp), %edx
  addl %edx, %eax
  movl %eax, %ecx
  popl %edx
  pushl %edx
  movl -32(%ebp), %edx
  addl %edx, %ecx
  movl %ecx, %eax
  popl %edx
  pushl %edx
  movl -36(%ebp), %edx
  addl %edx, %eax
  movl %eax, %ecx
  popl %edx
  movl %ecx, -44(%ebp)
  pushl %edx
  movl -44(%ebp), %edx
  pushl %edx
  popl %edx
  call printInt
  addl $4, %esp
  movl $0, %eax
  movl $0, %eax
  addl $32, %esp
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret
