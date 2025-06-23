@ -----------------------------------------------------------------------------
@ File:    postprocess_detect_arm.s
@ Author:  TrackieLLM Engineering Team
@ Purpose: High-performance Non-Maximum Suppression (NMS) for object detection.
@
@ Architecture: ARMv7-A (AArch32) with NEON extension.
@ Toolchain:    GNU Assembler (as)
@
@ ASSUMPTION: The input array of BoundingBox structs has been pre-sorted
@             by score in descending order.
@ -----------------------------------------------------------------------------

.section .text
.align 4
.global nms_greedy_neon
.type nms_greedy_neon, %function

@ -----------------------------------------------------------------------------
@ C-prototype:
@ void nms_greedy_neon(
@     BoundingBox* boxes,          // r0: Pointer to array of BBox structs
@     int          num_boxes,      // r1: Number of boxes in the array
@     float        iou_threshold   // s0: IoU threshold (passed in NEON reg)
@ );
@
@ C-struct layout (IMPORTANT):
@ struct BoundingBox {
@     float x1;        // offset 0
@     float y1;        // offset 4
@     float x2;        // offset 8
@     float y2;        // offset 12
@     float score;     // offset 16
@     int   suppressed; // offset 20 (using int for 4-byte alignment)
@ }; // sizeof(BoundingBox) = 24 bytes
@
@ Register Allocation:
@   r0: Base address of the boxes array.
@   r1: num_boxes.
@   r2: Outer loop counter (i).
@   r3: Inner loop counter (j).
@   r4: Pointer to current box_i.
@   r5: Pointer to current box_j.
@   r6: num_boxes - 1 for loop boundary.
@   r7: Temporary for struct member access.
@   r8: Size of BoundingBox struct (24).
@
@ NEON Register Allocation:
@   s0: iou_threshold (from argument).
@   q1 (d2, d3): Coords of box_i (x1, y1, x2, y2).
@   q2 (d4, d5): Coords of box_j (x1, y1, x2, y2).
@   q3 (d6, d7): Intersection coords (ix1, iy1, ix2, iy2).
@   q4 (d8, d9): Intersection width/height.
@   s10: Intersection area.
@   s11: Area of box_i.
@   s12: Area of box_j.
@   s13: Union area.
@   s14: Calculated IoU.
@   s15: Constant 0.0f.
@ -----------------------------------------------------------------------------

nms_greedy_neon:
    @ --- Function Prologue ---
    push    {r4-r8, lr}
    vmov.f32 s15, #0.0          @ s15 = 0.0f, used for max(0, ...)

    @ --- Initial Setup ---
    cmp     r1, #1
    ble     .L_exit_nms         @ If 0 or 1 box, nothing to do.
    mov     r2, #0              @ r2 = i = 0
    sub     r6, r1, #1          @ r6 = num_boxes - 1
    mov     r8, #24             @ r8 = sizeof(BoundingBox)

.L_outer_loop_i:
    cmp     r2, r6              @ while (i < num_boxes - 1)
    bge     .L_exit_nms

    @ Get address of box_i
    mul     r4, r2, r8
    add     r4, r0, r4          @ r4 = &boxes[i]

    @ Check if box_i is already suppressed
    ldr     r7, [r4, #20]       @ r7 = boxes[i].suppressed
    cmp     r7, #0
    bne     .L_continue_outer   @ if (suppressed) continue;

    @ Load coordinates of box_i into a NEON register
    vld1.32 {d2, d3}, [r4]      @ q1 = {x1, y1, x2, y2} of box_i

    @ Calculate area of box_i
    vsub.f32 s5, d2[1], d2[0]   @ s5 = x2 - x1 (width)
    vsub.f32 s6, d3[1], d3[0]   @ s6 = y2 - y1 (height)
    vmul.f32 s11, s5, s6        @ s11 = area_i

    @ Setup inner loop
    add     r3, r2, #1          @ r3 = j = i + 1

.L_inner_loop_j:
    cmp     r3, r1              @ while (j < num_boxes)
    bge     .L_continue_outer

    @ Get address of box_j
    mul     r5, r3, r8
    add     r5, r0, r5          @ r5 = &boxes[j]

    @ Check if box_j is already suppressed
    ldr     r7, [r5, #20]
    cmp     r7, #0
    bne     .L_continue_inner   @ if (suppressed) continue;

    @ --- IoU Calculation using NEON ---
    @ Load coordinates of box_j
    vld1.32 {d4, d5}, [r5]      @ q2 = {x1, y1, x2, y2} of box_j

    @ Calculate intersection coordinates
    @ ix1 = max(box_i.x1, box_j.x1), iy1 = max(box_i.y1, box_j.y1)
    @ ix2 = min(box_i.x2, box_j.x2), iy2 = min(box_i.y2, box_j.y2)
    vmax.f32 q3, q1, q2          @ q3 has {max(x1), max(y1), max(x2), max(y2)}
    vmin.f32 q3, q3, q1          @ This is a trick: min(max(i,j), i) = i, min(max(i,j), j) = j
                                @ Let's do it properly.
    vmax.f32 d6, d2, d4          @ d6 = {max(i.x1,j.x1), max(i.y1,j.y1)}
    vmin.f32 d7, d3, d5          @ d7 = {min(i.x2,j.x2), min(i.y2,j.y2)}

    @ Calculate intersection width and height
    vsub.f32 d8, d7, d6          @ d8 = {ix2-ix1, iy2-iy1} = {inter_w, inter_h}

    @ Clamp to zero if width or height is negative (no overlap)
    vmax.f32 q4, q4, q15         @ q4 = {max(0,w), max(0,h)}

    @ Calculate intersection area
    vmul.f32 s10, d8[0], d8[1]  @ s10 = inter_area = inter_w * inter_h

    @ If inter_area is 0, IoU is 0, so skip to next box
    vcmpe.f32 s10, #0.0
    vmrs    APSR_nzcv, fpscr    @ Move NEON flags to ARM flags
    beq     .L_continue_inner

    @ Calculate area of box_j
    vsub.f32 s5, d4[1], d4[0]   @ s5 = j.x2 - j.x1
    vsub.f32 s6, d5[1], d5[0]   @ s6 = j.y2 - j.y1
    vmul.f32 s12, s5, s6        @ s12 = area_j

    @ Calculate union area = area_i + area_j - inter_area
    vadd.f32 s13, s11, s12
    vsub.f32 s13, s13, s10      @ s13 = union_area

    @ Calculate IoU = inter_area / union_area
    vdiv.f32 s14, s10, s13      @ s14 = iou

    @ Compare IoU with threshold
    vcmp.f32 s14, s0            @ Compare iou with iou_threshold
    vmrs    APSR_nzcv, fpscr
    ble     .L_continue_inner   @ if (iou <= threshold) continue

    @ Suppress box_j
    mov     r7, #1
    str     r7, [r5, #20]       @ boxes[j].suppressed = 1

.L_continue_inner:
    add     r3, r3, #1          @ j++
    b       .L_inner_loop_j

.L_continue_outer:
    add     r2, r2, #1          @ i++
    b       .L_outer_loop_i

.L_exit_nms:
    @ --- Function Epilogue ---
    pop     {r4-r8, pc}

.size nms_greedy_neon, . - nms_greedy_neon
