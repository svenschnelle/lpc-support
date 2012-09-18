
#ifndef LPC_H
#define LPC_H

#include <stdint.h>
#include "TLAPlugin.h"

const char *busstates[] = {
        "BUS_STATE_START",
        "BUS_STATE_IDSEL",
        "BUS_STATE_CT_DIR",
        "BUS_STATE_ADDR",
        "BUS_STATE_TAR1",
        "BUS_STATE_TAR2",
        "BUS_STATE_TAR3",
        "BUS_STATE_SIZE",
        "BUS_STATE_SYNC1",
        "BUS_STATE_SYNC2",
        "BUS_STATE_DATA_READ",
        "BUS_STATE_DATA_WRITE",
        "BUS_STATE_MSIZE",
        "BUS_STATE_ABORT",
};

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

typedef enum {
        LPC_IO_READ,
        LPC_IO_WRITE,
        LPC_MEM_READ,
        LPC_MEM_WRITE,
        LPC_DMA_READ,
        LPC_DMA_WRITE,
} lpc_cycletype_t;

typedef enum {
        EC_READ=0x80,
        EC_WRITE=0x81,
        EC_QUERY=0x84,
} acpi_op_t;

typedef enum {
        ACPI_CMD,
        ACPI_ADDR,
        ACPI_DATA,
} acpi_state_t;

struct acpi_ctx {
        int data;
        int addr;
        int haveop:1;
        int haveaddr:1;
        acpi_op_t op;
        acpi_state_t state;
};

struct superio_ctx {
        int data;
        int addr;
        int haveaddr:1;
};

struct pmh7_ctx {
        int data;
        int addr;
        int haveaddr:1;
};

typedef enum {
        BUS_STATE_START,
        BUS_STATE_IDSEL,
        BUS_STATE_CT_DIR,
        BUS_STATE_ADDR,
        BUS_STATE_TAR1,
        BUS_STATE_TAR2,
        BUS_STATE_TAR3,
        BUS_STATE_SIZE,
        BUS_STATE_SYNC1,
        BUS_STATE_SYNC2,
        BUS_STATE_DATA_READ,
        BUS_STATE_DATA_WRITE,
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

typedef enum {
        LPC_SUBHANDLER_IGNORE,
        LPC_SUBHANDLER_MORE_DATA_NEEDED,
        LPC_SUBHANDLER_FINISH,
} lpc_subhandler_state_t;


struct pctx {
	struct lactx *lactx;
	struct lafunc func;

        int show_aborted_cycles;
        int show_unknown_cycles;
        bus_state_t busstate;

        int clockcount;

        lpc_cycletype_t lpc_cycletype;
        lpc_start_t lpc_start;
        int lpc_addrwidth;
        int lpc_datawidth;

        int lastdata;

        unsigned int lpc_address;
        unsigned int lpc_data;
        int lpc_idsel;
        int displayattribute;
};

struct lpc_subdecoder {
        unsigned int address;
        lpc_start_t start;
        lpc_cycletype_t cycle;
        void *ctx;
        lpc_subhandler_state_t  (*cb)(struct pctx *, void *, struct sequence **);
};

#endif
