bits 16

mov cx, bx
mov al, bh
mov bx, 0xF0FF
mov bx, [bx + 1] ; TODO(srvariable) decode this one, right now it produces mov di, bx
