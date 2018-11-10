/* Golay 3-bit error correction */
extern long encoding_table[4096];
extern long decoding_table[2048];

void init_golay(void);
long get_syndrome(long pattern);

#define ENCODE(x)	encoding_table[x]
#define DECODE(x)	((x) ^ decoding_table[get_syndrome(x)])

#define TDECODE(x,off)	(x)[0]

#define DATAMASK	0x000fff
#define CODEMASK	0x7fffff

#define DATABITS	12
#define CODEBITS	23
#define ERRORBITS	3
