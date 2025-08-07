#include <stdio.h>

enum node_tag { T_FREE, T_IND, T_AP, T_INT, T_INT64X, T_DBL, T_FLT32, T_PTR, T_FUNPTR, T_FORPTR, T_BADDYN, T_ARR, T_THID, T_MVAR,
                T_S, T_K, T_I, T_B, T_C,
                T_A, T_Y, T_SS, T_BB, T_CC, T_P, T_R, T_O, T_U, T_Z, T_J,
                T_K2, T_K3, T_K4, T_CCB,
                T_ADD, T_SUB, T_MUL, T_QUOT, T_REM, T_SUBR, T_UQUOT, T_UREM, T_NEG,
                T_AND, T_OR, T_XOR, T_INV, T_SHL, T_SHR, T_ASHR,
                T_POPCOUNT, T_CLZ, T_CTZ,
                T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE, T_ULT, T_ULE, T_UGT, T_UGE, T_ICMP, T_UCMP
};

int main() {
    printf("T_I = %d\n", T_I);
    printf("T_K = %d\n", T_K);
    printf("T_A = %d\n", T_A);
    printf("T_ADD = %d\n", T_ADD);
    printf("T_SUB = %d\n", T_SUB);
    printf("T_MUL = %d\n", T_MUL);
    printf("T_QUOT = %d\n", T_QUOT);
    printf("T_REM = %d\n", T_REM);
    printf("T_NEG = %d\n", T_NEG);
    printf("T_EQ = %d\n", T_EQ);
    printf("T_NE = %d\n", T_NE);
    printf("T_LT = %d\n", T_LT);
    printf("T_LE = %d\n", T_LE);
    printf("T_GT = %d\n", T_GT);
    printf("T_GE = %d\n", T_GE);
    printf("T_ICMP = %d\n", T_ICMP);
    return 0;
}
