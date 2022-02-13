#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "pdp6common.h"

#define RELOC   ((word)01000000000000)
#define NRELOC  ((word)02000000000000)
#define LRELOC  ((word)04000000000000)
#define LNRELOC ((word)010000000000000)
#define FOO     ((word)020000000000000)

#define W 0777777777777

#include "data.inc"

typedef unsigned char uchar;
typedef unsigned int uint;
#define nil NULL

#define DTLEN (01102*0200)

word dtbuf[DTLEN];
word filbuf[DTLEN];
word *dir;

#define LDB(p, s, w) ((w)>>(p) & (1<<(s))-1)
#define XLDB(ppss, w) LDB((ppss)>>6 & 077, (ppss)&077, w)
#define MASK(p, s) ((1<<(s))-1 << (p))
#define DPB(b, p, s, w) ((w)&~MASK(p,s) | (b)<<(p) & MASK(p,s))
#define XDPB(b, ppss, w) DPB(b, (ppss)>>6 & 077, (ppss)&077, w)

word tmploader[0200];
word tmpmacdmp[0400];

void
writesimh(FILE *f, word w)
{
	uchar c[8];
	uint l, r;

	r = LDB(0, 18, w);
	l = LDB(18, 18, w);
	c[0] = LDB(0, 8, l);
	c[1] = LDB(8, 8, l);
	c[2] = LDB(16, 8, l);
	c[3] = LDB(24, 8, l);
	c[4] = LDB(0, 8, r);
	c[5] = LDB(8, 8, r);
	c[6] = LDB(16, 8, r);
	c[7] = LDB(24, 8, r);
	fwrite(c, 1, 8, f);
}

word
readsimh(FILE *f)
{
	uchar c[8];
	hword w[2];
	if(fread(c, 1, 8, f) != 8)
		return ~0;
	w[0] = c[3]<<24 | c[2]<<16 | c[1]<<8 | c[0];
	w[1] = c[7]<<24 | c[6]<<16 | c[5]<<8 | c[4];
	return ((word)w[0]<<18 | w[1]) & 0777777777777;
}

enum Type
{
	Ascii,
	Dump,
	Sblk,
	Reloc
};

typedef struct File File;
struct File
{
	char fn1[7];
	char fn2[7];
	int mode;
};
File files[23];
#define NBLK (0121*7)
int blockinfo[NBLK];

/* Load directory block into C structure */
void
loaddir(void)
{
	int i, j;
	char tmp[10], *s, *t;
	word *name, *m1, *m2;
	word w;

	name = dir;
	m1 = &dir[056];
	m2 = &m1[027];
	for(i = 0; i < 23; i++){
		if(name[i*2] == 0 && name[i*2+1] == 0)
			continue;
		unsixbit(name[i*2], tmp);
		s = files[i].fn1;
		t = tmp;
		while(*t && *t != ' ')
			*s++ = *t++;
		*s++ = '\0';
		unsixbit(name[i*2+1], tmp);
		s = files[i].fn2;
		t = tmp;
		while(*t && *t != ' ')
			*s++ = *t++;
		*s++ = '\0';

		files[i].mode = (m1[i]&1)<<1 | m2[i]&1;
	}

	for(i = 0; i < 0121; i++){
		w = m1[i]>>1;
		for(j = 6; j >= 0; j--){
			blockinfo[i*7+j] = w & 037;
			w >>= 5;
		}
	}
}

/* Build directory block from C structure */
void
builddir(void)
{
	int i, j;
	word *name, *m1, *m2;

	memset(dir, 0, 0200*sizeof(word));

	name = dir;
	m1 = &dir[056];
	m2 = &m1[027];

	for(i = 0; i < 23; i++){
		if(files[i].fn1[0] == 0 || files[i].fn2[0] == 0)
			continue;
		name[i*2+0] = sixbit(files[i].fn1);
		name[i*2+1] = sixbit(files[i].fn2);
		m1[i] |= files[i].mode>>1 & 1;
		m2[i] |= files[i].mode & 1;
	}

	for(i = 0; i < 0121; i++)
		for(j = 0; j < 7; j++)
			m1[i] = DPB((word)blockinfo[i*7+j], 5*(6-j)+1, 5, m1[i]);

	dir[0177] = 0777777777777;

//	for(i = 0; i < 0200; i++)
//		fprintf(stderr, "%o %012lo\n", i, dir[i]);
}

void
initdir(void)
{
	memset(files, 0, sizeof(files));
	memset(blockinfo, 0, sizeof(blockinfo));
	blockinfo[1 -1] = 036;
	blockinfo[2 -1] = 036;
	blockinfo[3 -1] = 036;
	blockinfo[4 -1] = 036;
	blockinfo[5 -1] = 036;
	blockinfo[6 -1] = 036;
	blockinfo[7 -1] = 036;

	blockinfo[0100 -1] = 033;
}

void
printdir(void)
{
	int i;
	for(i = 0; i < NBLK; i++)
		fprintf(stderr, "%o: %o\n", i+1, blockinfo[i]);
}

/* Add bootloader code and MACDMP */
void
bootcode(void)
{
	assert(blockinfo[075 -1] == 0);
	blockinfo[075 -1] = 036;
	memcpy(&dtbuf[075*0200], tmploader, 0200*sizeof(word));

	assert(blockinfo[076 -1] == 0);
	blockinfo[076 -1] = 036;
	assert(blockinfo[077 -1] == 0);
	blockinfo[077 -1] = 036;
	memcpy(&dtbuf[076*0200], tmpmacdmp, 0400*sizeof(word));
}


int filenum;
int nwords;
int curblk;

void
initfile(int i)
{
	filenum = i;
	nwords = 0;
	curblk = -1;
}

void
addword(word w)
{
	/* no file selected */
	if(filenum == 0) return;

	/* last block was filled */
	if(nwords >= 0200)
		curblk = -1;

	/* need new block */
	if(curblk < 0){
		for(curblk = 0; curblk < NBLK; curblk++)
			if(blockinfo[curblk] == 0)
				goto found;
		fprintf(stderr, "no free block\n");
		filenum = -1;
		return;
found:
		blockinfo[curblk] = filenum;
		nwords = 0;
	}

//	fprintf(stderr, "%o/%o %012lo\n", curblk+1, nwords, w);
	dtbuf[(curblk+1)*0200 + nwords++] = w;
}

void
addfile(const char *filename, int mode, const char *fn1, const char *fn2)
{
	int i;
	word w;
	FILE *f;

	f = fopen(filename, "rb");
	if(f == nil){
		fprintf(stderr, "can't open %s\n", filename);
		return;
	}

	for(i = 0; i < 23; i++)
		if(files[i].fn1[0] == 0 && files[i].fn2[0] == 0)
			goto found;
	fprintf(stderr, "Directory full\n");
	return;
found:
	files[i].mode = mode;
	memset(files[i].fn1, 0, 7);
	strncpy(files[i].fn1, fn1, 6);
	memset(files[i].fn2, 0, 7);
	strncpy(files[i].fn2, fn2, 6);

	initfile(i+1);
	while(w = readwits(f), w != ~0)
		addword(w);

	fclose(f);
}

void
putblk(FILE *fp, word *blk)
{
	int i;
	for(i = 0; i < 0200; i++)
		writewits(blk[i], fp);
}

int
findfile(word n1, word n2)
{
	int i;
	for(i = 0; i < 23; i++)
		if(dir[i*2] == n1 && dir[i*2+1] == n2)
			return i+1;
	return 0;
}

void dumpfilefwd(FILE *fp, int n);
void dumpfilerev(FILE *fp, int n);

void
dumpfilerev(FILE *fp, int n)
{
	int i;
	for(i = NBLK-1; i >= 0; i--)
		if(blockinfo[i] == n)
			putblk(fp, &dtbuf[(i+1)*0200]);
	n = findfile(0, n);
	if(n)
		dumpfilefwd(fp, n);
}

void
dumpfilefwd(FILE *fp, int n)
{
	int i;
	for(i = 0; i < NBLK; i++)
		if(blockinfo[i] == n)
			putblk(fp, &dtbuf[(i+1)*0200]);
	n = findfile(0, n);
	if(n)
		dumpfilerev(fp, n);
}

void
dumpfile(int n)
{
	char fname[100];
	FILE *fp;
	int i;


	sprintf(fname, "dump/%s.%s", files[n-1].fn1, files[n-1].fn2);
//	sprintf(fname, "%s.%s", files[n-1].fn1, files[n-1].fn2);
	fp = fopen(fname, "wb");
	if(fp == nil){
		fprintf(stderr, "error: couldn't open '%s'\n", fname);
		return;
	}

	dumpfilefwd(fp, n);
	writewits(~0, fp);
	fclose(fp);
}

void
loadtape(FILE *f)
{
	int n;
	word w;

	n = 0;
	while(w = readsimh(stdin), w != ~0 && n < DTLEN)
		dtbuf[n++] = w;

	loaddir();
}

int
writetape(FILE *f)
{
	int n;

	builddir();

	int nfree = 0;
	for(curblk = 0; curblk < NBLK; curblk++)
		if(blockinfo[curblk] == 0)
			nfree++;
	for(n = 0; n < DTLEN; n++)
		writesimh(f, dtbuf[n]);
	return nfree;
}

word
reloc(word w, word r)
{
	if(w & RELOC)	return (w + r) & W;
	if(w & NRELOC)	return (w - r) & W;
	if(w & LRELOC)	return (w + (r<<18)) & W;
	if(w & LNRELOC)	return (w - (r<<18)) & W;
	return w;
}

void
relocate(word r)
{
	int i;
	int opntp;
	for(i = 0; i < 0400; i++)
		if(tmpmacdmp[i] & FOO)
			opntp = i;
	opntp += 037400 + r;
	printf("OPNTP: %o\n", opntp);
	for(i = 0; i < 0200; i++)
		if(tmploader[i] & FOO)
			tmploader[i] = fw(left(tmploader[i]), opntp-1);
		else
			tmploader[i] = reloc(tmploader[i], r);
	for(i = 0; i < 0400; i++)
		tmpmacdmp[i] = reloc(tmpmacdmp[i], r);
}

void
extape(void)
{
	int i;

	loadtape(stdin);

	for(i = 0; i < 23; i++)
		if(files[i].fn1[0] != '\0'){
			printf("<%s %s> %d\n", files[i].fn1, files[i].fn2, files[i].mode);
			dumpfile(i+1);
		}
}

void
cmd(void)
{
	char line[512], *p;
	char path[512], fn1[32], fn2[32];
	int n;

	initdir();
	int type = Dump;
	// cmds:
	//   i		init dir (implicit after w)
	//   b		boot tape
	//   ta		ascii
	//   td		dump
	//   ts		sblk
	//   tr		reloc
	//   f path [[FN1] FN2]
	//   w filename (init new tape)
	while(p = fgets(line, 512, stdin)){
		switch(*p++){
		case '%':
			// comment
			break;
		case 'i':
			initdir();
			break;
		case 'b': {
			int sz = 16;
			word rel;
			char disp = 'x';
			n = sscanf(p, "%d %c", &sz, &disp);
			memcpy(tmploader, loader, 0200*sizeof(word));
			// this is a bit ugly i think
			if(sz == 256){
				if(disp == 'd')
					memcpy(tmpmacdmp, macdmp_u256d, 0400*sizeof(word));
				else
					memcpy(tmpmacdmp, macdmp_u256, 0400*sizeof(word));
				rel = 0740000;
			}else if(sz == 64){
				memcpy(tmpmacdmp, macdmp, 0400*sizeof(word));
				rel = 0140000;
			}else if(sz == 16){
				memcpy(tmpmacdmp, macdmp, 0400*sizeof(word));
				rel = 0;
			}
			relocate(rel);
			bootcode();
			break;
		}
		case 't':
			switch(*p++){
			case 'a': type = Ascii; break;
			case 'd': type = Dump; break;
			case 's': type = Sblk; break;
			case 'r': type = Reloc; break;
			default: fprintf(stderr, "unknown type\n");
			}
			break;
		case 'f':
			// lazy, need better path parsing
			n = sscanf(p, "%s %s %s", path, fn1, fn2);
			switch(n){
			case 1: {
				// TODO; extract fn1 and fn2 from filename
				char *s = strrchr(path, '/');
				if(s == nil) s = path;
				strncpy(fn1, path, sizeof(fn1));
				char *d = strrchr(fn1, '.');
				if(d == nil){
					fprintf(stderr, "invalid filename\n");
					break;
				}
				*d++ = '\0';
				addfile(path, type, fn1, d);
				break;
			}
			case 2:
				addfile(path, type, "@", fn1);
				break;
			case 3:
				addfile(path, type, fn1, fn2);
				break;
			default:
				fprintf(stderr, "invalid filespec\n");
			}
			break;
		case 'w':
			// lazy again
			n = sscanf(p, "%s", path);
			if(n != 1){
				fprintf(stderr, "invalid filename\n");
				break;
			}
			FILE *f = fopen(path, "wb");
			if(f == nil){
				fprintf(stderr, "can't open file\n");
				break;
			}
			n = writetape(f);
			fprintf(stderr, "%d/%d blocks free\n", n, NBLK);
			fclose(f);
			initdir();
			break;
		default:
			fprintf(stderr, "unknown command\n");
		}
	}
}

int
main()
{
	dir = &dtbuf[0100*0200];

	// TODO: option?
//	relocate(0740000);	// moby

	cmd();
	// TODO: option?
//	extape();

	return 0;
}
