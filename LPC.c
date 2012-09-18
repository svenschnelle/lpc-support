#include "lpc.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <stdarg.h>
#include <windows.h>
#include <time.h>
#include <errno.h>

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
	
struct groupinfo groupinfo[] = {
	GROUP("CTRL", GROUP_TYPE_INPUT, 9, 0, NULL),
	GROUP("DATA", GROUP_TYPE_INPUT, 4, 0, NULL),
	GROUP("Decoded Data", GROUP_TYPE_FAKE_GROUP, 32, 8, NULL),
	GROUP("Decoded Address", GROUP_TYPE_FAKE_GROUP, 32, 16, NULL),
	GROUP("Decoded CycleType", GROUP_TYPE_FAKE_GROUP, 32, 32, "LPC_cycletype.tsf"),
	GROUP("Cycle Type", GROUP_TYPE_MNEMONIC, 0x11b, SEQUENCE_TEXT_WIDTH, NULL),
};

struct businfo businfo[] = { { .groupcount = ARRAY_SIZE(groupinfo)  } };

char *onoff[] = { "Off", "On", NULL, NULL };

struct modeinfo modeinfo[] = { { "Show unknown cycles", onoff, 1, 0 },
                               { "Show aborted cycles", onoff, 2, 0 } };

struct stringmodevalues stringmodevalues[] = { { "All cycles", DISPLAY_ATTRIBUTE_ALL },
                                               { "Decoded cycles", DISPLAY_ATTRIBUTE_DECODED },
                                               { "Highlevel", DISPLAY_ATTRIBUTE_HIGHLEVEL },
                                               { "Aborted cycles", DISPLAY_ATTRIBUTE_ABORTED } };

struct stringmodename stringmodename = { ARRAY_SIZE(stringmodevalues), "Show:", stringmodevalues, NULL };

struct acpi_ctx acpi_smm_ctx;
struct acpi_ctx acpi_ctx;
struct superio_ctx superio0_ctx, superio1_ctx;
struct pmh7_ctx pmh7_ctx;

struct acpi_register {
        unsigned char offset;
        unsigned char bitoffset;
        unsigned char bitlen;
        const char *name;
} acpi_registers[] = {
        { 0x00, 1, 1, "Enable Global Attention" },
        { 0x00, 2, 1, "Enable Hotkey" },
        { 0x00, 3, 1, "Enable sticky FN Key" },
        { 0x00, 4, 1, "Disable embedded Numpad emulation" },
        { 0x00, 5, 1, "Enable TWR" },
        { 0x00, 7, 1, "Enable Thermal Monitor" },
        { 0x01, 0, 1, "Panel backlight controlled by LID switch" },
        { 0x01, 4, 1, "Warn if PWR switch depressed" },
        { 0x01, 7, 1, "Do Not ignore other keys while FN pressed" },
        { 0x02, 5, 1, "Enable Smart Numlock" },
        { 0x02, 7, 1, "Enable Thinkvantage Button" },
        { 0x03, 0, 1, "Headphone state not affected by Speaker Mute" },
        { 0x03, 1, 1, "Ignore Phantom Keys" },
        { 0x03, 4, 1, "Charge/Discharge Preference" },
        { 0x03, 5, 2, "Audio button behaviour" },
        { 0x04, 1, 1, "Sound Mask: Critical Low Battery" },
        { 0x04, 2, 1, "Sound Mask: Low Battery" },
        { 0x04, 3, 1, "Sound Mask: Suspend" },
        { 0x04, 4, 1, "Sound Mask: VM Suspend" },
        { 0x04, 5, 1, "Sound Mask: Resume" },
        { 0x04, 6, 1, "Sound Mask: AC status" },
        { 0x04, 7, 1, "Sound Mask: Power Off" },
        { 0x05, 0, 1, "Sound Mask: Power Off Warn" },
        { 0x05, 1, 1, "Sound Mask: No HDD Warn" },
        { 0x05, 2, 1, "Sound Mask: Dead" },
        { 0x05, 3, 1, "Sound Mask: 440Hz" },
        { 0x05, 4, 1, "Sound Mask: 315Hz" },
        { 0x05, 5, 1, "Sound Mask: 2x315Hz" },
        { 0x05, 6, 1, "Sound Mask: 3x315Hz" },
        { 0x05, 7, 1, "Sound Mask: Inhibit Swap" },
        { 0x06, 0, 8, "Beep ID" },
        { 0x07, 0, 8, "Beep Repeat Interval" },
        { 0x08, 0, 8, "Beep Mask 2" },
        { 0x09, 0, 8, "Keyboard ID" },
        { 0x0a, 0, 8, "KMC Command control" },
        { 0x0c, 0, 4, "LED control" },
        { 0x0c, 5, 1, "LED Blink gradually" },
        { 0x0c, 6, 1, "Blinking" },
        { 0x0c, 7, 1, "Turn on" },
        { 0x0d, 0, 1, "Enable USB Power in ACPI S3/S4/S5" },
        { 0x0d, 2, 1, "Enable USB_AO_SEL0 in ACPI S3/S4/S5" },
        { 0x0d, 3, 1, "Enable USB_A0_SEL1 in ACPI S3/S4/S5" },
        { 0x0e, 0, 2, "Fn Key status" },
        { 0x0e, 2, 1, "Gravity sensor error" },
        { 0x0e, 3, 1, "Inhibit Peak Shift charge" },
        { 0x0e, 4, 1, "Discharge with the AC Adapter for Peak Shift" },
        { 0x0e, 5, 1, "Gravity sensor DIAG running" },
        { 0x0e, 6, 2, "Gravity ID" },
        { 0x0f, 0, 1, "Primary Battery charge inhibit" },
        { 0x0f, 1, 1, "Secondary Battery charge inhibit" },
        { 0x0f, 2, 1, "Primary Battery discharge refresh" },
        { 0x0f, 3, 1, "Secondary Battery discharge refresh" },
        { 0x0f, 4, 1, "HDD attached during Suspend" },
        { 0x0f, 5, 1, "Bay HDD was detached in suspend" },
        { 0x0f, 6, 1, "NumLock state" },
        { 0x0f, 7, 1, "Tablet mode switch status" },
        { 0x10, 0, 7, "Attention Control 0" },
        { 0x11, 0, 7, "Attention Control 1" },
        { 0x12, 0, 7, "Attention Control 2" },
        { 0x13, 0, 7, "Attention Control 3" },
        { 0x14, 0, 7, "Attention Control 4" },
        { 0x15, 0, 7, "Attention Control 5" },
        { 0x16, 0, 7, "Attention Control 6" },
        { 0x17, 0, 7, "Attention Control 7" },
        { 0x18, 0, 7, "Attention Control 8" },
        { 0x19, 0, 7, "Attention Control 9" },
        { 0x1a, 0, 7, "Attention Control 10" },
        { 0x1b, 0, 7, "Attention Control 11" },
        { 0x1c, 0, 7, "Attention Control 12" },
        { 0x1d, 0, 7, "Attention Control 13" },
        { 0x1e, 0, 7, "Attention Control 14" },
        { 0x1f, 0, 7, "Attention Control 15" },
        { 0x21, 0, 8, "Charge inhibit timer LSB" },
        { 0x22, 0, 8, "Charge inhibit timer MSB" },
        { 0x23, 0, 8, "Misc control" },
        { 0x2f, 0, 8, "Fan control" },
        { 0x30, 0, 7, "Volume" },
        { 0x30, 7, 1, "Mute" },
        { 0x31, 0, 2, "Fan select" },
        { 0x31, 2, 1, "Enable UWB" },
        { 0x31, 4, 1, "Disable PCI-e" },
        { 0x31, 5, 1, "Identify always on card" },
        { 0x31, 6, 1, "Enable USB for always on card" },
        { 0x31, 7, 1, "PCI-e standby" },
        { 0x32, 0, 1, "Resume Trigger: PME Event enable" },
        { 0x32, 1, 1, "Resume Trigger: Critical Low battery" },
        { 0x32, 2, 1, "Resume Trigger: LID open" },
        { 0x32, 3, 1, "Resume Trigger: Eject Button" },
        { 0x32, 4, 1, "Resume Trigger: Fn Key" },
        { 0x32, 5, 1, "Resume Trigger: Portfino" },
        { 0x32, 6, 1, "Resume Trigger: Modem Ring" },
        { 0x32, 7, 1, "Resume Trigger: Ultrabay unlock" },
        { 0x33, 0, 8, "EC event mask 1" },
        { 0x34, 1, 1, "BEEP active" },
        { 0x34, 2, 1, "SMBus Busy" },
        { 0x34, 4, 1, "Fan exists" },
        { 0x34, 5, 1, "Gravity sensor exists" },
        { 0x34, 7, 1, "Power consumption warning" },
        { 0x35, 0, 1, "Input devices locked by password" },
        { 0x35, 1, 1, "Input devices frozen" },
        { 0x35, 2, 1, "FAN POR done" },
        { 0x35, 3, 1, "Attention disabled temporary" },
        { 0x35, 4, 1, "Fan error" },
        { 0x35, 7, 1, "Thermal sensor error" },
        { 0x36, 0, 8, "EC event status 0 copy" },
        { 0x37, 0, 8, "EC event status 1 copy" },
        { 0x38, 0, 4, "Primary Battery Level" },
        { 0x38, 4, 1, "Primary Battery error" },
        { 0x38, 5, 1, "Primary Battery charging" },
        { 0x38, 6, 1, "Primary Battery discharging" },
        { 0x38, 7, 1, "Primary Battery present" },
        { 0x39, 0, 4, "Secondary Battery Level" },
        { 0x39, 4, 1, "Secondary Battery error" },
        { 0x39, 5, 1, "Secondary Battery charging" },
        { 0x39, 6, 1, "Secondary Battery discharging" },
        { 0x39, 7, 1, "Secondary Battery present" },
        { 0x3a, 0, 1, "Assert Audio Mute" },
        { 0x3a, 1, 1, "I2C select" },
        { 0x3a, 2, 1, "Power Off" },
        { 0x3a, 3, 1, "Wireless force enable" },
        { 0x3a, 4, 1, "Enable Bluetooth" },
        { 0x3a, 5, 1, "Enable WLAN" },
        { 0x3a, 6, 1, "Enable WWAN" },
        { 0x3a, 7, 1, "Inhibit usage of second battery" },
        { 0x3b, 0, 1, "Speaker Mute" },
        { 0x3b, 1, 1, "Enable Keyboard Light" },
        { 0x3b, 3, 1, "Assert Bluetooth detach" },
        { 0x3b, 4, 1, "Enable USB" },
        { 0x3b, 5, 1, "Inhibit communication with the primary battery" },
        { 0x3b, 6, 1, "Inhibit communication with the secondary battery" },
        { 0x4e, 0, 8, "EC event status 0" },
        { 0x4f, 0, 8, "EC event status 1" },
        { 0x3c, 0, 8, "Resume reason" },
        { 0x3d, 0, 8, "Password control" },
        { 0x3e, 0, 8, "Password data0" },
        { 0x3f, 0, 8, "Password data1" },
        { 0x40, 0, 8, "Password data2" },
        { 0x41, 0, 8, "Password data3" },
        { 0x42, 0, 8, "Password data4" },
        { 0x43, 0, 8, "Password data5" },
        { 0x44, 0, 8, "Password data6" },
        { 0x45, 0, 8, "Password data7" },
        { 0x46, 0, 1, "Sense status 0: Fn Key" },
        { 0x46, 2, 1, "Sense status 0: LID open" },
        { 0x46, 3, 1, "Sense status 0: Power off switch" },
        { 0x46, 4, 1, "Sense status 0: AC status" },
        { 0x46, 7, 1, "Sense status 0: LP mode" },
        { 0x47, 0, 1, "Sense status 1: Bay unlock" },
        { 0x47, 1, 1, "Sense status 1: Dock Event" },
        { 0x47, 2, 1, "Sense status 1: Bay not attached" },
        { 0x47, 3, 1, "Sense status 1: HDD in present bay" },
        { 0x48, 0, 1, "Sense status 2: Headphone present" },
        { 0x48, 1, 1, "Sense status 2: RFKILL status" },
        { 0x48, 4, 1, "Sense status 2: External graphics present" },
        { 0x48, 5, 1, "Sense status 2: Dock attached" },
        { 0x48, 6, 1, "Sense status 2: HDD detect" },
        { 0x49, 0, 8, "Sense status 3" },
        { 0x4c, 0, 6, "Event timer" },
        { 0x4c, 0, 7, "Event timer unit is second" },
        { 0x4d, 0, 6, "Event timer" },
        { 0x50, 0, 8, "SMB_PRCTL" },
        { 0x51, 0, 8, "SMB_STS" },
        { 0x52, 0, 8, "SMB_ADDR" },
        { 0x53, 0, 8, "SMB_CMD" },
        { 0x54, 0, 8, "SMB_DATA 0" },
        { 0x55, 0, 8, "SMB_DATA 1" },
        { 0x56, 0, 8, "SMB_DATA 2" } ,
        { 0x57, 0, 8, "SMB_DATA 3" },
        { 0x58, 0, 8, "SMB_DATA 4" },
        { 0x59, 0, 8, "SMB_DATA 5" },
        { 0x5a, 0, 8, "SMB_DATA 6" },
        { 0x5b, 0, 8, "SMB_DATA 7" },
        { 0x5c, 0, 8, "SMB_DATA 8" },
        { 0x5d, 0, 8, "SMB_DATA 9" },
        { 0x5e, 0, 8, "SMB_DATA 10" },
        { 0x5f, 0, 8, "SMB_DATA 11" },
        { 0x60, 0, 8, "SMB_DATA 12" },
        { 0x61, 0, 8, "SMB_DATA 13" },
        { 0x62, 0, 8, "SMB_DATA 14" },
        { 0x63, 0, 8, "SMB_DATA 15" },
        { 0x64, 0, 8, "SMB_DATA 16" },
        { 0x65, 0, 8, "SMB_DATA 17" },
        { 0x66, 0, 8, "SMB_DATA 18" },
        { 0x67, 0, 8, "SMB_DATA 19" },
        { 0x68, 0, 8, "SMB_DATA 20" },
        { 0x69, 0, 8, "SMB_DATA 21" },
        { 0x6a, 0, 8, "SMB_DATA 22" },
        { 0x6b, 0, 8, "SMB_DATA 23" },
        { 0x6c, 0, 8, "SMB_DATA 24" },
        { 0x6d, 0, 8, "SMB_DATA 25" },
        { 0x6e, 0, 8, "SMB_DATA 26" },
        { 0x6f, 0, 8, "SMB_DATA 27" },
        { 0x70, 0, 8, "SMB_DATA 28" },
        { 0x71, 0, 8, "SMB_DATA 29" },
        { 0x72, 0, 8, "SMB_DATA 30" },
        { 0x73, 0, 8, "SMB_DATA 31" },
        { 0x81, 0, 8, "Information area selector" },
        { 0x82, 0, 8, "Dual Function make timeout" },
        { 0x83, 0, 8, "Fn Key dual function select" },
        { 0x84, 0, 8, "Fan RPM" },
        { 0x85, 0, 8, "Fan RPM" },
        { 0x86, 0, 8, "Password status 0-7" },
        { 0x87, 0, 8, "Password status 8-15" },
        { 0x8f, 0, 8, "Set Fan duty" },
        { 0xa0, 0, 8, "Information area 0" },
        { 0xa1, 0, 8, "Information area 1" },
        { 0xa2, 0, 8, "Information area 2" },
        { 0xa3, 0, 8, "Information area 3" },
        { 0xa4, 0, 8, "Information area 4" },
        { 0xa5, 0, 8, "Information area 5" },
        { 0xa6, 0, 8, "Information area 6" },
        { 0xa7, 0, 8, "Information area 7" },
        { 0xa8, 0, 8, "Information area 8" },
        { 0xa9, 0, 8, "Information area 9" },
        { 0xaa, 0, 8, "Information area 10" },
        { 0xab, 0, 8, "Information area 11" },
        { 0xac, 0, 8, "Information area 12" },
        { 0xad, 0, 8, "Information area 13" },
        { 0xae, 0, 8, "Information area 14" },
        { 0xaf, 0, 8, "Information area 15" },
        { 0xb0, 0, 8, "Primary battery charge start threshold" },
        { 0xb1, 0, 8, "Primary battery charge end threshold" },
        { 0xb2, 0, 8, "Secondary battery charge start threshold" },
        { 0xb3, 0, 8, "Secondary battery charge end threshold" },
        { 0xb4, 0, 8, "Main Battery control" },
        { 0xb5, 0, 8, "Secondary Battery control" },
        { 0xc9, 0, 8, "AC adapter mac power rating LSB" },
        { 0xca, 0, 8, "AC adapter max power rating MSB" },
        { 0xcc, 0, 8, "AC adapter current power rating LSB" },
        { 0xcd, 0, 8, "AC adapter current power rating MSB" },
        { 0xce, 0, 2, "Windows Key mode" },
        { 0xce, 2, 2, "Application Key mode" },
        { 0xce, 4, 1, "swap Fn and left CTRL Key" },
        { 0xe8, 0, 8, "Firmware version" },
        { 0xe9, 0, 8, "Firmware version" },
        { 0xea, 0, 8, "Machine ID" },
        { 0xeb, 0, 8, "EC firmware function specification minor version" },
        { 0xec, 0, 8, "EC firmware capability 0" },
        { 0xed, 0, 8, "EC firmware capability 1" },
        { 0xee, 0, 4, "Primary battery highest level" },
        { 0xee, 4, 8, "Secondary battery highest level" },
        { 0xef, 0, 8, "EC firmware function specification version major" },
        { 0xf0, 0, 8, "Build ID 0" },
        { 0xf1, 0, 8, "Build ID 1" },
        { 0xf2, 0, 8, "Build ID 2" },
        { 0xf3, 0, 8, "Build ID 3" },
        { 0xf4, 0, 8, "Build ID 4" },
        { 0xf5, 0, 8, "Build ID 5" },
        { 0xf6, 0, 8, "Build ID 6" },
        { 0xf7, 0, 8, "Build ID 7" },
        { 0xf8, 0, 8, "Build Date" },
        { 0xf9, 0, 8, "Build Date" },
        { 0xfa, 0, 8, "Build Time" },
        { 0xfb, 0, 8, "Build Time" },
};

static void LogDebug(struct pctx *pctx, int level, const char *fmt, ...)
{
        char buf[4096];

        if (!logfile)
                logfile = fopen("c:\\Users\\svens\\tlatrace.txt", "w+");

        if (!logfile)
                return;

        if (level >= 10)
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

struct sequence *make_lpc_sequence(struct pctx *pctx)
{
        struct sequence *seqinfo = get_sequence(pctx);

        if (!seqinfo)
                return NULL;

        sprintf(seqinfo->textp, "%s", pctx->lpc_start ?  lpc_start[pctx->lpc_start] : lpc_cycletypes[pctx->lpc_cycletype]);

        seqinfo->flags = DISPLAY_ATTRIBUTE_DECODED;

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

#define ACPI_CMD_REG 0x66
#define ACPI_DATA_REG 0x62
#define ACPI_CMD_REG2 0x1604
#define ACPI_DATA_REG2 0x1600

static const char *acpi_op_to_string(acpi_op_t op)
{
        switch(op) {
        case EC_READ:
                return "EC_READ ";
        case EC_WRITE:
                return "EC_WRITE";
        case EC_QUERY:
                return "EC_QUERY";
        default:
                return "Unknown ACPI op";
        }
}

static lpc_subhandler_state_t pmh7_index_reg_write(struct pctx *pctx, void *_ctx,
                                           struct sequence **seqinfo)
{
        struct pmh7_ctx *ctx = (struct pmh7_ctx *)_ctx;
        ctx->haveaddr = 1;
        ctx->addr = pctx->lpc_data & 0xff;
        return LPC_SUBHANDLER_MORE_DATA_NEEDED;
}

static lpc_subhandler_state_t pmh7_data_reg_write(struct pctx *pctx, void *_ctx,
                                           struct sequence **_seqinfo)
{
        struct pmh7_ctx *ctx = (struct pmh7_ctx *)_ctx;
        struct sequence *seqinfo;

        if (!ctx->haveaddr)
                return LPC_SUBHANDLER_IGNORE;

        seqinfo = get_sequence(pctx);
        sprintf(seqinfo->textp, "PMH7 write %02X %02X", ctx->addr, pctx->lpc_data & 0xff);
        seqinfo->flags = DISPLAY_ATTRIBUTE_HIGHLEVEL;
        seqinfo->group_values[2].mask = 0xe;
        seqinfo->group_values[3].mask = 0xe;
        seqinfo->group_values[2].value = pctx->lpc_data & 0xff;
        seqinfo->group_values[3].value = ctx->addr;
        seqinfo->group_values[4].value = 17;
        seqinfo->group_values[4].mask = 0;
        *_seqinfo = seqinfo;
        return LPC_SUBHANDLER_FINISH;
}

static lpc_subhandler_state_t pmh7_data_reg_read(struct pctx *pctx, void *_ctx,
                                           struct sequence **_seqinfo)
{
        struct pmh7_ctx *ctx = (struct pmh7_ctx *)_ctx;
        struct sequence *seqinfo;

        if (!ctx->haveaddr)
                return LPC_SUBHANDLER_IGNORE;

        seqinfo = get_sequence(pctx);
        sprintf(seqinfo->textp, "PMH7 read %02X %02X", ctx->addr, pctx->lpc_data & 0xff);
        seqinfo->flags = DISPLAY_ATTRIBUTE_HIGHLEVEL;
        seqinfo->group_values[2].mask = 0xe;
        seqinfo->group_values[3].mask = 0xe;
        seqinfo->group_values[2].value = pctx->lpc_data & 0xff;
        seqinfo->group_values[3].value = ctx->addr;
        seqinfo->group_values[4].value = 17;
        seqinfo->group_values[4].mask = 0;
        *_seqinfo = seqinfo;
        return LPC_SUBHANDLER_FINISH;
}

static lpc_subhandler_state_t superio_index_reg_write(struct pctx *pctx, void *_ctx,
                                           struct sequence **seqinfo)
{
        struct superio_ctx *ctx = (struct superio_ctx *)_ctx;
        ctx->haveaddr = 1;
        ctx->addr = pctx->lpc_data & 0xff;
        return LPC_SUBHANDLER_MORE_DATA_NEEDED;
}

static lpc_subhandler_state_t superio_data_reg_write(struct pctx *pctx, void *_ctx,
                                           struct sequence **_seqinfo)
{
        struct superio_ctx *ctx = (struct superio_ctx *)_ctx;
        struct sequence *seqinfo;

        if (!ctx->haveaddr)
                return LPC_SUBHANDLER_IGNORE;

        seqinfo = get_sequence(pctx);
        sprintf(seqinfo->textp, "SuperIO %04X write %02X %02X", pctx->lpc_address, ctx->addr, pctx->lpc_data & 0xff);
        seqinfo->flags = DISPLAY_ATTRIBUTE_HIGHLEVEL;
        seqinfo->group_values[2].mask = 0xe;
        seqinfo->group_values[3].mask = 0xe;
        seqinfo->group_values[2].value = pctx->lpc_data & 0xff;
        seqinfo->group_values[3].value = ctx->addr;
        seqinfo->group_values[4].value = 18;
        seqinfo->group_values[4].mask = 0;
        *_seqinfo = seqinfo;
        return LPC_SUBHANDLER_FINISH;
}

static lpc_subhandler_state_t superio_data_reg_read(struct pctx *pctx, void *_ctx,
                                           struct sequence **_seqinfo)
{
        struct superio_ctx *ctx = (struct superio_ctx *)_ctx;
        struct sequence *seqinfo;

        if (!ctx->haveaddr)
                return LPC_SUBHANDLER_IGNORE;

        seqinfo = get_sequence(pctx);
        sprintf(seqinfo->textp, "SuperIO %04X read %02X %02X", pctx->lpc_address, ctx->addr, pctx->lpc_data & 0xff);
        seqinfo->flags = DISPLAY_ATTRIBUTE_HIGHLEVEL;
        seqinfo->group_values[2].mask = 0xe;
        seqinfo->group_values[3].mask = 0xe;
        seqinfo->group_values[2].value = pctx->lpc_data & 0xff;
        seqinfo->group_values[3].value = ctx->addr;
        seqinfo->group_values[4].value = 18;
        seqinfo->group_values[4].mask = 0;
        *_seqinfo = seqinfo;
        return LPC_SUBHANDLER_FINISH;
}

static lpc_subhandler_state_t acpi_status_reg_read(struct pctx *pctx, void *_ctx,
                                           struct sequence **seqinfo)
{
        return LPC_SUBHANDLER_MORE_DATA_NEEDED;
}

static lpc_subhandler_state_t acpi_cmd_reg_write(struct pctx *pctx, void *_ctx,
                                           struct sequence **_seqinfo)
{
        struct acpi_ctx *ctx = (struct acpi_ctx *)_ctx;
        struct sequence *seqinfo;

        LogDebug(pctx, 7, "%s: called\n", __func__);

        switch(pctx->lpc_data & 0xff) {
        case EC_QUERY:
        case EC_READ:
        case EC_WRITE:
                ctx->op = pctx->lpc_data & 0xff;
                ctx->haveop = 1;
                return LPC_SUBHANDLER_MORE_DATA_NEEDED;
        default:
                seqinfo = get_sequence(pctx);
                sprintf(seqinfo->textp, "Unknown ACPI command %02X", pctx->lpc_data & 0xff);
                seqinfo->flags = DISPLAY_ATTRIBUTE_HIGHLEVEL;
                *_seqinfo = seqinfo;
                return LPC_SUBHANDLER_FINISH;
        }
}

static struct sequence *acpi_reg_name(struct pctx *pctx, int addr, int data)
{
        struct sequence *sequence = NULL, *startseq = NULL;
        struct acpi_register *reg;
        unsigned int i, bit, value;

        for (bit = 0; bit < 8; bit++) {
                for (i = 0; i < ARRAY_SIZE(acpi_registers); i++) {
                        reg = acpi_registers + i;
                        if (reg->offset == addr && reg->bitoffset == bit) {
                                if (!startseq) {
                                        startseq = get_sequence(pctx);
                                        sequence = startseq;
                                }
                                value = (data >> reg->bitoffset) & ((1 << reg->bitlen) - 1);
                                sequence->next = get_sequence(pctx);
                                sequence->flags = DISPLAY_ATTRIBUTE_HIGHLEVEL;
                                sequence->group_values[2].mask = 0xe;
                                sequence->group_values[3].mask = 0xe;
                                sequence->group_values[2].value = data;
                                sequence->group_values[3].value = addr;
                                sequence->group_values[4].value = 16;
                                sequence->group_values[4].mask = 0;
                                sprintf(sequence->textp, "  %02X:%d = %x (%s)", addr, bit, value, reg->name);
                                sequence = sequence->next;
                        }
                }
        }
        return startseq;
}

static lpc_subhandler_state_t acpi_data_reg_write(struct pctx *pctx, void *_ctx,
                                            struct sequence **_seqinfo)
{
        struct acpi_ctx *ctx = (struct acpi_ctx *)_ctx;
        struct sequence *seqinfo;
        LogDebug(pctx, 7, "%s: called\n", __func__);
        if (!ctx->haveop)
                return LPC_SUBHANDLER_IGNORE;

        if (ctx->haveaddr) {
                seqinfo = get_sequence(pctx);
                sprintf(seqinfo->textp, "%s %04X %02X %02X", acpi_op_to_string(ctx->op), pctx->lpc_address, ctx->addr, pctx->lpc_data & 0xff);
                seqinfo->flags = DISPLAY_ATTRIBUTE_HIGHLEVEL;
                seqinfo->group_values[2].mask = 0xe;
                seqinfo->group_values[3].mask = 0xe;
                seqinfo->group_values[2].value = pctx->lpc_data & 0xff;
                seqinfo->group_values[3].value = ctx->addr;
                seqinfo->group_values[4].value = 16;
                seqinfo->group_values[4].mask = 0;
                seqinfo->next = acpi_reg_name(pctx, ctx->addr, pctx->lpc_data & 0xff);
                *_seqinfo = seqinfo;
                return LPC_SUBHANDLER_FINISH;
        }
        ctx->addr = pctx->lpc_data & 0xff;
        ctx->haveaddr = 1;
        return LPC_SUBHANDLER_MORE_DATA_NEEDED;
}

static lpc_subhandler_state_t acpi_data_reg_read(struct pctx *pctx, void *_ctx,
                                           struct sequence **_seqinfo)
{
        struct sequence *seqinfo;
        struct acpi_ctx *ctx = (struct acpi_ctx *)_ctx;

        LogDebug(pctx, 7, "%s: called\n", __func__);

        if (!ctx->haveop || (!ctx->haveaddr && ctx->op != EC_QUERY))
                return LPC_SUBHANDLER_IGNORE;

        ctx->data = pctx->lpc_data & 0xff;

        seqinfo = get_sequence(pctx);
        if (ctx->op == EC_READ) {
                seqinfo->group_values[3].mask = 0xe;
                sprintf(seqinfo->textp, "EC_READ %04X %02X %02X", pctx->lpc_address, ctx->addr, ctx->data);
        }

        if (ctx->op == EC_QUERY)
                sprintf(seqinfo->textp, "EC_QUERY %04X = %02X", pctx->lpc_address, ctx->data);

        seqinfo->flags = DISPLAY_ATTRIBUTE_HIGHLEVEL;
        seqinfo->group_values[2].mask = 0xe;
        seqinfo->group_values[2].value = pctx->lpc_data & 0xff;
        seqinfo->group_values[3].value = ctx->addr;
        seqinfo->group_values[4].value = 16;
        seqinfo->group_values[4].mask = 0;
        if (ctx->op == EC_READ)
                seqinfo->next = acpi_reg_name(pctx, ctx->addr, pctx->lpc_data & 0xff);
        *_seqinfo = seqinfo;
        return LPC_SUBHANDLER_FINISH;
}

struct lpc_subdecoder lpc_subdecoders[] = {
        { ACPI_DATA_REG, LPC_TARGET, LPC_IO_WRITE, &acpi_ctx, acpi_data_reg_write },
        { ACPI_DATA_REG, LPC_TARGET, LPC_IO_READ,  &acpi_ctx, acpi_data_reg_read },
        { ACPI_CMD_REG, LPC_TARGET, LPC_IO_WRITE, &acpi_ctx, acpi_cmd_reg_write },
//        { ACPI_CMD_REG, LPC_TARGET, LPC_IO_READ, &acpi_ctx, acpi_status_reg_read },
        { ACPI_DATA_REG2, LPC_TARGET, LPC_IO_WRITE, &acpi_smm_ctx, acpi_data_reg_write },
        { ACPI_DATA_REG2, LPC_TARGET, LPC_IO_READ, &acpi_smm_ctx, acpi_data_reg_read },
        { ACPI_CMD_REG2, LPC_TARGET, LPC_IO_WRITE, &acpi_smm_ctx, acpi_cmd_reg_write },
//        { ACPI_CMD_REG2, LPC_TARGET, LPC_IO_READ, &acpi_ctx, acpi_status_reg_read },
        { 0x2e, LPC_TARGET, LPC_IO_WRITE, &superio0_ctx, superio_index_reg_write },
        { 0x2f, LPC_TARGET, LPC_IO_WRITE, &superio0_ctx, superio_data_reg_write },
        { 0x2f, LPC_TARGET, LPC_IO_READ, &superio0_ctx, superio_data_reg_read },
        { 0x164e, LPC_TARGET, LPC_IO_WRITE, &superio1_ctx, superio_index_reg_write },
        { 0x164f, LPC_TARGET, LPC_IO_WRITE, &superio1_ctx, superio_data_reg_write },
        { 0x164f, LPC_TARGET, LPC_IO_READ, &superio1_ctx, superio_data_reg_read },
        { 0x15ec, LPC_TARGET, LPC_IO_WRITE, &pmh7_ctx, pmh7_index_reg_write },
        { 0x15ee, LPC_TARGET, LPC_IO_WRITE, &pmh7_ctx, pmh7_data_reg_write },
        { 0x15ee, LPC_TARGET, LPC_IO_READ, &pmh7_ctx, pmh7_data_reg_read },

};

static int check_lpc_start(struct pctx *pctx, int seq)
{
        int ctrl, ctrl2, ctrldiff, nextseq;

        if ((nextseq = pctx->func.LAFindSeq(pctx->lactx, seq, 1, -1)) < 0)
                return 0;

        ctrl = pctx->func.LAGroupValue(pctx->lactx, seq, 0);
        ctrl2 = pctx->func.LAGroupValue(pctx->lactx, nextseq, 0);
        ctrldiff = ctrl ^ ctrl2;

        if (ctrldiff & LFRAME_N && !(ctrl & LFRAME_N)) {
                LogDebug(pctx, 8, "%s: found start at sequence %d\n", __func__, seq);
                return 1;
        }

        return 0;
}

static int falling_edge(struct pctx *pctx, int seq)
{
        int ctrl, ctrl2, ctrldiff, nextseq;

        if ((nextseq = pctx->func.LAFindSeq(pctx->lactx, seq, 1, -1)) < 0)
                return 0;

        ctrl = pctx->func.LAGroupValue(pctx->lactx, seq, 0);
        ctrl2 = pctx->func.LAGroupValue(pctx->lactx, nextseq, 0);
        ctrldiff = ctrl ^ ctrl2;

        return ctrldiff & LCLK_N && ctrl & LCLK_N;
}

static int parse_lpc(struct pctx *pctx, int startseq)
{
        int ctrl, data, ctrldiff, addrwidth, matched;
        int seq = startseq;
        unsigned int i;

        pctx->busstate = BUS_STATE_START;
        pctx->lpc_data = 0;
        pctx->lpc_address = 0;
        pctx->lpc_cycletype = 0;
        pctx->lpc_start = 0;

        do {
                ctrl = pctx->func.LAGroupValue(pctx->lactx, seq, 0);
                data = pctx->func.LAGroupValue(pctx->lactx, seq, 1);

                if (!falling_edge(pctx, seq))
                        continue;

                LogDebug(pctx, 8, "parse_lpc: seq %8d %s data %02X ctrl %02X\n",
                         seq, busstates[pctx->busstate], data, ctrl);

                switch(pctx->busstate) {

                case BUS_STATE_START:

                        if (ctrl & LFRAME_N)
                                continue;

                        pctx->clockcount = 0;
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
                                return seq+1;
                        default:
                                pctx->busstate = BUS_STATE_START;
                                return seq+1;
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
                                        pctx->busstate = pctx->lpc_cycletype & 1 ? BUS_STATE_DATA_WRITE : BUS_STATE_TAR1;
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
                                pctx->busstate = BUS_STATE_START;
                                break;
                        }

                        switch(pctx->lpc_start) {
                        case LPC_FW_READ:
                                pctx->busstate = BUS_STATE_TAR1;
                                break;

                        case LPC_FW_WRITE:
                                pctx->busstate = BUS_STATE_DATA_WRITE;
                                break;
                        default:
                                pctx->busstate = BUS_STATE_START;
                                break;
                        }
                        break;

                case BUS_STATE_TAR1:

                        if (++pctx->clockcount == 2) {
                                pctx->clockcount = 0;
                                pctx->busstate = BUS_STATE_SYNC1;
                        }
                        break;


                case BUS_STATE_TAR2:

                        if (++pctx->clockcount == 2) {
                                pctx->clockcount = 0;
                                pctx->busstate = BUS_STATE_SYNC2;
                        }
                        break;

                case BUS_STATE_TAR3:

                        if (++pctx->clockcount == 2) {
                                pctx->clockcount = 0;
                                pctx->busstate = BUS_STATE_START;
                        }

                        LogDebug(pctx, 8, "addr: %04X, data %04X\n", pctx->lpc_address, pctx->lpc_data);
                        pctx->busstate = BUS_STATE_START;
                        return seq+1;

                case BUS_STATE_SYNC1:

                        if (!data) {
                                pctx->clockcount = 0;
                                pctx->lpc_data = 0;
                                pctx->busstate = BUS_STATE_DATA_READ;
                        }
                        break;

                case BUS_STATE_SYNC2:

                        if (!data) {
                                pctx->clockcount = 0;
                                pctx->busstate = BUS_STATE_TAR3;
                        }
                        break;

                case BUS_STATE_DATA_WRITE:

                        pctx->lpc_data |= (data & 0xf) << pctx->clockcount * 4;

                        if (++pctx->clockcount== 2) {
                                pctx->clockcount = 0;
                                pctx->busstate = BUS_STATE_TAR2;
                        }
                        break;

                case BUS_STATE_DATA_READ:

                        pctx->lpc_data |= (data & 0xf) << pctx->clockcount * 4;

                        if (++pctx->clockcount== 2) {
                                pctx->busstate = BUS_STATE_TAR3;
                                pctx->clockcount = 0;
                        }
                        break;

                case BUS_STATE_ABORT:
                        if (++pctx->clockcount == 5)
                                return seq+1;
                        break;
                default:
                        LogDebug(pctx, 9, "Illegal state @%d\n", seq);
                        break;
                }
                pctx->lastdata = data;
        } while((seq = pctx->func.LAFindSeq(pctx->lactx, seq, 1, -1)) > 0);
        return 0;
}

static const char *handler_state_to_string(lpc_subhandler_state_t state)
{
        switch(state) {
        case LPC_SUBHANDLER_IGNORE:
                return "LPC_SUBHANDLER_IGNORE";
        case LPC_SUBHANDLER_MORE_DATA_NEEDED:
                return "LPC_SUBHANDLER_MORE_DATA_NEEDED";
        case LPC_SUBHANDLER_FINISH:
                return "LPC_SUBHANDLER_FINISH";

        }
	return NULL;
}

static lpc_subhandler_state_t call_handler(struct pctx *pctx, int seq, struct sequence **seqinfo)
{
        struct lpc_subdecoder *d;
        unsigned int i;
        int ret = 0;

        for (i = 0; i < ARRAY_SIZE(lpc_subdecoders); i++) {
                d = lpc_subdecoders + i;
                if (d->cycle != pctx->lpc_cycletype ||
                    d->start != pctx->lpc_start ||
                    d->address != pctx->lpc_address)
                        continue;
                ret = d->cb(pctx, d->ctx, seqinfo);
                LogDebug(pctx, 8, "call_handler: cb %p, seq %d, ret %s\n", d->cb, seq, handler_state_to_string(ret));
                return ret;
        }
        return LPC_SUBHANDLER_IGNORE;
}
struct sequence *ParseSeq(struct pctx *pctx, int seq)
{
	struct sequence *seqinfo = NULL, *seqinfo2, *retseq;
	int nextseq, data, startseq = seq, lastseq;
        int ignore = 0, cnt;

	if (!pctx) {
		LogDebug(pctx, 9, "pctx NULL\n");
		return NULL;
	}

        pctx->displayattribute = pctx->func.LAInfo(pctx->lactx, TLA_INFO_DISPLAY_ATTRIBUTE, -1);

        if (!check_lpc_start(pctx, seq))
                return NULL;

        LogDebug(pctx, 8, "ParseSeq %d\n", seq);

        memset(&acpi_ctx, 0, sizeof(acpi_ctx));
        memset(&acpi_smm_ctx, 0, sizeof(acpi_smm_ctx));
        memset(&superio0_ctx, 0, sizeof(superio0_ctx));
        memset(&pmh7_ctx, 0, sizeof(pmh7_ctx));

        cnt = 0;
        startseq = seq;
        lastseq = 0;
        while((seq = parse_lpc(pctx, seq)) > 0) {
                LogDebug(pctx, 8, "lpc: seq %8d lastseq %d cnt %d start %01X type %01X addr %08X data %08X\n",
                         seq, lastseq, cnt, pctx->lpc_start, pctx->lpc_cycletype, pctx->lpc_address, pctx->lpc_data);
                switch (call_handler(pctx, seq, &seqinfo)) {
                case LPC_SUBHANDLER_MORE_DATA_NEEDED:
                        break;
                case LPC_SUBHANDLER_IGNORE:
                        if (cnt == 0) {
                                LogDebug(pctx, 6, "lpc: not ignoring\n");
                                return make_lpc_sequence(pctx);
                        }
                        break;
                case LPC_SUBHANDLER_FINISH:
                        LogDebug(pctx, 6, "lpc: finished decoding, seqinfo = %p\n", seqinfo);
                        retseq = seqinfo;
                        if (parse_lpc(pctx, startseq)) {
                                seqinfo2 = make_lpc_sequence(pctx);
                                while(seqinfo->next)
                                        seqinfo = seqinfo->next;
                                seqinfo->next = seqinfo2;

                        }
                        return retseq;
                }
                lastseq = seq;
                cnt++;
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
        return 0;
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
        if (mode >= 0) {
                LogDebug(pctx, 5, "%s: %d (%s), %d (%s)\n", __FUNCTION__,
                         request, modeinfo[mode].name, value, onoff[value]);

                firstseq = pctx->func.LAInfo(pctx->lactx, TLA_INFO_FIRST_SEQUENCE, -1);
                lastseq = pctx->func.LAInfo(pctx->lactx, TLA_INFO_LAST_SEQUENCE, -1);
                pctx->func.LAInvalidate(pctx->lactx, -1, firstseq, lastseq);

                switch(mode) {
                case 0:
                        if (request == 1)
                                return 1;

                        if (request == 2)
                                pctx->show_unknown_cycles = value;
                        value = pctx->show_unknown_cycles;
                        break;
                case 1:
                        if (request == 1)
                                return 1;

                        if (request == 2)
                                pctx->show_aborted_cycles = value;
                        value = pctx->show_aborted_cycles;
                        break;
                default:
                        break;
                }
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
        return 1;
}
