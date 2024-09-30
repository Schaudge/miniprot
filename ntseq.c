#include <string.h>
#include <stdio.h>
#include <zlib.h>
#include <math.h>
#include "mppriv.h"
#include "kalloc.h"
#include "kseq.h"
KSEQ_INIT2(, gzFile, gzread)

#include "khashl-km.h"
KHASHL_MAP_INIT(KH_LOCAL, mp_strmap_t, mp_strmap, kh_cstr_t, int32_t, kh_hash_str, kh_eq_str)

static void mp_ntseq_merge_name(mp_ntdb_t *nt)
{
	int32_t i;
	char *p;
	nt->l_name = 0;
	for (i = 0; i < nt->n_ctg; ++i)
		nt->l_name += strlen(nt->ctg[i].name) + 1;
	nt->name = Kmalloc(0, char, nt->l_name);
	for (i = 0, p = nt->name; i < nt->n_ctg; ++i) {
		memcpy(p, nt->ctg[i].name, strlen(nt->ctg[i].name) + 1);
		free(nt->ctg[i].name);
		nt->ctg[i].name = p;
		p += strlen(nt->ctg[i].name) + 1;
	}
}

mp_ntdb_t *mp_ntseq_read(const char *fn)
{
	gzFile fp;
	kseq_t *ks;
	mp_ntdb_t *d = 0;
	int64_t off = 0;

	fp = gzopen(fn, "r");
	if (fp == 0) return 0;
	ks = kseq_init(fp);

	d = Kcalloc(0, mp_ntdb_t, 1);
	while (kseq_read(ks) >= 0) {
		int64_t i, ltmp;
		mp_ctg_t *c;

		// update mp_ntdb_t::ctg
		if (d->n_ctg == d->m_ctg) {
			d->m_ctg += (d->m_ctg>>1) + 16;
			d->ctg = Krealloc(0, mp_ctg_t, d->ctg, d->m_ctg);
		}
		c = &d->ctg[d->n_ctg++];
		c->name = mp_strdup(ks->name.s);
		c->off = off;
		c->len = ks->seq.l;

		// update mp_ntdb_t::seq
		ltmp = (d->l_seq + ks->seq.l + 1) >> 1 << 1;
		if (ltmp > d->m_seq) {
			int64_t oldm = d->m_seq;
			d->m_seq = ltmp;
			kroundup64(d->m_seq);
			d->seq = Krealloc(0, uint8_t, d->seq, d->m_seq >> 1);
			memset(&d->seq[oldm>>1], 0, (d->m_seq - oldm) >> 1);
		}
		for (i = 0; i < ks->seq.l; ++i, ++off) {
			uint8_t b = ns_tab_nt4[(uint8_t)ks->seq.s[i]];
			d->seq[off >> 1] |= b << (off&1) * 4;
		}
		d->l_seq += ks->seq.l;
	}

	kseq_destroy(ks);
	gzclose(fp);
	mp_ntseq_merge_name(d);
	if (mp_verbose >= 3)
		fprintf(stderr, "[M::%s@%.3f*%.2f] read %ld bases in %d contigs\n", __func__, mp_realtime(), mp_percent_cpu(), (long)d->l_seq, d->n_ctg);
	return d;
}

void mp_ntseq_destroy(mp_ntdb_t *db)
{
	mp_strmap_t *h = (mp_strmap_t*)db->h;
	if (db == 0) return;
	if (h) mp_strmap_destroy(h);
	if (db->sc) free(db->sc);
	free(db->ctg); free(db->seq); free(db->name);
	free(db);
}

int64_t mp_ntseq_get(const mp_ntdb_t *db, int32_t cid, int64_t st, int64_t en, int32_t rev, uint8_t *seq)
{
	int64_t i, s, e, k;
	if (cid >= db->n_ctg || cid < 0) return -1;
	if (en < 0 || en > db->ctg[cid].len) en = db->ctg[cid].len;
	s = db->ctg[cid].off + st;
	e = db->ctg[cid].off + en;
	if (!rev) {
		for (i = s, k = 0; i < e; ++i)
			seq[k++] = db->seq[i>>1] >> ((i&1) * 4) & 0xf;
	} else {
		for (i = e - 1, k = 0; i >= s; --i) {
			uint8_t c = db->seq[i>>1] >> ((i&1) * 4) & 0xf;
			seq[k++] = c >= 4? c : 3 - c;
		}
	}
	return k;
}

int64_t mp_ntseq_get_by_v(const mp_ntdb_t *nt, int32_t vid, int64_t st, int64_t en, uint8_t *seq)
{
	int64_t ctg_len = nt->ctg[vid>>1].len;
	if (st < 0 || en < 0 || st >= ctg_len) return -1;
	en = en <= ctg_len? en : ctg_len;
	return mp_ntseq_get(nt, vid>>1, vid&1? ctg_len - en : st, vid&1? ctg_len - st : en, vid&1, seq);
}

void mp_ntseq_dump(FILE *fp, const mp_ntdb_t *nt)
{
	int32_t i, x[2];
	int64_t l = (nt->l_seq + 1) >> 1;
	x[0] = nt->n_ctg, x[1] = nt->l_name;
	fwrite(x, 4, 2, fp);
	fwrite(&nt->l_seq, 8, 1, fp);
	for (i = 0; i < nt->n_ctg; ++i)
		fwrite(&nt->ctg[i].len, 8, 1, fp);
	fwrite(nt->seq, 1, l, fp);
	fwrite(nt->name, 1, nt->l_name, fp);
}

mp_ntdb_t *mp_ntseq_restore(FILE *fp)
{
	int32_t i, x[2];
	int64_t off = 0, l;
	mp_ntdb_t *nt;
	char *p;

	nt = Kcalloc(0, mp_ntdb_t, 1);
	fread(x, 4, 2, fp);
	fread(&nt->l_seq, 8, 1, fp);
	nt->n_ctg = nt->m_ctg = x[0];
	nt->l_name = x[1];
	nt->m_seq = nt->l_seq;
	l = (nt->l_seq + 1) >> 1;
	nt->ctg = Kcalloc(0, mp_ctg_t, nt->n_ctg);
	for (i = 0; i < nt->n_ctg; ++i) {
		fread(&nt->ctg[i].len, 8, 1, fp);
		nt->ctg[i].off = off;
		off += nt->ctg[i].len;
	}
	nt->seq = Kmalloc(0, uint8_t, l);
	nt->name = Kmalloc(0, char, nt->l_name);
	fread(nt->seq, 1, l, fp);
	fread(nt->name, 1, nt->l_name, fp);
	for (i = 0, p = nt->name; i < nt->n_ctg; ++i) {
		nt->ctg[i].name = p;
		p += strlen(p) + 1;
	}
	return nt;
}

void mp_ntseq_index_name(mp_ntdb_t *nt)
{
	mp_strmap_t *h;
	int32_t i;
	h = mp_strmap_init2(0);
	for (i = 0; i < nt->n_ctg; ++i) {
		int absent;
		khint_t k;
		k = mp_strmap_put(h, nt->ctg[i].name, &absent);
		if (!absent) {
			fprintf(stderr, "ERROR: duplicated contig name!\n");
			continue;
		}
		kh_val(h, k) = i;
	}
	nt->h = (void*)h;
}

int32_t mp_ntseq_name2id(const mp_ntdb_t *nt, const char *name)
{
	const mp_strmap_t *h = (const mp_strmap_t*)nt->h;
	khint_t k;
	if (h == 0) return -1;
	k = mp_strmap_get(h, name);
	return k == kh_end(h)? -1 : kh_val(h, k);
}

int32_t mp_ntseq_read_spsc(mp_ntdb_t *nt, const char *fn, int32_t c0)
{
	gzFile fp;
	kstring_t str = {0,0,0};
	kstream_t *ks;
	int dret;
	int64_t n_read = 0;

	if (c0 < 0 || c0 > 100) return -1; // too large
	fp = fn && strcmp(fn, "-") != 0? gzopen(fn, "rb") : gzdopen(0, "rb");
	if (fp == 0) return -1;
	nt->sc = Kcalloc(0, mp_spsc_t, nt->n_ctg * 2);
	ks = ks_init(fp);
	while (ks_getuntil(ks, KS_SEP_LINE, &str, &dret) >= 0) {
		mp_spsc_t *s;
		char *p, *q, *name = 0;
		int32_t i, type = -1, strand = 0, cid = -1, score;
		int64_t pos = -1;
		double score_ori = -1.0;
		for (i = 0, p = q = str.s;; ++p) {
			if (*p == '\t' || *p == 0) {
				int c = *p;
				if (i == 0) {
					name = q;
				} else if (i == 1) {
					pos = atol(q);
				} else if (i == 2) {
					strand = *q == '+'? 1 : '-'? -1 : 0;
				} else if (i == 3) {
					type = *q == 'D'? 0 : *q == 'A'? 1 : -1;
				} else if (i == 4) {
					score_ori = atof(q);
					break;
				}
				if (c == 0) break;
				q = p + 1, ++i;
			}
		}
		if (i < 4) continue; // not enough fields
		if (score_ori <= 0.0) continue;
		score = (int32_t)(2. * log2(score_ori) + .499) + c0;
		if (score < 0) continue;
		if (score > 127) score = 127;
		cid = mp_ntseq_name2id(nt, name);
		if (cid < 0 || type < 0 || strand == 0 || pos < 0) continue; // FIXME: give a warning!
		s = &nt->sc[cid << 1 | (strand > 0? 0 : 1)];
		Kgrow(0, uint64_t, s->a, s->n, s->m);
		s->a[s->n++] = (uint64_t)pos<<9 | type<<8 | score;
		++n_read;
	}
	ks_destroy(ks);
	gzclose(fp);
	// TODO: SORT!!!
	if (mp_verbose >= 3)
		fprintf(stderr, "[M::%s] read %ld splice scores\n", __func__, (long)n_read);
	return 0;
}
