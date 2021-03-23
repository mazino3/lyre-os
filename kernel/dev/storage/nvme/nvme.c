#include <dev/dev.h>
#include <sys/pci.h>
#include <lib/alloc.h>
#include <lib/print.h>
#include <mm/vmm.h>

struct nvme_id_power_state {
    uint16_t        max_power;  /* centiwatts */
    uint8_t         rsvd2;
    uint8_t         flags;
    uint32_t        entry_lat;  /* microseconds */
    uint32_t        exit_lat;   /* microseconds */
    uint8_t         read_tput;
    uint8_t         read_lat;
    uint8_t         write_tput;
    uint8_t         write_lat;
    uint16_t        idle_power;
    uint8_t         idle_scale;
    uint8_t         rsvd19;
    uint16_t        active_power;
    uint8_t         active_work_scale;
    uint8_t         rsvd23[9];
};

enum {
    NVME_PS_FLAGS_MAX_POWER_SCALE   = 1 << 0,
    NVME_PS_FLAGS_NON_OP_STATE  = 1 << 1,
};

struct nvme_id_ctrl {
    uint16_t        vid;
    uint16_t        ssvid;
    char            sn[20];
    char            mn[40];
    char            fr[8];
    uint8_t         rab;
    uint8_t         ieee[3];
    uint8_t         mic;
    uint8_t         mdts;
    uint16_t        cntlid;
    uint32_t        ver;
    uint8_t         rsvd84[172];
    uint16_t        oacs;
    uint8_t         acl;
    uint8_t         aerl;
    uint8_t         frmw;
    uint8_t         lpa;
    uint8_t         elpe;
    uint8_t         npss;
    uint8_t         avscc;
    uint8_t         apsta;
    uint16_t        wctemp;
    uint16_t        cctemp;
    uint8_t         rsvd270[242];
    uint8_t         sqes;
    uint8_t         cqes;
    uint8_t         rsvd514[2];
    uint32_t        nn;
    uint16_t        oncs;
    uint16_t        fuses;
    uint8_t         fna;
    uint8_t         vwc;
    uint16_t        awun;
    uint16_t        awupf;
    uint8_t         nvscc;
    uint8_t         rsvd531;
    uint16_t        acwu;
    uint8_t         rsvd534[2];
    uint32_t        sgls;
    uint8_t         rsvd540[1508];
    struct nvme_id_power_state  psd[32];
    uint8_t         vs[1024];
};

struct nvme_lbaf {
    uint16_t            ms;
    uint8_t             ds;
    uint8_t             rp;
};

enum {
    NVME_NS_FEAT_THIN       = 1 << 0,
    NVME_NS_FLBAS_LBA_MASK  = 0xf,
    NVME_NS_FLBAS_META_EXT  = 0x10,
    NVME_LBAF_RP_BEST       = 0,
    NVME_LBAF_RP_BETTER     = 1,
    NVME_LBAF_RP_GOOD       = 2,
    NVME_LBAF_RP_DEGRADED   = 3,
    NVME_NS_DPC_PI_LAST     = 1 << 4,
    NVME_NS_DPC_PI_FIRST    = 1 << 3,
    NVME_NS_DPC_PI_TYPE3    = 1 << 2,
    NVME_NS_DPC_PI_TYPE2    = 1 << 1,
    NVME_NS_DPC_PI_TYPE1    = 1 << 0,
    NVME_NS_DPS_PI_FIRST    = 1 << 3,
    NVME_NS_DPS_PI_MASK     = 0x7,
    NVME_NS_DPS_PI_TYPE1    = 1,
    NVME_NS_DPS_PI_TYPE2    = 2,
    NVME_NS_DPS_PI_TYPE3    = 3,
};

struct nvme_id_ns {
    uint64_t            nsze;
    uint64_t            ncap;
    uint64_t            nuse;
    uint8_t             nsfeat;
    uint8_t             nlbaf;
    uint8_t             flbas;
    uint8_t             mc;
    uint8_t             dpc;
    uint8_t             dps;
    uint8_t             nmic;
    uint8_t             rescap;
    uint8_t             fpi;
    uint8_t             rsvd33;
    uint16_t            nawun;
    uint16_t            nawupf;
    uint16_t            nacwu;
    uint16_t            nabsn;
    uint16_t            nabo;
    uint16_t            nabspf;
    uint16_t            rsvd46;
    uint64_t            nvmcap[2];
    uint8_t             rsvd64[40];
    uint8_t             nguid[16];
    uint8_t             eui64[8];
    struct nvme_lbaf    lbaf[16];
    uint8_t             rsvd192[192];
    uint8_t             vs[3712];
};

/* I/O commands */

enum nvme_opcode {
    nvme_cmd_flush          = 0x00,
    nvme_cmd_write          = 0x01,
    nvme_cmd_read           = 0x02,
    nvme_cmd_write_uncor    = 0x04,
    nvme_cmd_compare        = 0x05,
    nvme_cmd_write_zeroes   = 0x08,
    nvme_cmd_dsm            = 0x09,
    nvme_cmd_resv_register  = 0x0d,
    nvme_cmd_resv_report    = 0x0e,
    nvme_cmd_resv_acquire   = 0x11,
    nvme_cmd_resv_release   = 0x15,
};

struct nvme_common_command {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            nsid;
    uint32_t            cdw2[2];
    uint64_t            metadata;
    uint64_t            prp1;
    uint64_t            prp2;
    uint32_t            cdw10[6];
};

struct nvme_rw_command {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            nsid;
    uint64_t            rsvd2;
    uint64_t            metadata;
    uint64_t            prp1;
    uint64_t            prp2;
    uint64_t            slba;
    uint16_t            length;
    uint16_t            control;
    uint32_t            dsmgmt;
    uint32_t            reftag;
    uint16_t            apptag;
    uint16_t            appmask;
};

struct nvme_dsm_cmd {
    uint8_t         opcode;
    uint8_t         flags;
    uint16_t            command_id;
    uint32_t            nsid;
    uint64_t            rsvd2[2];
    uint64_t            prp1;
    uint64_t            prp2;
    uint32_t            nr;
    uint32_t            attributes;
    uint32_t            rsvd12[4];
};

/* Admin commands */

enum nvme_admin_opcode {
    nvme_admin_delete_sq        = 0x00,
    nvme_admin_create_sq        = 0x01,
    nvme_admin_get_log_page     = 0x02,
    nvme_admin_delete_cq        = 0x04,
    nvme_admin_create_cq        = 0x05,
    nvme_admin_identify         = 0x06,
    nvme_admin_abort_cmd        = 0x08,
    nvme_admin_set_features     = 0x09,
    nvme_admin_get_features     = 0x0a,
    nvme_admin_async_event      = 0x0c,
    nvme_admin_activate_fw      = 0x10,
    nvme_admin_download_fw      = 0x11,
    nvme_admin_format_nvm       = 0x80,
    nvme_admin_security_send    = 0x81,
    nvme_admin_security_recv    = 0x82,
};

#define NVME_QUEUE_PHYS_CONTIG  (1 << 0)
#define NVME_SQ_PRIO_MEDIUM	    (2 << 1)
#define NVME_CQ_IRQ_ENABLED     (1 << 1)
#define NVME_FEAT_NUM_QUEUES	 0x07

struct nvme_identify {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            nsid;
    uint64_t            rsvd2[2];
    uint64_t            prp1;
    uint64_t            prp2;
    uint32_t            cns;
    uint32_t            rsvd11[5];
};

struct nvme_features {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            nsid;
    uint64_t            rsvd2[2];
    uint64_t            prp1;
    uint64_t            prp2;
    uint32_t            fid;
    uint32_t            dword11;
    uint32_t            rsvd12[4];
};

struct nvme_create_cq {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            rsvd1[5];
    uint64_t            prp1;
    uint64_t            rsvd8;
    uint16_t            cqid;
    uint16_t            qsize;
    uint16_t            cq_flags;
    uint16_t            irq_vector;
    uint32_t            rsvd12[4];
};

struct nvme_create_sq {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            rsvd1[5];
    uint64_t            prp1;
    uint64_t            rsvd8;
    uint16_t            sqid;
    uint16_t            qsize;
    uint16_t            sq_flags;
    uint16_t            cqid;
    uint32_t            rsvd12[4];
};

struct nvme_delete_queue {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            rsvd1[9];
    uint16_t            qid;
    uint16_t            rsvd10;
    uint32_t            rsvd11[5];
};

struct nvme_abort_cmd {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            rsvd1[9];
    uint16_t            sqid;
    uint16_t            cid;
    uint32_t            rsvd11[5];
};

struct nvme_download_firmware {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            rsvd1[5];
    uint64_t            prp1;
    uint64_t            prp2;
    uint32_t            numd;
    uint32_t            offset;
    uint32_t            rsvd12[4];
};

struct nvme_format_cmd {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            nsid;
    uint64_t            rsvd2[4];
    uint32_t            cdw10;
    uint32_t            rsvd11[5];
};

struct nvme_command {
    union {
        struct nvme_common_command common;
        struct nvme_rw_command rw;
        struct nvme_identify identify;
        struct nvme_features features;
        struct nvme_create_cq create_cq;
        struct nvme_create_sq create_sq;
        struct nvme_delete_queue delete_queue;
        struct nvme_download_firmware dlfw;
        struct nvme_format_cmd format;
        struct nvme_dsm_cmd dsm;
        struct nvme_abort_cmd abort;
    };
};

struct nvme_completion {
    uint32_t    result;     /* Used by admin commands to return data */
    uint32_t    rsvd;
    uint16_t    sq_head;    /* how much of this queue may be reclaimed */
    uint16_t    sq_id;      /* submission queue that generated this entry */
    uint16_t    command_id; /* of the command which completed */
    uint16_t    status;     /* did the command fail, and if so, why? */
};

struct nvme_bar {
    uint64_t cap;   /* Controller Capabilities */
    uint32_t vs;    /* Version */
    uint32_t intms; /* Interrupt Mask Set */
    uint32_t intmc; /* Interrupt Mask Clear */
    uint32_t cc;    /* Controller Configuration */
    uint32_t rsvd1; /* Reserved */
    uint32_t csts;  /* Controller Status */
    uint32_t rsvd2; /* Reserved */
    uint32_t aqa;   /* Admin Queue Attributes */
    uint64_t asq;   /* Admin SQ Base Address */
    uint64_t acq;   /* Admin CQ Base Address */
};

#define NVME_CAP_MQES(cap)      ((cap) & 0xffff)
#define NVME_CAP_TIMEOUT(cap)   (((cap) >> 24) & 0xff)
#define NVME_CAP_STRIDE(cap)    (((cap) >> 32) & 0xf)
#define NVME_CAP_MPSMIN(cap)    (((cap) >> 48) & 0xf)
#define NVME_CAP_MPSMAX(cap)    (((cap) >> 52) & 0xf)

enum {
    NVME_CC_ENABLE          = 1 << 0,
    NVME_CC_CSS_NVM         = 0 << 4,
    NVME_CC_MPS_SHIFT       = 7,
    NVME_CC_ARB_RR          = 0 << 11,
    NVME_CC_ARB_WRRU        = 1 << 11,
    NVME_CC_ARB_VS          = 7 << 11,
    NVME_CC_SHN_NONE        = 0 << 14,
    NVME_CC_SHN_NORMAL      = 1 << 14,
    NVME_CC_SHN_ABRUPT      = 2 << 14,
    NVME_CC_SHN_MASK        = 3 << 14,
    NVME_CC_IOSQES          = 6 << 16,
    NVME_CC_IOCQES          = 4 << 20,
    NVME_CSTS_RDY           = 1 << 0,
    NVME_CSTS_CFS           = 1 << 1,
    NVME_CSTS_SHST_NORMAL   = 0 << 2,
    NVME_CSTS_SHST_OCCUR    = 1 << 2,
    NVME_CSTS_SHST_CMPLT    = 2 << 2,
    NVME_CSTS_SHST_MASK     = 3 << 2,
};

struct nvme_queue {
    volatile struct nvme_command *submit;
    volatile struct nvme_completion *completion;
    volatile uint32_t *submit_db;
    volatile uint32_t *complete_db;
    uint16_t queue_elements;
    uint16_t cq_vector;
    uint16_t sq_head;
    uint16_t sq_tail;
    uint16_t cq_head;
    uint8_t  cq_phase;
    uint16_t qid;
    uint32_t command_id;

    uint64_t *prps;
};

struct nvme_device {
    volatile struct nvme_bar *nvme_base;
    size_t doorbell_stride;
    size_t queue_slots;
    size_t lba_size;
    struct nvme_queue queues[2];
    int max_prps;
    int max_lba;
    size_t overwritten_slot;
    size_t num_lbas;
    size_t max_transfer_shift;
    void *jumpbuf;
};

struct nvme_resource {
    struct resource;
    size_t dev;
};

DYNARRAY_NEW(struct nvme_device *, nvme_devices);

static void nvme_initialize_queue(int device, struct nvme_queue *queue, size_t queue_slots, size_t qid) {
    queue->submit = alloc(sizeof(struct nvme_command) * queue_slots);
    queue->completion = alloc(sizeof(struct nvme_completion) * queue_slots);
    queue->submit_db = (uint32_t*)((size_t)nvme_devices.storage[device]->nvme_base + PAGE_SIZE + (2 * qid * (4 << nvme_devices.storage[device]->doorbell_stride)));
    queue->complete_db = (uint32_t*)((size_t)nvme_devices.storage[device]->nvme_base + PAGE_SIZE + ((2 * qid + 1) * (4 << nvme_devices.storage[device]->doorbell_stride)));
    queue->queue_elements = queue_slots;
    queue->cq_vector = 0;
    queue->sq_head = 0;
    queue->sq_tail = 0;
    queue->cq_head = 0;
    queue->cq_phase = 1;
    queue->qid = qid;
    queue->command_id = 0;

    queue->prps = alloc(nvme_devices.storage[device]->max_prps * queue_slots * sizeof(uint64_t));
}

static void nvme_submit_cmd(struct nvme_queue *queue, struct nvme_command command) {
    uint16_t tail = queue->sq_tail;
    queue->submit[tail] = command;
    tail++;
    if (tail == queue->queue_elements)
        tail = 0;
    *(queue->submit_db) = tail;
    queue->sq_tail = tail;
}

static uint16_t nvme_submit_cmd_wait(struct nvme_queue *queue, struct nvme_command command) {
    uint16_t head = queue->cq_head;
    uint16_t phase = queue->cq_phase;
    command.common.command_id = queue->command_id++;
    nvme_submit_cmd(queue, command);
    uint16_t status = 0;

    while(1) {
        status = queue->completion[queue->cq_head].status;
        if ((status & 0x1) == phase) {
            break;
        }
    }

    status >>= 1;
    if (status) {
        print("nvme: command error %X", status);
        return status;
    }

    head++;
    if (head == queue->queue_elements) {
        head = 0;
        queue->cq_phase = !(queue->cq_phase);
    }
    *(queue->complete_db) = head;
    queue->cq_head = head;

    return status;
}

static int nvme_identify(int device, struct nvme_id_ctrl *id) {
    int length = sizeof(struct nvme_id_ctrl);
    struct nvme_command command = {0};
    command.identify.opcode = nvme_admin_identify;
    command.identify.nsid = 0;
    command.identify.cns = 1;
    command.identify.prp1 = (size_t)id - MEM_PHYS_OFFSET;
    int offset = (size_t)id & (PAGE_SIZE - 1);
    length -= (PAGE_SIZE - offset);
    if (length <= 0) {
        command.identify.prp2 = (size_t)0;
    } else {
        size_t addr = (size_t)id + (PAGE_SIZE - offset);
        command.identify.prp2 = (size_t)addr;
    }

    uint16_t status = nvme_submit_cmd_wait(&nvme_devices.storage[device]->queues[0], command);
    if (status != 0) {
        return -1;
    }

    int shift = 12 + NVME_CAP_MPSMIN(nvme_devices.storage[device]->nvme_base->cap);
    size_t max_transf_shift;
    if(id->mdts) {
        max_transf_shift = shift + id->mdts;
    } else {
        max_transf_shift = 20;
    }
    nvme_devices.storage[device]->max_transfer_shift = max_transf_shift;
    return 0;
}

static int nvme_get_ns_info(int device, int ns_num, struct nvme_id_ns *id_ns) {
    struct nvme_command command = {0};
    command.identify.opcode = nvme_admin_identify;
    command.identify.nsid = ns_num;
    command.identify.cns = 0;
    command.identify.prp1 = (size_t)id_ns - MEM_PHYS_OFFSET;

    uint16_t status = nvme_submit_cmd_wait(&nvme_devices.storage[device]->queues[0], command);
    if (status != 0) {
        return -1;
    }

    return 0;
}

static int nvme_set_queue_count(int device, int count) {
    struct nvme_command command = {0};
    command.features.opcode = nvme_admin_set_features;
    command.features.prp1 = 0;
    command.features.fid = NVME_FEAT_NUM_QUEUES;
    command.features.dword11 = (count - 1) | ((count - 1) << 16);
    uint16_t status = nvme_submit_cmd_wait(&nvme_devices.storage[device]->queues[0], command);
    if (status != 0) {
        return -1;
    }
    return 0;
}

static int nvme_create_queue_pair(int device, uint16_t qid) {
    nvme_set_queue_count(device, 4);
    nvme_initialize_queue(device, &nvme_devices.storage[device]->queues[1], nvme_devices.storage[device]->queue_slots, 1);
    struct nvme_command cq_command = {0};
    cq_command.create_cq.opcode = nvme_admin_create_cq;
    cq_command.create_cq.prp1 = ((size_t)((nvme_devices.storage[device]->queues[1].completion))) - MEM_PHYS_OFFSET;
    cq_command.create_cq.cqid = qid;
    cq_command.create_cq.qsize = nvme_devices.storage[device]->queue_slots - 1;
    cq_command.create_cq.cq_flags = NVME_QUEUE_PHYS_CONTIG;
    cq_command.create_cq.irq_vector = 0;
    uint16_t status = nvme_submit_cmd_wait(&nvme_devices.storage[device]->queues[0], cq_command);
    if (status != 0) {
        return -1;
    }

    struct nvme_command sq_command = {0};
    sq_command.create_sq.opcode = nvme_admin_create_sq;
    sq_command.create_sq.prp1 = ((size_t)((nvme_devices.storage[device]->queues[1].submit)) - MEM_PHYS_OFFSET);
    sq_command.create_sq.sqid = qid;
    sq_command.create_sq.cqid = qid;
    sq_command.create_sq.qsize = nvme_devices.storage[device]->queue_slots - 1;
    sq_command.create_sq.sq_flags = NVME_QUEUE_PHYS_CONTIG | NVME_SQ_PRIO_MEDIUM;
    status = nvme_submit_cmd_wait(&nvme_devices.storage[device]->queues[0], sq_command);
    if (status != 0) {
        return -1;
    }
    return 0;
 }

static int nvme_rw_lba(int device, void *buf, size_t lba_start, size_t lba_count, int write) {
    if (lba_start + lba_count >= nvme_devices.storage[device]->num_lbas) {
        lba_count -= (lba_start + lba_count) - nvme_devices.storage[device]->num_lbas;
    }
    int page_offset = (size_t)buf & (PAGE_SIZE - 1);
    int use_prp2 = 0;
    int use_prp_list = 0;
    uint32_t command_id = nvme_devices.storage[device]->queues[1].command_id;
    if ((lba_count * nvme_devices.storage[device]->lba_size) > PAGE_SIZE) {
        if ((lba_count * nvme_devices.storage[device]->lba_size) > (PAGE_SIZE * 2)) {
            int prp_num = ((lba_count - 1) * nvme_devices.storage[device]->lba_size) / PAGE_SIZE;
            if (prp_num > nvme_devices.storage[device]->max_prps) {
                print("nvme: max prps exceeded");
                return -1;
            }
            for(int i = 0; i < prp_num; i++) {
                nvme_devices.storage[device]->queues[1].prps[i + command_id * nvme_devices.storage[device]->max_prps] = ((size_t)(buf - MEM_PHYS_OFFSET - page_offset) + PAGE_SIZE + i * PAGE_SIZE);
            }
            use_prp2 = 0;
            use_prp_list = 1;
        } else {
            use_prp2 = 1;
        }
    }
    struct nvme_command command = {0};
    command.rw.opcode = write ? nvme_cmd_write : nvme_cmd_read;
    command.rw.flags = 0;
    command.rw.nsid = 1;
    command.rw.control = 0;
    command.rw.dsmgmt = 0;
    command.rw.reftag = 0;
    command.rw.apptag = 0;
    command.rw.appmask = 0;
    command.rw.metadata = 0;
    command.rw.slba = lba_start;
    command.rw.length = lba_count - 1;
    if (use_prp_list) {
        command.rw.prp1 = (size_t)((size_t)buf - MEM_PHYS_OFFSET);
        command.rw.prp2 = (size_t)(&nvme_devices.storage[device]->queues[1].prps[command_id * nvme_devices.storage[device]->max_prps]) - MEM_PHYS_OFFSET;
    } else if(use_prp2) {
        command.rw.prp2 = (size_t)((size_t)(buf) + PAGE_SIZE - MEM_PHYS_OFFSET);
    } else {
        command.rw.prp1 = (size_t)((size_t)buf - MEM_PHYS_OFFSET);
    }
    uint16_t status = nvme_submit_cmd_wait(&nvme_devices.storage[device]->queues[1], command);
    if (status != 0) {
        print("nvme: read/write operation failed with status %x", status);
        return -1;
    }
    return 0;
}

ssize_t nvme_read(struct resource *this, void *buf, off_t loc, size_t count) {
    struct nvme_resource *this_nvme = (struct nvme_resource*)this;
    size_t lba_size = nvme_devices.storage[this_nvme->dev]->lba_size;
    size_t num = this_nvme->dev;
    if ((loc % lba_size) != 0 || ((count % lba_size) != 0)) {
        print("unaligned access to lba\n");
        return -1;
    }

    size_t jbuf_size = nvme_devices.storage[num]->max_lba * lba_size;

    for (size_t i = 0; i < count; i += jbuf_size) {
        size_t lba_count = (count - i < jbuf_size ? count - i : jbuf_size) / lba_size;

        nvme_rw_lba(this_nvme->dev, nvme_devices.storage[num]->jumpbuf,
                    loc + i / lba_size, lba_count, 0);

        memcpy(buf + i, nvme_devices.storage[num]->jumpbuf, lba_count * lba_size);
    }
}

ssize_t nvme_write(struct resource *this, void *buf, off_t loc, size_t count) {
    struct nvme_resource *this_nvme = (struct nvme_resource*)this;
    size_t lba_size = nvme_devices.storage[this_nvme->dev]->lba_size;
    size_t num = this_nvme->dev;
    if ((loc % lba_size) != 0 || ((count % lba_size) != 0)) {
        print("unaligned access to lba\n");
        return -1;
    }

    size_t jbuf_size = nvme_devices.storage[num]->max_lba * lba_size;

    for (size_t i = 0; i < count; i += jbuf_size) {
        size_t lba_count = (count - i < jbuf_size ? count - i : jbuf_size) / lba_size;

        nvme_rw_lba(this_nvme->dev, nvme_devices.storage[num]->jumpbuf,
                    loc + i / lba_size, lba_count, 1);

        memcpy(buf + i, nvme_devices.storage[num]->jumpbuf, lba_count * lba_size);
    }
}

static bool nvme_init(struct pci_device *ndevice) {
    print("nvme: initializing nvme device\n");

    struct nvme_device *device = alloc(sizeof(struct nvme_device));
    struct pci_bar_t bar = {0};
    pci_read_bar(ndevice, 0, &bar);

    volatile struct nvme_bar *nvme_base = (struct nvme_bar*)(bar.base + MEM_PHYS_OFFSET);
    device->nvme_base = nvme_base;
    pci_enable_busmastering(ndevice);

    //Enable mmio.
    pci_write_device_dword(ndevice, 0x4, pci_read_device_dword(ndevice, 0x4) | (1 << 1));

    //The controller needs to be disabled in order to configure the admin queues.
    print("nvme: disabling controller\n");
    uint32_t cc = nvme_base->cc;
    if (cc & NVME_CC_ENABLE) {
        cc &= ~NVME_CC_ENABLE;
        nvme_base->cc = cc;
    }
    while(((nvme_base->csts) & NVME_CSTS_RDY) != 0){};
    print("nvme: controller disabled\n");

    device->queue_slots = NVME_CAP_MQES(nvme_base->cap);
    device->doorbell_stride = NVME_CAP_STRIDE(nvme_base->cap);
    size_t num = DYNARRAY_INSERT(nvme_devices, device);
    nvme_initialize_queue(num, &nvme_devices.storage[num]->queues[0], nvme_devices.storage[num]->queue_slots, 0);

    //Initialize admin queues
    //admin queue attributes
    uint32_t aqa = nvme_devices.storage[num]->queue_slots - 1;
    aqa |= aqa << 16;
    aqa |= aqa << 16;
    nvme_base->aqa = aqa;
    cc = NVME_CC_CSS_NVM
        | NVME_CC_ARB_RR | NVME_CC_SHN_NONE
        | NVME_CC_IOSQES | NVME_CC_IOCQES
        | NVME_CC_ENABLE;
    nvme_base->asq = (size_t)nvme_devices.storage[num]->queues[0].submit - MEM_PHYS_OFFSET;
    nvme_base->acq = (size_t)nvme_devices.storage[num]->queues[0].completion - MEM_PHYS_OFFSET;
    nvme_base->cc = cc;
    //enable the controller and wait for it to be ready.
    while(1) {
        uint32_t status = nvme_base->csts;
        if (status & NVME_CSTS_RDY) {
            break;
        } else if (status & NVME_CSTS_CFS) {
            print("nvme: controller fatal status\n");
            return -1;
        }
    }
    print("nvme: controller restarted\n");

    struct nvme_id_ctrl *id = (struct nvme_id_ctrl *)alloc(sizeof(struct nvme_id_ctrl));
    int status = nvme_identify(num, id);
    if (status != 0) {
        print("nvme: Failed to identify NVME device\n");
        return -1;
    }

	struct nvme_id_ns *id_ns = alloc(sizeof(struct nvme_id_ns));
    nvme_get_ns_info(num, 1, id_ns);
    if (status != 0) {
        print("nvme: Failed to get namespace info for namespace 1\n");
        return -1;
    }

    //The maximum transfer size is in units of 2^(min page size)
    size_t lba_shift = id_ns->lbaf[id_ns->flbas & NVME_NS_FLBAS_LBA_MASK].ds;
    size_t max_lbas = 1 << (nvme_devices.storage[num]->max_transfer_shift - lba_shift);
    nvme_devices.storage[num]->max_prps = (max_lbas * (1 << lba_shift)) / PAGE_SIZE;
    nvme_devices.storage[num]->max_lba = max_lbas;

    status = nvme_create_queue_pair(num, 1);
    if (status != 0) {
        print("nvme: Failed to create i/o queues\n");
        return -1;
    }

    size_t formatted_lba = id_ns->flbas & NVME_NS_FLBAS_LBA_MASK;
    nvme_devices.storage[num]->lba_size = 1 << id_ns->lbaf[formatted_lba].ds;
    print("nvme: namespace 1 size %X lbas, lba size: %X bytes\n", id_ns->nsze, nvme_devices.storage[num]->lba_size);
    nvme_devices.storage[num]->num_lbas = id_ns->nsze;
    nvme_devices.storage[num]->jumpbuf = alloc(max_lbas * nvme_devices.storage[num]->lba_size);

    int n = num;
    int count = 0;

    struct nvme_resource *nvdev = resource_create(sizeof(struct nvme_resource));
    nvdev->dev = num;
    nvdev->write = nvme_write;
    nvdev->read = nvme_read;

    while(n) {
        n /= 10;
        count++;
    }

    char name[10 + count];
    snprint(name, 10 + count, "nvme%d", num);

    dev_add_new(nvdev, name);

    return true;
}

PCI_CLASS_DRIVER(
        nvme_init, {0x01, 0x08, 0x02});
