/*
 * Copyright (c) 2013-2014, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "pti-ild.h"

#include "ptunit.h"

#include <string.h>


enum pti_interest {
	pti_interesting = 1,
	pti_boring = 0
};

static const uint64_t pti_addr = 0xffccffccffccull;

/* Check that an instruction
 * - can be length-decoded
 * - is correctly length-decoded
 * - can be classified
 * - is corectly diagnosed as interesting/boring
 *
 * Does not check whether the classification is correct.
 * This is left to the calling test.
 */
static struct ptunit_result ptunit_ild_decode(pti_ild_t *ild,
					      pti_bool_t interest,
					      pti_uint32_t size)
{
	pti_bool_t lret, dret;

	lret = pti_instruction_length_decode(ild);
	ptu_int_eq(lret, 1);
	ptu_uint_eq(ild->length, size);

	dret = pti_instruction_decode(ild);
	ptu_int_eq(dret, interest);

	return ptu_passed();
}

/* Initialize a PT ILD decoder for testing.
 *
 * We can't use a fixture since we don't know the instruction size upfront.
 */
static void ptunit_ild_init(pti_ild_t *ild, pti_uint8_t *insn,
			    pti_uint32_t size,
			    pti_machine_mode_enum_t mode)
{
	memset(ild, 0, sizeof(*ild));
	ild->itext = insn;
	ild->max_bytes = size;
	ild->mode = mode;
	ild->runtime_address = pti_addr;
}

/* Check that a boring instruction is decoded correctly. */
static struct ptunit_result ptunit_ild_boring(pti_uint8_t *insn,
					      pti_uint32_t size,
					      pti_machine_mode_enum_t mode)
{
	pti_ild_t ild;

	ptunit_ild_init(&ild, insn, size, mode);
	ptu_test(ptunit_ild_decode, &ild, pti_boring, size);

	return ptu_passed();
}

/* Check that an interesting instruction is decoded and classified correctly. */
static struct ptunit_result ptunit_ild_classify(pti_uint8_t *insn,
						pti_uint32_t size,
						pti_machine_mode_enum_t mode,
						pti_inst_enum_t iclass)
{
	pti_ild_t ild;

	ptunit_ild_init(&ild, insn, size, mode);
	ptu_test(ptunit_ild_decode, &ild, pti_interesting, size);
	ptu_int_eq(ild.iclass, iclass);

	return ptu_passed();
}

/* Macros to automatically update the test location. */
#define ptu_boring(insn, size, mode)		\
	ptu_check(ptunit_ild_boring, insn, size, mode)

#define ptu_classify(insn, size, mode, iclass)			\
	ptu_check(ptunit_ild_classify, insn, size, mode, iclass)

/* Macros to also automatically supply the instruction size. */
#define ptu_boring_s(insn, mode)			\
	ptu_boring(insn, sizeof(insn), mode)

#define ptu_classify_s(insn, mode, iclass)		\
	ptu_classify(insn, sizeof(insn), mode, iclass)


static struct ptunit_result push(void)
{
	pti_uint8_t insn[] = { 0x68, 0x11, 0x22, 0x33, 0x44 };

	ptu_boring_s(insn, PTI_MODE_64);

	return ptu_passed();
}

static struct ptunit_result jmp_rel(void)
{
	pti_uint8_t insn[] = { 0xE9, 0x60, 0xF9, 0xFF, 0xFF };

	ptu_classify_s(insn, PTI_MODE_64, PTI_INST_JMP_E9);

	return ptu_passed();
}

static struct ptunit_result long_nop(void)
{
	pti_uint8_t insn[] = { 0x66, 0x66, 0x66, 0x66,
			       0x66, 0x66, 0X2E, 0X0F,
			       0X1F, 0x84, 0x00, 0x00,
			       0x00, 0x00, 0x00 };

	ptu_boring_s(insn, PTI_MODE_64);

	return ptu_passed();
}

static struct ptunit_result mov_al_64(void)
{
	pti_uint8_t insn[] = { 0x48, 0xA1, 0x3f, 0xaa, 0xbb,
			       0xcc, 0xdd, 0xee, 0xfF,
			       0X11 };

	ptu_boring_s(insn, PTI_MODE_64);

	return ptu_passed();
}

static struct ptunit_result mov_al_32(void)
{
	pti_uint8_t insn[] = { 0xA1, 0x3f, 0xaa, 0xbb,
			       0xcc, 0xdd, 0xee, 0xfF,
			       0X11 };

	ptu_boring(insn, 5, PTI_MODE_64);

	return ptu_passed();
}

static struct ptunit_result mov_al_16(void)
{
	pti_uint8_t insn[] = { 0x66, 0xA1, 0x3f, 0xaa, 0xbb,
			       0xcc, 0xdd, 0xee, 0xfF,
			       0X11 };

	ptu_boring(insn, 4, PTI_MODE_64);

	return ptu_passed();
}

static struct ptunit_result rdtsc(void)
{
	pti_uint8_t insn[] = { 0x0f, 0x31 };

	ptu_boring_s(insn, PTI_MODE_64);

	return ptu_passed();
}

int main(int argc, const char **argv)
{
	struct ptunit_suite suite;

	/* Initialize the PT ILD. */
	pti_ild_init();

	suite = ptunit_mk_suite(argc, argv);

	ptu_run(suite, push);
	ptu_run(suite, jmp_rel);
	ptu_run(suite, long_nop);
	ptu_run(suite, mov_al_64);
	ptu_run(suite, mov_al_32);
	ptu_run(suite, mov_al_16);
	ptu_run(suite, rdtsc);

	ptunit_report(&suite);
	return suite.nr_fails;
}
