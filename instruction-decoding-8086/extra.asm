bits 16
mov cx, bx
mov bx, cx
mov al, bh
mov dh, al
mov bx, 0xf0ff
mov bx, [0xf0ff]
mov bl, [bx+si]
mov bl, [bx+0x1]
mov bx, [bp]
mov bx, [bx+0x7f]
mov bx, [bx+si+0x7f]
mov [bx+si+0x7f], bx
mov [bx], ax
