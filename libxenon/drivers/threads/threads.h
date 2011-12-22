#ifndef THREADS_H
#define	THREADS_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#define MAX_THREAD_COUNT 256
    
typedef void (*thread_interrupt_proc)(unsigned int);
typedef unsigned int (*thread_ipi_proc)(unsigned int);
typedef int (*thread_proc)(void*);

#pragma pack(push, 1)

typedef struct _PROCESSOR_FPU_VPU_SAVE
{
    double Fpr[32]; // Floating save
    double Fpscr; // Floating status save
    // float VrSave[128][4]; // Vector save (UNUSED)
    // float VscrSave[4]; // Vector status save (UNUSED)
} PROCESSOR_FPU_VPU_SAVE; // 0x108

// Thread context
typedef struct _CONTEXT
{
    unsigned long long Msr;   // Machine State
    unsigned long long Iar;   // Instruction Address
    unsigned long long Lr;    // Link
    unsigned long long Ctr;   // Counter
    
    // General purpose
    unsigned long long Gpr[32];
    
    unsigned long long Cr;    // Condition
    unsigned long long Xer;   // Fixed Point Exception
    
    PROCESSOR_FPU_VPU_SAVE FpuVpu; // Floating/Vector save
} CONTEXT, *PCONTEXT; // 0x238

// The structure that lives on register 13
typedef struct _PROCESSOR_DATA_BLOCK
{
    // Register space for external interrupts
    unsigned long long RegisterSave[32];
    unsigned long long LRSave;
    unsigned long long CTRSave;
    unsigned long long CRSave;
    unsigned long long XERSave;
    unsigned long long IARSave;      // Also SRR0 (0x120)
    unsigned long long MSRSave;      // Also SRR1 (0x128)
    PROCESSOR_FPU_VPU_SAVE *FPUVPUSave; // Saves the other regs (0x130)
    unsigned int DAR;
    unsigned long long Reserved1; // Data Address for segfaults
    
    unsigned char CurrentProcessor;  // What processor are we? (offset 0x140)
    unsigned char Irq;               // Interrupt request level (offset 0x141)
    unsigned char Reserved2[2];         // Reserved
    
    // Thread List
    struct _THREAD *FirstThread;
    struct _THREAD *LastThread;
    //struct _THREAD *ListPtr;
    
    // Thread switch process queue
    struct _THREAD *FirstSwapProcess;
    struct _THREAD *LastSwapProcess;
    unsigned int SwapProcessLock;
    
    thread_interrupt_proc InterruptTable[0x20]; // Interrupt function pointers
    
    // IPI data
    unsigned int IpiLock; // Lock
    thread_ipi_proc IpiProc;
    unsigned int IpiContext;
    unsigned volatile int *IpiIncrement; // This ptr is incremented after Ipi completion
    
    // Scheduling stuff
    long long QuantumEnd; // When this quantum ends (clock)
    struct _THREAD *CurrentThread; // Currently running thread
    
    // Locks
    unsigned int Lock; // To synchronize access
    
    // Recursion
    volatile unsigned int ExceptionRecursion; // To synchronize access to the interrupts
    
} PROCESSOR_DATA_BLOCK;

// The thread structure
typedef struct _THREAD
{
    // Thread Context
    CONTEXT Context; // 0
    
    // Assigned Processor
    PROCESSOR_DATA_BLOCK *ThisProcessor; // 0x238
    
    // Thread List
    struct _THREAD *NextThread; // 0x23C
    struct _THREAD *PreviousThread; // 0x240
    
    // List of all threads
    struct _THREAD *NextThreadFull; // 0x244
    struct _THREAD *PreviousThreadFull; // 0x248
    
    // Ready list
    struct _THREAD *PreviousThreadReady; // 0x24C
    struct _THREAD *NextThreadReady; // 0x250
    
    // If the object is valid
    unsigned char Valid; // 0x254
    // Priority
    unsigned char Priority; // 0x255
    // Priority boost
    unsigned char PriorityBoost; // 0x256
    // Maximum Priority Boost
    unsigned char MaxPriorityBoost; // 0x257
    // If we are currently running this thread
    unsigned char ThreadIsRunning; // 0x258
    // Our suspend count
    unsigned char SuspendCount; // A count of zero means we can't be resumed anymore (running) // 0x259
    // If the handle is still open
    unsigned char HandleOpen; // You have to close this before we dealloc the thread object!! // 0x25A
    // The thread ID (its unique, i promise)
    unsigned char ThreadId; // 0x25B
    // If we should kill this thread off in the scheduler
    unsigned char ThreadTerminated; // 0x25C
    unsigned char Reserved[3]; // 0x25D
    
    void *DebugData; // Just a pointer so you can stick whatever you want on the object // 0x260
    
    // To wake a thread up from sleep,
                // just set SleepTime to zero, scheduler will handle the rest
    long long SleepTime; // How long until we wake up (milliseconds * 2500) // 0x264
    
    // TODO: Have a list of objects that if signaled, will wake us up
    
    char * StackBase; // The bottom of our stack // 0x26C
    unsigned int StackSize; // The size of our stack // 0x270
    
} THREAD, *PTHREAD; // 0x274

// A list of threads
typedef struct _THREAD_LIST
{
    PTHREAD FirstThread;
    PTHREAD LastThread;
} THREAD_LIST, *PTHREAD_LIST;
#pragma pack(pop)

#define thread_get_processor_block() ({PROCESSOR_DATA_BLOCK *rval; \
      asm volatile("mr %0, 13" : "=r" (rval)); rval;})

// Init
void threading_init();
// Shutdown
void threading_shutdown();

// Thread management

#define THREAD_FLAG_CREATE_SUSPENDED 1 // Creates the thread with 1 suspend

// Returns thread pointer
PTHREAD thread_create(void* entrypoint, unsigned int stack_size,
        void* argument, unsigned int flags);

// Call this to free up the handle for use by other threads, after calling
// DO NOT TOUCH THE HANDLE AGAIN
void thread_close(PTHREAD pthr);

// Returns the pointer to the current thread, feel free to use on the thread you fetch,
// As you are running, thus your pointer is valid
PTHREAD thread_get_current();

// Swap the thread's processor
void thread_set_processor(PTHREAD pthr, unsigned int processor);

// Suspend/Resume thread
// Both return the value of the suspend count BEFORE the function goes through
// If it can't suspend/resume more, it returns -1
int thread_suspend(PTHREAD pthr);
int thread_resume(PTHREAD pthr);

// Set thread priority (0-15), 7 is default, 0 is idle thread
void thread_set_priority(PTHREAD pthr, unsigned int priority);
// Set the priority boost, 5 is default
void thread_set_priority_boost(PTHREAD pthr, unsigned int boost);

// Set the quantum for the scheduling engine, default quantum is 20ms
void process_set_quantum_length(unsigned int milliseconds);

// End this thread
void thread_terminate(unsigned int returnCode);

// Sleep
void thread_sleep(int milliseconds);

// Raise/Lower irql
int thread_raise_irql(unsigned int irql);
int thread_lower_irql(unsigned int irql);

// Raises IRQL to 2, then locks
int thread_spinlock(unsigned int *addr);
// Unlocks, then lowers irql to irql
void thread_unlock(unsigned int *addr, unsigned int irql);

// Runs "entrypoint" on all processors at the same time
// Return value is value of processor this is called on
unsigned int thread_send_ipi(thread_ipi_proc entrypoint, unsigned int context);

// These disable and enable interrupts, which also disables scheduling
// USE SPARINGLY!!
// (These only affect the core you are on)
unsigned int thread_disable_interrupts();
void thread_enable_interrupts(unsigned int msr);

// Flush context
void dump_thread_context(CONTEXT *context);
void restore_thread_context(CONTEXT *context);

#ifdef	__cplusplus
}
#endif

#endif	/* THREADS_H */

