
#ifndef LPC_H
#define LPC_H

#include <stdint.h>

#define SEQUENCE_TEXT_WIDTH 64

enum TLA_INFO {
        TLA_INFO_FIRST_SEQUENCE,
        TLA_INFO_LAST_SEQUENCE,
        TLA_INFO_DISPLAY_ATTRIBUTE,
        TLA_INFO_3,
        TLA_INFO_MNEMONICS_WIDTH=5
};

const char *busstates[] = {
        "BUS_STATE_START",
        "BUS_STATE_IDLE",
        "BUS_STATE_IDSEL",
        "BUS_STATE_CT_DIR",
        "BUS_STATE_ADDR",
        "BUS_STATE_TAR",
        "BUS_STATE_SIZE",
        "BUS_STATE_SYNC",
        "BUS_STATE_DATA",
        "BUS_STATE_MSIZE",
        "BUS_STATE_ABORT",
};

typedef enum {
        BUS_STATE_START,
        BUS_STATE_IDLE,
        BUS_STATE_IDSEL,
        BUS_STATE_CT_DIR,
        BUS_STATE_ADDR,
        BUS_STATE_TAR,
        BUS_STATE_SIZE,
        BUS_STATE_SYNC,
        BUS_STATE_DATA,
        BUS_STATE_MSIZE,
        BUS_STATE_ABORT,
} bus_state_t;

typedef enum {
        LPC_TARGET=0,
        LPC_GRANT0=2,
        LPC_GRANT1=3,
        LPC_FW_READ=0xd,
        LPC_FW_WRITE=0xe,
        LPC_ABORT=0xf
} lpc_start_t;

struct lactx;
struct businfo {
        const char *name;
	int val4;
	int val8;
	int valc;
	int val10;
	uint16_t val14;
        uint16_t groupcount;
	void *val18;
	int val1c;
};

struct modeinfo {
	char *name;
	char **options;
	int val1;
	int val2;
};

struct stringmodevalues {
        char *name;
        int val;
};

struct stringmodename {
        int entries;
        char *name;
        struct stringmodevalues *values;
        void *unknown;
};

struct group_value {
        int value;
        int mask;
};

struct sequence {
	struct sequence *next;
	char *textp;
	uint8_t flags;
        char field_9;
        char field_A;
        char field_B;
	struct group_value *group_values;
	int field_10;
	int field_14;
	int field_18;
	int field_1C;
        char text[SEQUENCE_TEXT_WIDTH];
};

struct seqlog {
        struct seqlog *next;
        struct sequence *sequence;
        int seq;
};

struct lafunc {
	int unknown;	/* field_0 */
	char *support_path;		/* field_4 */
	char *support_sep;  /* field_8 */
	char *support_name;	/* field_C */
	char *support_name2;	/* field_10 */
	char *support_ext; /* field_14 */
	void *(*rda_malloc)(int size); /* field_18 */
	void *(*rda_calloc)(int members, int size); /* field_1C */
	void *(*rda_realloc)(void *p, int size); /* field_20 */
	void (*rda_free)(void *p); /* field_24 */
	void (*LABus)(struct lactx *lactx, int seqno);				/* 28 */
	int  (*LAInfo)(struct lactx *, enum TLA_INFO, int16_t bus);			/* 2C */
	void (*LAError)(struct lactx *, int, char *, ...);				/* 30 */
	int (*LAFindSeq)(struct lactx *, int seq, int skip, int16_t bus);			/* 34 */
	char *(*LAFormatValue)(struct lactx *lactx, int group, char *inBuf, int maxLength);		/* 38 */
	void (*LAGap)(struct lactx *lactx, int seq);			/* 3c */
	int (*LAGroupValue)(struct lactx *lactx, int seqno, int group);			/* 40 */
	void (*LAInvalidate)(struct lactx *lactx, int bus, int seq1, int seq2);			/* 44 */
	void (*LASeqToText)(struct lactx *lactx, int seq, char *out, int buflen);			/* 48 */
	void (*LAGroupWidth_)();		/* 4c */
	void (*LATimeStamp_ps_)(struct lactx *lactx, int seq, int out);		/* 50 */
	void (*LASysTrigTime_ps_)(void);	/* 54 */
	void (*LABusModTrigTime_ps_)(void);	/* 58 */
	void (*LABusModTimeOffset_ps_)(void);	/* 5c */
	void (*LAGroupInvalidBitMask_)(void);	/* 60 */
	void (*LAContigLongToSeq_)(void);		/* 64 */
	int (*LALongToSeq_)(struct lactx *lactx, int bus, int val);				/* 68 */
	int (*LALongToValidSeq_)(struct lactx *lactx, int bus, int val1, int val2);		/* 6c */
	int (*LASeqToContigLong_)(struct lactx *lactx, int seqno);		/* 70 */
	void (*LASeqToLong_)(struct lactx *lactx, int seqno);				/* 74 */
	void (*LASubDisasmLoad)(struct lactx *lactx, void *filename);			/* 78 */
	void (*LASubDisasmUnload)(struct lactx *,  void *subdisasm);		/* 7c */
	void *(*LASubDisasmFuncTablePtr_)(struct lactx *lactx, void *subdisasm);	/* 80 */
	int (*LAWhichBusMod_)(struct lactx *lactx, int seq);			/* 84 */
	int (*LASeqDisplayFormat_)(struct lactx *lactx);		/* 88 */
	void (*LAInteractiveUI2_)(struct lactx *lactx);		/* 8c */
	char (*LAProgAbort_)(struct lactx *lactx, int seq);				/* 90 */
	void (*LATimestamp_ps_ToText)(void);	/* 94 */
	int (*LATimeStampDisplayFormat)(struct lactx *lactx);	/* 98 */
	int (*LAReferenceTime_ps_)(struct lactx *lactx, void *out);		/* 9c */
	char (*LABusModSysTrigTime_ps_)(struct lactx *lactx, int bus, int modnum, void *out);	/* a0 */
	void (*LABusModFrameOffset_ps_)(struct lactx *lactx, int bus, int modnum, void *out);	/* a4 */
	void (*LABusModTimeToUserAlignedTime_ps_)(struct lactx *lactx, int bus, int modnum, void *out); /* a8 */
	char (*LABusModTrigSample)(struct lactx *lactx, int bus, int modnum, void *out);		/* ac */
	void (*LABusModWallClockStart_)(void);	/* b0 */
	void *field_B4;							/* b4 */
	void (*LAReferenceTime_ps_2)(void);		/* b8 */
	int (*LASampleStatusBits)(struct lactx *lactx, int seqno);		/* bc */
	void (*LASampleStatusBitsType_)(struct lactx *lactx, int seqno, int bus, int modnum);
	void (*LAGroupViolationBitMask_)(struct lactx *lactx, int seqno, int group);
	void (*LAGroupViolationBitMaskType_)(struct lactx *lactx, int group);
	void (*LABusModVariantName_)(struct lactx *lactx, int bus, int modnum, char *out, int buflen);
	void (*LASystemName_)(struct lactx *lactx, int bus, int modnum, char *out, int buflen);
	void (*LASystemPath_)(struct lactx *lactx, int bus, int modnum, char *out, int buflen);
	void *field_DC;
};

struct pctx {
	struct lactx *lactx;
	struct lafunc func;

        bus_state_t busstate;

        int clockcount;

        int lpc_cycletype;
        lpc_start_t lpc_start;
        int lpc_addrwidth;
        int lpc_datawidth;

        int lastdata;

        int lpc_address;
        int lpc_data;
        int lpc_idsel;
};

struct groupinfo {
	char *groupname;
	char field_4;
	char field_5;
	char grouptype;
	char field_7;
	uint16_t width;
	uint16_t default_columns;
        int field_C;
        const char *default_symbolfile;
        char likegroup;
        char field_15;
        int16_t field_16;
        int16_t field_18;
};

enum MODEINFO {
	MODEINFO_MAX_BUS=0,
	MODEINFO_MAX_GROUP=1,
	MODEINFO_MAX_MODE=2,
	MODEINFO_3=3,
	MODEINFO_GETNAME=4,
	MODEINFO_MAX,
};
typedef enum {
        DISPLAY_ATTRIBUTE_HARDWARE=1,
        DISPLAY_ATTRIBUTE_SOFTWARE=2,
        DISPLAY_ATTRIBUTE_CONTROL_FLOW=4,
        DISPLAY_ATTRIBUTE_SUBROUTINE=8,
        DISPLAY_ATTRIBUTE_SPECIAL=15
} display_attribute_t;

typedef enum {
        GROUP_TYPE_INPUT = 0,
        GROUP_TYPE_1 = 1,
        GROUP_TYPE_MNEMONIC = 2,
        GROUP_TYPE_FAKE_GROUP = 3,
} group_type_t;


__declspec(dllexport) struct pctx *ParseReinit(struct pctx *pctx, struct lactx *lactx, struct lafunc *func);
__declspec(dllexport) int ParseFinish(struct pctx *pctx);
__declspec(dllexport) int ParseInfo(struct pctx *pctx, unsigned int request);
__declspec(dllexport) int ParseMarkMenu(struct pctx *, int seq, char ***, char **, char **);
__declspec(dllexport) int ParseMarkGet(struct pctx *pctx, int seq);
__declspec(dllexport) int ParseMarkSet(struct pctx *pctx, int seq, int);
__declspec(dllexport) int ParseMarkNext(struct pctx *pctx, int seq, int);
__declspec(dllexport) int ParseModeGetPut(struct pctx *pctx, int16_t mode, int, int request);
__declspec(dllexport) struct sequence *ParseSeq(struct pctx *, int seq);
__declspec(dllexport) struct businfo *ParseBusInfo(struct pctx *, uint16_t bus);
__declspec(dllexport) struct modeinfo *ParseModeInfo(struct pctx *pctx, uint16_t mode);
__declspec(dllexport) struct groupinfo *ParseGroupInfo(struct pctx *pctx, uint16_t group);
__declspec(dllexport) int ParseDisasmReinit(struct pctx *, int request);
__declspec(dllexport) int ParseExtInfo_(struct pctx *, int request, void *out);
__declspec(dllexport) int ParseStringModeGetPut_(struct pctx *pctx, int mode, int value, int request);

#endif
