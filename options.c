#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "miniprot.h"
#include "nasw.h"

#define MP_MAX_INTRON_COEF 3.6 // max_intron = sqrt(genome_size) * coef

void mp_idxopt_init(mp_idxopt_t *io)
{
	memset(io, 0, sizeof(*io));
	io->trans_code = MP_CODON_STD;
	io->bbit = 8;
	io->min_aa_len = 30;
	#if MP_BITS_PER_AA == 4
	io->kmer = 6;
	#else
	io->kmer = 8;
	#endif
	io->mod_bit = 1;
}

void mp_mapopt_set_fs(mp_mapopt_t *mo, int32_t fs)
{
	assert(fs >= INT8_MIN && fs <= INT8_MAX);
	mo->fs = fs;
	ns_set_stop_sc(mo->asize, mo->mat, mo->fs);
}

void mp_mapopt_set_max_intron(mp_mapopt_t *mo, int64_t gsize)
{
	int64_t x;
	x = (int64_t)(sqrt((double)gsize) * MP_MAX_INTRON_COEF + 1.);
	if (x < mo->min_max_intron) x = mo->min_max_intron;
	if (x > mo->max_max_intron) x = mo->max_max_intron;
	mo->bw = mo->max_intron = x;
	if (mp_verbose >= 3)
		fprintf(stderr, "[M::%s] set max intron size to %d\n", __func__, mo->max_intron);
}

void mp_mapopt_init(mp_mapopt_t *mo)
{
	memset(mo, 0, sizeof(*mo));
	mo->flag = 0;
	mo->mini_batch_size = 2000000;
	mo->max_occ = 20000;
	mo->max_gap = 1000;
	mo->max_intron = 200000;
	mo->min_max_intron = 10000, mo->max_max_intron = 300000;
	mo->bw = mo->max_intron;
	mo->min_chn_cnt = 3;
	mo->max_chn_max_skip = 25;
	mo->max_chn_iter = 1000000;
	mo->min_chn_sc = 0;
	mo->chn_coef_log = 0.75f;
	mo->max_ext = 10000;
	mo->max_ava = 1000;
	mo->mask_level = 0.5f;
	mo->mask_len = INT32_MAX;
	mo->pri_ratio = 0.7f;
	mo->best_n = 30;
	mo->out_n = 1000;
	mo->out_sim = 0.99f;
	mo->out_cov = 0.1f;
	#if MP_BITS_PER_AA == 4
	mo->kmer2 = 5;
	#else
	mo->kmer2 = 7;
	#endif

	mo->go = 11, mo->ge = 1;
	mo->io = 29;
	mo->fs = 23;
	mo->io_end = 19;
	mo->ie_coef = .5f;
	mo->sp_model = NS_S_GENERIC;
	mo->sp_null_bonus = -7;
	mo->sp_scale = 1.0f;
	mo->end_bonus = 5;
	mo->xdrop = 100;
	mo->asize = 22;
	memcpy(mo->mat, ns_mat_blosum62, 484);
	ns_set_stop_sc(mo->asize, mo->mat, mo->fs);

	mo->gff_delim = -1;
	mo->gff_prefix = "MP";
	mo->max_intron_flank = 200;
}

int32_t mp_mapopt_check(const mp_mapopt_t *mo)
{
	if (mo->sp_model < 0 || mo->sp_model > 2) {
		fprintf(stderr, "[ERROR]\033[1;31m option -j should be between 0 and 2\033[0m\n");
		return -1;
	}
	return 0;
}
