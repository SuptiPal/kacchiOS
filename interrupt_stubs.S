.section .text

.extern scheduler_interrupt

.global irq0_stub
.global yield_stub
.global exception_stub

/* Hardware timer interrupt */
irq0_stub:
    pushal
    movl %esp, %eax

    pushl $1
    pushl %eax
    call scheduler_interrupt
    addl $8, %esp

    movl %eax, %esp
    popal
    iret

/* Voluntary yield through software interrupt */
yield_stub:
    pushal
    movl %esp, %eax

    pushl $0
    pushl %eax
    call scheduler_interrupt
    addl $8, %esp

    movl %eax, %esp
    popal
    iret

/* Stop safely on an unexpected CPU exception */
exception_stub:
    cli
1:
    hlt
    jmp 1b

.section .note.GNU-stack,"",@progbits