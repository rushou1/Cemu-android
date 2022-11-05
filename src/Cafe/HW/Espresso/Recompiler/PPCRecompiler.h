#pragma once

#define PPC_REC_CODE_AREA_START		(0x00000000) // lower bound of executable memory area. Recompiler expects this address to be 0
#define PPC_REC_CODE_AREA_END		(0x10000000) // upper bound of executable memory area
#define PPC_REC_CODE_AREA_SIZE		(PPC_REC_CODE_AREA_END - PPC_REC_CODE_AREA_START)

#define PPC_REC_ALIGN_TO_4MB(__v)	(((__v)+4*1024*1024-1)&~(4*1024*1024-1))

#define PPC_REC_MAX_VIRTUAL_GPR		(40) // enough to store 32 GPRs + a few SPRs + temp registers (usually only 1-2)

struct ppcRecRange_t
{
	uint32 ppcAddress;
	uint32 ppcSize;
	void* storedRange;
};

struct PPCRecFunction_t
{
	uint32 ppcAddress;
	uint32 ppcSize; // ppc code size of function
	void*  x86Code; // pointer to x86 code
	size_t x86Size;
	std::vector<ppcRecRange_t> list_ranges;
};

#include "Cafe/HW/Espresso/Recompiler/IML/IMLInstruction.h"

typedef struct _ppcRecompilerSegmentPoint_t
{
	sint32 index;
	struct IMLSegment* imlSegment;
	_ppcRecompilerSegmentPoint_t* next;
	_ppcRecompilerSegmentPoint_t* prev;
}ppcRecompilerSegmentPoint_t;

struct raLivenessLocation_t
{
	sint32 index;
	bool isRead;
	bool isWrite;

	raLivenessLocation_t() = default;

	raLivenessLocation_t(sint32 index, bool isRead, bool isWrite)
		: index(index), isRead(isRead), isWrite(isWrite) {};
};

struct raLivenessSubrangeLink_t
{
	struct raLivenessSubrange_t* prev;
	struct raLivenessSubrange_t* next;
};

struct raLivenessSubrange_t
{
	struct raLivenessRange_t* range;
	IMLSegment* imlSegment;
	ppcRecompilerSegmentPoint_t start;
	ppcRecompilerSegmentPoint_t end;
	// dirty state tracking
	bool _noLoad;
	bool hasStore;
	bool hasStoreDelayed;
	// next
	raLivenessSubrange_t* subrangeBranchTaken;
	raLivenessSubrange_t* subrangeBranchNotTaken;
	// processing
	uint32 lastIterationIndex;
	// instruction locations
	std::vector<raLivenessLocation_t> list_locations;
	// linked list (subranges with same GPR virtual register)
	raLivenessSubrangeLink_t link_sameVirtualRegisterGPR;
	// linked list (all subranges for this segment)
	raLivenessSubrangeLink_t link_segmentSubrangesGPR;
};

struct raLivenessRange_t
{
	sint32 virtualRegister;
	sint32 physicalRegister;
	sint32 name;
	std::vector<raLivenessSubrange_t*> list_subranges;
};

struct PPCSegmentRegisterAllocatorInfo_t
{
	// analyzer stage
	bool isPartOfProcessedLoop{}; // used during loop detection
	sint32 lastIterationIndex{};
	// linked lists
	raLivenessSubrange_t* linkedList_allSubranges{};
	raLivenessSubrange_t* linkedList_perVirtualGPR[PPC_REC_MAX_VIRTUAL_GPR]{};
};

struct PPCRecVGPRDistances_t
{
	struct _RegArrayEntry
	{
		sint32 usageStart{};
		sint32 usageEnd{};
	}reg[PPC_REC_MAX_VIRTUAL_GPR];
	bool isProcessed[PPC_REC_MAX_VIRTUAL_GPR]{};
};

#include "Cafe/HW/Espresso/Recompiler/IML/IMLSegment.h"

struct IMLInstruction* PPCRecompilerImlGen_generateNewEmptyInstruction(struct ppcImlGenContext_t* ppcImlGenContext);

struct ppcImlGenContext_t
{
	PPCRecFunction_t* functionRef;
	uint32* currentInstruction;
	uint32  ppcAddressOfCurrentInstruction;
	// fpr mode
	bool LSQE{ true };
	bool PSE{ true };
	// cycle counter
	uint32 cyclesSinceLastBranch; // used to track ppc cycles
	// temporary general purpose registers
	uint32 mappedRegister[PPC_REC_MAX_VIRTUAL_GPR];
	// temporary floating point registers (single and double precision)
	uint32 mappedFPRRegister[256];
	// list of intermediate instructions
	IMLInstruction* imlList;
	sint32 imlListSize;
	sint32 imlListCount;
	// list of segments
	std::vector<IMLSegment*> segmentList2;
	// code generation control
	bool hasFPUInstruction; // if true, PPCEnter macro will create FP_UNAVAIL checks -> Not needed in user mode
	// register allocator info
	struct  
	{
		std::vector<raLivenessRange_t*> list_ranges;
	}raInfo;
	// analysis info
	struct  
	{
		bool modifiesGQR[8];
	}tracking;

	// append raw instruction
	IMLInstruction& emitInst()
	{
		return *PPCRecompilerImlGen_generateNewEmptyInstruction(this);
	}
};

typedef void ATTR_MS_ABI (*PPCREC_JUMP_ENTRY)();

typedef struct  
{
	PPCRecFunction_t* ppcRecompilerFuncTable[PPC_REC_ALIGN_TO_4MB(PPC_REC_CODE_AREA_SIZE/4)]; // one virtual-function pointer for each potential ppc instruction
	PPCREC_JUMP_ENTRY ppcRecompilerDirectJumpTable[PPC_REC_ALIGN_TO_4MB(PPC_REC_CODE_AREA_SIZE/4)]; // lookup table for ppc offset to native code function
	// x64 data
	alignas(16) uint64 _x64XMM_xorNegateMaskBottom[2];
	alignas(16) uint64 _x64XMM_xorNegateMaskPair[2];
	alignas(16) uint64 _x64XMM_xorNOTMask[2];
	alignas(16) uint64 _x64XMM_andAbsMaskBottom[2];
	alignas(16) uint64 _x64XMM_andAbsMaskPair[2];
	alignas(16) uint32 _x64XMM_andFloatAbsMaskBottom[4];
	alignas(16) uint64 _x64XMM_singleWordMask[2];
	alignas(16) double _x64XMM_constDouble1_1[2];
	alignas(16) double _x64XMM_constDouble0_0[2];
	alignas(16) float  _x64XMM_constFloat0_0[2];
	alignas(16) float  _x64XMM_constFloat1_1[2];
	alignas(16) float  _x64XMM_constFloatMin[2];
	alignas(16) uint32 _x64XMM_flushDenormalMask1[4];
	alignas(16) uint32 _x64XMM_flushDenormalMaskResetSignBits[4];
	// PSQ load/store scale tables
	double _psq_ld_scale_ps0_ps1[64 * 2];
	double _psq_ld_scale_ps0_1[64 * 2];
	double _psq_st_scale_ps0_ps1[64 * 2];
	double _psq_st_scale_ps0_1[64 * 2];
	// MXCSR
	uint32 _x64XMM_mxCsr_ftzOn;
	uint32 _x64XMM_mxCsr_ftzOff;
}PPCRecompilerInstanceData_t;

extern PPCRecompilerInstanceData_t* ppcRecompilerInstanceData;
extern bool ppcRecompilerEnabled;

void PPCRecompiler_init();

void PPCRecompiler_allocateRange(uint32 startAddress, uint32 size);

void PPCRecompiler_invalidateRange(uint32 startAddr, uint32 endAddr);

extern void ATTR_MS_ABI (*PPCRecompiler_enterRecompilerCode)(uint64 codeMem, uint64 ppcInterpreterInstance);
extern void ATTR_MS_ABI (*PPCRecompiler_leaveRecompilerCode_visited)();
extern void ATTR_MS_ABI (*PPCRecompiler_leaveRecompilerCode_unvisited)();

#define PPC_REC_INVALID_FUNCTION	((PPCRecFunction_t*)-1)

// todo - move some of the stuff above into PPCRecompilerInternal.h

// recompiler interface

void PPCRecompiler_recompileIfUnvisited(uint32 enterAddress);
void PPCRecompiler_attemptEnter(struct PPCInterpreter_t* hCPU, uint32 enterAddress);
void PPCRecompiler_attemptEnterWithoutRecompile(struct PPCInterpreter_t* hCPU, uint32 enterAddress);
