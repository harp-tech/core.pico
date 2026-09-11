#ifndef CORE_REG_BITS_H
#define CORE_REG_BITS_H

struct OperationCtrlBits
{
    unsigned OP_MODE      : 2;
    unsigned HEARTBEAT_EN : 1;
    unsigned DUMP         : 1;
    unsigned MUTE_RPL     : 1;
    unsigned VISUAL_EN     : 1;
    unsigned OPLED_EN      : 1;
    unsigned ALIVE_EN     : 1;
};


struct ResetDevBits
{
    unsigned RST_DEF         : 1;
    unsigned RST_EE          : 1;
    unsigned SAVE            : 1;
    unsigned NAME_TO_DEFAULT : 1;
    unsigned                 : 1;
    unsigned UPDATE_FIRMWARE : 1;
    unsigned BOOT_DEF        : 1;
    unsigned BOOT_EE         : 1;
};

struct ClockConfigBits
{
    unsigned CLK_REP    : 1;
    unsigned CLK_GEN    : 1;
    unsigned            : 1;
    unsigned REP_ABLE   : 1;
    unsigned GEN_ABLE   : 1;
    unsigned            : 1;
    unsigned CLK_UNLOCK : 1;
    unsigned CLK_LOCK   : 1;
};

#endif // CORE_REG_BITS_H
