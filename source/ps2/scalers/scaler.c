
#include "common.h"

#include "scale2x.h"
#include "scale3x.h"
#include "2xSaI.h"

#include <assert.h>

#define SSDST(bits, num) (scale2x_uint##bits *)dst##num
#define SSSRC(bits, num) (const scale2x_uint##bits *)src##num

/**
 * Apply the Scale2x effect on a group of rows. Used internally.
 */
static inline void stage_scale2x(void* dst0, void* dst1, const void* src0, const void* src1, const void* src2, unsigned pixel, unsigned pixel_per_row)
{
	switch (pixel) {
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
		case 1 : scale2x_8_mmx(SSDST(8,0), SSDST(8,1), SSSRC(8,0), SSSRC(8,1), SSSRC(8,2), pixel_per_row); break;
		case 2 : scale2x_16_mmx(SSDST(16,0), SSDST(16,1), SSSRC(16,0), SSSRC(16,1), SSSRC(16,2), pixel_per_row); break;
		case 4 : scale2x_32_mmx(SSDST(32,0), SSDST(32,1), SSSRC(32,0), SSSRC(32,1), SSSRC(32,2), pixel_per_row); break;
#else
		case 1 : scale2x_8_def(SSDST(8,0), SSDST(8,1), SSSRC(8,0), SSSRC(8,1), SSSRC(8,2), pixel_per_row); break;
		//case 2 : scale2x_16_def(SSDST(16,0), SSDST(16,1), SSSRC(16,0), SSSRC(16,1), SSSRC(16,2), pixel_per_row); break;
		case 2 : scale2x_16_mmi(SSDST(16,0), SSDST(16,1), SSSRC(16,0), SSSRC(16,1), SSSRC(16,2), pixel_per_row); break;
		case 4 : scale2x_32_def(SSDST(32,0), SSDST(32,1), SSSRC(32,0), SSSRC(32,1), SSSRC(32,2), pixel_per_row); break;
#endif
	}
}

static inline void stage_scale3x(void* dst0, void* dst1, void* dst2, const void* src0, const void* src1, const void* src2, unsigned pixel, unsigned pixel_per_row)
{
	switch (pixel) {
		case 1 : scale3x_8_def(SSDST(8,0), SSDST(8,1), SSDST(8,2), SSSRC(8,0), SSSRC(8,1), SSSRC(8,2), pixel_per_row); break;
		case 2 : scale3x_16_def(SSDST(16,0), SSDST(16,1), SSDST(16,2), SSSRC(16,0), SSSRC(16,1), SSSRC(16,2), pixel_per_row); break;
		case 4 : scale3x_32_def(SSDST(32,0), SSDST(32,1), SSDST(32,2), SSSRC(32,0), SSSRC(32,1), SSSRC(32,2), pixel_per_row); break;
	}
}


#define SCDST(i) (dst+(i)*dst_slice)
#define SCSRC(i) (src+(i)*src_slice)
#define SCMID(i) (mid[(i)])

/**
 * Apply the Scale2x effect on a bitmap.
 * The destination bitmap is filled with the scaled version of the source bitmap.
 * The source bitmap isn't modified.
 * The destination bitmap must be manually allocated before calling the function,
 * note that the resulting size is exactly 2x2 times the size of the source bitmap.
 * \param void_dst Pointer at the first pixel of the destination bitmap.
 * \param dst_slice Size in bytes of a destination bitmap row.
 * \param void_src Pointer at the first pixel of the source bitmap.
 * \param src_slice Size in bytes of a source bitmap row.
 * \param pixel Bytes per pixel of the source and destination bitmap.
 * \param width Horizontal size in pixels of the source bitmap.
 * \param height Vertical size in pixels of the source bitmap.
 */
static void _scale2x(void* void_dst, unsigned dst_slice, const void* void_src, unsigned src_slice, unsigned pixel, unsigned width, unsigned height)
{
	unsigned char* dst = (unsigned char*)void_dst;
	const unsigned char* src = (const unsigned char*)void_src;
	unsigned count;

	assert(height >= 2);

	count = height;

	stage_scale2x(SCDST(0), SCDST(1), SCSRC(0), SCSRC(0), SCSRC(1), pixel, width);

	dst = SCDST(2);

	count -= 2;
	while (count) {
		stage_scale2x(SCDST(0), SCDST(1), SCSRC(0), SCSRC(1), SCSRC(2), pixel, width);

		dst = SCDST(2);
		src = SCSRC(1);

		--count;
	}

	stage_scale2x(SCDST(0), SCDST(1), SCSRC(0), SCSRC(1), SCSRC(1), pixel, width);

}

static void _scale3x(void* void_dst, unsigned dst_slice, const void* void_src, unsigned src_slice, unsigned pixel, unsigned width, unsigned height)
{
	unsigned char* dst = (unsigned char*)void_dst;
	const unsigned char* src = (const unsigned char*)void_src;
	unsigned count;

	assert(height >= 2);

	count = height;

	stage_scale3x(SCDST(0), SCDST(1), SCDST(2), SCSRC(0), SCSRC(0), SCSRC(1), pixel, width);

	dst = SCDST(3);

	count -= 2;
	while (count) {
		stage_scale3x(SCDST(0), SCDST(1), SCDST(2), SCSRC(0), SCSRC(1), SCSRC(2), pixel, width);

		dst = SCDST(3);
		src = SCSRC(1);

		--count;
	}

	stage_scale3x(SCDST(0), SCDST(1), SCDST(2), SCSRC(0), SCSRC(1), SCSRC(1), pixel, width);
}

static void scale2x_nn(void* void_dst, void* void_src, unsigned w1, unsigned h1)
{
	__asm__ __volatile__(
		".set noreorder\n\t"
		
		"addiu $8, $0, 4 \n"
		
		"movz $10, %2, $0 \n"
		"divu $10, $8\n"
		"nop\n"

		"mflo $8\n" 
		"mfhi $9\n"
		
		"addi $8, $8, -1 \n"
		"addi $13, %3, -1 \n"
		
		"addiu $10, $0, 4 \n"
		"multu $14, %2, $10 \n"
		
		"j 1f\n"
		"movz $10, $8, $0 \n"
		
		"0:\n"
		
		"addu %1, %1, $14 \n"

		"1:\n"
		
		"ld $11, 0(%0) \n"
		"pextlh $11, $11, $11 \n"
		"sq $11, 0(%1) \n"
		
		"addu $12, %1, $14 \n"
		"sq $11, 0($12) \n"
		
		"addiu %0, %0, 8 \n"
		"addiu %1, %1, 16 \n"
		
		"bne $10, $0, 1b \n"
		"addi $10, $10, -1\n"
		
		"beq $9, $0, 3f \n"
		"movz $10, $8, $0 \n"
		
		"2:\n"
		/* trza zachowac t1
		"addi $9, $9, -1 \n"
		
		"lw $11, 0(%0) \n"
		"pextlh $11, $11, $11 \n"
		"sd $11, 0(%1) \n"
		
		"addu $12, %1, $14 \n"
		"sd $11, 0($12) \n"
		
		"bne $9, $0, 2b \n"
		"nop\n"
		*/
		"3:\n"
		"bne $13, $0, 0b \n"
		"addi $13, $13, -1 \n"
		
		".set reorder\n\t"
		
		: "=r" (void_src), "=r" (void_dst), "=r" (w1), "=r" (h1)
		: "0" (void_src), "1" (void_dst), "2" (w1), "3" (h1)
		: "t0", "t1", "t2", "t3", "t4", "t5", "t6"
	);
}

static void scale3x_nn(void* void_dst, void* void_src, unsigned w1, unsigned h1)
{
	__asm__ __volatile__(
		".set noreorder\n\t"
				
		"addiu $8, $0, 4 \n"
		
		"movz $10, %2, $0 \n" //t2 width
		"divu $10, $8\n" // width/4 odczytuje sie 4 bajty
		"nop\n"

		"mflo $8\n" //iloraz
		"mfhi $9\n" //reszta
		
		"addi $8, $8, -1 \n"
		"addi $13, %3, -1 \n"
		
		"addiu $10, $0, 6 \n"
		"multu $14, %2, $10 \n" //t6 width * 2 * 3
		
		"j 1f\n"
		"movz $10, $8, $0 \n" //t2 iloraz
		
		"0:\n"
		
		"addu %1, %1, $14 \n"
		"addu %1, %1, $14 \n"

		"1:\n"
		
		"ld $11, 0(%0) \n"
		"pextlh $12, $11, $11 \n"
		
		"pextlh $15, $11, $12 \n"
		
		"pextuh $24, $15, $15 \n"
		
		"sd $15, 0(%1) \n"
		"sd $24, 8(%1) \n"
		
		"pinth $11, $24, $11 \n"
		"pcpyud $11, $11, $0 \n"
		"sd $11, 16(%1) \n"
		
		"addu $12, %1, $14 \n"
		"sd $15, 0($12) \n"
		"sd $24, 8($12) \n"
		"sd $11, 16($12) \n"
		
		"addu $12, $12, $14 \n"
		"sd $15, 0($12) \n"
		"sd $24, 8($12) \n"
		"sd $11, 16($12) \n"
		
		"addiu %0, %0, 8 \n"
		"addiu %1, %1, 24 \n"
			
		"bne $10, $0, 1b \n"
		"addi $10, $10, -1 \n"
		
		"beq $9, $0, 3f \n"
		"movz $10, $8, $0 \n"
		
		"2:\n"
		/*
		"addi $9, $9, -1 \n"
		
		"lw $11, 0(%0) \n"
		"pextlh $11, $11, $11 \n"
		"sd $11, 0(%1) \n"
		
		"addu $12, %1, $14 \n"
		"sd $11, 0($12) \n"
		
		"bne $9, $0, 2b \n"
		"nop\n"
		*/
		"3:\n"
		"bne $13, $0, 0b \n"
		"addi $13, $13, -1\n"
		
		".set reorder\n\t"
		
		: "=r" (void_src), "=r" (void_dst), "=r" (w1), "=r" (h1)
		: "0" (void_src), "1" (void_dst), "2" (w1), "3" (h1)
		: "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8"
	);
}

u8 scaler_init = 0;

video_scale_type PerGameScaleMode = none;
video_scale_type ScaleMode = none;

void scaler(video_scale_type type, void* void_dst, const void* void_src, unsigned w_src, unsigned h_src, unsigned w_dst, unsigned h_dst)
{
	if(!scaler_init)
	{
		scaler_init = Init_2xSaI(555);
	}

	switch(type)
	{
		case scale2xnn:
				scale2x_nn((u16 *)void_dst, (u16 *)void_src, w_src, h_src); break;
		case scale2x:
				_scale2x(void_dst, w_dst * 2, void_src, w_src * 2, 2, w_src, h_src); break;
		case sup2xsai:
				Super2xSaI((u8*)void_src, w_src * 2, 0, void_dst, w_dst * 2, w_src, h_src); break;
		case supeagle:
				SuperEagle((u8*)void_src, w_src * 2, 0, void_dst, w_dst * 2, w_src, h_src); break;
		case _2xsai:
				_2xSaI((u8*)void_src, w_src * 2, 0, void_dst, w_dst * 2, w_src, h_src); break;
		case scale2xsai:
				Scale_2xSaI((u8*)void_src, w_src * 2, 0, void_dst, w_dst * 2, w_dst, h_dst, w_src, h_src); break;
		case scale3xnn:
				scale3x_nn((u16 *)void_dst, (u16 *)void_src, w_src, h_src); break;
		case scale3x:
				_scale3x(void_dst, w_dst * 2, void_src, w_src * 2, 2, w_src, h_src); break;
		default:
			break;
	}
}
