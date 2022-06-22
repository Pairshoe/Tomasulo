#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE   255
#define LABLE_SIZE 255
#define MAP_SIZE   255

#define REG_LEN  5
#define IMM_LEN  16
#define ADDR_LEN 26

#define LW_OPCODE   "100011"
#define SW_OPCODE   "101011"
#define ADD_OPCODE  "000000"
#define ADDI_OPCODE "001000"
#define SUB_OPCODE  "000000"
#define AND_OPCODE  "000000"
#define ANDI_OPCODE "001100"
#define BEQZ_OPCODE "000100"
#define J_OPCODE    "000010"
#define HALT_OPCODE "000001"
#define NOOP_OPCODE "000011"

#define ADD_FUNC "00000100000"
#define SUB_FUNC "00000100010"
#define AND_FUNC "00000100100"

void decToBin(int dec, char *buffer, int buffer_size) {
    buffer += buffer_size;
    *buffer-- = '\0';
    for (int i = buffer_size - 1; 0 <= i; i--) {
        *buffer-- = (dec & 1) + '0';
        dec >>= 1;
    }
}

int binToDec(char *bin) {
    int dec = 0;
    for (int i = 0; i < 31; i++) {
        dec += bin[i] - '0';
        dec <<= 1;
    }
    dec += bin[31] - '0';
    return dec;
}

int id = 0;
struct labelToAddr {
    char label[LABLE_SIZE];
    int address;
} map[MAP_SIZE];

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("error: usage: %s <assemble-code file> <machine-code file>\n", argv[0]);
        exit(0);
    }

    // read assemble-code file
    FILE *fin, *fout;
    fin = fopen(argv[1], "r");
    if (fin == NULL) {
        printf("error: can't open file %s", argv[1]);
        perror("fopen");
        exit(1);
    }

    int address = 0;
    char buf[BUF_SIZE];
    while (fgets(buf, BUF_SIZE, fin)) {
        char *op = strtok(buf, " \n\r");
        if (strcmp(op, "lw") != 0 && strcmp(op, "sw") != 0 && strcmp(op, "add") != 0 &&
            strcmp(op, "addi") != 0 && strcmp(op, "sub") != 0 && strcmp(op, "and") != 0 &&
            strcmp(op, "andi") != 0 && strcmp(op, "beqz") != 0 && strcmp(op, "j") != 0 &&
            strcmp(op, "halt") != 0 && strcmp(op, "noop") != 0) {
            strcpy(map[id].label, op);
            map[id++].address = address;
        }
        address++;
    }

    fclose(fin);

    fin = fopen(argv[1], "r");
    fout = fopen("tmp.txt", "w");

    address = 0;
    while (fgets(buf, BUF_SIZE, fin)) {
        printf("%s", buf);
        char *op = strtok(buf, " \n\r");
        if (strcmp(op, "lw") != 0 && strcmp(op, "sw") != 0 && strcmp(op, "add") != 0 &&
            strcmp(op, "addi") != 0 && strcmp(op, "sub") != 0 && strcmp(op, "and") != 0 &&
            strcmp(op, "andi") != 0 && strcmp(op, "beqz") != 0 && strcmp(op, "j") != 0 &&
            strcmp(op, "halt") != 0 && strcmp(op, "noop") != 0) {
            op = strtok(NULL, " \n\r");
        }
        if (strcmp(op, "lw") == 0 || strcmp(op, "sw") == 0 || strcmp(op, "addi") == 0 || 
            strcmp(op, "andi") == 0) {
            char *rd = strtok(NULL, ",");
            char *rs1 = strtok(NULL, ",");
            char *imm = strtok(NULL, " \n\r");
            char rd_buf[REG_LEN + 1];
            char rs1_buf[REG_LEN + 1];
            char imm_buf[IMM_LEN + 1];
            decToBin(atoi(rd + 1), rd_buf, REG_LEN);
            decToBin(atoi(rs1 + 1), rs1_buf, REG_LEN);
            decToBin(atoi(imm), imm_buf, IMM_LEN);
            if (strcmp(op, "lw") == 0) {
                fprintf(fout, LW_OPCODE"%s%s%s\n", rs1_buf, rd_buf, imm_buf);
            } else if (strcmp(op, "sw") == 0) {
                fprintf(fout, SW_OPCODE"%s%s%s\n", rs1_buf, rd_buf, imm_buf);
            } else if (strcmp(op, "addi") == 0) {
                fprintf(fout, ADDI_OPCODE"%s%s%s\n", rs1_buf, rd_buf, imm_buf);
            } else {
                fprintf(fout, ANDI_OPCODE"%s%s%s\n", rs1_buf, rd_buf, imm_buf);
            }
        } else if (strcmp(op, "beqz") == 0) {
            char *rs1 = strtok(NULL, ",");
            char *imm = strtok(NULL, " \n\r");
            char rs1_buf[REG_LEN + 1];
            char rd_buf[REG_LEN + 1];
            char imm_buf[IMM_LEN + 1];
            int offset = 0;
            for (int i = 0; i < id; i++) {
                if (strcmp(imm, map[i].label) == 0) {
                    offset = map[i].address - address;
                    printf("offset = %d - %d = %d\n", map[i].address, address, offset);
                }
            }
            decToBin(atoi(rs1 + 1), rs1_buf, REG_LEN);
            decToBin(0, rd_buf, REG_LEN);
            decToBin(offset - 1, imm_buf, IMM_LEN);
            fprintf(fout, BEQZ_OPCODE"%s%s%s\n", rs1_buf, rd_buf, imm_buf);
        } else if (strcmp(op, "add") == 0 || strcmp(op, "sub") == 0 || strcmp(op, "and") == 0) {
            char *rd = strtok(NULL, ",");
            char *rs1 = strtok(NULL, ",");
            char *rs2 = strtok(NULL, " \n\r");
            char rd_buf[REG_LEN + 1];
            char rs1_buf[REG_LEN + 1];
            char rs2_buf[REG_LEN + 1];
            decToBin(atoi(rd + 1), rd_buf, REG_LEN);
            decToBin(atoi(rs1 + 1), rs1_buf, REG_LEN);
            decToBin(atoi(rs2 + 1), rs2_buf, REG_LEN);
            if (strcmp(op, "add") == 0) {
                fprintf(fout, ADD_OPCODE"%s%s%s"ADD_FUNC"\n", rs1_buf, rs2_buf, rd_buf);
            } else if (strcmp(op, "sub") == 0) {
                fprintf(fout, SUB_OPCODE"%s%s%s"SUB_FUNC"\n", rs1_buf, rs2_buf, rd_buf);
            } else {
                fprintf(fout, AND_OPCODE"%s%s%s"AND_FUNC"\n", rs1_buf, rs2_buf, rd_buf);
            }
        } else if (strcmp(op, "j") == 0) {
            char *addr = strtok(NULL, " \n\r");
            char addr_buf[ADDR_LEN + 1];
            int offset = 0;
            for (int i = 0; i < id; i++) {
                if (strcmp(addr, map[i].label) == 0) {
                    offset = map[i].address - address;
                    printf("offset = %d - %d = %d\n", map[i].address, address, offset);
                    break;
                }
            }
            decToBin(offset - 1, addr_buf, ADDR_LEN);
            fprintf(fout, J_OPCODE"%s\n", addr_buf);
        } else if (strcmp(op, "halt") == 0) {
            fprintf(fout, HALT_OPCODE"00000000000000000000000000\n");
        } else if (strcmp(op, "noop") == 0) {
            fprintf(fout, NOOP_OPCODE"00000000000000000000000000\n");
        } else {  // label
            printf("error: unknown instruction op %s\n", op);
            exit(1);
        }
        address++;
    }

    fclose(fout);
    fclose(fin);

    fin = fopen("tmp.txt", "r");
    fout = fopen(argv[2], "w");

    while (fgets(buf, BUF_SIZE, fin)) {
        int dec = binToDec(strtok(buf, "\n"));
        fprintf(fout, "%d\n", dec);
    }

    fclose(fout);
    fclose(fin);

  return 0;
}
