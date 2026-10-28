.section .text

.global context_switch
.type context_switch, @function

context_switch:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    movl 20(%esp), %eax
    movl %esp, (%eax)

    movl 24(%esp), %esp

    popl %edi
    popl %esi
    popl %ebx
    popl %ebp

    ret

.section .note.GNU-stack,"",@progbits