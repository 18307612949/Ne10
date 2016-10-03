/*
 *  Copyright 2013-15 ARM Limited and Contributors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of ARM Limited nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY ARM LIMITED AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL ARM LIMITED AND CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* license of Kiss FFT */
/*
Copyright (c) 2003-2010, Mark Borgerding

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the author nor the names of any contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * NE10 Library : dsp/NE10_fft_float32.c
 */

#include "NE10_types.h"
#include "NE10_macros.h"
#include "NE10_fft.h"

/*
 * This function calculates the FFT for power-of-two input sizes using an ordered, mixed
 * radix-4/8 DIT algorithm.
 *
 * At each stage, 'fstride' holds the number of butterfly "sections" to be processed,
 * while 'mstride' holds the number of butterflies to be performed in each section. After
 * radix-4 butterflies, for example, we quarter the the 'fstride' (number of sections) and
 * quadruple the 'mstride' (size of each section) for the next stage. The exception to
 * this is the first stage, in which 'mstride' does not apply (as it is implicitly 1).
 *
 * The algorithm first performs either a radix-8 or radix-4 pass, depending on the size
 * of the input/output (as dictated by the 'factors' array), and then continually applies
 * radix-4 butterflies to completion. The order in which results are stored after each
 * stage allows stages to load and store elements contiguously between iterations, and for
 * the final output order to be correct.
 */
static void ne10_mixed_radix_butterfly_float32_c (ne10_fft_cpx_float32_t *out,
        ne10_fft_cpx_float32_t *in,
        ne10_int32_t *factors,
        ne10_fft_cpx_float32_t *twiddles,
        ne10_fft_cpx_float32_t *buffer)
{
    ne10_int32_t stage_count = factors[0];
    ne10_int32_t fstride = factors[1];
    ne10_int32_t mstride = factors[(stage_count << 1) - 1];
    ne10_int32_t first_radix = factors[stage_count << 1];
    ne10_int32_t step, f_count, m_count;
    ne10_fft_cpx_float32_t *src = in;
    ne10_fft_cpx_float32_t *dst = out;
    ne10_fft_cpx_float32_t *out_final = out;
    ne10_fft_cpx_float32_t *tw, *tmp;
    const ne10_float32_t TW_81 = 0.70710678;

    ne10_fft_cpx_float32_t scratch[16];
    ne10_fft_cpx_float32_t scratch_in[8];
    ne10_fft_cpx_float32_t scratch_out[8];
    ne10_fft_cpx_float32_t scratch_tw[6];

    // The first stage (using hardcoded twiddles)
    if (first_radix == 2) // For radix-4 factoring, this means nfft is of form 2^{odd}
    {
        // Instead of performing a radix-2 butterfly as the factors array suggests,
        // we will instead perform a radix-8 butterfly.
        // (For C2C, fstride = nfft / 2, fstride1 = nfft / 8).
        ne10_int32_t fstride1 = (fstride / 4);
        for (f_count = 0; f_count < fstride1; f_count++)
        {
            dst = &out[f_count * 8];

            // X[0] +/- X[4N/8]
            scratch_in[0].r = src[0].r + src[0 + fstride].r;
            scratch_in[0].i = src[0].i + src[0 + fstride].i;
            scratch_in[1].r = src[0].r - src[0 + fstride].r;
            scratch_in[1].i = src[0].i - src[0 + fstride].i;

            // X[N/8] +/- X[5N/8]
            scratch_in[2].r = src[fstride1].r + src[fstride1 + fstride].r;
            scratch_in[2].i = src[fstride1].i + src[fstride1 + fstride].i;
            scratch_in[3].r = src[fstride1].r - src[fstride1 + fstride].r;
            scratch_in[3].i = src[fstride1].i - src[fstride1 + fstride].i;

            // X[2N/8] +/- X[6N/8]
            scratch_in[4].r = src[fstride1 * 2].r + src[fstride1 * 2 + fstride].r;
            scratch_in[4].i = src[fstride1 * 2].i + src[fstride1 * 2 + fstride].i;
            scratch_in[5].r = src[fstride1 * 2].r - src[fstride1 * 2 + fstride].r;
            scratch_in[5].i = src[fstride1 * 2].i - src[fstride1 * 2 + fstride].i;

            // X[3N/8] +/- X[7N/8]
            scratch_in[6].r = src[fstride1 * 3].r + src[fstride1 * 3 + fstride].r;
            scratch_in[6].i = src[fstride1 * 3].i + src[fstride1 * 3 + fstride].i;
            scratch_in[7].r = src[fstride1 * 3].r - src[fstride1 * 3 + fstride].r;
            scratch_in[7].i = src[fstride1 * 3].i - src[fstride1 * 3 + fstride].i;


            scratch[0] = scratch_in[0]; // X[0] + X[4N/8]
            scratch[1] = scratch_in[1]; // X[0] - X[4N/8]
            scratch[2] = scratch_in[2]; // X[N/8] + X[5N/8]
            scratch[4] = scratch_in[4]; // X[2N/8] + X[6N/8]
            scratch[6] = scratch_in[6]; // X[3N/8] + X[7N/8]

            // (X[2N/8] - X[6N/8]) * -i
            scratch[5].r = scratch_in[5].i;
            scratch[5].i = -scratch_in[5].r;

            // (X[N/8] - X[5N/8]) * (TW_81 - TW_81i)
            scratch[3].r = (scratch_in[3].r + scratch_in[3].i) * TW_81;
            scratch[3].i = (scratch_in[3].i - scratch_in[3].r) * TW_81;

            // (X[3N/8] - X[7N/8]) * (TW_81 + TW_81i)
            scratch[7].r = (scratch_in[7].r - scratch_in[7].i) * TW_81;
            scratch[7].i = (scratch_in[7].i + scratch_in[7].r) * TW_81;

            // Combine the (X[0] +/- X[4N/8]) and (X[2N/8] +/- X[6N/8]) components
            scratch[8].r  = scratch[0].r + scratch[4].r;
            scratch[8].i  = scratch[0].i + scratch[4].i;
            scratch[9].r  = scratch[1].r + scratch[5].r;
            scratch[9].i  = scratch[1].i + scratch[5].i;
            scratch[10].r = scratch[0].r - scratch[4].r;
            scratch[10].i = scratch[0].i - scratch[4].i;
            scratch[11].r = scratch[1].r - scratch[5].r;
            scratch[11].i = scratch[1].i - scratch[5].i;

            // Combine the (X[N/8] +/- X[5N/8]) and (X[3N/8] +/- X[7N/8]) components
            scratch[12].r = scratch[2].r + scratch[6].r;
            scratch[12].i = scratch[2].i + scratch[6].i;
            scratch[13].r = scratch[3].r - scratch[7].r;
            scratch[13].i = scratch[3].i - scratch[7].i;
            scratch[14].r = scratch[2].r - scratch[6].r;
            scratch[14].i = scratch[2].i - scratch[6].i;
            scratch[15].r = scratch[3].r + scratch[7].r;
            scratch[15].i = scratch[3].i + scratch[7].i;

            // Combine the two combined components (for the full radix-8 butterfly)
            scratch_out[0].r = scratch[8].r  + scratch[12].r;
            scratch_out[0].i = scratch[8].i  + scratch[12].i;
            scratch_out[1].r = scratch[9].r  + scratch[13].r;
            scratch_out[1].i = scratch[9].i  + scratch[13].i;
            scratch_out[2].r = scratch[10].r + scratch[14].i;
            scratch_out[2].i = scratch[10].i - scratch[14].r;
            scratch_out[3].r = scratch[11].r + scratch[15].i;
            scratch_out[3].i = scratch[11].i - scratch[15].r;
            scratch_out[4].r = scratch[8].r  - scratch[12].r;
            scratch_out[4].i = scratch[8].i  - scratch[12].i;
            scratch_out[5].r = scratch[9].r  - scratch[13].r;
            scratch_out[5].i = scratch[9].i  - scratch[13].i;
            scratch_out[6].r = scratch[10].r - scratch[14].i;
            scratch_out[6].i = scratch[10].i + scratch[14].r;
            scratch_out[7].r = scratch[11].r - scratch[15].i;
            scratch_out[7].i = scratch[11].i + scratch[15].r;

            // Store the results
            dst[0] = scratch_out[0];
            dst[1] = scratch_out[1];
            dst[2] = scratch_out[2];
            dst[3] = scratch_out[3];
            dst[4] = scratch_out[4];
            dst[5] = scratch_out[5];
            dst[6] = scratch_out[6];
            dst[7] = scratch_out[7];

            src++;
        } // f_count

        // Update variables for the next stages
        step = fstride >> 1; // For C2C, 1/4 of input size (fstride is nfft/2)
        mstride *= 4;
        fstride /= 16;
        stage_count -= 2;
        twiddles += 6; // Skip the twiddles we no longer need (as we did radix 8, not 2)
    }
    else if (first_radix == 4) // For radix-4 factoring, this means nfft is of form 2^{even}
    {
        for (f_count = fstride; f_count; f_count--)
        {
            // Load the four input values for a radix-4 butterfly
            scratch_in[0] = src[0];           // X[0]
            scratch_in[1] = src[fstride * 1]; // X[N/4]
            scratch_in[2] = src[fstride * 2]; // X[2N/4]
            scratch_in[3] = src[fstride * 3]; // X[3N/4]

            // X[0] +/- X[2N/4]
            scratch[0].r = scratch_in[0].r + scratch_in[2].r;
            scratch[0].i = scratch_in[0].i + scratch_in[2].i;
            scratch[1].r = scratch_in[0].r - scratch_in[2].r;
            scratch[1].i = scratch_in[0].i - scratch_in[2].i;

            // X[N/4] +/- X[3N/4]
            scratch[2].r = scratch_in[1].r + scratch_in[3].r;
            scratch[2].i = scratch_in[1].i + scratch_in[3].i;
            scratch[3].r = scratch_in[1].r - scratch_in[3].r;
            scratch[3].i = scratch_in[1].i - scratch_in[3].i;

            // Combine the (X[0] +/- X[2N/4]) and (X[N/4] +/- X[3N/4]) components (for
            // the full radix-4 butterfly)
            scratch_out[0].r = scratch[0].r + scratch[2].r;
            scratch_out[0].i = scratch[0].i + scratch[2].i;
            scratch_out[1].r = scratch[1].r + scratch[3].i; // scratch[1] - i*scratch[3]
            scratch_out[1].i = scratch[1].i - scratch[3].r;
            scratch_out[2].r = scratch[0].r - scratch[2].r;
            scratch_out[2].i = scratch[0].i - scratch[2].i;
            scratch_out[3].r = scratch[1].r - scratch[3].i; // scratch[1] + i*scratch[3]
            scratch_out[3].i = scratch[1].i + scratch[3].r;

            // Store the results
            *dst++ = scratch_out[0];
            *dst++ = scratch_out[1];
            *dst++ = scratch_out[2];
            *dst++ = scratch_out[3];

            src++;
        } // f_count

        // Update variables for the next stages
        step = fstride; // For C2C, 1/4 of input size (fstride is nfft/4)
        stage_count--;
        fstride /= 4;
    }

    // The next stage should read the output of the first stage as input
    in = out;
    out = buffer;

    // Middle stages (after the first, excluding the last)
    for (; stage_count > 1; stage_count--)
    {
        src = in;
        for (f_count = 0; f_count < fstride; f_count++)
        {
            dst = &out[f_count * (mstride * 4)];
            tw = twiddles; // Reset the twiddle pointer for the next section
            for (m_count = mstride; m_count; m_count--)
            {
                // Load the three twiddles and four input values for a radix-4 butterfly
                scratch_tw[0] = tw[0];           // w^{k}
                scratch_tw[1] = tw[mstride * 1]; // w^{2k}
                scratch_tw[2] = tw[mstride * 2]; // w^{3k}
                scratch_in[0] = src[0];
                scratch_in[1] = src[step * 1];
                scratch_in[2] = src[step * 2];
                scratch_in[3] = src[step * 3];

                // Multiply input elements by their associated twiddles
                scratch[0] = scratch_in[0];
                scratch[1].r = scratch_in[1].r * scratch_tw[0].r - scratch_in[1].i * scratch_tw[0].i;
                scratch[1].i = scratch_in[1].i * scratch_tw[0].r + scratch_in[1].r * scratch_tw[0].i;
                scratch[2].r = scratch_in[2].r * scratch_tw[1].r - scratch_in[2].i * scratch_tw[1].i;
                scratch[2].i = scratch_in[2].i * scratch_tw[1].r + scratch_in[2].r * scratch_tw[1].i;
                scratch[3].r = scratch_in[3].r * scratch_tw[2].r - scratch_in[3].i * scratch_tw[2].i;
                scratch[3].i = scratch_in[3].i * scratch_tw[2].r + scratch_in[3].r * scratch_tw[2].i;

                // X[0] +/- X[2N/4]
                scratch[4].r = scratch[0].r + scratch[2].r;
                scratch[4].i = scratch[0].i + scratch[2].i;
                scratch[5].r = scratch[0].r - scratch[2].r;
                scratch[5].i = scratch[0].i - scratch[2].i;

                // X[N/4] +/- X[3N/4]
                scratch[6].r = scratch[1].r + scratch[3].r;
                scratch[6].i = scratch[1].i + scratch[3].i;
                scratch[7].r = scratch[1].r - scratch[3].r;
                scratch[7].i = scratch[1].i - scratch[3].i;

                // Combine the (X[0] +/- X[2N/4]) and (X[N/4] +/- X[3N/4]) components (for
                // the full radix-4 butterfly)
                scratch_out[0].r = scratch[4].r + scratch[6].r;
                scratch_out[0].i = scratch[4].i + scratch[6].i;
                scratch_out[1].r = scratch[5].r + scratch[7].i;
                scratch_out[1].i = scratch[5].i - scratch[7].r;
                scratch_out[2].r = scratch[4].r - scratch[6].r;
                scratch_out[2].i = scratch[4].i - scratch[6].i;
                scratch_out[3].r = scratch[5].r - scratch[7].i;
                scratch_out[3].i = scratch[5].i + scratch[7].r;

                // Store the results
                dst[0] = scratch_out[0];
                dst[mstride * 1] = scratch_out[1];
                dst[mstride * 2] = scratch_out[2];
                dst[mstride * 3] = scratch_out[3];

                tw++;
                src++;
                dst++;
            } // m_count
        } // f_count

        // Update variables for the next stages
        twiddles += mstride * 3;
        mstride *= 4;
        fstride /= 4;

        // Swap the input and output buffers for the next stage
        tmp = in;
        in = out;
        out = tmp;
    } // stage_count

    // The last stage
    if (stage_count)
    {
        src = in;

        // Always write to the final output buffer (if necessary, we can calculate this
        // in-place as the final stage reads and writes at the same offsets)
        dst = out_final;

        for (f_count = fstride; f_count; f_count--) // Note: for C2C, fstride = 1
        {
            tw = twiddles; // Reset the twiddle pointer for the next section
            for (m_count = mstride; m_count; m_count--)
            {
                // Load the three twiddles and four input values for a radix-4 butterfly
                scratch_tw[0] = tw[0];
                scratch_tw[1] = tw[mstride * 1];
                scratch_tw[2] = tw[mstride * 2];
                scratch_in[0] = src[0];
                scratch_in[1] = src[step * 1];
                scratch_in[2] = src[step * 2];
                scratch_in[3] = src[step * 3];

                // Multiply input elements by their associated twiddles
                scratch[0] = scratch_in[0];
                scratch[1].r = scratch_in[1].r * scratch_tw[0].r - scratch_in[1].i * scratch_tw[0].i;
                scratch[1].i = scratch_in[1].i * scratch_tw[0].r + scratch_in[1].r * scratch_tw[0].i;
                scratch[2].r = scratch_in[2].r * scratch_tw[1].r - scratch_in[2].i * scratch_tw[1].i;
                scratch[2].i = scratch_in[2].i * scratch_tw[1].r + scratch_in[2].r * scratch_tw[1].i;
                scratch[3].r = scratch_in[3].r * scratch_tw[2].r - scratch_in[3].i * scratch_tw[2].i;
                scratch[3].i = scratch_in[3].i * scratch_tw[2].r + scratch_in[3].r * scratch_tw[2].i;

                // X[0] +/- X[2N/4]
                scratch[4].r = scratch[0].r + scratch[2].r;
                scratch[4].i = scratch[0].i + scratch[2].i;
                scratch[5].r = scratch[0].r - scratch[2].r;
                scratch[5].i = scratch[0].i - scratch[2].i;

                // X[N/4] +/- X[3N/4]
                scratch[6].r = scratch[1].r + scratch[3].r;
                scratch[6].i = scratch[1].i + scratch[3].i;
                scratch[7].r = scratch[1].r - scratch[3].r;
                scratch[7].i = scratch[1].i - scratch[3].i;

                // Combine the (X[0] +/- X[2N/4]) and (X[N/4] +/- X[3N/4]) components (for
                // the full radix-4 butterfly)
                scratch_out[0].r = scratch[4].r + scratch[6].r;
                scratch_out[0].i = scratch[4].i + scratch[6].i;
                scratch_out[1].r = scratch[5].r + scratch[7].i;
                scratch_out[1].i = scratch[5].i - scratch[7].r;
                scratch_out[2].r = scratch[4].r - scratch[6].r;
                scratch_out[2].i = scratch[4].i - scratch[6].i;
                scratch_out[3].r = scratch[5].r - scratch[7].i;
                scratch_out[3].i = scratch[5].i + scratch[7].r;

                // Store the results
                dst[0] = scratch_out[0];
                dst[step * 1] = scratch_out[1];
                dst[step * 2] = scratch_out[2];
                dst[step * 3] = scratch_out[3];

                tw++;
                src++;
                dst++;
            } // m_count
        } // f_count
    } // last stage
}

/*
 * This function calculates the inverse FFT, and is very similar in structure to its
 * complement "ne10_mixed_radix_butterfly_float32_c".
 */
static void ne10_mixed_radix_butterfly_inverse_float32_c (ne10_fft_cpx_float32_t *out,
        ne10_fft_cpx_float32_t *in,
        ne10_int32_t *factors,
        ne10_fft_cpx_float32_t *twiddles,
        ne10_fft_cpx_float32_t *buffer)
{
    ne10_int32_t stage_count = factors[0];
    ne10_int32_t fstride = factors[1];
    ne10_int32_t mstride = factors[(stage_count << 1) - 1];
    ne10_int32_t first_radix = factors[stage_count << 1];
    ne10_float32_t one_by_nfft = (1.0f / (ne10_float32_t) (fstride * first_radix));
    ne10_int32_t step, f_count, m_count;
    ne10_fft_cpx_float32_t *src = in;
    ne10_fft_cpx_float32_t *dst = out;
    ne10_fft_cpx_float32_t *out_final = out;
    ne10_fft_cpx_float32_t *tw, *tmp;
    const ne10_float32_t TW_81 = 0.70710678;

    ne10_fft_cpx_float32_t scratch[16];
    ne10_fft_cpx_float32_t scratch_in[8];
    ne10_fft_cpx_float32_t scratch_out[8];
    ne10_fft_cpx_float32_t scratch_tw[6];

    // The first stage (using hardcoded twiddles)
    if (first_radix == 2) // nfft is of form 2^{odd}
    {
        // Perform a radix-8 butterfly (rather than the "suggested" radix-2)
        ne10_int32_t fstride1 = (fstride / 4);
        for (f_count = 0; f_count < fstride1; f_count++)
        {
            dst = &out[f_count * 8];

            // Prepare sums for the butterfly calculations
            scratch_in[0].r = src[0].r + src[0 + fstride].r;
            scratch_in[0].i = src[0].i + src[0 + fstride].i;
            scratch_in[1].r = src[0].r - src[0 + fstride].r;
            scratch_in[1].i = src[0].i - src[0 + fstride].i;
            scratch_in[2].r = src[fstride1].r + src[fstride1 + fstride].r;
            scratch_in[2].i = src[fstride1].i + src[fstride1 + fstride].i;
            scratch_in[3].r = src[fstride1].r - src[fstride1 + fstride].r;
            scratch_in[3].i = src[fstride1].i - src[fstride1 + fstride].i;
            scratch_in[4].r = src[fstride1 * 2].r + src[fstride1 * 2 + fstride].r;
            scratch_in[4].i = src[fstride1 * 2].i + src[fstride1 * 2 + fstride].i;
            scratch_in[5].r = src[fstride1 * 2].r - src[fstride1 * 2 + fstride].r;
            scratch_in[5].i = src[fstride1 * 2].i - src[fstride1 * 2 + fstride].i;
            scratch_in[6].r = src[fstride1 * 3].r + src[fstride1 * 3 + fstride].r;
            scratch_in[6].i = src[fstride1 * 3].i + src[fstride1 * 3 + fstride].i;
            scratch_in[7].r = src[fstride1 * 3].r - src[fstride1 * 3 + fstride].r;
            scratch_in[7].i = src[fstride1 * 3].i - src[fstride1 * 3 + fstride].i;

            // Multiply some of these by hardcoded radix-8 twiddles
            scratch[0] = scratch_in[0];
            scratch[1] = scratch_in[1];
            scratch[2] = scratch_in[2];
            scratch[3].r = (scratch_in[3].r - scratch_in[3].i) * TW_81;
            scratch[3].i = (scratch_in[3].i + scratch_in[3].r) * TW_81;
            scratch[4] = scratch_in[4];
            scratch[5].r = -scratch_in[5].i;
            scratch[5].i = scratch_in[5].r;
            scratch[6].r = scratch_in[6].r;
            scratch[6].i = scratch_in[6].i;
            scratch[7].r = (scratch_in[7].r + scratch_in[7].i) * TW_81;
            scratch[7].i = (scratch_in[7].i - scratch_in[7].r) * TW_81;

            // Combine the first set of pairs of these sums
            scratch[8].r = scratch[0].r  + scratch[4].r;
            scratch[8].i = scratch[0].i  + scratch[4].i;
            scratch[9].r = scratch[1].r  + scratch[5].r;
            scratch[9].i = scratch[1].i  + scratch[5].i;
            scratch[10].r = scratch[0].r - scratch[4].r;
            scratch[10].i = scratch[0].i - scratch[4].i;
            scratch[11].r = scratch[1].r - scratch[5].r;
            scratch[11].i = scratch[1].i - scratch[5].i;

            // Combine the second set of pairs of these sums
            scratch[12].r = scratch[2].r + scratch[6].r;
            scratch[12].i = scratch[2].i + scratch[6].i;
            scratch[13].r = scratch[3].r - scratch[7].r;
            scratch[13].i = scratch[3].i - scratch[7].i;
            scratch[14].r = scratch[2].r - scratch[6].r;
            scratch[14].i = scratch[2].i - scratch[6].i;
            scratch[15].r = scratch[3].r + scratch[7].r;
            scratch[15].i = scratch[3].i + scratch[7].i;

            // Combine these combined components (for the full radix-8 butterfly)
            scratch_out[0].r = scratch[8].r  + scratch[12].r;
            scratch_out[0].i = scratch[8].i  + scratch[12].i;
            scratch_out[1].r = scratch[9].r  + scratch[13].r;
            scratch_out[1].i = scratch[9].i  + scratch[13].i;
            scratch_out[2].r = scratch[10].r - scratch[14].i;
            scratch_out[2].i = scratch[10].i + scratch[14].r;
            scratch_out[3].r = scratch[11].r - scratch[15].i;
            scratch_out[3].i = scratch[11].i + scratch[15].r;
            scratch_out[4].r = scratch[8].r  - scratch[12].r;
            scratch_out[4].i = scratch[8].i  - scratch[12].i;
            scratch_out[5].r = scratch[9].r  - scratch[13].r;
            scratch_out[5].i = scratch[9].i  - scratch[13].i;
            scratch_out[6].r = scratch[10].r + scratch[14].i;
            scratch_out[6].i = scratch[10].i - scratch[14].r;
            scratch_out[7].r = scratch[11].r + scratch[15].i;
            scratch_out[7].i = scratch[11].i - scratch[15].r;

            // Store the results
            dst[0] = scratch_out[0];
            dst[1] = scratch_out[1];
            dst[2] = scratch_out[2];
            dst[3] = scratch_out[3];
            dst[4] = scratch_out[4];
            dst[5] = scratch_out[5];
            dst[6] = scratch_out[6];
            dst[7] = scratch_out[7];

            src++;
        } // f_count

        // Update variables for the next stages
        step = fstride >> 1;
        mstride *= 4;
        fstride /= 16;
        stage_count -= 2;
        twiddles += 6;

        if (stage_count == 0)
        {
            dst = out;
            for (f_count = 0; f_count < 8; f_count++)
            {
                dst[f_count].r *= one_by_nfft;
                dst[f_count].i *= one_by_nfft;
            }
        }
    }
    else if (first_radix == 4) // nfft is of form 2^{even}
    {
        for (f_count = fstride; f_count; f_count--)
        {
            // Load the four input values for a radix-4 butterfly
            scratch_in[0] = src[0];
            scratch_in[1] = src[fstride * 1];
            scratch_in[2] = src[fstride * 2];
            scratch_in[3] = src[fstride * 3];

            // Prepare the first set of sums for the butterfly calculations
            scratch[0].r = scratch_in[0].r + scratch_in[2].r;
            scratch[0].i = scratch_in[0].i + scratch_in[2].i;
            scratch[1].r = scratch_in[0].r - scratch_in[2].r;
            scratch[1].i = scratch_in[0].i - scratch_in[2].i;

            // Prepare the second set of sums for the butterfly calculations
            scratch[2].r = scratch_in[1].r + scratch_in[3].r;
            scratch[2].i = scratch_in[1].i + scratch_in[3].i;
            scratch[3].r = scratch_in[1].r - scratch_in[3].r;
            scratch[3].i = scratch_in[1].i - scratch_in[3].i;

            // Combine these sums (for the full radix-4 butterfly)
            scratch_out[0].r = scratch[0].r + scratch[2].r;
            scratch_out[0].i = scratch[0].i + scratch[2].i;
            scratch_out[1].r = scratch[1].r - scratch[3].i;
            scratch_out[1].i = scratch[1].i + scratch[3].r;
            scratch_out[2].r = scratch[0].r - scratch[2].r;
            scratch_out[2].i = scratch[0].i - scratch[2].i;
            scratch_out[3].r = scratch[1].r + scratch[3].i;
            scratch_out[3].i = scratch[1].i - scratch[3].r;

            // Store the results
            *dst++ = scratch_out[0];
            *dst++ = scratch_out[1];
            *dst++ = scratch_out[2];
            *dst++ = scratch_out[3];

            src++;
        } // f_count

        // Update variables for the next stages
        step = fstride;
        stage_count--;
        fstride /= 4;

        if (stage_count == 0)
        {
            dst = out;
            for (f_count = 0; f_count < 4; f_count++)
            {
                dst[f_count].r *= one_by_nfft;
                dst[f_count].i *= one_by_nfft;
            }
        }
    }

    // The next stage should read the output of the first stage as input
    in = out;
    out = buffer;

    // Middle stages (after the first, excluding the last)
    for (; stage_count > 1; stage_count--)
    {
        src = in;
        for (f_count = 0; f_count < fstride; f_count++)
        {
            dst = &out[f_count * (mstride * 4)];
            tw = twiddles;
            for (m_count = mstride; m_count ; m_count --)
            {
                // Load the three twiddles and four input values for a radix-4 butterfly
                scratch_tw[0] = tw[0];
                scratch_tw[1] = tw[mstride * 1];
                scratch_tw[2] = tw[mstride * 2];
                scratch_in[0] = src[0];
                scratch_in[1] = src[step * 1];
                scratch_in[2] = src[step * 2];
                scratch_in[3] = src[step * 3];

                // Multiply input elements by their associated twiddles
                scratch[0] = scratch_in[0];
                scratch[1].r = scratch_in[1].r * scratch_tw[0].r + scratch_in[1].i * scratch_tw[0].i;
                scratch[1].i = scratch_in[1].i * scratch_tw[0].r - scratch_in[1].r * scratch_tw[0].i;
                scratch[2].r = scratch_in[2].r * scratch_tw[1].r + scratch_in[2].i * scratch_tw[1].i;
                scratch[2].i = scratch_in[2].i * scratch_tw[1].r - scratch_in[2].r * scratch_tw[1].i;
                scratch[3].r = scratch_in[3].r * scratch_tw[2].r + scratch_in[3].i * scratch_tw[2].i;
                scratch[3].i = scratch_in[3].i * scratch_tw[2].r - scratch_in[3].r * scratch_tw[2].i;

                // Prepare the first set of sums for the butterfly calculations
                scratch[4].r = scratch[0].r + scratch[2].r;
                scratch[4].i = scratch[0].i + scratch[2].i;
                scratch[5].r = scratch[0].r - scratch[2].r;
                scratch[5].i = scratch[0].i - scratch[2].i;

                // Prepare the second set of sums for the butterfly calculations
                scratch[6].r = scratch[1].r + scratch[3].r;
                scratch[6].i = scratch[1].i + scratch[3].i;
                scratch[7].r = scratch[1].r - scratch[3].r;
                scratch[7].i = scratch[1].i - scratch[3].i;

                // Combine these sums (for the full radix-4 butterfly)
                scratch_out[0].r = scratch[4].r + scratch[6].r;
                scratch_out[0].i = scratch[4].i + scratch[6].i;
                scratch_out[1].r = scratch[5].r - scratch[7].i;
                scratch_out[1].i = scratch[5].i + scratch[7].r;
                scratch_out[2].r = scratch[4].r - scratch[6].r;
                scratch_out[2].i = scratch[4].i - scratch[6].i;
                scratch_out[3].r = scratch[5].r + scratch[7].i;
                scratch_out[3].i = scratch[5].i - scratch[7].r;

                // Store the results
                dst[0] = scratch_out[0];
                dst[mstride * 1] = scratch_out[1];
                dst[mstride * 2] = scratch_out[2];
                dst[mstride * 3] = scratch_out[3];

                tw++;
                src++;
                dst++;
            } // m_count
        } // f_count

        // Update variables for the next stages
        twiddles += mstride * 3;
        mstride *= 4;
        fstride /= 4;

        // Swap the input and output buffers for the next stage
        tmp = in;
        in = out;
        out = tmp;
    } // stage_count

    // The last stage
    if (stage_count)
    {
        src = in;

        // Always write to the final output buffer (if necessary, we can calculate this
        // in-place as the final stage reads and writes at the same offsets)
        dst = out_final;

        for (f_count = 0; f_count < fstride; f_count++)
        {
            tw = twiddles;
            for (m_count = mstride; m_count; m_count--)
            {
                // Load the three twiddles and four input values for a radix-4 butterfly
                scratch_tw[0] = tw[0];
                scratch_tw[1] = tw[mstride * 1];
                scratch_tw[2] = tw[mstride * 2];
                scratch_in[0] = src[0];
                scratch_in[1] = src[step * 1];
                scratch_in[2] = src[step * 2];
                scratch_in[3] = src[step * 3];

                // Multiply input elements by their associated twiddles
                scratch[0] = scratch_in[0];
                scratch[1].r = scratch_in[1].r * scratch_tw[0].r + scratch_in[1].i * scratch_tw[0].i;
                scratch[1].i = scratch_in[1].i * scratch_tw[0].r - scratch_in[1].r * scratch_tw[0].i;
                scratch[2].r = scratch_in[2].r * scratch_tw[1].r + scratch_in[2].i * scratch_tw[1].i;
                scratch[2].i = scratch_in[2].i * scratch_tw[1].r - scratch_in[2].r * scratch_tw[1].i;
                scratch[3].r = scratch_in[3].r * scratch_tw[2].r + scratch_in[3].i * scratch_tw[2].i;
                scratch[3].i = scratch_in[3].i * scratch_tw[2].r - scratch_in[3].r * scratch_tw[2].i;

                // Prepare the first set of sums for the butterfly calculations
                scratch[4].r = scratch[0].r + scratch[2].r;
                scratch[4].i = scratch[0].i + scratch[2].i;
                scratch[5].r = scratch[0].r - scratch[2].r;
                scratch[5].i = scratch[0].i - scratch[2].i;

                // Prepare the second set of sums for the butterfly calculations
                scratch[6].r = scratch[1].r + scratch[3].r;
                scratch[6].i = scratch[1].i + scratch[3].i;
                scratch[7].r = scratch[1].r - scratch[3].r;
                scratch[7].i = scratch[1].i - scratch[3].i;

                // Combine these sums (for the full radix-4 butterfly) and multiply by
                // (1 / nfft).
                scratch_out[0].r = (scratch[4].r + scratch[6].r) * one_by_nfft;
                scratch_out[0].i = (scratch[4].i + scratch[6].i) * one_by_nfft;
                scratch_out[1].r = (scratch[5].r - scratch[7].i) * one_by_nfft;
                scratch_out[1].i = (scratch[5].i + scratch[7].r) * one_by_nfft;
                scratch_out[2].r = (scratch[4].r - scratch[6].r) * one_by_nfft;
                scratch_out[2].i = (scratch[4].i - scratch[6].i) * one_by_nfft;
                scratch_out[3].r = (scratch[5].r + scratch[7].i) * one_by_nfft;
                scratch_out[3].i = (scratch[5].i - scratch[7].r) * one_by_nfft;

                // Store the results
                dst[0] = scratch_out[0];
                dst[step * 1] = scratch_out[1];
                dst[step * 2] = scratch_out[2];
                dst[step * 3] = scratch_out[3];

                tw++;
                src++;
                dst++;
            } // m_count
        } // f_count
    } // last stage
}

static void ne10_fft_split_r2c_1d_float32 (ne10_fft_cpx_float32_t *dst,
        const ne10_fft_cpx_float32_t *src,
        ne10_fft_cpx_float32_t *twiddles,
        ne10_int32_t ncfft)
{
    ne10_int32_t k;
    ne10_fft_cpx_float32_t fpnk, fpk, f1k, f2k, tw, tdc;

    tdc.r = src[0].r;
    tdc.i = src[0].i;

    dst[0].r = tdc.r + tdc.i;
    dst[ncfft].r = tdc.r - tdc.i;
    dst[ncfft].i = dst[0].i = 0;

    for (k = 1; k <= ncfft / 2 ; ++k)
    {
        fpk    = src[k];
        fpnk.r =   src[ncfft - k].r;
        fpnk.i = - src[ncfft - k].i;

        f1k.r = fpk.r + fpnk.r;
        f1k.i = fpk.i + fpnk.i;

        f2k.r = fpk.r - fpnk.r;
        f2k.i = fpk.i - fpnk.i;

        tw.r = f2k.r * (twiddles[k - 1]).r - f2k.i * (twiddles[k - 1]).i;
        tw.i = f2k.r * (twiddles[k - 1]).i + f2k.i * (twiddles[k - 1]).r;

        dst[k].r = (f1k.r + tw.r) * 0.5f;
        dst[k].i = (f1k.i + tw.i) * 0.5f;
        dst[ncfft - k].r = (f1k.r - tw.r) * 0.5f;
        dst[ncfft - k].i = (tw.i - f1k.i) * 0.5f;
    }
}

static void ne10_fft_split_c2r_1d_float32 (ne10_fft_cpx_float32_t *dst,
        const ne10_fft_cpx_float32_t *src,
        ne10_fft_cpx_float32_t *twiddles,
        ne10_int32_t ncfft)
{

    ne10_int32_t k;
    ne10_fft_cpx_float32_t fk, fnkc, fek, fok, tmp;


    dst[0].r = (src[0].r + src[ncfft].r) * 0.5f;
    dst[0].i = (src[0].r - src[ncfft].r) * 0.5f;

    for (k = 1; k <= ncfft / 2; k++)
    {
        fk = src[k];
        fnkc.r = src[ncfft - k].r;
        fnkc.i = -src[ncfft - k].i;

        fek.r = fk.r + fnkc.r;
        fek.i = fk.i + fnkc.i;

        tmp.r = fk.r - fnkc.r;
        tmp.i = fk.i - fnkc.i;

        fok.r = tmp.r * twiddles[k - 1].r + tmp.i * twiddles[k - 1].i;
        fok.i = tmp.i * twiddles[k - 1].r - tmp.r * twiddles[k - 1].i;

        dst[k].r = (fek.r + fok.r) * 0.5f;
        dst[k].i = (fek.i + fok.i) * 0.5f;

        dst[ncfft - k].r = (fek.r - fok.r) * 0.5f;
        dst[ncfft - k].i = (fok.i - fek.i) * 0.5f;
    }
}

/**
 * @defgroup C2C_FFT_IFFT Floating & Fixed Point Complex-to-Complex FFT
 * @ingroup groupDSPs
 * @{
 *
 * \par
 * A Fast Fourier Transform (FFT) is an efficient method of computing the Discrete Fourier Transform (DFT), or its inverse.
 * The FFT is widely used for many applications in engineering, science and mathmatics.
 *
 * \par Variants
 * This set of functions implements a complex-to-complex 1D FFT/IFFT, for power-of-two input sizes. This includes floating
 * point (fft_c2c_1d_float32), and fixed point (fft_c2c_1d_int32, fft_c2c_1d_int16) variants.
 *
 * \par
 * Note: These functions operate out-of-place, using different buffers for input and output. Additionally, a temporary buffer is
 * used internally. The input and output buffers should be allocated by the user, and must be of a size greater than or equal to
 * (fftSize * sizeof (ne10_fft_cpx_float32_t)).
 *
 * \par Data format
 * The input and output buffers have the same simple format, interleaving the real and imaginary parts of contiguous complex elements:
 * <pre> {real[0], imag[0], real[1], imag[1], real[2], imag[2].... real[fftSize-2], imag[fftSize-2], real[fftSize-1], imag[fftSize-1]} </pre>
 *
 * \par Supported lengths
 * \par
 * Internally, the functions utilize a mixed radix 2/4 DIT algorithm, supporting input sizes of the form <code>2^N</code> (N > 0).
 * Some functions also support non-power-of-two sizes (using other radices), as indicated in the function descriptions.
 *
 * \par Example usage
 * A single precision floating point FFT/IFFT example code snippet follows.
 *
 * <pre>
 * #include "NE10.h"
 * ...
 * {
 *     fftSize = 1024; // 2^N, N > 0 (e.g. 1024 at N = 10)
 *     in = (ne10_fft_cpx_float32_t*) NE10_MALLOC (fftSize * sizeof (ne10_fft_cpx_float32_t));
 *     out = (ne10_fft_cpx_float32_t*) NE10_MALLOC (fftSize * sizeof (ne10_fft_cpx_float32_t));
 *     ne10_fft_cfg_float32_t cfg;
 *     ...
       cfg = ne10_fft_alloc_c2c_float32 (fftSize);
 *     ...
 *     // FFT
 *     ne10_fft_c2c_1d_float32_c (out, in, cfg, 0);
 *     ...
 *     // IFFT
 *     ne10_fft_c2c_1d_float32_c (out, in, cfg, 1);
 *     ...
 *     NE10_FREE (in);
 *     NE10_FREE (out);
 *     NE10_FREE (cfg);
 * }
 * </pre>
 *
 * Note:
 * \par
 * The 'ne10_fft_cfg_float32_t' variable 'cfg' is a pointer to a configuration structure generated by ne10_fft_alloc_c2c_float32(fftSize).
 * For different inputs of the same fftSize, the same configuration structure can (and, indeed, should) be used. A brief outline of the
 * core contents of this configuration structure follows:
 * - cfg->twiddles
 *   \n This is a pointer to a table of "twiddle factors" that are used in computing the FFT/IFFT.
 * - cfg->factors
 *   \n This is a buffer of "factors", which suggests to the core algorithm how the input can be broken down into smaller calculations.
 * - cfg->buffer
 *   \n This is a pointer to a temporary buffer used internally in calculations. This buffer is allocated when the configuration
 *   structure is set up, and is of size (fftSize * sizeof (ne10_fft_cpx_float32_t)).
 */

/**
 * @brief User-callable function to create a configuration structure for the C2C C FFT/IFFT.
 * @param[in]   nfft             length of FFT
 * @return      st               pointer to the FFT configuration memory, allocated with malloc.
 *
 * This function allocates and initialises an ne10_fft_cfg_float32_t configuration structure for the
 * C complex-to-complex FFT/IFFT. As part of this, it reserves a buffer used internally by the FFT
 * algorithm, factors the length of the FFT into simpler chunks, and generates a "twiddle
 * table" of coefficients used in the FFT "butterfly" calculations.
 */
ne10_fft_cfg_float32_t ne10_fft_alloc_c2c_float32_c (ne10_int32_t nfft)
{
    ne10_fft_cfg_float32_t st = NULL;
    ne10_uint32_t memneeded = sizeof (ne10_fft_state_float32_t)
                              + sizeof (ne10_int32_t) * (NE10_MAXFACTORS * 2) /* factors*/
                              + sizeof (ne10_fft_cpx_float32_t) * nfft        /* twiddle*/
                              + sizeof (ne10_fft_cpx_float32_t) * nfft        /* buffer*/
                              + NE10_FFT_BYTE_ALIGNMENT;     /* 64-bit alignment*/

    st = (ne10_fft_cfg_float32_t) NE10_MALLOC (memneeded);

    // Only backward FFT is scaled by default.
    st->is_forward_scaled = 0;
    st->is_backward_scaled = 1;

    if (st == NULL)
    {
        return st;
    }

    uintptr_t address = (uintptr_t) st + sizeof (ne10_fft_state_float32_t);
    NE10_BYTE_ALIGNMENT (address, NE10_FFT_BYTE_ALIGNMENT);
    st->factors = (ne10_int32_t*) address;
    st->twiddles = (ne10_fft_cpx_float32_t*) (st->factors + (NE10_MAXFACTORS * 2));
    st->buffer = st->twiddles + nfft;
    st->nfft = nfft;

    ne10_int32_t result = ne10_factor (nfft, st->factors, NE10_FACTOR_DEFAULT);
    if (result == NE10_ERR)
    {
        NE10_FREE (st);
        return st;
    }

    // Check if ALGORITHM FLAG is NE10_FFT_ALG_ANY.
    {
        ne10_int32_t stage_count    = st->factors[0];
        ne10_int32_t algorithm_flag = st->factors[2 * (stage_count + 1)];

        // Enable radix-8.
        if (algorithm_flag == NE10_FFT_ALG_ANY)
        {
            result = ne10_factor (st->nfft, st->factors, NE10_FACTOR_EIGHT);
            if (result == NE10_ERR)
            {
                PRINT_HIT;
                NE10_FREE (st);
                return st;
            }
        }
    }

    ne10_fft_generate_twiddles_float32 (st->twiddles, st->factors, nfft);

    return st;
}

/**
 * @brief Mixed radix-2/3/4/5 complex C FFT/IFFT of single precision floating point data.
 * @param[out]  *fout            pointer to the output buffer (out-of-place)
 * @param[in]   *fin             pointer to the input buffer (out-of-place)
 * @param[in]   cfg              pointer to the configuration struct
 * @param[in]   inverse_fft      whether this is an FFT or IFFT (0: FFT, 1: IFFT)
 * @return none.
 *
 * This function implements a mixed radix-2/3/4/5 complex C FFT/IFFT, supporting input lengths of the
 * form 2^N*3^M*5^K (N, M, K > 0). This is an out-of-place algorithm. For usage information, please
 * check test/test_suite_fft_float32.c
 */
void ne10_fft_c2c_1d_float32_c (ne10_fft_cpx_float32_t *fout,
                                ne10_fft_cpx_float32_t *fin,
                                ne10_fft_cfg_float32_t cfg,
                                ne10_int32_t inverse_fft)
{
    ne10_int32_t stage_count = cfg->factors[0];
    ne10_int32_t algorithm_flag = cfg->factors[2 * (stage_count + 1)];

    assert ((algorithm_flag == NE10_FFT_ALG_24)
            || (algorithm_flag == NE10_FFT_ALG_ANY));

    switch (algorithm_flag)
    {
    case NE10_FFT_ALG_24:
        if (inverse_fft)
        {
            ne10_mixed_radix_butterfly_inverse_float32_c (fout, fin, cfg->factors, cfg->twiddles, cfg->buffer);
        }
        else
        {
            ne10_mixed_radix_butterfly_float32_c (fout, fin, cfg->factors, cfg->twiddles, cfg->buffer);
        }
        break;
    case NE10_FFT_ALG_ANY:
        if (inverse_fft)
        {
            ne10_mixed_radix_generic_butterfly_inverse_float32_c (fout, fin,
                    cfg->factors, cfg->twiddles, cfg->buffer, cfg->is_backward_scaled);
        }
        else
        {
            ne10_mixed_radix_generic_butterfly_float32_c (fout, fin,
                    cfg->factors, cfg->twiddles, cfg->buffer, cfg->is_forward_scaled);
        }
        break;
    }
}

/**
 * @}
 */ //end of C2C_FFT_IFFT group

/**
 * @defgroup R2C_FFT_IFFT Floating & Fixed Point Real-to-Complex FFT
 * @ingroup groupDSPs
 * @{
 *
 * \par
 * In Ne10 library, there has been complex FFT. But in many practical applications, signals are often real only.
 * So that we implement the real to complex FFT and complex to real IFFT based on complex FFT.
 *
 * \par Function list
 * This set of functions implements r2c one-dimensional FFT/c2r IFFT with <code>2^N</code>(N>1) size. The function list is as follows:
 * -  fft_r2c_1d_float32
 * -  fft_c2r_1d_float32
 * -  fft_r2c_1d_int32
 * -  fft_c2r_1d_int32
 * -  fft_r2c_1d_int16
 * -  fft_c2r_1d_int16
 *
 * \par
 * Note: The functions operate on out-of-place buffer which use different buffer for input and output. We need a temp buffer for internal usage.
 * This buffer is allocated by the users and the size is (fftSize * sizeof (ne10_fft_cpx_float32_t)).
 *
 * \par The format of input and output:
 * - r2c FFT
 *   \n Input:{real[0], real[1],real[2],.... real[fftSize-2], real[fftSize-1]}. The length of input is fftSize real data.
 *   \n Output:{real[0], imag[0], real[1], imag[1], real[2], imag[2].... real[fftSize/2], imag[fftSize/2],}. The length of output should be fftSize complex data.
 *   For the reason that output[i] is the conjugate of output[fftSize-i], so that we give here output[0]~output[fftSize/2].
 * - c2r IFFT
 *   \n Input:{real[0], imag[0], real[1], imag[1], real[2], imag[2].... real[fftSize-1], imag[fftSize-1],} The length of input is fftSize complex data.
 *   \n Output:{real[0], real[1],real[2],.... real[fftSize-2], real[fftSize-1]}. The length of output is fftSize real data.
 *
 * \par Lengths supported by the transform:
 * \par
 * The r2c FFT is based on complex FFT, so that the FFT size supported is the length
 * <code>2^N</code> (N is 2, 3, 4, 5, 6, ......).
 *
 * \par Usage:
 * The basic usage of these functions is simple. We take float fft as an example and it looks like the code as follows.
 *
 * <pre>
 * #include "NE10.h"
 * ...
 * {
 *     fftSize = 2^N; //N is 2, 3, 4, 5, 6....
 *     in = (ne10_float32_t*) NE10_MALLOC (fftSize * sizeof (ne10_float32_t));
 *     out = (ne10_fft_cpx_float32_t*) NE10_MALLOC (fftSize * sizeof (ne10_fft_cpx_float32_t));
 *     ne10_fft_r2c_cfg_float32_t cfg;
 *     ...
       cfg = ne10_fft_alloc_r2c_float32 (fftSize);
 *     ...
 *     //FFT
 *     ne10_fft_r2c_1d_float32_c (out, in, cfg);
 *     ...
 *     //IFFT
 *     ne10_fft_c2r_1d_float32_c (in, out, cfg);
 *     ...
 *     NE10_FREE (cfg);
 *     NE10_FREE (in);
 *     NE10_FREE (out);
 * }
 * </pre>
 *
 * Note:
 * \par
 * ne10_fft_r2c_cfg_float32_t cfg is the pointer which points to the buffer storing the twiddles and factors. It's generated in ne10_fft_alloc_r2c_float32(fftSize). If the fftSize is same, you needn't generate it again.
 * - cfg->twiddles
 *   \n This is pointer to the twiddle factor table.
 * - cfg->super_twiddles
 *   \n This is pointer to the twiddle factor which for spliting complex and real.
 * - cfg->factors
 *   \n This is factors buffer: 0: stage number, 1: stride for the first stage, others: factors.
 *   \n For example, 128 could be split into 4x32, 4x8, 4x2, 2x1. The stage is 4, the stride of first stage is <code>128/2 = 64</code>. So that the factor buffer is[4, 64, 4, 32, 4, 8, 4, 2, 2, 1]
 * - cfg->buffer
 *   \n This is pointer to the temp buffer for FFT calculation. This buffer is allocated in init function and the size is (fftSize * sizeof (ne10_fft_cpx_float32_t)).
 *
 */

// For NE10_UNROLL_LEVEL > 0, please refer to NE10_rfft_float.c
#if (NE10_UNROLL_LEVEL == 0)

/**
 * @brief User-callable function to create a configuration structure for the R2C/C2R C FFT/IFFT.
 * @param[in]   nfft             length of FFT
 * @return      st               pointer to the FFT configuration memory, allocated with malloc.
 *
 * This function allocates and initialises an ne10_fft_r2c_cfg_float32_t configuration structure for the
 * C real-to-complex and complex-to-real FFT/IFFT. As part of this, it reserves a buffer used internally
 * by the FFT algorithm, factors the length of the FFT into simpler chunks, and generates a "twiddle
 * table" of coefficients used in the FFT "butterfly" calculations.
 */
ne10_fft_r2c_cfg_float32_t ne10_fft_alloc_r2c_float32 (ne10_int32_t nfft)
{
    ne10_fft_r2c_cfg_float32_t st = NULL;
    ne10_int32_t ncfft = nfft >> 1;

    ne10_uint32_t memneeded = sizeof (ne10_fft_r2c_state_float32_t)
                              + sizeof (ne10_int32_t) * (NE10_MAXFACTORS * 2) /* factors */
                              + sizeof (ne10_fft_cpx_float32_t) * ncfft       /* twiddle*/
                              + sizeof (ne10_fft_cpx_float32_t) * (ncfft / 2) /* super twiddles*/
                              + sizeof (ne10_fft_cpx_float32_t) * nfft        /* buffer*/
                              + NE10_FFT_BYTE_ALIGNMENT;     /* 64-bit alignment*/

    st = (ne10_fft_r2c_cfg_float32_t) NE10_MALLOC (memneeded);

    if (st)
    {
        uintptr_t address = (uintptr_t) st + sizeof (ne10_fft_r2c_state_float32_t);
        NE10_BYTE_ALIGNMENT (address, NE10_FFT_BYTE_ALIGNMENT);
        st->factors = (ne10_int32_t*) address;
        st->twiddles = (ne10_fft_cpx_float32_t*) (st->factors + (NE10_MAXFACTORS * 2));
        st->super_twiddles = st->twiddles + ncfft;
        st->buffer = st->super_twiddles + (ncfft / 2);
        st->ncfft = ncfft;

        ne10_int32_t result = ne10_factor (ncfft, st->factors, NE10_FACTOR_DEFAULT);
        if (result == NE10_ERR)
        {
            NE10_FREE (st);
            return st;
        }

        ne10_int32_t i, j;
        ne10_int32_t *factors = st->factors;
        ne10_fft_cpx_float32_t *twiddles = st->twiddles;
        ne10_fft_cpx_float32_t *tw;
        ne10_int32_t stage_count = factors[0];
        ne10_int32_t fstride1 = factors[1];
        ne10_int32_t fstride2 = fstride1 * 2;
        ne10_int32_t fstride3 = fstride1 * 3;
        ne10_int32_t m;

        const ne10_float32_t pi = NE10_PI;
        ne10_float32_t phase1;
        ne10_float32_t phase2;
        ne10_float32_t phase3;

        for (i = stage_count - 1; i > 0; i--)
        {
            fstride1 >>= 2;
            fstride2 >>= 2;
            fstride3 >>= 2;
            m = factors[2 * i + 1];
            tw = twiddles;
            for (j = 0; j < m; j++)
            {
                phase1 = -2 * pi * fstride1 * j / ncfft;
                phase2 = -2 * pi * fstride2 * j / ncfft;
                phase3 = -2 * pi * fstride3 * j / ncfft;
                tw->r = (ne10_float32_t) cos (phase1);
                tw->i = (ne10_float32_t) sin (phase1);
                (tw + m)->r = (ne10_float32_t) cos (phase2);
                (tw + m)->i = (ne10_float32_t) sin (phase2);
                (tw + m * 2)->r = (ne10_float32_t) cos (phase3);
                (tw + m * 2)->i = (ne10_float32_t) sin (phase3);
                tw++;
            }
            twiddles += m * 3;
        }

        tw = st->super_twiddles;
        for (i = 0; i < ncfft / 2; i++)
        {
            phase1 = -pi * ( (ne10_float32_t) (i + 1) / ncfft + 0.5f);
            tw->r = (ne10_float32_t) cos (phase1);
            tw->i = (ne10_float32_t) sin (phase1);
            tw++;
        }

    }
    return st;
}

/**
 * @brief Mixed radix-2/4 real-to-complex C FFT of single precision floating point data.
 * @param[out]  *fout            point to the output buffer
 * @param[in]   *fin             point to the input buffer
 * @param[in]   cfg              point to the config struct
 * @return none.
 *
 * The function implements a mixed radix-2/4 FFT (real to complex). The length of 2^N(N is 3, 4, 5, 6 ....etc) is supported.
 * Otherwise, we alloc a temp buffer(the size is same as input buffer) for storing intermedia.
 * For the usage of this function, please check test/test_suite_fft_float32.c
 */
void ne10_fft_r2c_1d_float32_c (ne10_fft_cpx_float32_t *fout,
                                ne10_float32_t *fin,
                                ne10_fft_r2c_cfg_float32_t cfg)
{
    ne10_fft_cpx_float32_t * tmpbuf = cfg->buffer;

    ne10_mixed_radix_butterfly_float32_c (tmpbuf, (ne10_fft_cpx_float32_t*) fin, cfg->factors, cfg->twiddles, fout);
    ne10_fft_split_r2c_1d_float32 (fout, tmpbuf, cfg->super_twiddles, cfg->ncfft);
}

/**
 * @brief Mixed radix-2/4 complex-to-real C IFFT of single precision floating point data.
 * @param[out]  *fout            point to the output buffer
 * @param[in]   *fin             point to the input buffer
 * @param[in]   cfg              point to the config struct
 * @return none.
 *
 * The function implements a mixed radix-2/4 FFT (complex to real). The length of 2^N(N is 3, 4, 5, 6 ....etc) is supported.
 * Otherwise, we alloc a temp buffer(the size is same as input buffer) for storing intermedia.
 * For the usage of this function, please check test/test_suite_fft_float32.c
 */
void ne10_fft_c2r_1d_float32_c (ne10_float32_t *fout,
                                ne10_fft_cpx_float32_t *fin,
                                ne10_fft_r2c_cfg_float32_t cfg)
{
    ne10_fft_cpx_float32_t * tmpbuf1 = cfg->buffer;
    ne10_fft_cpx_float32_t * tmpbuf2 = cfg->buffer + cfg->ncfft;

    ne10_fft_split_c2r_1d_float32 (tmpbuf1, fin, cfg->super_twiddles, cfg->ncfft);
    ne10_mixed_radix_butterfly_inverse_float32_c ( (ne10_fft_cpx_float32_t*) fout, tmpbuf1, cfg->factors, cfg->twiddles, tmpbuf2);
}

/**
 * @} end of R2C_FFT_IFFT group
 */
#endif // NE10_UNROLL_LEVEL
