#include "idt.h"
#include "pic.h"

uint64_t countdown = 0;
void     timer_handler() {
    if (countdown > 0) {
        countdown--;
    }
    PIC_sendEOI(PIC_EOI);
}

void sleep(uint64_t ms) {
	countdown = ms;
	while (countdown > 0) {
		asm volatile("hlt");
	}
}

void num_to_error_name(uint8_t interrupt_number, uint8_t* error_name) {
	switch (interrupt_number) {
	case 0:
		strcat(error_name, "Divide Error");
		break;
	case 1:
		strcat(error_name, "Debug");
		break;
	case 2:
		strcat(error_name, "NMI Interrupt");
		break;
	case 3:
		strcat(error_name, "Breakpoint");
		break;
	case 4:
		strcat(error_name, "Overflow");
		break;
	case 5:
		strcat(error_name, "Bound Range Exceeded");
		break;
	case 6:
		strcat(error_name, "Invalid Opcode");
		break;
	case 7:
		strcat(error_name, "Device Not Available");
		break;
	case 8:
		strcat(error_name, "Double Fault");
		break;
	case 9:
		strcat(error_name, "Coprocessor Segment Overrun");
		break;
	case 10:
		strcat(error_name, "Invalid TSS");
		break;
	case 11:
		strcat(error_name, "Segment Not Present");
		break;
	case 12:
		strcat(error_name, "Stack-Segment Fault");
		break;
	case 13:
		strcat(error_name, "General Protection Fault");
		break;
	case 14:
		strcat(error_name, "Page Fault");
		break;
	case 15:
		strcat(error_name, "Reserved");
		break;
	case 16:
		strcat(error_name, "x87 FPU Floating-Point Error");
		break;
	case 17:
		strcat(error_name, "Alignment Check");
		break;
	case 18:
		strcat(error_name, "Machine-Check");
		break;
	case 19:
		strcat(error_name, "SIMD Floating-Point Exception");
		break;
	case 20:
		strcat(error_name, "Virtualization Exception");
		break;
	case 21:
		strcat(error_name, "Reserved");
		break;
	case 22:
		strcat(error_name, "Reserved");
		break;
	case 23:
		strcat(error_name, "Reserved");
		break;
	case 24:
		strcat(error_name, "Reserved");
		break;
	case 25:
		strcat(error_name, "Reserved");
		break;
	case 26:
		strcat(error_name, "Reserved");
		break;
	case 27:
		strcat(error_name, "Reserved");
		break;
	case 28:
		strcat(error_name, "Hypervisor Injection Exception");
		break;
	case 29:
		strcat(error_name, "VMM Communication Exception");
		break;
	case 30:
		strcat(error_name, "Security Exception");
		break;
	case 31:
		strcat(error_name, "Reserved");
		break;
	default:
		strcat(error_name, "Unknown interrupt, probably reserved");
		break;
	}
}

typedef struct {
	// The lower 16 bits of the ISR's address
	uint16_t isr_low;
	// The GDT segment selector that the CPU will load into
	// CS before calling the ISR
	uint16_t kernel_cs;
	// The IST in the TSS that the CPU will load into RSP; set to
	// zero for now
	uint8_t  ist;
	// Type and attributes; see the IDT page
	uint8_t  attributes;
	// The higher 16 bits of the lower 32 bits of the ISR's address
	uint16_t isr_mid;
	// The higher 32 bits of the ISR's address
	uint32_t isr_high;
	// Set to zero
	uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed)) idtr_t;

// Create an array of IDT entries; aligned for performance
__attribute__((aligned(0x10))) static idt_entry_t idt[IDT_MAX_DESCRIPTORS];

static idtr_t idtr;

extern void* isr_stub_table[];

// This does not return, but gcc will complain if we give it the noreturn
// attribute... So here it is for your benefit.
//
// __attribute__((noreturn))
void exception_handler() {
	__asm__ volatile("hlt"); // Completely hangs the computer
}

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
	idt_entry_t* descriptor = &idt[vector];

	descriptor->isr_low    = (uint64_t)isr & 0xFFFF;
	descriptor->isr_mid    = ((uint64_t)isr >> 16) & 0xFFFF;
	descriptor->isr_high   = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
	descriptor->kernel_cs  = 0x08; // Refers to the offset of gdt64.code from gdt64.
	descriptor->ist        = 0;
	descriptor->attributes = flags;
	descriptor->reserved   = 0;
}

void idt_init() {
	// Remap PIC interrupt numbers.
	PIC_remap(0x20, 0x28);

	debug("idt_init: initating IDT");
	idtr.base  = (uint64_t)&idt[0];
	idtr.limit = (uint16_t)sizeof(idt_entry_t) * IDT_MAX_DESCRIPTORS - 1;

	// Highest number should correspond to the length of isr_stub_table in
	// `idt.asm`.
	for (uint8_t vector = 0; vector <= 47; vector++) {
		// 64-bit Interrupt Gate: 0x8E (p=1, dpl=0b00, type=0b1110 => type_attributes=0b1000_1110=0x8E)
		idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
	}

	enable_mouse();
	enable_ata();

	// Load the new IDT.
	__asm__ volatile("lidt %0" : : "m"(idtr));

	// Set the interrupt flag.
	__asm__ volatile("sti");

	// TODO: This print is broken? If we put it before `sti`, it works.
	debug("idt_init: IDT initialized");
}
