#include "VSimTop__Dpi.h"
#include "coremark_common.h"
CoremarkState coremark_state;
extern "C" void v_difftest_ArchEvent(char io_coreid, int io_intrNO, int io_cause, long long io_exceptionPC, int io_exceptionInst) {
coremark_state.exception={uint64_t(io_intrNO),uint64_t(io_cause),uint64_t(io_exceptionPC),uint64_t(io_exceptionInst)};
}
extern "C" void v_difftest_ArchFpRegState(char io_coreid, long long io_fpr_0, long long io_fpr_1, long long io_fpr_2, long long io_fpr_3, long long io_fpr_4, long long io_fpr_5, long long io_fpr_6, long long io_fpr_7, long long io_fpr_8, long long io_fpr_9, long long io_fpr_10, long long io_fpr_11, long long io_fpr_12, long long io_fpr_13, long long io_fpr_14, long long io_fpr_15, long long io_fpr_16, long long io_fpr_17, long long io_fpr_18, long long io_fpr_19, long long io_fpr_20, long long io_fpr_21, long long io_fpr_22, long long io_fpr_23, long long io_fpr_24, long long io_fpr_25, long long io_fpr_26, long long io_fpr_27, long long io_fpr_28, long long io_fpr_29, long long io_fpr_30, long long io_fpr_31) {
coremark_state.fpr[0]=io_fpr_0;
coremark_state.fpr[1]=io_fpr_1;
coremark_state.fpr[2]=io_fpr_2;
coremark_state.fpr[3]=io_fpr_3;
coremark_state.fpr[4]=io_fpr_4;
coremark_state.fpr[5]=io_fpr_5;
coremark_state.fpr[6]=io_fpr_6;
coremark_state.fpr[7]=io_fpr_7;
coremark_state.fpr[8]=io_fpr_8;
coremark_state.fpr[9]=io_fpr_9;
coremark_state.fpr[10]=io_fpr_10;
coremark_state.fpr[11]=io_fpr_11;
coremark_state.fpr[12]=io_fpr_12;
coremark_state.fpr[13]=io_fpr_13;
coremark_state.fpr[14]=io_fpr_14;
coremark_state.fpr[15]=io_fpr_15;
coremark_state.fpr[16]=io_fpr_16;
coremark_state.fpr[17]=io_fpr_17;
coremark_state.fpr[18]=io_fpr_18;
coremark_state.fpr[19]=io_fpr_19;
coremark_state.fpr[20]=io_fpr_20;
coremark_state.fpr[21]=io_fpr_21;
coremark_state.fpr[22]=io_fpr_22;
coremark_state.fpr[23]=io_fpr_23;
coremark_state.fpr[24]=io_fpr_24;
coremark_state.fpr[25]=io_fpr_25;
coremark_state.fpr[26]=io_fpr_26;
coremark_state.fpr[27]=io_fpr_27;
coremark_state.fpr[28]=io_fpr_28;
coremark_state.fpr[29]=io_fpr_29;
coremark_state.fpr[30]=io_fpr_30;
coremark_state.fpr[31]=io_fpr_31;
}
extern "C" void v_difftest_ArchIntRegState(char io_coreid, long long io_gpr_0, long long io_gpr_1, long long io_gpr_2, long long io_gpr_3, long long io_gpr_4, long long io_gpr_5, long long io_gpr_6, long long io_gpr_7, long long io_gpr_8, long long io_gpr_9, long long io_gpr_10, long long io_gpr_11, long long io_gpr_12, long long io_gpr_13, long long io_gpr_14, long long io_gpr_15, long long io_gpr_16, long long io_gpr_17, long long io_gpr_18, long long io_gpr_19, long long io_gpr_20, long long io_gpr_21, long long io_gpr_22, long long io_gpr_23, long long io_gpr_24, long long io_gpr_25, long long io_gpr_26, long long io_gpr_27, long long io_gpr_28, long long io_gpr_29, long long io_gpr_30, long long io_gpr_31) {
coremark_state.gpr[0]=io_gpr_0;
coremark_state.gpr[1]=io_gpr_1;
coremark_state.gpr[2]=io_gpr_2;
coremark_state.gpr[3]=io_gpr_3;
coremark_state.gpr[4]=io_gpr_4;
coremark_state.gpr[5]=io_gpr_5;
coremark_state.gpr[6]=io_gpr_6;
coremark_state.gpr[7]=io_gpr_7;
coremark_state.gpr[8]=io_gpr_8;
coremark_state.gpr[9]=io_gpr_9;
coremark_state.gpr[10]=io_gpr_10;
coremark_state.gpr[11]=io_gpr_11;
coremark_state.gpr[12]=io_gpr_12;
coremark_state.gpr[13]=io_gpr_13;
coremark_state.gpr[14]=io_gpr_14;
coremark_state.gpr[15]=io_gpr_15;
coremark_state.gpr[16]=io_gpr_16;
coremark_state.gpr[17]=io_gpr_17;
coremark_state.gpr[18]=io_gpr_18;
coremark_state.gpr[19]=io_gpr_19;
coremark_state.gpr[20]=io_gpr_20;
coremark_state.gpr[21]=io_gpr_21;
coremark_state.gpr[22]=io_gpr_22;
coremark_state.gpr[23]=io_gpr_23;
coremark_state.gpr[24]=io_gpr_24;
coremark_state.gpr[25]=io_gpr_25;
coremark_state.gpr[26]=io_gpr_26;
coremark_state.gpr[27]=io_gpr_27;
coremark_state.gpr[28]=io_gpr_28;
coremark_state.gpr[29]=io_gpr_29;
coremark_state.gpr[30]=io_gpr_30;
coremark_state.gpr[31]=io_gpr_31;
}
extern "C" void v_difftest_AtomicEvent(char io_coreid, svBit io_atomicResp, long long io_atomicAddr, long long io_atomicData, char io_atomicMask, char io_atomicFuop, long long io_atomicOut) {
}
extern "C" void v_difftest_CSRState(char io_coreid, char io_priviledgeMode, long long io_mstatus, long long io_sstatus, long long io_mepc, long long io_sepc, long long io_mtval, long long io_stval, long long io_mtvec, long long io_stvec, long long io_mcause, long long io_scause, long long io_satp, long long io_mip, long long io_mie, long long io_mscratch, long long io_sscratch, long long io_mideleg, long long io_medeleg) {
coremark_state.csr={uint64_t(io_priviledgeMode), uint64_t(io_mstatus), uint64_t(io_sstatus), uint64_t(io_mepc), uint64_t(io_sepc), uint64_t(io_mtval), uint64_t(io_stval), uint64_t(io_mtvec), uint64_t(io_stvec), uint64_t(io_mcause), uint64_t(io_scause), uint64_t(io_satp), uint64_t(io_mip), uint64_t(io_mie), uint64_t(io_mscratch), uint64_t(io_sscratch), uint64_t(io_mideleg), uint64_t(io_medeleg)};
}
extern "C" void v_difftest_DebugMode(char io_coreid, svBit io_debugMode, long long io_dcsr, long long io_dpc, long long io_dscratch0, long long io_dscratch1) {
}
extern "C" void v_difftest_FpWriteback(char io_coreid, svBit io_valid, int io_dest, long long io_data) {
}
extern "C" void v_difftest_InstrCommit(char io_coreid, char io_index, svBit io_valid, char io_special, svBit io_skip, svBit io_isRVC, svBit io_rfwen, svBit io_fpwen, int io_wpdest, char io_wdest, long long io_pc, int io_instr) {
if(io_coreid!=0)__builtin_abort();
if(io_valid)coremark_state.commits.push_back({unsigned(static_cast<unsigned char>(io_index)), unsigned(static_cast<unsigned char>(io_special)), unsigned(static_cast<unsigned char>(io_skip)), unsigned(static_cast<unsigned char>(io_isRVC)), unsigned(static_cast<unsigned char>(io_fpwen)), unsigned(static_cast<unsigned char>(io_rfwen)), unsigned(static_cast<unsigned char>(io_wdest)), unsigned(io_wpdest), uint64_t(io_pc), uint32_t(io_instr)});
}
extern "C" void v_difftest_IntWriteback(char io_coreid, svBit io_valid, int io_dest, long long io_data) {
}
extern "C" void v_difftest_LoadEvent(char io_coreid, char io_index, svBit io_valid, long long io_paddr, char io_opType, char io_fuType) {
}
extern "C" void v_difftest_LrScEvent(char io_coreid, svBit io_valid, svBit io_success) {
}
extern "C" void v_difftest_RefillEvent(char io_coreid, svBit io_valid, long long io_addr, long long io_data_0, long long io_data_1, long long io_data_2, long long io_data_3, long long io_data_4, long long io_data_5, long long io_data_6, long long io_data_7, svBit io_cacheid) {
}
extern "C" void v_difftest_RunaheadCommitEvent(char io_coreid, char io_index, svBit io_valid, long long io_pc) {
}
extern "C" void v_difftest_SbufferEvent(char io_coreid, char io_index, svBit io_sbufferResp, long long io_sbufferAddr, char io_sbufferData_0, char io_sbufferData_1, char io_sbufferData_2, char io_sbufferData_3, char io_sbufferData_4, char io_sbufferData_5, char io_sbufferData_6, char io_sbufferData_7, char io_sbufferData_8, char io_sbufferData_9, char io_sbufferData_10, char io_sbufferData_11, char io_sbufferData_12, char io_sbufferData_13, char io_sbufferData_14, char io_sbufferData_15, char io_sbufferData_16, char io_sbufferData_17, char io_sbufferData_18, char io_sbufferData_19, char io_sbufferData_20, char io_sbufferData_21, char io_sbufferData_22, char io_sbufferData_23, char io_sbufferData_24, char io_sbufferData_25, char io_sbufferData_26, char io_sbufferData_27, char io_sbufferData_28, char io_sbufferData_29, char io_sbufferData_30, char io_sbufferData_31, char io_sbufferData_32, char io_sbufferData_33, char io_sbufferData_34, char io_sbufferData_35, char io_sbufferData_36, char io_sbufferData_37, char io_sbufferData_38, char io_sbufferData_39, char io_sbufferData_40, char io_sbufferData_41, char io_sbufferData_42, char io_sbufferData_43, char io_sbufferData_44, char io_sbufferData_45, char io_sbufferData_46, char io_sbufferData_47, char io_sbufferData_48, char io_sbufferData_49, char io_sbufferData_50, char io_sbufferData_51, char io_sbufferData_52, char io_sbufferData_53, char io_sbufferData_54, char io_sbufferData_55, char io_sbufferData_56, char io_sbufferData_57, char io_sbufferData_58, char io_sbufferData_59, char io_sbufferData_60, char io_sbufferData_61, char io_sbufferData_62, char io_sbufferData_63, long long io_sbufferMask) {
}
extern "C" void v_difftest_StoreEvent(char io_coreid, char io_index, svBit io_valid, long long io_storeAddr, long long io_storeData, char io_storeMask) {
}
extern "C" void v_difftest_TrapEvent(char io_coreid, svBit io_valid, long long io_cycleCnt, long long io_instrCnt, svBit io_hasWFI, char io_code, long long io_pc) {
if(io_valid){coremark_state.trap=true;coremark_state.trap_pc=io_pc;coremark_state.trap_code=static_cast<unsigned char>(io_code);}
}
