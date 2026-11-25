
_start:
    # Imprime 111 antes de dormir
    mov r1, 111
    mov r0, 3
    syscall

    # Quebra de linha
    mov r0, 2
    syscall

    # Dorme por aproximadamente 3 segundos
    mov r1, 3
    mov r0, 6
    syscall

    # Executa somente depois de acordar
    mov r1, 222
    mov r0, 3
    syscall

    # Quebra de linha
    mov r0, 2
    syscall

    # Encerra o processo
    mov r0, 0
    syscall
