#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINELENGTH 1000   // 机器指令的最大长度
#define MEMSIZE       10000  // 内存的最大容量
#define NUMREGS       32     // 寄存器数量

/*
 * 操作码和功能码定义
 */
#define regRegALU 0   // 寄存器-寄存器的 ALU 运算的操作码为 0
#define LW        35
#define SW        43
#define ADDI      8
#define ANDI      12
#define BEQZ      4
#define J         2
#define HALT      1
#define NOOP      3
#define addFunc   32  // ALU 运算的功能码
#define subFunc   34
#define andFunc   36

#define NOOPINSTRUCTION 0x0c000000;

/*
 * 执行单元
 */	
#define LOAD1  0
#define LOAD2  1
#define STORE1 2
#define STORE2 3
#define INT1   4
#define INT2   5

#define NUMUNITS 6            // 执行单元数量
char *unitname[NUMUNITS] = {  // 执行单元的名称
  "LOAD1", "LOAD2", "STORE1", "STORE2", "INT1", "INT2"
};

/*
 * 不同操作所需要的周期数
 */
#define BRANCHEXEC 3	// 分支操作
#define LDEXEC     2	// Load
#define STEXEC     2	// Store
#define INTEXEC    1	// 整数运算

/*
 * 指令状态
 */
#define ISSUING       0	 // 发射
#define EXECUTING     1	 // 执行
#define WRITINGRESULT 2	 // 写结果
#define COMMITTING    3	 // 提交
char *statename[4] = {   // 状态名称
  "ISSUING", "EXECUTING", "WRITINGRESULT", "COMMTITTING"
};

#define RBSIZE	16  // ROB 有 16 个单元
#define BTBSIZE	8   // 分支预测缓冲栈有 8 个单元

/*
 * 2 bit 分支预测状态
 */
#define STRONGNOT   0
#define WEAKTAKEN   1
#define WEAKNOT     2
#define STRONGTAKEN	3
char *predname[4] = {   // 状态名称
  "STRONGNOT", "WEAKTAKEN", "WEAKNOT", "STRONGTAKEN"
};

/*
 * 分支跳转结果
 */
#define NOTTAKEN 0
#define TAKEN    1

/*
 * 保留站的数据结构
 */
typedef struct _resStation {
  int instr;	     // 指令
  int busy;		     // 空闲标志位
  int Vj;		       // Vj, Vk 存放操作数
  int Vk;
  int Qj;		       // Qj, Qk 存放将会生成结果的执行单元编号
  int Qk;		       // 为零则表示对应的 V 有效
  int exTimeLeft;  // 指令执行的剩余时间
  int reorderNum;  // 该指令对应的 ROB 项编号
} resStation;

/*
 * ROB 项的数据结构
 */
typedef struct _reorderEntry {
  int busy;          // 空闲标志位
  int instr;		     // 指令
  int execUnit;		   // 执行单元编号
  int instrStatus;   // 指令的当前状态
  int valid;		     // 表明结果是否有效的标志位
  int result;		     // 在提交之前临时存放结果
  int storeAddress;  // store 指令的内存地址
  int branchCmp;     // beqz 指令的比较结果
  int branchPC;      // beqz 指令的 PC
} reorderEntry;

/*
 * 寄存器状态的数据结构
 */
typedef struct _regResultEntry {
  int valid;       // 1 表示寄存器值有效, 否则 0
  int reorderNum;  // 如果值无效, 记录 ROB 中哪个项目会提交结果
} regResultEntry;

/*
 * 分支预测缓冲栈的数据结构
 */
typedef struct _btbEntry {
  int valid;         // 有效位
  int branchPC;      // 分支指令的 PC 值
  int branchTarget;  // when predict taken, update PC with target
  int branchPred;    // 预测: 2 bit 分支历史
} btbEntry;

/*
 * 虚拟机状态的数据结构
 */
typedef struct _machineState {
  int pc;		                          // PC
  int cycles;                         // 已经过的周期数
  resStation reservation[NUMUNITS];		// 保留站
  reorderEntry	reorderBuf[RBSIZE];		// ROB
  regResultEntry regResult[NUMREGS];  // 寄存器状态
  btbEntry	btBuf[BTBSIZE];           // 分支预测缓冲栈
  int memory[MEMSIZE];                // 内存
  int regFile[NUMREGS];               // 寄存器
} machineState;

void printState(machineState *statePtr, int memorySize) {
	int i;
	
	printf("Cycles: %d\n", statePtr->cycles);
	
	printf("\t pc = %d\n", statePtr->pc);
	
	printf("\t Reservation stations:\n");
	for (i = 0; i < NUMUNITS; i++) {
		if (statePtr->reservation[i].busy == 1) {
			printf("\t \t Reservation station %s: ", unitname[i]);
			if (statePtr->reservation[i].Qj == -1) {
        printf("Vj = %d ", statePtr->reservation[i].Vj);
      } else {
        printf("Qj = '%d' ", statePtr->reservation[i].Qj);
      }
			if (statePtr->reservation[i].Qk == -1) {
        printf("Vk = %d ", statePtr->reservation[i].Vk);
      } else {
        printf("Qk = '%d' ", statePtr->reservation[i].Qk);
      }
			printf(" ExTimeLeft = %d  RBNum = %d\n", 
				statePtr->reservation[i].exTimeLeft,
				statePtr->reservation[i].reorderNum);
		}
	}
	
	printf("\t Reorder buffers:\n");
	for (i = 0; i < RBSIZE; i++) {
		if (statePtr->reorderBuf[i].busy == 1) {
			printf("\t \t Reorder buffer %d: ",i);
			printf("instr %d  executionUnit '%s'  state %s  valid %d  result %d storeAddress %d\n",
				statePtr->reorderBuf[i].instr,
				unitname[statePtr->reorderBuf[i].execUnit],
				statename[statePtr->reorderBuf[i].instrStatus], 
				statePtr->reorderBuf[i].valid, statePtr->reorderBuf[i].result,
				statePtr->reorderBuf[i].storeAddress); 
		}
	}
    
	printf("\t Register result status:\n");
	for (i = 1; i < NUMREGS; i++) {
		if (!statePtr->regResult[i].valid) {
			printf("\t \t Register %d: ",i);
			printf("waiting for reorder buffer number %d\n",
				statePtr->regResult[i].reorderNum);
		}
	}
	
	/*
	 * [TODO]如果你实现了动态分支预测, 将这里的注释取消
	 */
  printf("\t Branch target buffer:\n");
  for (i=0; i<BTBSIZE; i++){
    if (statePtr->btBuf[i].valid){
      printf("\t \t Entry %d: PC=%d, Target=%d, Pred=%d\n",
      i, statePtr->btBuf[i].branchPC, statePtr->btBuf[i].branchTarget,
      statePtr->btBuf[i].branchPred);
  }
  }
	 
	printf("\t Memory:\n");
	for (i = 0; i < memorySize; i++) {
		printf("\t \t memory[%-2d] = %d\n", i, statePtr->memory[i]);
	}
	
	printf("\t Registers:\n");
	for (i = 0; i < NUMREGS; i++) {
		printf("\t \t regFile[%-2d] = %d\n", i, statePtr->regFile[i]);
	}
}

int convertNum16(int num) {
  /* convert a 16 bit number into a 32-bit or 64-bit number */
  if (num & 0x8000) {
    num -= 65536;
  }
  return(num);
}

int convertNum26(int num) {
  /* convert a 26 bit number into a 32-bit or 64-bit number */
  if (num & 0x200000) {
    num -= 67108864;
  }
  return(num);
}

/*
 * 这里对指令进行解码，转换成程序可以识别的格式，需要根据指令格式来进行。
 * 可以考虑使用高级语言中的位和逻辑运算
 */
/*
 * 返回指令的第一个寄存器RS1
 */
int field0(int instruction) {
  int res = 0;
  for (int i = 25; 21 < i; i--) {
    res += (instruction >> i) & 0x1;
    res <<= 1;
  }
  res += (instruction >> 21) & 0x1;
  return res;
}

/*
 * 返回指令的第二个寄存器，RS2或者Rd
 */
int field1(int instruction) {
  int res = 0;
  for (int i = 20; 16 < i; i--) {
    res += (instruction >> i) & 0x1;
    res <<= 1;
  }
  res += (instruction >> 16) & 0x1;
  return res;
}

/*
 * 返回指令的第三个寄存器，Rd
 */
int field2(int instruction) {
  int res = 0;
  for (int i = 15; 11 < i; i--) {
    res += (instruction >> i) & 0x1;
    res <<= 1;
  }
  res += (instruction >> 11) & 0x1;
  return res;
}

/*
 * 返回I型指令的立即数部分
 */
int immediate(int instruction) {
  int res = 0;
  for (int i = 15; 0 < i; i--) {
    res += (instruction >> i) & 0x1;
    res <<= 1;
  }
  res += instruction & 0x1;
  return convertNum16(res);
}

/*
 * 返回J型指令的跳转地址
 */
int jumpAddr(int instruction) {
  int res = 0;
  for (int i = 25; 0 < i; i--) {
    res += (instruction >> i) & 0x1;
    res <<= 1;
  }
  res += instruction & 0x1;
  return convertNum26(res);
}

/*
 * 返回指令的操作码
 */
int opcode(int instruction) {
  int res = 0;
  for (int i = 31; 26 < i; i--) {
    res += (instruction >> i) & 0x1;
    res <<= 1;
  }
  res += (instruction >> 26) & 0x1;
  return res;
}

/*
 * 返回R型指令的功能域
 */
int func(int instruction) {
  int res = 0;
  for (int i = 10; 0 < i; i--) {
    res += (instruction >> i) & 0x1;
    res <<= 1;
  }
  res += instruction & 0x1;
  return res;
  return 0;
}

void printInstruction(int instr) {
    char opcodeString[10];
    char funcString[11];
    int funcCode;
    int op;

    if (opcode(instr) == regRegALU) {
      funcCode = func(instr);
      if (funcCode == addFunc) {
        strcpy(opcodeString, "add");
      } else if (funcCode == subFunc) {
        strcpy(opcodeString, "sub");
      } else if (funcCode == andFunc) {
        strcpy(opcodeString, "and");
      } else {
        strcpy(opcodeString, "alu");
      }
      printf("%s %d %d %d \n", opcodeString, field2(instr), field0(instr),
            field1(instr));
    } else if (opcode(instr) == LW) {
      strcpy(opcodeString, "lw");
      printf("%s %d %d %d\n", opcodeString, field1(instr), field0(instr),
            immediate(instr));
    } else if (opcode(instr) == SW) {
      strcpy(opcodeString, "sw");
      printf("%s %d %d %d\n", opcodeString, field1(instr), field0(instr),
	          immediate(instr));
    } else if (opcode(instr) == ADDI) {
      strcpy(opcodeString, "addi");
      printf("%s %d %d %d\n", opcodeString, field1(instr), field0(instr),
	          immediate(instr));
    } else if (opcode(instr) == ANDI) {
      strcpy(opcodeString, "andi");
      printf("%s %d %d %d\n", opcodeString, field1(instr), field0(instr),
	          immediate(instr));
    } else if (opcode(instr) == BEQZ) {
      strcpy(opcodeString, "beqz");
      printf("%s %d %d %d\n", opcodeString, field1(instr), field0(instr),
	          immediate(instr));
    } else if (opcode(instr) == J) {
      strcpy(opcodeString, "j");
      printf("%s %d\n", opcodeString, jumpAddr(instr));
    } else if (opcode(instr) == HALT) {
      strcpy(opcodeString, "halt");
      printf("%s\n", opcodeString);
    } else if (opcode(instr) == NOOP) {
      strcpy(opcodeString, "noop");
      printf("%s\n", opcodeString);
    } else {
      strcpy(opcodeString, "data");
      printf("%s %d\n", opcodeString, instr);
    }
}

void printFileInstr(machineState *statePtr, int memorySize) {
  for (int i = 16; i < memorySize; i++) {
    // code
    printf("code=");
    printInstruction(statePtr->memory[i]);
    // instr
    printf("instr=%d\n", statePtr->memory[i]);
  }
}

void printFileState(machineState *statePtr, int memorySize) {
  printf("Cycle=%d\n", statePtr->cycles);
  // reorder buffer
  for (int i = 0; i < RBSIZE; i++) {
    if (statePtr->reorderBuf[i].busy == 1) {
      printf("RB%d-Busy=%d\n", i, 1);
      printf("RB%d-Instr=%d\n", i, statePtr->reorderBuf[i].instr);
      if (statePtr->reorderBuf[i].instrStatus != 3) {
        printf("RB%d-ExecUnit=%s\n", i, unitname[statePtr->reorderBuf[i].execUnit]);
      }
      printf("RB%d-InstrStatus=%s\n", i, statename[statePtr->reorderBuf[i].instrStatus]);
      if (opcode(statePtr->reorderBuf[i].instr) == NOOP || opcode(statePtr->reorderBuf[i].instr) == HALT) {
        printf("RB%d-Valid=%d\n", i, 0);
      } else {
        printf("RB%d-Valid=%d\n", i, statePtr->reorderBuf[i].valid);
        if (statePtr->reorderBuf[i].valid == 1) {
          printf("RB%d-Result=%d\n", i, statePtr->reorderBuf[i].result);
        }
      }
      if (opcode(statePtr->reorderBuf[i].instr) == SW) {
        printf("RB%d-StoreAddress=%d\n", i, statePtr->reorderBuf[i].storeAddress);
      }
      if (opcode(statePtr->reorderBuf[i].instr) == BEQZ) {
        printf("RB%d-BranchCmp=%d\n", i, statePtr->reorderBuf[i].branchCmp);
      }
      if (opcode(statePtr->reorderBuf[i].instr == BEQZ)) {
        printf("RB%d-BranchPC=%d\n", i, statePtr->reorderBuf[i].branchPC);
      }
    } else {
      printf("RB%d-Busy=%d\n", i, 0);
    }
  }
  // reservation station
  for (int i = 0; i < NUMUNITS; i++) {
    if (statePtr->reservation[i].busy == 1) {
      printf("RS%d-Busy=%d\n", i, 1);
      printf("RS%d-Instr=%d\n", i, statePtr->reservation[i].instr);
      if (statePtr->reservation[i].Qj == -1) {
        printf("RS%d-Vj=%d\n", i, statePtr->reservation[i].Vj);
      }
      if (statePtr->reservation[i].Qk == -1) {
        printf("RS%d-Vk=%d\n", i, statePtr->reservation[i].Vk);
      }
      printf("RS%d-Qj=%d\n", i, statePtr->reservation[i].Qj);
      printf("RS%d-Qk=%d\n", i, statePtr->reservation[i].Qk);
      printf("RS%d-ExTimeLeft=%d\n", i, statePtr->reservation[i].exTimeLeft);
      printf("RS%d-ReorderNum=%d\n", i, statePtr->reservation[i].reorderNum);
    } else {
      printf("RS%d-Busy=%d\n", i, 0);
    }
  }
  // branch target table
  for (int i = 0; i < BTBSIZE; i++) {
    if (statePtr->btBuf[i].valid) {
      printf("BT%d-Valid=%d\n", i, 1);
      printf("BT%d-BranchPC=%d\n", i, statePtr->btBuf[i].branchPC);
      printf("BT%d-BranchPred=%s\n", i, predname[statePtr->btBuf[i].branchPred]);
      printf("BT%d-BranchTarget=%d\n", i, statePtr->btBuf[i].branchTarget);
    } else {
      printf("BT%d-Valid=%d\n", i, 0);
    }
  }
  // register
  for (int i = 0; i < NUMREGS; i++) {
    printf("R%d-Value=%d\n", i, statePtr->regFile[i]);
    printf("R%d-Valid=%d\n", i, statePtr->regResult[i].valid);
    if (statePtr->regResult[i].valid == 0) {
      printf("R%d-ReorderNum=%d\n", i, statePtr->regResult[i].reorderNum);
    }
  }
  // memory
  for (int i = 0; i < memorySize; i++) {
    printf("MEM%d-Value=%d\n", i, statePtr->memory[i]);
  }
}

int main(int argc, char *argv[]) {
  FILE *filePtr;
  int pc, done, instr;
  char line[MAXLINELENGTH];
  machineState *statePtr;
  int memorySize;
  int success, newBuf, op, halt, unit;
  int headRB, tailRB;
  int regA, regB, immed, address;
  int flush;
  int rbnum;
  
  if (argc != 2) {
    printf("error: usage: %s <machine-code file>\n", argv[0]);
    exit(1);
  }

  /*
   * 初始化, 读输入文件等
   */
  filePtr = fopen(argv[1], "r");
  if (filePtr == NULL) {
    printf("error: can't open file %s", argv[1]);
    perror("fopen");
    exit(1);
  }

  /*
   * 分配数据结构空间
   */
  statePtr = (machineState *) malloc(sizeof(machineState));

  /* 
   * 将机器指令读入到内存中
   */
  for (int i = 0; i < MEMSIZE; i++) {
    statePtr->memory[i] = 0;
  }
  pc = 16;
  done = 0;
  while (!done) {
    if (fgets(line, MAXLINELENGTH, filePtr) == NULL){
      done = 1;
    } else {
      if (sscanf(line, "%d\n", &instr) != 1) {
          printf("error in reading address %d\n", pc);
          exit(1);
      }
      statePtr->memory[pc] = instr;
      // printf("memory[%d] = %d\n", pc, statePtr->memory[pc]);
      // printInstruction(instr);
      pc = pc + 1;
    }
  }

  memorySize = pc;
  halt = 0;

  // printf("\n");

  /*
   * 状态初始化
   */
  statePtr->pc = 16;
  statePtr->cycles = 0;
  for (int i = 0; i < NUMREGS; i++) {
    statePtr->regFile[i] = 0;
  }
  for (int i = 0; i < NUMUNITS; i++) {
    statePtr->reservation[i].busy = 0;
  }
  for (int i = 0; i < RBSIZE; i++) {
    statePtr->reorderBuf[i].busy = 0;
  }

  headRB = -1;
  tailRB = -1;

  for (int i = 0; i < NUMREGS; i++) {
    statePtr->regResult[i].valid = 1;
  }
  for (int i = 0; i < BTBSIZE; i++) {
    statePtr->btBuf[i].valid = 0;
  }

  printFileInstr(statePtr, memorySize);

  /*
   * 处理指令
   */
  while (1) {  /* 执行循环:你应该在执行 HALT 指令时跳出这个循环 */

    // printState(statePtr, memorySize);

    printFileState(statePtr, memorySize);

    /*
     * 基本要求:
     * 首先, 确定是否需要清空流水线或提交位于 ROB 的队首的指令.
     * 我们处理分支跳转的缺省方法是假设跳转不成功, 如果我们的预测是错误的,
     * 就需要清空流水线(ROB/保留站/寄存器状态), 设置新的 PC = 跳转目标.
     * 如果不需要清空, 并且队首指令能够提交, 在这里更新状态:
     *     对寄存器访问, 修改寄存器;
     *     对内存写操作, 修改内存.
     * 在完成清空或提交操作后, 不要忘了释放保留站并更新队列的首指针.
     */
    if (statePtr->reorderBuf[headRB].busy && statePtr->reorderBuf[headRB].instrStatus == COMMITTING) {
      int instr = statePtr->reorderBuf[headRB].instr;
      if (opcode(instr) == BEQZ) {
        /*
         * 选作内容:
         * 在提交的时候, 我们知道跳转指令的最终结果.
         * 有三种可能的情况: 预测跳转成功, 预测跳转不成功, 不能预测(因为分支预测缓冲栈中没有对应的项目).
         * 如果我们预测跳转成功:
         *     如果我们的预测是正确的, 只需要继续执行就可以了;
         *     如果我们的预测是错误的, 即实际没有发生跳转, 就必须重新设置正确的PC值, 并清空流水线.
         * 如果我们预测跳转不成功:
         *     如果预测是正确的, 继续执行;
         *     如果预测是错误的, 即实际上发生了跳转, 就必须将PC设置为跳转目标, 并清空流水线.
         * 如果我们不能预测跳转是否成功:
         *     如果跳转成功, 仍然需要清空流水线, 将PC修改为跳转目标.
         * 在遇到分支时, 需要更新分支预测缓冲站的内容.
         */
        for (int i = 0; i < BTBSIZE; i++) {
          if (statePtr->btBuf[i].valid == 1 && statePtr->btBuf[i].branchPC == statePtr->reorderBuf[headRB].branchPC) {
            if (statePtr->reorderBuf[headRB].branchCmp == 1) {  // 发生跳转
              switch (statePtr->btBuf[i].branchPred) {
                case STRONGNOT:  // 预测错误
                  statePtr->btBuf[i].branchPred = WEAKNOT;
                  // 设置跳转地址
                  statePtr->pc = statePtr->reorderBuf[headRB].result;
                  // 清空 ROB
                  for (int i = 0; i < RBSIZE; i++) {
                    statePtr->reorderBuf[i].busy = 0;
                  }
                  // 清空保留站
                  for (int i = 0; i < NUMUNITS; i++) {
                    statePtr->reservation[i].busy = 0;
                  }
                  // 清空寄存器状态
                  for (int i = 0; i < NUMREGS; i++) {
                    statePtr->regResult[i].valid = 1;
                  }
                  // 更新队列的首指针
                  headRB = -1;
                  tailRB = -1;
                  break;
                case WEAKNOT:  // 预测错误
                  statePtr->btBuf[i].branchPred = WEAKTAKEN;
                  statePtr->btBuf[i].branchTarget = statePtr->reorderBuf[headRB].result;
                  // 设置跳转地址
                  statePtr->pc = statePtr->reorderBuf[headRB].result;
                  // 清空 ROB
                  for (int i = 0; i < RBSIZE; i++) {
                    statePtr->reorderBuf[i].busy = 0;
                  }
                  // 清空保留站
                  for (int i = 0; i < NUMUNITS; i++) {
                    statePtr->reservation[i].busy = 0;
                  }
                  // 清空寄存器状态
                  for (int i = 0; i < NUMREGS; i++) {
                    statePtr->regResult[i].valid = 1;
                  }
                  // 更新队列的首指针
                  headRB = -1;
                  tailRB = -1;
                  break;
                case WEAKTAKEN:  // 预测正确
                  statePtr->btBuf[i].branchPred = STRONGTAKEN;
                  // 释放保留站
                  statePtr->reorderBuf[headRB].busy = 0;
                  // 更新队列的首指针
                  headRB = (headRB + 1) % RBSIZE;
                  break;
                default:  // 预测正确
                  // 释放保留站
                  statePtr->reorderBuf[headRB].busy = 0;
                  // 更新队列的首指针
                  headRB = (headRB + 1) % RBSIZE;
                  break;
              }
            } else {
                switch (statePtr->btBuf[i].branchPred) {  // 不发生跳转
                  case STRONGNOT:  // 预测正确
                    // 释放保留站
                    statePtr->reorderBuf[headRB].busy = 0;
                    // 更新队列的首指针
                    headRB = (headRB + 1) % RBSIZE;
                    break;
                  case WEAKNOT:  // 预测正确
                    statePtr->btBuf[i].branchPred = STRONGNOT;
                    // 释放保留站
                    statePtr->reorderBuf[headRB].busy = 0;
                    // 更新队列的首指针
                    headRB = (headRB + 1) % RBSIZE;
                    break;
                  case WEAKTAKEN:  // 预测错误
                    statePtr->btBuf[i].branchPred = WEAKNOT;
                    // 设置跳转地址
                    statePtr->pc = statePtr->reorderBuf[headRB].branchPC + 1;
                    // 清空 ROB
                    for (int i = 0; i < RBSIZE; i++) {
                      statePtr->reorderBuf[i].busy = 0;
                    }
                    // 清空保留站
                    for (int i = 0; i < NUMUNITS; i++) {
                      statePtr->reservation[i].busy = 0;
                    }
                    // 清空寄存器状态
                    for (int i = 0; i < NUMREGS; i++) {
                      statePtr->regResult[i].valid = 1;
                    }
                    // 更新队列的首指针
                    headRB = -1;
                    tailRB = -1;
                    break;
                  default:  // 预测错误
                    statePtr->btBuf[i].branchPred = WEAKTAKEN;
                    // 设置跳转地址
                    statePtr->pc = statePtr->reorderBuf[headRB].branchPC + 1;
                    // 清空 ROB
                    for (int i = 0; i < RBSIZE; i++) {
                      statePtr->reorderBuf[i].busy = 0;
                    }
                    // 清空保留站
                    for (int i = 0; i < NUMUNITS; i++) {
                      statePtr->reservation[i].busy = 0;
                    }
                    // 清空寄存器状态
                    for (int i = 0; i < NUMREGS; i++) {
                      statePtr->regResult[i].valid = 1;
                    }
                    // 更新队列的首指针
                    headRB = -1;
                    tailRB = -1;
                    break;
                }
            }
            break;
          }
        }
      } else if (opcode(instr) == J) {
        // 设置跳转地址
        statePtr->pc = statePtr->reorderBuf[headRB].result;
        // 清空 ROB
        for (int i = 0; i < RBSIZE; i++) {
          statePtr->reorderBuf[i].busy = 0;
        }
        // 清空保留站
        for (int i = 0; i < NUMUNITS; i++) {
          statePtr->reservation[i].busy = 0;
        }
        // 清空寄存器状态
        for (int i = 0; i < NUMREGS; i++) {
          statePtr->regResult[i].valid = 1;
        }
        // 更新队列的首指针
        headRB = -1;
        tailRB = -1;
      } else if (opcode(instr) == HALT) {
        // 释放保留站, 更新队列的首指针
        headRB = (headRB + 1) % RBSIZE;
        // 停机
        break;
      } else if (opcode(instr) == NOOP) {
        // 释放保留站
        statePtr->reorderBuf[headRB].busy = 0;
        // 更新队列的首指针
        headRB = (headRB + 1) % RBSIZE;
        // 不进行操作
      } else {
        if (opcode(instr) == SW) {  // 修改内存
          int storeAddress = statePtr->reorderBuf[headRB].storeAddress;
          if (statePtr->reorderBuf[headRB].valid == 1) {
            statePtr->memory[storeAddress] = statePtr->reorderBuf[headRB].result;
          }
        } else {  // 修改寄存器
          int rd = (opcode(instr) == regRegALU) ? field2(instr) : field1(instr);
          if (!statePtr->regResult[rd].valid && statePtr->regResult[rd].reorderNum == headRB) {
            if (statePtr->reorderBuf[headRB].valid == 1) {
              statePtr->regFile[rd] = statePtr->reorderBuf[headRB].result;
            }
            statePtr->regResult[rd].valid = 1;
          }
        }
        // 释放保留站
        statePtr->reorderBuf[headRB].busy = 0;
        // 更新队列的首指针
        headRB = (headRB + 1) % RBSIZE;
      }
    }   

    /*
     * 提交完成.
     * 检查所有保留站中的指令, 对下列状态, 分别完成所需的操作:
     */
    int RBNum = (headRB <= tailRB) ? tailRB - headRB + 1 : RBSIZE + tailRB - headRB + 1;
    if (headRB == -1 && tailRB == -1) {
      RBNum = 0;
      headRB = 0;
    }
    for (int i = headRB; i < headRB + RBNum; i++) {
      reorderEntry *RBPtr = &(statePtr->reorderBuf[i % RBSIZE]);
      if (RBPtr->busy == 1) {
        resStation *execUnit = &(statePtr->reservation[RBPtr->execUnit]);
        if (RBPtr->instrStatus == ISSUING) {
          /*
           * 对 Issuing 状态:
           * 检查两个操作数是否都已经准备好, 如果是, 将指令状态修改为 Executing
           */
          if (execUnit->busy == 1) {
            if (execUnit->Qj == -1 && execUnit->Qk == -1) {
              RBPtr->instrStatus = EXECUTING;
            }
          }
        } else if (RBPtr->instrStatus == EXECUTING) {
          /*
           * 对 Executing 状态:
           * 执行剩余时间递减;
           * 在执行完成时, 将指令状态修改为 Writing Result
           */
          if (execUnit->busy == 1) {
            execUnit->exTimeLeft--;
            if (execUnit->exTimeLeft == 0) {
              RBPtr->instrStatus = WRITINGRESULT;
            }
          }
        } else if (RBPtr->instrStatus == WRITINGRESULT) {
          /* 
           * 对 Writing Result 状态:
           * 将结果复制到正在等待该结果的其他保留站中去;
           * 还需要将结果保存在 ROB 中的临时存储区中.
           * 释放指令占用的保留站, 将指令状态修改为 Committing
           */
          // 计算结果
          int result = 0;
          switch (opcode(execUnit->instr)) {
            case LW:
              result = statePtr->memory[execUnit->Vj + immediate(execUnit->instr)];
              break;
            case SW:
              result = execUnit->Vk;
              RBPtr->storeAddress = execUnit->Vj + immediate(execUnit->instr);
              break;
            case regRegALU:
              switch (func(execUnit->instr)) {
                case addFunc:
                  result = execUnit->Vj + execUnit->Vk;
                  break;
                case subFunc:
                  result = execUnit->Vj - execUnit->Vk;
                  break;
                default:
                  result = execUnit->Vj & execUnit->Vk;
                  break;
              }
              break;
            case ADDI:
              result = execUnit->Vj + immediate(execUnit->instr);
              break;
            case ANDI:
              result = execUnit->Vj & immediate(execUnit->instr);
              break;
            case BEQZ:
              result = execUnit->Vk + immediate(execUnit->instr);
              RBPtr->branchCmp = (execUnit->Vj == 0) ? 1 : 0;
              break;
            case J:
              result = execUnit->Vk + immediate(execUnit->instr);
              break;
            default:
              break;
          }
          // 写回 ROB
          RBPtr->valid = 1;
          RBPtr->result = result;
          // 更新保留站
          for (int i = 0; i < NUMUNITS; i++) {
            if (statePtr->reservation[i].busy) {
              if (statePtr->reservation[i].Qj == execUnit->reorderNum) {
                statePtr->reservation[i].Qj = -1;
                statePtr->reservation[i].Vj = result;
              }
              if (statePtr->reservation[i].Qk == execUnit->reorderNum) {
                statePtr->reservation[i].Qk = -1;
                statePtr->reservation[i].Vk = result;
              }
            }
          }
          // 释放保留站
          execUnit->busy = 0;
          RBPtr->instrStatus = COMMITTING;
        }
      }
    }

    /*
     * 最后, 当我们处理完了保留站中的所有指令后, 检查是否能够发射一条新的指令.
     * 首先检查 ROB 中是否有空闲的空间,
     * 如果有，再检查所需运算单元是否有空闲的保留站,
     * 如果有, 发射指令.
     * 
     * 在ROB的队尾检查是否有空闲的空间,
     * ROB是一个循环队列, 它可以容纳 RBSIZE 个项目.
     * 新的指令被添加到队列的末尾, 指令提交则是从队首进行的.
     * 当队列的首指针或尾指针到达数组中的最后一项时, 它应滚动到数组的第一项.
     * 
     * 发射指令:
     * 填写保留站和 ROB 项的内容.
     * 注意, 要在所有的字段中写入正确的值.
     * 检查寄存器状态, 相应的在 Vj,Vk 和 Qj,Qk 字段中设置正确的值:
     * 对于 I 类型指令, 设置 Qk=0,Vk=0;
     * 对于 SW 指令, 如果寄存器有效, 将寄存器中的内存基地址保存在 Vj 中;
     * 对于 BEQZ 和 J 指令, 将当前 PC+1 的值保存在 Vk 字段中.
     * 如果指令在提交时会修改寄存器的值, 还需要在这里更新寄存器状态数据结构.
     */
    if (RBNum < RBSIZE) {
      if (statePtr->pc < memorySize) {
        int instr = statePtr->memory[statePtr->pc];
        if (opcode(instr) == regRegALU) {  // R 型指令
          if (!statePtr->reservation[INT1].busy || !statePtr->reservation[INT2].busy) {
            int execUnit = (statePtr->reservation[INT1].busy == 1) ? INT2 : INT1;
            // 提交到 ROB
            tailRB = (tailRB + 1) % RBSIZE;
            statePtr->reorderBuf[tailRB].busy = 1;
            statePtr->reorderBuf[tailRB].instr = instr;
            statePtr->reorderBuf[tailRB].execUnit = execUnit;
            statePtr->reorderBuf[tailRB].instrStatus = ISSUING;
            statePtr->reorderBuf[tailRB].valid = 0;
            // 提交到保留站
            statePtr->reservation[execUnit].busy = 1;
            statePtr->reservation[execUnit].instr = instr;
            // Vj, Qj
            int rs1 = field0(instr);
            if (statePtr->regResult[rs1].valid == 1) {
              statePtr->reservation[execUnit].Vj = statePtr->regFile[rs1];
              statePtr->reservation[execUnit].Qj = -1;
            } else {
              if (statePtr->reorderBuf[statePtr->regResult[rs1].reorderNum].instrStatus == COMMITTING) {
                statePtr->reservation[execUnit].Vj = statePtr->reorderBuf[statePtr->regResult[rs1].reorderNum].result;
                statePtr->reservation[execUnit].Qj = -1;
              } else {
                statePtr->reservation[execUnit].Qj = statePtr->regResult[rs1].reorderNum;
              }
            }
            // Vk, Qk
            int rs2 = field1(instr);
            if (statePtr->regResult[rs2].valid == 1) {
              statePtr->reservation[execUnit].Vk = statePtr->regFile[rs2];
              statePtr->reservation[execUnit].Qk = -1;
            } else {
              if (statePtr->reorderBuf[statePtr->regResult[rs2].reorderNum].instrStatus == COMMITTING) {
                statePtr->reservation[execUnit].Vk = statePtr->reorderBuf[statePtr->regResult[rs2].reorderNum].result;
                statePtr->reservation[execUnit].Qk = -1;
              } else {
                statePtr->reservation[execUnit].Qk = statePtr->regResult[rs2].reorderNum;
              }
            }
            statePtr->reservation[execUnit].exTimeLeft = 1;
            statePtr->reservation[execUnit].reorderNum = tailRB;
            // 更新寄存器状态
            int rd = field2(instr);
            statePtr->regResult[rd].valid = 0;
            statePtr->regResult[rd].reorderNum = tailRB;
            // 更新 PC
            statePtr->pc++;
          }
        } else if (opcode(instr) == ADDI || opcode(instr) == ANDI || opcode(instr) == BEQZ ||
                  opcode(instr) == LW || opcode(instr) == SW) {  // I 型指令
          int execUnit = -1;
          if (opcode(instr) == ADDI || opcode(instr) == ANDI || opcode(instr) == BEQZ) {  // ADDI, ANDI, BEQZ 
            if (!statePtr->reservation[INT1].busy || !statePtr->reservation[INT2].busy) {
              execUnit = (statePtr->reservation[INT1].busy == 1) ? INT2 : INT1;
            }
          } else if (opcode(instr) == LW) {  // LW
            if (!statePtr->reservation[LOAD1].busy || !statePtr->reservation[LOAD2].busy) {
              execUnit = (statePtr->reservation[LOAD1].busy == 1) ? LOAD2 : LOAD1;
            }
          } else {  // SW
            if (!statePtr->reservation[STORE1].busy || !statePtr->reservation[STORE2].busy) {
              execUnit = (statePtr->reservation[STORE1].busy == 1) ? STORE2 : STORE1;
            }
          }
          if (execUnit != -1) {
            // 提交到 ROB
            tailRB = (tailRB + 1) % RBSIZE;
            statePtr->reorderBuf[tailRB].busy = 1;
            statePtr->reorderBuf[tailRB].instr = instr;
            statePtr->reorderBuf[tailRB].execUnit = execUnit;
            statePtr->reorderBuf[tailRB].instrStatus = ISSUING;
            statePtr->reorderBuf[tailRB].valid = 0;
            if (opcode(instr) == BEQZ) {
              statePtr->reorderBuf[tailRB].branchPC = statePtr->pc;
            }
            // 提交到保留站
            statePtr->reservation[execUnit].busy = 1;
            statePtr->reservation[execUnit].instr = instr;
            // Vj, Qj
            int rs1 = field0(instr);
            if (statePtr->regResult[rs1].valid == 1) {
              statePtr->reservation[execUnit].Vj = statePtr->regFile[rs1];
              statePtr->reservation[execUnit].Qj = -1;
            } else {
              if (statePtr->reorderBuf[statePtr->regResult[rs1].reorderNum].instrStatus == COMMITTING) {
                statePtr->reservation[execUnit].Vj = statePtr->reorderBuf[statePtr->regResult[rs1].reorderNum].result;
                statePtr->reservation[execUnit].Qj = -1;
              } else {
                statePtr->reservation[execUnit].Qj = statePtr->regResult[rs1].reorderNum;
              }
            }
            // Vk, Qk
            if (opcode(instr) == BEQZ) {
              statePtr->reservation[execUnit].Vk = statePtr->pc + 1;
              statePtr->reservation[execUnit].Qk = -1;
            } else if (opcode(instr) == SW) {
              int rd = field1(instr);
              if (statePtr->regResult[rd].valid == 1) {
                statePtr->reservation[execUnit].Vk = statePtr->regFile[rd];
                statePtr->reservation[execUnit].Qk = -1;
              } else {
                if (statePtr->reorderBuf[statePtr->regResult[rd].reorderNum].instrStatus == COMMITTING) {
                  statePtr->reservation[execUnit].Vk = statePtr->reorderBuf[statePtr->regResult[rd].reorderNum].result;
                  statePtr->reservation[execUnit].Qk = -1;
                } else {
                  statePtr->reservation[execUnit].Qk = statePtr->regResult[rd].reorderNum;
                }
              }
            } else {
              statePtr->reservation[execUnit].Vk = 0;
              statePtr->reservation[execUnit].Qk = -1;
            }
            if (opcode(instr) == ADDI || opcode(instr) == ANDI) {  // ADDI, ANDI
              statePtr->reservation[execUnit].exTimeLeft = 1;
            } else if (opcode(instr) == LW || opcode(instr) == SW) {  // LW, SW
              statePtr->reservation[execUnit].exTimeLeft = 2;
            } else {  // BEQZ
              statePtr->reservation[execUnit].exTimeLeft = 3;
            }
            statePtr->reservation[execUnit].reorderNum = tailRB;
            // 更新寄存器状态
            if (opcode(instr) == ADDI || opcode(instr) == ANDI || opcode(instr) == LW) {
              int rd = field1(instr);
              statePtr->regResult[rd].valid = 0;
              statePtr->regResult[rd].reorderNum = tailRB;
            }
            /*
             * 选作内容:
             * 在发射跳转指令时, 将PC修改为正确的目标: 是pc = pc+1, 还是pc = 跳转目标?
             * 在发射其他的指令时, 只需要设置pc = pc+1.
             */
            if (opcode(instr) == BEQZ) {
              int isCached = 0;
              for (int i = 0; i < BTBSIZE; i++) {
                if (statePtr->btBuf[i].branchPC == statePtr->pc) {
                  isCached = 1;
                  if (statePtr->btBuf[i].branchPred == STRONGTAKEN || statePtr->btBuf[i].branchPred == WEAKTAKEN) {
                    statePtr->pc = statePtr->btBuf[i].branchTarget;
                  } else {
                    statePtr->pc++;
                  }
                  break;
                }
              }
              if (isCached == 0) {
                int isFull = 1;
                // 更新 BTB
                for (int i = 0; i < BTBSIZE; i++) {
                  if (statePtr->btBuf[i].valid == 0) {
                    isFull = 0;
                    statePtr->btBuf[i].valid = 1;
                    statePtr->btBuf[i].branchPC = statePtr->pc;
                    statePtr->btBuf[i].branchPred = STRONGNOT;
                    statePtr->btBuf[i].branchTarget = statePtr->pc + 1;
                    break;
                  }
                }
                // 如果缓冲栈已满
                if (isFull == 1) {
                  int rand = statePtr->cycles % BTBSIZE;
                  statePtr->btBuf[rand].valid = 1;
                  statePtr->btBuf[rand].branchPC = statePtr->pc;
                  statePtr->btBuf[rand].branchPred = STRONGNOT;
                  statePtr->btBuf[rand].branchTarget = statePtr->pc + 1;
                }
                // 更新 PC
                statePtr->pc++;
              }
            } else {
              statePtr->pc++;
            }
          }
        } else {  // J 型指令
          if (!statePtr->reservation[INT1].busy || !statePtr->reservation[INT2].busy) {
            int execUnit = (statePtr->reservation[INT1].busy == 1) ? INT2 : INT1;
            // 提交到 ROB
            tailRB = (tailRB + 1) % RBSIZE;
            statePtr->reorderBuf[tailRB].busy = 1;
            statePtr->reorderBuf[tailRB].instr = instr;
            statePtr->reorderBuf[tailRB].execUnit = execUnit;
            statePtr->reorderBuf[tailRB].instrStatus = ISSUING;
            statePtr->reorderBuf[tailRB].valid = 0;
            // 提交到保留站
            statePtr->reservation[execUnit].busy = 1;
            statePtr->reservation[execUnit].instr = instr;
            // Vk, Qk
            statePtr->reservation[execUnit].Vk = statePtr->pc + 1;
            statePtr->reservation[execUnit].Qk = -1;
            statePtr->reservation[execUnit].exTimeLeft = 1;
            statePtr->reservation[execUnit].reorderNum = tailRB;
            // 更新 PC
            statePtr->pc++;
          }
        }
      }
    }
	    
    /*
    * 周期计数加1
    */
    statePtr->cycles++;
  }  /* while (1) */
	// printf("halting machine\n");
  printFileState(statePtr, memorySize);

  printf("%d", statePtr->cycles);

  return 0;
}
