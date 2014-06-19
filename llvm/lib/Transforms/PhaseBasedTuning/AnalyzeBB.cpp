//===- AnalyzeBB.cpp - Performs analysis on basic blocks ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file gathers metrics for instruction type and reuse distances.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "analyzebb"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/InstVisitor.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define MIN_BB_SIZE 5

STATISTIC(BBCount, "Number of basic blocks");
STATISTIC(ActualBBCount, "Number of basic blocks analyzed");

struct BBMetrics {
  int SimpleInst; // e.g.: add, sub
  int ComplexInst; // e.g.: mul, div
  int MemoryInst; // e.g.: load, store
  int CallInst; // Call and Invoke instrs
  float AvgDepDist; // Average reuse distance
};

namespace {
  struct AnalyzeBB: public BasicBlockPass,
                    public InstVisitor<AnalyzeBB, Instruction*> {
    static char ID;
    DenseMap<const BasicBlock *, BBMetrics> BBMetricsMap;
    AnalyzeBB() : BasicBlockPass(ID) { }

    virtual bool runOnBasicBlock(BasicBlock &BB);

    void visitCallSite(CallSite CS);
    void visitLoadInstruction(LoadInst &I);
    void visitStoreInstruction(StoreInst &I);
    void visitCmpInst(CmpInst &I);
    void visitInstruction(BinaryOperator &I);

    // We don't modify the program, so we preserve all analyses.
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<TargetTransformInfo>();
      AU.setPreservesAll();
    }
  };

  AnalyzeBB::AnalyzeBB() {
    BBMetricsMap = new DenseMap<const BasicBlock *, BBMetrics>;
  }

  bool AnalyzeBB::runOnBasicBlock(BasicBlock &BB) {
    ++BBCount;

    if (BB.size() < MIN_BB_SIZE) {
      return false;
    }

    ++ActualBBCount;

    // Calculate opcode metrics
    for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E; ++I) {
      Instruction *II = cast<Instruction>(I);
      switch (II->getOpcode()) {
      case Instruction::Mul:
      case Instruction::FMul:
      case Instruction::UDiv:
      case Instruction::SDiv:
      case Instruction::FDiv:
      case Instruction::URem:
      case Instruction::SRem:
      case Instruction::FRem:
        BBMetricsMap[&BB].ComplexInst++;
        break;
      case Instruction::Store:
      case Instruction::Load:
      case Instruction::AtomicCmpXchg:
      case Instruction::AtomicRMW:
      case Instruction::GetElementPtr:
        BBMetricsMap[&BB].MemoryInst++;
        break;
      case Instruction::Call:
      case Instruction::Invoke:
        BBMetricsMap[&BB].CallInst++;
        break;
      default:
        if (II->isBinaryOp() || isa<CmpInst>(II)) {
          BBMetricsMap[&BB].SimpleInst++;
        }
      }
    }

    return false;
  }
}

char AnalyzeBB::ID = 0;
static RegisterPass<AnalyzeBB>
Y("analyzeBB", "Analyze basic blocks for phase information");
