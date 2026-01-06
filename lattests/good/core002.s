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
  .asciz "foo"
.text
__latte_str_0:
  movl $.LC0, %eax
  ret
__latte_str_1:
  movl $.LC1, %eax
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
.L0:
  call foo
  movl $0, %eax
  movl $0, %eax
  popl %ebp
  ret
.globl foo
foo:
  pushl %ebp
  movl %esp, %ebp
.L1:
  call __latte_str_1
  pushl %eax
  call printString
  addl $4, %esp
  movl $0, %eax
  popl %ebp
  ret
