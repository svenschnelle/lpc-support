#include "lpc.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <stdarg.h>
#include <windows.h>
#include <time.h>
#include <errno.h>

#define ARRAY_SIZE(_x) (sizeof(_x)/sizeof(_x[0]))

typedef enum {
        LSMI_N = 0x01,
        LPCPD_N = 0x02,
        LPME_N = 0x04,
        CLKRUN_N = 0x08,
        SERIRQ = 0x10,
        LDRQ_N = 0x20,
        LFRAME_N = 0x40,
        LRESET_N = 0x80,
        LCLK_N = 0x100,
} ctrl_signal_t;

typedef enum {
        CYCLE_UNKNOWN=0,
        CYCLE_IO_READ,
        CYCLE_IO_WRITE,
        CYCLE_MEM_READ,
        CYCLE_MEM_WRITE,
        CYCLE_FW_READ,
        CYCLE_FW_WRITE,
} cycle_type_t;

static FILE *logfile = NULL;
static int idsel_values[16] = { 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

const char *modeinfo_names[MODEINFO_MAX] = {
	"MAX_BUS",
	"MAX_GROUP",
	"MAX_MODE",
	"3",
	"GETNAME"
};

static const char *lpc_start[16] = {
        "Target Cycle",
        "Reserved",
        "Grant for busmaster 0",
        "Grant for busmaster 1",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Firmware Read Cycle",
        "Firmware Write Cycle",
        "Stop/Abort"
};

static const char *lpc_cycletypes[8] = {
        "I/O Read",
        "I/O Write",
        "Memory Read",
        "Memory Write",
        "DMA Read",
        "DMA Write",
        "Reserved",
        "Reserved"
};

struct groupinfo groupinfo[] = { { .groupname = "CTRL", .width = 9, .field_16 = -1 },
                                 { .groupname = "DATA", .width = 4, .field_16 = -1 },
                                 { .groupname = "Decoded Data", .grouptype = GROUP_TYPE_FAKE_GROUP, .width = 32, .field_16 = -1 },
                                 { .groupname = "Decoded Address", .grouptype = GROUP_TYPE_FAKE_GROUP, .width = 32, .field_16 = -1 },
                                 { .groupname = "Decoded CycleType", .grouptype = GROUP_TYPE_FAKE_GROUP, .width = 8, .field_16 = -1, .default_symbolfile="LPC_cycletype.tsf"  },
                                 { .groupname = "Cycle Type" , .grouptype = GROUP_TYPE_MNEMONIC, .field_7 = 1, .width = SEQUENCE_TEXT_WIDTH, .field_16 = -1  }};

struct businfo businfo[] = { { .groupcount = ARRAY_SIZE(groupinfo) } };

char *onoff[] = { "On", "Off", NULL, NULL };

struct modeinfo modeinfo[] = { /*{ "Show unknown cycles", onoff, 1, 0 },
                               { "Show all cycles", onoff, 1, 0 },
                               { "Testmenu3", onoff, 1, 0 },
                               { "Testmenu4", onoff, 1, 0 }*/ };

struct stringmodevalues stringmodevalues[4] = { { "All cycles", 1 },
                                                { "Decoded cycles", 2 },
                                                { "Aborted cycles", 3 } };

struct stringmodename stringmodename = { ARRAY_SIZE(stringmodevalues), "Show:", stringmodevalues, NULL };

static void LogDebug(struct pctx *pctx, int level, const char *fmt, ...)
{
        char buf[4096];

        if (!logfile)
                logfile = fopen("c:\\tlatrace.txt", "w+");

        if (!logfile)
                return;

        if (level >= 9)
                return;

	va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        fprintf(logfile, "%s", buf);
        fflush(logfile);
}

int ParseFinish(struct pctx *pctx)
{
	LogDebug(pctx, 8, "%s(%p)\n", __FUNCTION__, pctx);
	if (logfile)
		fclose(logfile);
	logfile = NULL;
	pctx->func.rda_free(pctx);
	return 0;
}

static struct sequence *get_sequence(struct pctx *pctx)
{
        struct sequence *ret;
        unsigned int i;

        if (!(ret = pctx->func.rda_calloc(1, sizeof(struct sequence)))) {
                LogDebug(pctx, 6, "calloc failed\n");
                return NULL;
        }

        ret->textp = ret->text;
        ret->group_values = calloc(ARRAY_SIZE(groupinfo), sizeof(struct group_value));
        for(i = 0; i < ARRAY_SIZE(groupinfo); i++)
                ret->group_values[i].mask = 0xf;
        return ret;
}

static struct sequence *make_lpc_sequence(struct pctx *pctx)
{
        struct sequence *seqinfo = get_sequence(pctx);

        if (!seqinfo)
                return NULL;

        sprintf(seqinfo->textp, "%s", pctx->lpc_start ?  lpc_start[pctx->lpc_start] : lpc_cycletypes[pctx->lpc_cycletype]);

        seqinfo->flags = DISPLAY_ATTRIBUTE_SOFTWARE;

        pctx->busstate = BUS_STATE_IDLE;
        seqinfo->group_values[2].value = pctx->lpc_data;
        seqinfo->group_values[3].value = pctx->lpc_address;

        switch(pctx->lpc_datawidth) {
        case 2:
                seqinfo->group_values[2].mask = 0xe;
                break;
        case 4:
                seqinfo->group_values[2].mask = 0x7;
                break;
        case 16:
                seqinfo->group_values[2].mask = 0;
                break;
        default:
                break;
        }

        switch(pctx->lpc_addrwidth) {
        case 4:
                seqinfo->group_values[3].mask = 0xc;
                break;
        case 7:
        case 8:
                seqinfo->group_values[3].mask = 0;
                break;
        default:
                break;
        }

        seqinfo->group_values[4].mask = 0;
        if (!pctx->lpc_start) {
                if (pctx->lpc_cycletype < 8)
                        seqinfo->group_values[4].value = pctx->lpc_cycletype;
        } else {
                switch(pctx->lpc_start) {
                case LPC_TARGET:
                        seqinfo->group_values[4].value = 8;
                        break;
                case LPC_GRANT0:
                        seqinfo->group_values[4].value = 9;
                        break;
                case LPC_GRANT1:
                        seqinfo->group_values[4].value = 10;
                        break;
                case LPC_FW_READ:
                        seqinfo->group_values[4].value = 13;
                        break;
                case LPC_FW_WRITE:
                        seqinfo->group_values[4].value = 14;
                        break;
                case LPC_ABORT:
                        seqinfo->group_values[4].value = 15;
                        break;
                default:
                        break;
                }
        }
        return seqinfo;
}
static struct sequence *parse_lpc(struct pctx *pctx, int startseq)
{
	struct sequence *seqinfo = NULL;
        int ctrl, data, ctrldiff, addrwidth, lastctrl = -1;
        int seq = startseq;

        pctx->busstate = BUS_STATE_START;
        pctx->clockcount = 0;
        pctx->lpc_address = 0;
        pctx->lpc_data = 0;

        do {
                ctrl = pctx->func.LAGroupValue(pctx->lactx, seq, 0);
                data = pctx->func.LAGroupValue(pctx->lactx, seq, 1);
                ctrldiff = lastctrl ^ ctrl;
                lastctrl = ctrl;

                if (ctrldiff & LCLK_N && ctrl & LCLK_N)
                        continue;

                if (pctx->busstate != BUS_STATE_IDLE && pctx->busstate != BUS_STATE_START
                    && (ctrldiff & LFRAME_N) && !(ctrl & LFRAME_N)) {
                        LogDebug(pctx, 8, "Abort cycle detected: %d\n", pctx->busstate);
                        pctx->busstate = BUS_STATE_ABORT;
                        pctx->clockcount = 0;
                }

                if (pctx->busstate <= ARRAY_SIZE(busstates))
                        LogDebug(pctx, 8, "bus state %s at seq %d (data %01X, ctrl %04X, ctrldiff %04X)\n", busstates[pctx->busstate], seq, data & 0xf, ctrl, ctrldiff);

                switch(pctx->busstate) {
                case BUS_STATE_IDLE:
                        return NULL;

                case BUS_STATE_START:

                        pctx->lpc_start = data & 0xf;
                        pctx->lpc_address = 0;
                        pctx->lpc_data = 0;

                        switch(pctx->lpc_start) {
                        case LPC_TARGET:
                                pctx->busstate = BUS_STATE_CT_DIR;
                                break;

                        case LPC_FW_READ:
                        case LPC_FW_WRITE:
                                pctx->busstate = BUS_STATE_IDSEL;
                                break;

                        case LPC_GRANT0:
                        case LPC_GRANT1:
                                break;
                        case LPC_ABORT:
                                return NULL;
                        default:
                                LogDebug(pctx, 6, "Unknown cycle type %d\n", pctx->lpc_start);
                                pctx->busstate = BUS_STATE_IDLE;
                                break;
                        };
                        break;

                case BUS_STATE_IDSEL:

                        pctx->clockcount = 0;
                        pctx->busstate = BUS_STATE_ADDR;
                        pctx->lpc_idsel = data & 0xf;
                        pctx->lpc_addrwidth = 7;
                        break;

                case BUS_STATE_CT_DIR:

                        pctx->lpc_cycletype = (data & 0xe) >> 1;
                        pctx->lpc_datawidth = 2;
                        if (!(pctx->lpc_cycletype & 0xe))
                                pctx->lpc_addrwidth = 4;
                        else
                                pctx->lpc_addrwidth = 8;
                        pctx->busstate = BUS_STATE_ADDR;
                        break;

                case BUS_STATE_ADDR:

                        pctx->lpc_address |= (data & 0xf) << ((pctx->lpc_addrwidth-1) * 4 - pctx->clockcount * 4);
                        if (++pctx->clockcount == pctx->lpc_addrwidth) {
                                pctx->clockcount = 0;
                                switch(pctx->lpc_start) {
                                default:
                                case LPC_TARGET:
                                        pctx->busstate = pctx->lpc_cycletype & 1 ? BUS_STATE_DATA : BUS_STATE_TAR;
                                        break;
                                case LPC_FW_READ:
                                case LPC_FW_WRITE:
                                        pctx->busstate = BUS_STATE_MSIZE;
                                        break;
                                }
                        }
                        break;

                case BUS_STATE_MSIZE:

                        pctx->clockcount = 0;

                        switch(data & 0xf) {
                        case 0:
                                pctx->lpc_datawidth = 2;
                                break;
                        case 1:
                                pctx->lpc_datawidth = 4;
                                break;
                        case 2:
                                pctx->lpc_datawidth = 16;
                                break;
                        case 4:
                                pctx->lpc_datawidth = 64;
                                break;
                        case 7:
                                pctx->lpc_datawidth = 256;
                                break;
                        default:
                                pctx->busstate = BUS_STATE_IDLE;
                                break;
                        }

                        switch(pctx->lpc_start) {
                        case LPC_FW_READ:
                                pctx->busstate = BUS_STATE_TAR;
                                break;

                        case LPC_FW_WRITE:
                                pctx->busstate = BUS_STATE_DATA;
                                break;
                        default:
                                pctx->busstate = BUS_STATE_IDLE;
                                break;
                        }
                        break;

                case BUS_STATE_TAR:

                        if (++pctx->clockcount == 2) {
                                pctx->clockcount = 0;
                                pctx->busstate = BUS_STATE_SYNC;
                        }
                        break;

                case BUS_STATE_SYNC:

                        if (!data) {
                                pctx->clockcount = 0;
                                pctx->lpc_data = 0;
                                pctx->busstate = BUS_STATE_DATA;
                        }
                        break;

                case BUS_STATE_DATA:

                        pctx->lpc_data |= (data & 0xf) << pctx->clockcount * 4;

                        if (++pctx->clockcount== 2) {
                                pctx->clockcount = 0;
                                LogDebug(pctx, 8, "BUS_STATE_DATA @%d addr: %04X, data %04X\n", seq, pctx->lpc_address, pctx->lpc_data);
                                pctx->busstate = BUS_STATE_IDLE;
                                return make_lpc_sequence(pctx);
                        }
                        break;

                case BUS_STATE_ABORT:
                        if (++pctx->clockcount == 5) {
                                seqinfo = make_lpc_sequence(pctx);
                                seqinfo->next = get_sequence(pctx);
                                seqinfo->next->flags = DISPLAY_ATTRIBUTE_CONTROL_FLOW;
                                sprintf(seqinfo->next->text, "  *** Cycle aborted ***");
                                pctx->busstate = BUS_STATE_IDLE;
                                return seqinfo;
                        }
                        break;

                default:
                        LogDebug(pctx, 9, "Illegal state @%d\n", seq);
                        break;
                }
                pctx->lastdata = data;
        } while((seq = pctx->func.LAFindSeq(pctx->lactx, seq, 1, -1)) > 0);
        return NULL;
}

struct sequence *ParseSeq(struct pctx *pctx, int seq)
{
	struct sequence *seqinfo = NULL;
	int ctrl, ctrl2, ctrldiff, nextseq, data;

	if (!pctx) {
		LogDebug(pctx, 9, "pctx NULL\n");
		return NULL;
	}

        if ((nextseq = pctx->func.LAFindSeq(pctx->lactx, seq, 1, -1)) < 0)
                return NULL;

        ctrl = pctx->func.LAGroupValue(pctx->lactx, seq, 0);
        ctrl2 = pctx->func.LAGroupValue(pctx->lactx, nextseq, 0);
        ctrldiff = ctrl ^ ctrl2;

        LogDebug(pctx, 9, "ParseSeq %d (%d) ctrl %04X ctrldiff %04X\n", seq, nextseq, ctrl, ctrldiff);

        if (ctrldiff & LFRAME_N && !(ctrl & LFRAME_N)) {
                LogDebug(pctx, 8, "%s: found start at sequence %d\n", __func__, seq);
                if (!(seqinfo = parse_lpc(pctx, seq)))
                        return NULL;

                data = pctx->func.LAGroupValue(pctx->lactx, seq, 1);
                seqinfo->group_values[0].value = ctrl;
                seqinfo->group_values[1].value = data;
                seqinfo->group_values[0].mask = 0;
                seqinfo->group_values[1].mask = 0;
                return seqinfo;
        }

        if (pctx->func.LAInfo(pctx->lactx, TLA_INFO_DISPLAY_ATTRIBUTE, -1) == DISPLAY_ATTRIBUTE_HARDWARE) {
                seqinfo = get_sequence(pctx);
                seqinfo->flags = DISPLAY_ATTRIBUTE_HARDWARE;
                seqinfo->group_values[0].value = ctrl;
                seqinfo->group_values[1].value = pctx->func.LAGroupValue(pctx->lactx, seq, 1);
                seqinfo->group_values[0].mask = 0;
                seqinfo->group_values[1].mask = 0;
                return seqinfo;
        }
        return NULL;
}

int ParseMarkNext(struct pctx *pctx, int seq, int a3)
{
	LogDebug(pctx, 9, "%s: sequence %d, a3 %d\n", __FUNCTION__, seq, a3);
	return 0;
}

int ParseMarkSet(struct pctx *pctx, int seq, int a3)
{
	LogDebug(pctx, 9, "%s\n", __FUNCTION__);
	return 0;
}

int ParseMarkGet(struct pctx *pctx, int seq)
{
	LogDebug(pctx, 9, "%s: sequence %d\n", __FUNCTION__, seq);
	return 0;
}


int ParseMarkMenu(struct pctx *pctx, int seq, char ***names, char **entries, char **val)
{
        static char *_entries[] = { "Testentry", "Testentry2", NULL };
	LogDebug(pctx, 8, "%s: sequence %d\n", __FUNCTION__, seq);
        *names = _entries;
        *entries = "Test";
        *val = "Test";
	return 2;
}

int ParseInfo(struct pctx *pctx, unsigned int request)
{
	LogDebug(pctx, 8, "%s: %s\n", __FUNCTION__,
                 request > ARRAY_SIZE(modeinfo_names) ? "invalid" : modeinfo_names[request]);

	switch(request) {
        case MODEINFO_MAX_BUS:
                return ARRAY_SIZE(businfo);
        case MODEINFO_MAX_GROUP:
                return ARRAY_SIZE(groupinfo);
        case MODEINFO_GETNAME:
                return (int)"LPC";
        case 3:
                return 1;
        case MODEINFO_MAX_MODE:
                return ARRAY_SIZE(modeinfo);
        default:
                LogDebug(pctx, 6, "%s: invalid request: %d\n", __FUNCTION__, request);
                return 0;
	}
	return 0;
}

int ParseExtInfo_(struct pctx *pctx, int request, void *out)
{
	LogDebug(pctx, 8, "%s: %d\n", __FUNCTION__, request);
        switch(request) {
        case 0:
                *(struct stringmodename **)out = &stringmodename;
                return 1;
        case 1:
        case 2:
/*        case 3:
        case 4:
        case 5:*/
        case 7:
                *(int *)out = 1;
                return 1;
        default:
                return 0;

        }
}

struct businfo *ParseBusInfo(struct pctx *pctx, uint16_t bus)
{
	LogDebug(pctx, 8, "%s: %08x\n", __FUNCTION__, bus);

	if (bus >= ARRAY_SIZE(businfo))
		return NULL;
	return businfo+bus;
}

struct groupinfo *ParseGroupInfo(struct pctx *pctx, uint16_t group)
{
	LogDebug(pctx, 8, "%s: %08x\n", __FUNCTION__, group);

	if (group > ARRAY_SIZE(groupinfo))
                return NULL;
	return groupinfo+group;
}

struct modeinfo *ParseModeInfo(struct pctx *pctx, uint16_t mode)
{
	LogDebug(pctx, 8, "%s: %d\n", __FUNCTION__, mode);
	if (mode > ARRAY_SIZE(modeinfo))
                return NULL;
	return modeinfo+mode;
}

int ParseModeGetPut(struct pctx *pctx, int16_t mode, int value, int request)
{
        int firstseq, lastseq;
        if (mode < 0 || mode > (signed)ARRAY_SIZE(modeinfo)) {
                LogDebug(pctx, 8, "%s: mode=%d request=%d value=%s (%d))\n", __FUNCTION__,
                         mode, request, stringmodevalues[value-1].name, value);
        } else {
                LogDebug(pctx, 8, "%s: %d (%s), %d (%s)\n", __FUNCTION__,
                         request, modeinfo[mode].name, value, onoff[value]);
                firstseq = pctx->func.LAInfo(pctx->lactx, TLA_INFO_FIRST_SEQUENCE, -1);
                lastseq = pctx->func.LAInfo(pctx->lactx, TLA_INFO_LAST_SEQUENCE, -1);
                pctx->func.LAInvalidate(pctx->lactx, -1, firstseq, lastseq);
        }
	return value;
}


int ParseStringModeGetPut_(struct pctx *pctx, int mode, int value, int request)
{
	LogDebug(pctx, 8, "%s: %d (%s), %d (%s)\n", __FUNCTION__,
                 request, modeinfo[mode].name, value, onoff[value]);
	return value;
}

struct pctx *ParseReinit(struct pctx *pctx, struct lactx *lactx, struct lafunc *func)
{
	struct pctx *ret = NULL;

        LogDebug(ret, 8, "%s(%p, %p, %p)\n", __FUNCTION__, pctx, lactx, func);

	if (pctx)
                return pctx;

	if (!(ret = func->rda_calloc(1, sizeof(struct pctx)))) {
		func->LAError(0, 9, "Out of Memory");
		return NULL;
	}

	ret->lactx = lactx;
        memcpy(&ret->func, func, sizeof(struct lafunc));
	return ret;
}

int ParseDisasmReinit(struct pctx *pctx, int request)
{
        struct seqlog *seqlog;

	LogDebug(pctx, 8, "%s(%p, %d)\n", __FUNCTION__, pctx, request);
        LogDebug(pctx, 8, "cache size %d\n", pctx->func.LAInfo(pctx->lactx, 3, -1));
        return 1;
}
