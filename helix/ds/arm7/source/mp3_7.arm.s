.text                                                 
.arm                                                  

@ desinterleave an mp3 stereo source                      
@ r0 = interleaved data, r1 = left, r2 = right, r3 = len  

    .global AS_StereoDesinterleave                        

AS_StereoDesinterleave: 
    stmfd sp!, {r4-r11} 
                        
_loop:                  

    ldmia r0!, {r4-r11} 

    strh r4, [r1], #2   
    subs r3, #1         
    beq _done           
                        
    strh r5, [r1], #2   
    subs r3, #1         
    beq _done           
                        
    strh r6, [r1], #2   
    subs r3, #1         
    beq _done           
                        
    strh r7, [r1], #2   
    subs r3, #1         
    beq _done           
                        
    strh r8, [r1], #2   
    subs r3, #1         
    beq _done           
                        
    strh r9, [r1], #2   
    subs r3, #1         
    beq _done           
                        
    strh r10, [r1], #2  
    subs r3, #1         
    beq _done           
                        
    strh r11, [r1], #2  
    subs r3, #1         
    bne _loop           
_done:                  

    ldmia sp!, {r4-r11} 
    bx lr               
.end